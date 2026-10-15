# OpenSCAD — rozšíření (fork `cam_and_drawings`)

Tenhle fork přidává do OpenSCADu **kamery v modelu**, **technické výkresy s kótami**,
**přepínatelné pohledy (stavy sestavy)**, **ohyb geometrie** a **barevný export**.
Všechno se definuje přímo v `.scad` a aktualizuje se se změnou modelu. Detailní
návrh a stav viz [`ROADMAP.md`](ROADMAP.md); orientace v kódu [`CLAUDE.md`](CLAUDE.md).

## Cheatsheet — co fork přidává

**Nové moduly ve zdrojáku** (neviditelné „markery", nerozbijí render/export):

| modul | k čemu | hlavní argumenty |
|---|---|---|
| `camera(...)` | kamera v modelu → offscreen render (PNG, i s barvami) | `focal`,`sensor` **nebo** `fov`; `width`,`height`; `name`; `projection="perspective"\|"orthogonal"`; `marker` |
| `dimref(...)` | kotva kóty (klikací bod pro rozměr) | `name`; `d=`/`r=` (⌀/R hodnota); `kvota=` (auto-pár podle jména) |
| `section(...)` | rovina řezu pro výkres (řez = lokální XY, pohled +Z) | `name`; `flip`; `hatch` (rozteč šraf) |
| `label(...)` | 3D textový popisek s odkazovou čárou | `name`; `text` |
| `view(...) { ... }` | **přepínatelný pohled / stav** modelu | `name` |
| `bend(...) { ... }` | ohyb geometrie i importovaného STL kolem osy ‖ Y | `at`,`r`,`angle`; `xlim`,`ylim`,`zlim` (maska) |

**Anotace v komentářích** (spravuje je GUI — klikáním/tažením ve výkresu; píšou se
samy do zdrojáku): `// @dim` (kóta), `// @sheet` (nastavení listu), `// @detail`
(výřez ve větším měřítku), `// @label` (text popisku).

**Nové CLI:** `--export-cameras <prefix>`, `--model-view <cesta>`,
`--all-model-views` (viz [Rozšířené CLI](#rozšířené-cli) níže).

**Barevný export:** `-o model.obj` zapíše i sesterský `.mtl` (barvy z `color()`),
takže je přečte Blender; OFF/3MF barvy zachovají taky.


<img width="1678" height="1048" alt="openscad_cam_and_drawings" src="https://github.com/user-attachments/assets/1e412a96-f9d1-4be7-9609-59a8a267944f" />

---

## Kamery v modelu — `camera()`

Kamera je bod v modelu (pozici a natočení dědí z rodičovských transformací),
který nese parametry objektivu a rozlišení. GUI dok **„Cameras"** z každé
`camera()` vyrenderuje offscreen snímek (tlačítko *Save PNG*), včetně barev z
`color()`. V hlavním 3D pohledu se kamery kreslí jako drátěné frustumy (skryté v F6).

```scad
translate([50, 50, 60]) rotate([0,0,135]) rotate([45,0,0])
    camera(name = "hero", focal = 35, sensor = 24, width = 1920, height = 1080);

camera(name = "top", projection = "orthogonal", marker = 40);   // ohnisko lze i přes fov=
```

Objektiv zadáš buď `focal`+`sensor` (svislý FOV se dopočítá), nebo přímo `fov`.
`marker` = délka frustumu v mm (default auto). Headless dávkový render: `--export-cameras`.

## Technické výkresy a kóty — `dimref()`, `section()`, `label()` + dok „Drawing"

Dok **„Drawing"** je WYSIWYG vektorového výkresu: standardní ISO-E pohledy
(nárys/bokorys/půdorys/…), řezy, detaily, kóty a popisky na jednom listu; co vidíš,
to se vyexportuje (SVG/DXF přes *Save Sheet*).

- **`dimref("A")`** je kotva rozměru. Ve výkresu (nebo v 3D po F6) klikneš dvě
  kotvy → do zdrojáku se zapíše `// @dim A B proj=front`. Kótu pak **taháš myší**
  (odsazení), otáčíš (⌀/R), dvojklikem edituješ (label/přesnost/šipky/projekce).
  - `dimref("A", d=8)` (nebo `r=`) nese hodnotu pro **průměr/poloměr**;
    `dimref("A", kvota="width")` spáruje dvě stejnojmenné kotvy automaticky.
  - Typy kót: délka, ⌀/R, úhel (`// @dim angle V A B`), řetěz/kontinuální
    (`// @dim chain A B C…`).
- **`section("A")`** definuje rovinu řezu (rovina = lokální XY, pohled podél +Z);
  `flip` obrátí, `hatch` řídí šrafy. Ve výkresu se řez vykreslí jako dlaždice
  „Řez A–A" i s čárou řezu v odpovídajícím pohledu.
- **`label("A", text="Motor mount")`** je 3D textový popisek s odkazovou čárou;
  funguje ve výkrese i ve snímcích kamer.
- **`// @sheet`** nese nastavení listu (zapnuté pohledy, měřítko, mezery, výplň),
  **`// @detail`** výřez ve větším měřítku. Obojí spravuje dok, píše se do zdrojáku.

Kóty/popisky jsou komentáře → přepočítají se bez F6 (geometrie se nemění).

## Přepínatelné pohledy / stavy — `view()`

`view("jméno"){ … }` je **přepínatelný stav modelu**. V GUI je nahoře v liště
rozbalovací **přepínač pohledů**; přepnutím se model přepočítá a 3D náhled,
kamery i výkres ukážou zvolený pohled. V headless se pohled volí přes `--model-view`.

```scad
base();                       // společné: mimo view() = vidět vždy

view("pcb")      pcb();
view("kryt")     housing();

view("assembly") {            // celek; lze vnořit varianty
    housing();
    pcb();
    view("exploded")          // "assembly/exploded" = celek + rozklad
        translate([0,0,30]) lid();
}
```

Pravidla: jeden aktivní pohled; zobrazí se **společná geometrie + zvolený pohled a
jeho předci** (vnořené pohledy se **hromadí dolů**). Default = **root** (jen společné
= chování jako bez `view()`, plná zpětná kompatibilita). Kóty/`// @sheet` psané
uvnitř bloku patří tomu pohledu; kóta se kreslí, jen když je její pohled na aktivní
cestě. Řídí se přes speciální proměnnou `$view` (vektor jmen; běžně ji nenastavuješ ručně).

## Ohyb geometrie — `bend()`

`bend()` ohne své děti (**i importované STL** — nelineárně, pod úrovní meshe)
kolem osy rovnoběžné s Y. Materiál `x < at` stojí, pásmo `[at, at+r·|angle|]` se
navine na oblouk o poloměru `r`, zbytek jede tuze za ním. `angle > 0` zvedá k +Z.

```scad
bend(at = 20, r = 5, angle = 90)
    import("flex_pcb.stl");

bend(at = 0, r = 3, angle = 45, xlim = [10, 40])   // maskou omez, co se ohýbá
    children();
```

Maska `xlim`/`ylim`/`zlim` vymezí, co se deformuje (víc ohybů na různých místech
desky je nezávislých); zbytek se klasifikuje podle souvislosti meshe.

## Rozšířené CLI

Nad rámec standardního OpenSCADu (`-o out.stl`, `--camera`, `-D`, …):

| přepínač | co dělá |
|---|---|
| `--export-cameras <prefix>` | dávkový render **všech `camera()`** → `<prefix>_<jméno>.png`. Respektuje výběr pohledů: `--model-view A` renderuje i kamery uvnitř `view("A")`; `--all-model-views` projede všechny pohledy (tag pohledu ve jméně) |
| `--model-view <cesta>` | zvolí aktivní pohled; vnořený jako cesta `assembly/exploded`. **Opakovatelný** = dávka vybraných pohledů (soubor na pohled). Jediný `--model-view` nechává `-o` beze změny |
| `--all-model-views` | vyexportuje **každý `view()`** v modelu, jeden soubor na pohled |
| `--default-model-views` | vyexportuje jen pohledy, které si o daný export **řekly v modelu**: `camera_export=true` pro `--export-cameras`, `drawings_export=true` pro `--export-drawing`, `model_export=true` pro `-o`. Vždy tag pohledu ve jméně. Když není označen žádný pohled → varování a nic (ne chyba, aby CI matice nespadla) |
| `--list-model-views[=text\|json]` | vypíše pohledy modelu a jejich vlajky a skončí (jen parsuje) — pro **matrix job v CI** |
| `--export-drawing <prefix>` | **technický výkres** (ortho pohledy + kóty + řezy) zvolených pohledů → `<prefix>[_pohled].<fmt>`. `--drawing-format svg\|pdf\|png\|dxf` (default svg). Respektuje `--model-view`/`--all-model-views`. Potřebuje Qt + offscreen (běž pod `xvfb-run`) |
| `-o model.obj` | OBJ **+ sesterský `.mtl`** s barvami z `color()` (pro Blender) |

Tag pohledu se u dávky vkládá do jména výstupu: `out.stl` → `out_pcb.stl`,
`out_assembly_exploded.stl`. Totéž platí pro `--summary-file` (každý pohled má
vlastní JSON) a `-d deps.d` uvádí skutečně zapsaná jména souborů.

Přednost při výběru pohledů: `--all-model-views` > `--model-view` >
`--default-model-views` (kombinace = varování, že se `--default-model-views`
ignoruje).

### Které pohledy exportovat (vlajky v modelu)

Každý `view()` si může říct, do kterých dávkových exportů patří — tři nezávislé
volitelné parametry, všechny default `false`:

```scad
view(name = "assembly", camera_export = true)                      { … }  // jen snímky kamer
view(name = "celek",    drawings_export = true)                    { … }  // jen výkres
view(name = "pcb",      drawings_export = true, model_export = true) { … } // výkres + STL
view(name = "debug")                                               { … }  // nic
```

Vlajky se čtou z AST, takže musí být **literál** `true`/`false` — hodnota
spočítaná z proměnné se pro výběr pohledů čte jako `false`.

Jméno pohledu je **label, ne identita**: aktivní jsou *všechny* bloky s tím
jménem naráz (modul volaný 2×, každá iterace `for`, dva bloky stejného jména).
Enumerace pohledů to respektuje — projde i těla volaných uživatelských modulů
(včetně modulů z `use <lib.scad>`) a obě větve `if`/`else`, a každou **cestu
pohledu vypíše/vyexportuje jen jednou**. Modul volaný uvnitř `view("outer")` dá
zanořenou cestu `outer/<jeho pohled>`.

> **Pozor:** `--view` zůstává původní přepínač zobrazení viewportu (axes/edges/…);
> výběr pohledů modelu je proto `--model-view`.

Příklady:

```bash
# render všech kamer
openscad model.scad --export-cameras out_

# jeden pohled do STL / všechny pohledy naráz
openscad model.scad --model-view pcb            -o pcb.stl
openscad model.scad --model-view assembly/exploded -o exploded.stl
openscad model.scad --all-model-views           -o out.stl   # -> out_<pohled>.stl

# barevný OBJ pro Blender
openscad model.scad -o model.obj

# technický výkres (kóty!) do SVG/PNG — pod xvfb (potřebuje Qt + offscreen)
xvfb-run -a openscad model.scad --export-drawing out --model-view pcb           # out_pcb.svg
xvfb-run -a openscad model.scad --export-drawing out --all-model-views --drawing-format pdf

# CI: jen to, co si model označil (viz vlajky výše)
openscad model.scad -o out.stl                --default-model-views  # model_export
xvfb-run -a openscad model.scad --export-cameras cam --default-model-views     # camera_export
xvfb-run -a openscad model.scad --export-drawing dwg --default-model-views     # drawings_export

# výčet pohledů pro matrix job
openscad model.scad --list-model-views          # pcb<TAB>drawings_export,model_export
openscad model.scad --list-model-views=json | jq -r '.[]|select(.camera_export)|.path'
```

Hotová šablona workflow: [`doc/ci/render-views.yml`](doc/ci/render-views.yml)
(schválně **není** v `.github/workflows/`, aby se nespouštěla v tomto repu —
zkopíruj si ji do svého projektu s modelem).

### Headless build (pro CI / GitHub Actions)

CLI + rozšíření modelu (`view()`, `--model-view`, `--export-cameras`, geometrické
exporty) běží i **bez Qt GUI**, výrazně menší a s málo závislostmi. Doporučená
varianta nechává **OpenGL zapnuté**, aby fungoval **render kamer**:

```bash
cmake -B build -G Ninja -DEXPERIMENTAL=1 -DCMAKE_BUILD_TYPE=Release -DHEADLESS=ON
cmake --build build -j"$(nproc)"
```

- `-DHEADLESS=ON` vypustí Qt GUI frontend (doky Cameras/Drawing, editor). **Bez
  Qt**, ~50 knihoven místo ~120; render kamer (`--export-cameras`, `-o *.png`)
  **funguje** přes offscreen GL (`OffscreenView`).
- `-DNULLGL=ON` (navíc) vypustí i OpenGL → **nejmíň** závislostí, ale **render
  kamer/PNG přestane fungovat**; zbydou jen geometrické výstupy
  (STL/OFF/3MF/OBJ/CSG/DXF/SVG) + `--model-view`/`--all-model-views`.
- Render kamer potřebuje **GL kontext** → v CI spouštěj pod `xvfb-run` (Mesa/EGL
  fallback na GLX), např. `xvfb-run -a openscad --export-cameras out_ model.scad`.
- Interaktivní výkresy/kóty a GUI přepínač pohledů jsou součást GUI → v headless
  nejsou; dávkový export pohledů přes `--model-view` funguje.

**Přenositelné AppImage** — dvě varianty přes `scripts/make-appimage.sh` (Docker,
Ubuntu 22.04 → glibc 2.35):

```bash
scripts/make-appimage.sh              # plná GUI verze
scripts/make-appimage.sh --headless   # CLI + GL (bez Qt GUI), přípona _headless
```

---

<sub>⬆ Výše je dokumentace tohoto forku. Níže následuje původní (upstream) README OpenSCADu.</sub>

---

[![GitHub (master)](https://img.shields.io/github/checks-status/openscad/openscad/master.svg?logo=github&label=build&logoColor=black&colorA=f9d72c&style=plastic)](https://github.com/openscad/openscad/actions)
[![CircleCI (master)](https://img.shields.io/circleci/project/github/openscad/openscad/master.svg?logo=circleci&logoColor=black&colorA=f9d72c&style=plastic)](https://circleci.com/gh/openscad/openscad/tree/master)
[![Coverity Scan](https://img.shields.io/coverity/scan/2510.svg?colorA=f9d72c&logoColor=black&style=plastic)](https://scan.coverity.com/projects/2510)

[![Visit our IRC channel](https://kiwiirc.com/buttons/irc.libera.chat/openscad.png)](https://kiwiirc.com/client/irc.libera.chat/#openscad)

# What is OpenSCAD?
<p><a href="https://opencollective.com/openscad/donate"><img align="right" src="https://opencollective.com/openscad/donate/button@2x.png?color=white" width="200"/></a>

OpenSCAD is a software for creating solid 3D CAD objects. It is free software and
available for Linux/UNIX, MS Windows and macOS.</p>

Unlike most free software for creating 3D models (such as the famous
application Blender), OpenSCAD focuses on the CAD aspects rather than the
artistic aspects of 3D modeling. Thus this might be the application you are
looking for when you are planning to create 3D models of machine parts but
probably not the tool for creating computer-animated movies.

OpenSCAD is not an interactive modeler. Instead it is more like a
3D-compiler that reads a script file that describes the object and renders
the 3D model from this script file (see examples below). This gives you, the
designer, complete control over the modeling process and enables you to easily
change any step in the modeling process or make designs that are defined by
configurable parameters.

OpenSCAD provides two main modeling techniques: First there is constructive
solid geometry (aka CSG) and second there is extrusion of 2D outlines. As the data
exchange format for these 2D outlines Autocad DXF files are used. In
addition to 2D paths for extrusion it is also possible to read design parameters
from DXF files. Besides DXF files OpenSCAD can read and create 3D models in the
STL and OFF file formats.

# Contents

- [Getting Started](#getting-started)
- [Documentation](#documentation)
    - [Building OpenSCAD](#building-openscad)
        - [Prerequisites](#prerequisites)
        - [Getting the source code](#getting-the-source-code)
        - [Building for macOS](#building-for-macos)
        - [Building for Linux/BSD](#building-for-linuxbsd)
        - [Building for Linux/BSD on systems with older or missing dependencies](#building-for-linuxbsd-on-systems-with-older-or-missing-dependencies)
        - [Building for Windows](#building-for-windows)
        - [Compilation](#compilation)

# Getting started

You can download the latest binaries of OpenSCAD at
<https://www.openscad.org/downloads.html>. Install binaries as you would any other
software.

When you open OpenSCAD, you'll see three frames within the window. The
left frame is where you'll write code to model 3D objects. The right
frame is where you'll see the 3D rendering of your model.

Let's make a tree! Type the following code into the left frame:

```scad
cylinder(h = 30, r = 8);
```

Then render the 3D model by hitting F5. Now you can see a cylinder for
the trunk in our tree. Now let's add the bushy/leafy part of the tree
represented by a sphere. To do so, we will union a cylinder and a
sphere.

```scad
union() {
    cylinder(h = 30, r = 8);
    sphere(20);
}
```

But, it's not quite right! The bushy/leafy are around the base of the
tree. We need to move the sphere up the z-axis.

```scad
union() {
    cylinder(h = 30, r = 8);
    translate([0, 0, 40]) sphere(20);
}
```

And that's it! You made your first 3D model! There are other primitive
shapes that you can combine with other set operations (union,
intersection, difference) and transformations (rotate, scale,
translate) to make complex models! Check out all the other language
features in the [OpenSCAD
Manual](https://en.wikibooks.org/wiki/OpenSCAD_User_Manual).

# Documentation

Have a look at the OpenSCAD Homepage (https://www.openscad.org/documentation.html) for documentation.

## Building OpenSCAD

To build OpenSCAD from source, follow the instructions for the
platform applicable to you below.

### Prerequisites

To build OpenSCAD, you need some libraries and tools. The version
numbers in brackets specify the versions which have been used for
development. Other versions may or may not work as well.

If you're using a newer version of Ubuntu, you can install these
libraries from aptitude. If you're using Mac, or an older Linux/BSD, there
are build scripts that download and compile the libraries from source.
Follow the instructions for the platform you're compiling on below.

* A C++ compiler supporting C++17
* [cmake (3.5 ->)](https://cmake.org/)
* [Qt (5.15 ->)](https://qt.io/)
* [QScintilla2 (2.9 ->)](https://riverbankcomputing.com/software/qscintilla/)
* [CGAL (5.4 ->)](https://www.cgal.org/)
 * [GMP (5.x)](https://gmplib.org/)
 * [MPFR (3.x)](https://www.mpfr.org/)
* [boost (1.70 ->)](https://www.boost.org/)
* [OpenCSG (1.4.2 ->)](http://www.opencsg.org/)
* [GLEW (1.5.4 ->)](http://glew.sourceforge.net/)
* [Eigen (3.x)](https://eigen.tuxfamily.org/)
* [glib2 (2.x)](https://developer.gnome.org/glib/)
* [fontconfig (2.10 -> )](https://fontconfig.org/)
* [freetype2 (2.4 -> )](https://freetype.org/)
* [harfbuzz (0.9.19 -> )](https://www.freedesktop.org/wiki/Software/HarfBuzz/)
* [libzip (0.10.1 -> )](https://libzip.org/)
* [Bison (2.4 -> )](https://www.gnu.org/software/bison/)
* [Flex (2.5.35 -> )](http://flex.sourceforge.net/)
* [pkg-config (0.26 -> )](https://www.freedesktop.org/wiki/Software/pkg-config/)
* [double-conversion (2.0.1 -> )](https://github.com/google/double-conversion/)

For the test suite, additional requirements are:
* Python3 (3.9 -> )
* [Ghostscript (10.x ->)](https://www.ghostscript.com/index.html)
* [Catch2 (3.0 ->)](https://github.com/catchorg/Catch2)

### Getting the source code

Install git (https://git-scm.com/) onto your system. Then run a clone:

    git clone https://github.com/openscad/openscad.git

This will download the latest sources into a directory named `openscad`.

To pull the various submodules (incl. the [MCAD library](https://github.com/openscad/MCAD)), do the following:

    cd openscad
    git submodule update --init --recursive

### Contributing Changes

You can create an issue to plan and discuss your change by visiting https://github.com/openscad/openscad/issues.

If you want to work on an existing issue and plan to contribute changes via a PR later, you can assign the issue to yourself by commenting:

`/assign-me`

in a comment on the issue.

### Building for macOS

Prerequisites:

* Xcode
* automake, libtool, cmake, pkg-config, wget, meson, python-packaging (we recommend installing these using Homebrew)

Install Dependencies:

After building dependencies using one of the following options, follow the instructions in the *Compilation* section.

1. **From source**

    Run the script that sets up the environment variables:

        source scripts/setenv-macos.sh

    Then run the script to compile all the dependencies:

        ./scripts/macosx-build-dependencies.sh

2. **Homebrew** (assumes [Homebrew](https://brew.sh/) is already installed)

        ./scripts/macosx-build-homebrew.sh

### Building for Linux/BSD

First, make sure that you have git installed (often packaged as 'git-core'
or 'scmgit'). Once you've cloned this git repository, download and install
the dependency packages listed above using your system's package
manager. A convenience script is provided that can help with this
process on some systems:

    sudo ./scripts/uni-get-dependencies.sh qt6

After installing dependencies, check their versions. You can run this
script to help you:

    ./scripts/check-dependencies.sh

Take care that you don't have old local copies anywhere (`/usr/local/`).
If all dependencies are present and of a high enough version, skip ahead
to the Compilation instructions.

### Building for Linux/BSD on systems with older or missing dependencies

If some of your system dependency libraries are missing or old, then you
can download and build newer versions into `$HOME/openscad_deps` by
following this process. First, run the script that sets up the
environment variables.

    source ./scripts/setenv-unibuild.sh

Then run the script to compile all the prerequisite libraries above:

    ./scripts/uni-build-dependencies.sh

Note that huge dependencies like gcc, qt, or glib2 are not included
here, only the smaller ones (boost, CGAL, opencsg, etc). After the
build, again check dependencies.

    ./scripts/check-dependencies.sh

After that, follow the Compilation instructions below.

### Building on Nix

A [development Nix shell](scripts/nix) is included for local, incremental compilation.

### Building for Windows

OpenSCAD for Windows is usually cross-compiled from Linux. If you wish to
attempt an MSVC build on Windows, please see this site:
https://en.wikibooks.org/wiki/OpenSCAD_User_Manual/Building_on_Windows

MSVC build support has been added to OpenSCAD. For instructions on how to build it,
refer to [building with MSVC](doc/win-build.md).

To cross-build, first make sure that you have all necessary dependencies
of the MXE project ( listed at https://mxe.cc/#requirements ). Don't install
MXE itself, the scripts below will do that for you under `$HOME/openscad_deps/mxe`

Then get your development tools installed to get GCC. Then after you've
cloned this git repository, start a new clean bash shell and run the
script that sets up the environment variables.

    source ./scripts/setenv-mingw-xbuild.sh 64

Then run the script to download & compile all the prerequisite libraries above:

    ./scripts/mingw-x-build-dependencies.sh 64

Note that this process can take several hours, and tens of gigabytes of
disk space, as it uses the [https://mxe.cc](https://mxe.cc) system to cross-build many
libraries. After it is complete, build OpenSCAD and package it to an
installer:

    ./scripts/release-common.sh mingw64

For a 32-bit Windows cross-build, replace 64 with 32 in the above instructions.

### Building for WebAssembly

We support building OpenSCAD headless for WebAssembly w/ Emscripten, using a premade Docker image built in [openscad/openscad-wasm](https://github.com/openscad/openscad-wasm) (which also has usage examples)

#### Browser

The following command creates `build-web/openscad.wasm` & `build-web/openscad.js`:

```bash
./scripts/wasm-base-docker-run.sh emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Debug -DEXPERIMENTAL=1
./scripts/wasm-base-docker-run.sh cmake --build build-web -j2
```

[openscad/openscad-playground](https://github.com/openscad/openscad-playground) uses this WASM build to provide a [Web UI](https://ochafik.com/openscad2/) with a subset of features of OpenSCAD.

> [!NOTE]
> With a debug build (`-DCMAKE_BUILD_TYPE=Debug`), you can set C++ breakpoints in Firefox and in Chrome (the latter [needs an extension](https://developer.chrome.com/docs/devtools/wasm)).

#### Standalone node.js build

The following command creates `build-node/openscad.js`, which is executable (requires `node`):

```bash
./scripts/wasm-base-docker-run.sh emcmake cmake -B build-node -DWASM_BUILD_TYPE=node -DCMAKE_BUILD_TYPE=Debug -DEXPERIMENTAL=1
./scripts/wasm-base-docker-run.sh cmake --build build-node -j2
build-node/openscad.js --help
```

> [!NOTE]
> With a debug build (`-DCMAKE_BUILD_TYPE=Debug`), you can set C++ breakpoints in VSCode + Node ([needs an extension](https://code.visualstudio.com/docs/nodejs/nodejs-debugging#_debugging-webassembly)).

#### node.js module build

The following command creates `build-node/openscad.js`, which is a node module that can be imported into other node programs:

```bash
./scripts/wasm-base-docker-run.sh emcmake cmake -B build-node -DWASM_BUILD_TYPE=node-module -DCMAKE_BUILD_TYPE=Debug -DEXPERIMENTAL=1
./scripts/wasm-base-docker-run.sh cmake --build build-node -j2
```

Example usage:

```js
import OpenSCAD from "./build-node/openscad.js";

const instance = await OpenSCAD({
    noInitialRun: true,
    print: (text) => {
        console.debug('stdout: ' + text);
    },
    printErr: (text) => {
        console.debug('stderr: ' + text);
    },
});

const inFile = "model.scad";
const outFile = "model.stl";

instance.callMain([
    inFile,
    "-o", outFile,
    "--backend=manifold",
    "--export-format=binstl",
]);

// Model is now generated
```

### Compilation

First, run `cmake -B build -DEXPERIMENTAL=1` to generate a Makefile in the `build` folder.

Then run `cmake --build build`. Finally, on Linux you might run `cmake --install build` as root.

If you had problems compiling from source, raise a new issue in the
[issue tracker on the github page](https://github.com/openscad/openscad/issues).

This site and it's subpages can also be helpful:
https://en.wikibooks.org/wiki/OpenSCAD_User_Manual/Building_OpenSCAD_from_Sources

Once built, you can run tests with `ctest` from the `build` directory.

Note: Both `cmake --build` and `ctest` accepts a `-j N` argument for distributing the load over `N` parallel processes.

### Running CI workflows locally

*   Install [circleci-cli](https://circleci.com/docs/2.0/local-cli/) (you'll need an API key)

    *Note*: we also use GitHub Workflows, but only to run tests on Windows (which we cross-build for in the Linux-based CircleCI workflows below). Also, [act](https://github.com/nektos/act) doesn't like our submodule setup anyway.

*   Run the CI jobs

	```bash
	# When "successful", these will fail to upload at the very end of the workflow.
	circleci local execute --job  openscad-mxe-64bit
	circleci local execute --job  openscad-mxe-32bit
	circleci local execute --job  openscad-appimage-64bit
	```

	*Note*: openscad-macos can't be built locally.

*   If/when GCC gets randomly killed, give docker more RAM (e.g. 4GB per concurrent image you plan to run)

*   To debug the jobs more interactively, you can go the manual route (inspect .circleci/config.yml to get the actual docker image you need)

	```bash
	docker run --entrypoint=/bin/bash -it openscad/mxe-x86_64-gui:latest
	```

	Then once you get the console:

	```bash
	git clone https://github.com/%your username%/openscad.git workspace
	cd workspace
	git checkout %your branch%
	git submodule init
	git submodule update

	# Then execute the commands from .circleci/config.yml:
	#    export NUMCPU=2
	#    ...
	#    ./scripts/release-common.sh -snapshot -mingw64 -v "$OPENSCAD_VERSION"
	```
