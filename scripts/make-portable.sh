#!/usr/bin/env bash
#
# make-portable.sh — zabalí zkompilovaný OpenSCAD (větev cam_and_drawings) do
# přenositelného tarballu pro jiný STEJNÝ Debian 13.
#
# Vyrobí jediný archiv openscad-cam-bin.tgz, který se rozbalí do složky
# openscad-cam/ s tímto layoutem (binárka je v ROOTu složky, ne v build/):
#
#   openscad-cam/
#     openscad            <- spustitelná binárka
#     color-schemes/ shaders/ locale/ libraries/ fonts/ templates/ examples/
#     install-deps.sh     <- apt-get install běhových závislostí
#     runtime-pkgs.txt    <- seznam běhových balíků (z ldd -> dpkg)
#     README.txt
#
# OpenSCAD najde resource složky vedle binárky (PlatformUtils.cc: searchpath "."),
# takže stačí spustit ./openscad z rozbalené složky.
#
# Použití:
#   scripts/make-portable.sh [REPO_DIR] [OUT_DIR]
#   REPO_DIR  kořen repa openscad (default: odvozeno z umístění skriptu)
#   OUT_DIR   kam uložit .tgz     (default: /home/user)
#
set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]:-$0}" )" && pwd )"
REPO="${1:-$( cd "$SCRIPT_DIR/.." && pwd )}"
OUT="${2:-/home/user}"
BIN="$REPO/build/openscad"
NAME="openscad-cam"

# Resource adresáře, které openscad čte za běhu (hledá je vedle binárky).
RES_DIRS=(color-schemes shaders locale libraries fonts templates examples)

[ -x "$BIN" ] || { echo "CHYBA: nenalezena binárka $BIN — nejdřív ji přelož (ninja -C build openscad)"; exit 1; }
cd "$REPO"

# --- staging: sestav strom openscad-cam/ s binárkou v rootu -------------------
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
DEST="$STAGE/$NAME"
mkdir -p "$DEST"

echo ">> skládám složku $NAME/ (binárka v rootu + resource složky)…"
cp -a "$BIN" "$DEST/openscad"
cp -a "${RES_DIRS[@]}" "$DEST/"

# --- běhové balíky (ldd -> dpkg) ---------------------------------------------
echo ">> zjišťuji běhové balíky (ldd -> dpkg)…"
ldd "$BIN" | grep -oE '/[^ ]+\.so[^ ]*' | sort -u | while read -r so; do
  real="$(readlink -f "$so")"
  dpkg -S "$real" 2>/dev/null | awk -F: -v r="$real" '$0 ~ (": " r "$"){print $1}'
done | sort -u > "$DEST/runtime-pkgs.txt"
# libstdc++6 se kvůli "++" v ldd/grep občas ztratí — doplň ho natvrdo.
grep -qx libstdc++6 "$DEST/runtime-pkgs.txt" || echo libstdc++6 >> "$DEST/runtime-pkgs.txt"
sort -u -o "$DEST/runtime-pkgs.txt" "$DEST/runtime-pkgs.txt"

{
  echo "#!/usr/bin/env bash"
  echo "sudo apt-get update"
  echo "sudo apt-get install -y $(tr '\n' ' ' < "$DEST/runtime-pkgs.txt")"
} > "$DEST/install-deps.sh"
chmod +x "$DEST/install-deps.sh"

# --- README ------------------------------------------------------------------
cat > "$DEST/README.txt" <<'EOF'
OpenSCAD (větev cam_and_drawings) — přenositelný binární balík pro Debian 13.

Použití na cílovém stroji (stejný Debian 13):
  tar xzf openscad-cam-bin.tgz      # vyrobí složku openscad-cam/
  cd openscad-cam
  bash install-deps.sh              # doinstaluje běhové závislosti
  ./openscad                        # spusť

Binárka i resource složky (examples, libraries, fonts, …) jsou v této složce;
openscad je najde automaticky (hledá je vedle sebe).
EOF

# --- tarball -----------------------------------------------------------------
echo ">> balím tarball…"
tar czf "$OUT/openscad-cam-bin.tgz" -C "$STAGE" "$NAME"

echo ">> hotovo:"
ls -lh "$OUT/openscad-cam-bin.tgz" | awk '{print "   "$5"\t"$NF}'
echo
echo "Na cílovém Debianu 13:"
echo "   tar xzf openscad-cam-bin.tgz"
echo "   cd $NAME && bash install-deps.sh && ./openscad"
