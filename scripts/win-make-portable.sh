#!/usr/bin/env bash
#
# win-make-portable.sh — zabalí zkompilovaný OpenSCAD (MSYS2 UCRT64) do
# přenositelného .zip pro jiné Windows PC, kde MSYS2 není.
#
# Spouštět z MSYS2 UCRT64 shellu:
#   scripts/win-make-portable.sh [REPO_DIR] [OUT_DIR]
#
# Layout výsledné složky (openscad.exe hledá resource složky vedle sebe,
# PlatformUtils.cc: searchpath "."):
#
#   openscad-cam-<verze>-win64/
#     openscad.exe        <- GUI binárka
#     openscad.com        <- konzolový wrapper (CLI výstup v cmd.exe/PowerShellu)
#     *.dll               <- běhové knihovny z /ucrt64/bin
#     platforms/ styles/ imageformats/ tls/ ...   <- Qt pluginy
#     fonts/              <- Liberation + fontconfig konfigurace (fonts.conf, conf.d)
#     color-schemes/ shaders/ locale/ libraries/ templates/ examples/
#
set -uo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]:-$0}" )" && pwd )"
REPO="${1:-$( cd "$SCRIPT_DIR/.." && pwd )}"
OUT="${2:-$REPO/build/deploy}"
BUILD="$REPO/build"
UCRT=/ucrt64

[ -n "${MSYSTEM:-}" ] && [ "$MSYSTEM" = "UCRT64" ] || {
  echo "CHYBA: spusťte z MSYS2 UCRT64 shellu (MSYSTEM=${MSYSTEM:-unset})"; exit 1; }
[ -x "$BUILD/openscad.exe" ] || {
  echo "CHYBA: nenalezena binárka $BUILD/openscad.exe — nejdřív přeložte (cmake --build build)"; exit 1; }

VERSION="$("$BUILD/openscad.exe" --version 2>&1 | head -1 | sed 's/.*version //')"
NAME="openscad-cam-${VERSION}-win64"
STAGE="$OUT/$NAME"

echo "### verze: $VERSION"
echo "### staging: $STAGE"
rm -rf "$STAGE"
mkdir -p "$STAGE"

# --- 1) binárka + resource složky přes vlastní install pravidla projektu -------
# Na WIN32 instaluje CMake všechno do "." (CMakeLists.txt: OPENSCAD_BINDIR=".")
echo "### [1/6] cmake --install"
cmake --install "$BUILD" --prefix "$STAGE" >/dev/null || { echo "CHYBA: cmake --install selhal"; exit 1; }

# --- 2) konzolový wrapper -----------------------------------------------------
# openscad.exe je linkovaná jako GUI (WIN32_EXECUTABLE), takže se odpojí od
# konzole a CLI výstup nikam nejde. openscad.com to obchází.
echo "### [2/6] openscad.com"
cp -a "$BUILD/winconsole/openscad.com" "$STAGE/" || { echo "CHYBA: chybí winconsole/openscad.com"; exit 1; }

# --- 3) fontconfig ------------------------------------------------------------
# FontCache.cc:144 nastaví FONTCONFIG_PATH jen když vedle binárky leží
# fonts/fonts.conf. Bez toho by fontconfig hledal napevno zakompilovanou cestu
# /ucrt64/etc/fonts, která na cílovém PC neexistuje -> text() by se nevykreslil.
# Stejně to dělá oficiální mingw balíček (release-common.sh:279).
echo "### [3/6] fontconfig konfigurace"
cp -a "$UCRT"/etc/fonts/. "$STAGE/fonts/"

# --- 4) Qt pluginy + Qt DLL ---------------------------------------------------
echo "### [4/6] windeployqt6"
windeployqt6 --release --compiler-runtime "$STAGE/openscad.exe" >/dev/null 2>&1 \
  || echo "  VAROVÁNÍ: windeployqt6 skončil nenulově, spoléhám na krok 5"

# --- 5) tranzitivní uzávěr ne-systémových DLL ---------------------------------
# Pravidlo: DLL kopírujeme jen tehdy, když existuje v /ucrt64/bin. Co tam není,
# je systémová knihovna Windows (KERNEL32, OPENGL32, api-ms-win-crt-*, ...).
# Iterujeme do pevného bodu, protože závislosti mají vlastní závislosti; skenují
# se i Qt pluginy v podadresářích (jejich DLL se hledají vedle .exe, tedy v rootu).
echo "### [5/6] sběr závislostí (objdump, do pevného bodu)"
round=0
while :; do
  round=$((round+1))
  added=0
  while IFS= read -r f; do
    while IFS= read -r dll; do
      [ -z "$dll" ] && continue
      if [ ! -e "$STAGE/$dll" ] && [ -f "$UCRT/bin/$dll" ]; then
        cp -a "$UCRT/bin/$dll" "$STAGE/"
        added=$((added+1))
      fi
    done < <(objdump -p "$f" 2>/dev/null | sed -n 's/.*DLL Name: //p')
  done < <(find "$STAGE" \( -name '*.dll' -o -name '*.exe' -o -name '*.com' \) -type f)
  echo "  kolo $round: +$added DLL"
  [ "$added" -eq 0 ] && break
  [ "$round" -gt 20 ] && { echo "  VAROVÁNÍ: uzávěr nekonverguje, končím"; break; }
done

# --- 6) zip -------------------------------------------------------------------
echo "### [6/6] zip"
rm -f "$OUT/$NAME.zip"
( cd "$OUT" && cmake -E tar cf "$NAME.zip" --format=zip "$NAME" ) || { echo "CHYBA: zip selhal"; exit 1; }

cat > "$STAGE/README.txt" <<EOF
OpenSCAD $VERSION (větev cam_and_drawings) — přenositelný build pro Windows x64

Spuštění:
  openscad.exe            GUI
  openscad.com --help     CLI (v cmd.exe / PowerShellu použijte .com, ne .exe)

Nic se neinstaluje, složku lze rozbalit kamkoliv. Všechny knihovny a Qt pluginy
jsou uvnitř; MSYS2 na cílovém počítači není potřeba.

Pokud GUI nenaběhne kvůli OpenGL (stará grafika, RDP, virtuální stroj), jde
vedle openscad.exe podložit softwarový OpenGL (Mesa opengl32.dll).
EOF
( cd "$OUT" && cmake -E tar cf "$NAME.zip" --format=zip "$NAME" ) >/dev/null

SIZE=$(du -h "$OUT/$NAME.zip" | cut -f1)
COUNT=$(find "$STAGE" -type f | wc -l)
echo
echo "### HOTOVO"
echo "### zip:     $OUT/$NAME.zip  ($SIZE)"
echo "### složka:  $STAGE  ($COUNT souborů)"
