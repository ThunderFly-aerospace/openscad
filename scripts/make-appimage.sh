#!/usr/bin/env bash
#
# make-appimage.sh — postaví přenositelný OpenSCAD .AppImage v Docker/Podman
# kontejneru (Ubuntu 22.04, glibc 2.35).
#
# Je to soběstačná lokální varianta CI receptu z .circleci/config.yml
# (job "openscad-appimage"), bez CI specifik (sccache, podpis GPG).
#
# PROČ KONTEJNER: linuxdeploy bundluje knihovny, ALE NE glibc. Výsledná
# AppImage běží jen tam, kde glibc >= glibc, proti níž se buildilo. Ubuntu
# 22.04 v image má glibc 2.35 → AppImage poběží na Ubuntu 22.04+ a čemkoli
# s glibc >= 2.35. (Nativní build na Debian 13 = glibc 2.41 by běžel jen na
# bleeding-edge distrech — proto se staví v kontejneru.)
#
# POŽADAVKY: docker nebo podman.
#
# Použití:
#   scripts/make-appimage.sh [--headless] [OUT_DIR]
#   --headless  postavit CLI variantu bez Qt GUI (-DHEADLESS=ON), ALE s OpenGL,
#               takže render kamer (--export-cameras, PNG) dál funguje. Menší,
#               míň závislostí; jméno dostane příponu "_headless".
#   OUT_DIR     kam uložit .AppImage (default: kořen repa)
#
# Výsledek: OUT_DIR/OpenSCAD-<verze>[_<vetev>][_headless]-x86_64.AppImage
#
set -euo pipefail

# --- cesty + režim -----------------------------------------------------------
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]:-$0}" )" && pwd )"
REPO="$( cd "$SCRIPT_DIR/.." && pwd )"

HEADLESS_MODE=0
OUT=""
for arg in "$@"; do
  case "$arg" in
    --headless) HEADLESS_MODE=1 ;;
    *)          OUT="$arg" ;;
  esac
done
OUT="${OUT:-$REPO}"
IMAGE="openscad-appimage-base"

if [ "$HEADLESS_MODE" -eq 1 ]; then
  BUILD_DIR="build-appimage-headless"   # oddělený od GUI varianty i nativního build/
  CMAKE_HEADLESS_FLAG="-DHEADLESS=ON"    # bez Qt GUI; NULLGL NEzapínat → GL zůstane
  LD_QT_PLUGIN=""                        # headless binárka nemá Qt → žádný qt plugin
  OUTPUT_TAG="_headless"
  echo ">> režim: HEADLESS (bez Qt GUI, s OpenGL pro render kamer)"
else
  BUILD_DIR="build-appimage"            # oddělený od nativního build/ — nepřepsat ho!
  CMAKE_HEADLESS_FLAG=""
  LD_QT_PLUGIN="--plugin qt"
  OUTPUT_TAG=""
fi

mkdir -p "$OUT"
OUT="$( cd "$OUT" && pwd )"
cd "$REPO"

# --- container runtime -------------------------------------------------------
if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
  RT=docker
elif command -v podman >/dev/null 2>&1; then
  RT=podman
else
  echo "CHYBA: není dostupný docker ani podman. Spusť tenhle skript na stroji s kontejnery." >&2
  exit 1
fi
echo ">> container runtime: $RT"

# --- verze / commit (stejný zdroj jako CI) -----------------------------------
# shellcheck source=/dev/null
source "$SCRIPT_DIR/establish_version.sh"
OPENSCAD_VERSION="$(openscad_version)"
OPENSCAD_COMMIT="$(openscad_commit)"
# suffix z názvu větve (jako CI), prázdný na master
BRANCH="$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo '')"
if [ -n "$BRANCH" ] && [ "$BRANCH" != master ] && [ "$BRANCH" != HEAD ]; then
  SUFFIX="_$(echo "$BRANCH" | sed -e 's,/,.,g')"
else
  SUFFIX=""
fi
echo ">> verze: ${OPENSCAD_VERSION}${SUFFIX}  commit: ${OPENSCAD_COMMIT}"

# --- 1) základní image (Ubuntu 22.04 + build deps + linuxdeploy) -------------
echo ">> stavím základní image '$IMAGE' (Dockerfile: appimage-base target)…"
"$RT" build \
  --file scripts/docker/appimage-base/Dockerfile \
  --target appimage-base \
  --tag "$IMAGE" \
  .

# --- 2) build recept, který poběží UVNITŘ kontejneru -------------------------
# (převzato z APPIMAGE_BUILD_SCRIPT v .circleci/config.yml; sccache a GPG podpis
#  vynechány. Buildí do build-appimage/, aby nešahal na nativní build/.)
INNER="$(mktemp)"
trap 'rm -f "$INNER"' EXIT
cat > "$INNER" <<INNER_EOF
#!/usr/bin/env bash
set -euo pipefail
cd /openscad
rm -rf "${BUILD_DIR}/AppDir"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
cmake .. \
    -DOPENSCAD_VERSION="\${OPENSCAD_VERSION}" \
    -DOPENSCAD_COMMIT="\${OPENSCAD_COMMIT}" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_BUILTIN_OPENCSG=ON \
    -DMANIFOLD_PYBIND=OFF \
    -DMANIFOLD_TEST=OFF \
    -DENABLE_PYTHON=ON \
    -DENABLE_TESTS=OFF \
    -DUSE_QT6=ON \
    -DSNAPSHOT=ON \
    -DEXPERIMENTAL=ON \
    ${CMAKE_HEADLESS_FLAG}
cmake --build . -j"\$(nproc)"
DESTDIR=./AppDir cmake --install .

export PATH=/appimage/usr/bin:"\${PATH}"
export EXTRA_QT_PLUGINS=svg
export PYTHON_VERSION=\$(python3 -c 'import sys; v=sys.version_info; print(f"{v.major}.{v.minor}.{v.micro}")')
export LINUXDEPLOY_OUTPUT_VERSION="\${OPENSCAD_VERSION}\${SUFFIX}${OUTPUT_TAG}"
export QMAKE=/usr/bin/qmake6
linuxdeploy --plugin python ${LD_QT_PLUGIN} --output appimage --appdir AppDir

mkdir -p /out
cp -v OpenSCAD-*.AppImage /out/
INNER_EOF
chmod +x "$INNER"

# --- 3) spustit build v kontejneru ------------------------------------------
echo ">> spouštím build v kontejneru…"
"$RT" run --rm \
  -e OPENSCAD_VERSION="$OPENSCAD_VERSION" \
  -e OPENSCAD_COMMIT="$OPENSCAD_COMMIT" \
  -e SUFFIX="$SUFFIX" \
  -v "$REPO:/openscad" \
  -v "$OUT:/out" \
  -v "$INNER:/tmp/inner-build.sh:ro" \
  -w /openscad \
  "$IMAGE" \
  /tmp/inner-build.sh

# --- 4) srovnat vlastnictví (docker píše jako root) --------------------------
if [ "$RT" = docker ]; then
  "$RT" run --rm -v "$REPO:/openscad" -v "$OUT:/out" "$IMAGE" \
    chown -R "$(id -u):$(id -g)" "/openscad/${BUILD_DIR}" /out 2>/dev/null || true
fi

echo
echo ">> hotovo:"
ls -lh "$OUT"/OpenSCAD-*.AppImage 2>/dev/null | awk '{print "   "$5"\t"$NF}'
echo
echo "Spuštění na cílovém stroji (glibc >= 2.35, tj. Ubuntu 22.04+):"
echo "   chmod +x OpenSCAD-*.AppImage"
echo "   ./OpenSCAD-*.AppImage"
echo "Pozn.: Ubuntu 24.04+ má FUSE3 → buď 'sudo apt install libfuse2t64',"
echo "       nebo './OpenSCAD-*.AppImage --appimage-extract-and-run'."
