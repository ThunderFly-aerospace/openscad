# CLAUDE.md — orientace v kódu OpenSCADu pro plánované rozšíření

Tento soubor shrnuje, kde v kódu leží infrastruktura, o kterou se opírají dvě
plánované funkce (viz `ROADMAP.md`): **(1) kamery v modelu** a **(2) technické
výkresy s kvótami**. Cesty jsou relativní ke kořeni repa.

## Verze

- Stabilní verze (kterou autor běžně používá) je přibližně **2021.01**.
- Vývoj probíhá na `master`, který je ~2976 commitů napřed (tag
  `openscad-2026.01.01-TEST2`). **Nové funkce stavíme na `master`** — obsahuje
  potřebnou infrastrukturu (barvy v preview/exportu, měřicí nástroj,
  výběr objektu → skok do zdroje, `$vpf`), kterou 2021.01 nemá.

## Parser, AST a komentáře

- Hlavní gramatika: `src/core/lexer.l`, `src/core/parser.y`.
  **Komentáře jsou při lexování zahozeny** (`lexer.l:205-219`) — do AST se
  nedostanou.
- Customizer proto **znovu načítá zdrojový text** a metadata z komentářů dolu-
  je zvlášť: `CommentParser::collectParameters(fulltext, root_file)`
  (`src/core/customizer/CommentParser.cc:260`). Volá se po parse: GUI
  `src/gui/MainWindow.cc:1722`, CLI `src/openscad.cc:626`.
  - Mini-gramatika pro obsah jednoho komentáře: `comment_lexer.l`,
    `comment_parser.y` (prefix `comment_`).
  - Anotace se k assignmentům vážou **podle čísla řádku**
    (`assignment->location().firstLine()`).
- `Annotation` (`src/core/customizer/Annotation.h`) = jméno + `Expression`,
  bez vlastní lokace (lokaci nese vlastnící `Assignment`).
- **Customizer NIKDY nezapisuje zpět do `.scad`** — hodnoty ukládá do JSON
  sidecaru (`ParameterSet.cc`) a do AST jen dočasně v paměti
  (`ParameterObject.cc:573` `apply()` → `assignment->setExpr(...)`).
  Zápis metadat zpět do zdrojáku by byl nový kód (kotvit lze přes
  `location().firstLine()`).

### AST / uzly — klíčové třídy
- `Location` — `src/core/AST.h:12-49` (first/last line/col + `path`).
- `ASTNode` (drží `Location loc`) — `src/core/AST.h:51-65`.
- `Assignment` — `src/core/Assignment.h` (`getExpr/setExpr`, anotace).
- `ModuleInstantiation : ASTNode` — `src/core/ModuleInstantiation.h`
  (drží `arguments`, `scope`, `modname`, `location()`).
- `AbstractNode` — `src/core/node.h:31`. Každý uzel drží
  `const ModuleInstantiation *modinst` (`node.h:58`) a unikátní
  `int idx` (`node.h:66`, `index()`), přiřazený v konstruktoru
  (`node.cc:44`). → `node->modinst->location()` = zdrojová lokace.
- `TransformNode` (`src/core/TransformNode.h`) drží 4×4 `matrix` spočítanou
  při instanciaci — **klíčové pro odvození world-pozice marker-uzlů**
  (kamery, kotvy kvót) procházením stromu.
- Užitečné pomůcky: `AbstractNode::getNodeByID(idx, path)` (`node.cc:53`),
  `getCodeLocation(...)` (`node.cc:71`), `findNodesWithSameMod(...)`
  (`node.cc:117`).

### Registrace builtin modulu (šablona pro `camera()`)
- `Builtins::init(...)` a `register_builtin_*()` v `src/core/Builtins.cc:49-90`.
- Nejbližší šablona: `register_builtin_color()` + `ColorNode`
  (`src/core/ColorNode.{h,cc}`).
- **Neznámý modul = jen varování** (`Context.cc:127` „Ignoring unknown
  module"), render pokračuje. **Uživatelský modul přebíjí builtin**
  (`ScopeContext.cc:55` zkouší `scope->lookup<UserModule*>` dřív než builtin) —
  proto do modelu NEdávat prázdný `module camera(){}`, přebil by builtin.

## Kamera a rendering

- `Camera` — `src/glview/Camera.h`. Drží `object_trans`, `object_rot`
  (Euler), `fov` (svislé, ve stupních; **ohnisko se převádí ručně**),
  `pixel_width/height`, `projection` (ORTHOGONAL/PERSPECTIVE),
  `viewer_distance`. `setup(params)`: 7 čísel = gimbal, 6 = vektor
  (eye+center). Model dnes ovládá **jednu** kameru přes
  `$vpt/$vpr/$vpd/$vpf` (`Camera::updateView`, `Camera.cc:103`).
- GL matice: `GLView::setupCamera()` (`src/glview/GLView.cc:124-155`,
  `gluPerspective(cam.fov, aspect, ...)`). Kreslení scény:
  `GLView::paintGL()` (`GLView.cc:157+`).
- Renderery: `Renderer` (`src/glview/Renderer.*`), `VBORenderer`,
  **`OpenCSGRenderer`** (výchozí preview, `src/glview/preview/`),
  `ThrownTogetherRenderer`, `PolySetRenderer`/`CGALRenderer` (F6/export).
- **Barvy z `color()` se v preview zobrazují** přímo z CSG uzlů
  (`OpenCSGRenderer.cc:213,287`; barva žije na `CSGLeaf::color`,
  `CSGNode.h:99`). Do STL se barva ztrácí (`export_stl.cc` barvu nemá);
  OFF/3MF ji zachovají. → **Snímek z preview kamery ty barvy zachytí** =
  řešení „Blender nepřečte barvy z STL".
- **Offscreen render do PNG už existuje**: `OffscreenView`
  (`src/glview/OffscreenView.*`, FBO + GL kontext) a `export_png.cc`
  (`export_png(...)`, `prepare_preview(tree, options, camera)` staví
  OpenCSG/ThrownTogether renderer). To je hlavní bod k recyklaci pro
  render kamer.
- CLI `--camera` se parsuje v `get_camera(vm)` (`src/openscad.cc:323-388`),
  `camera.setup(...)` nastaví `locked=true`.

## GUI (Qt)

- `MainWindow` — `src/gui/MainWindow.{h,cc}` (+ `MainWindow.ui`,
  `setupUi` v `setupWindow()`). Akce/menu v `.ui`, propojení jménem
  (`on_<akce>_triggered()`) nebo v `setupMenusAndActions()`
  (`MainWindow.cc:3817`).
- **Doky**: základní třída `Dock : QDockWidget` (`src/gui/Dock.{h,cc}`).
  Registrace v `setupDocks()` (`MainWindow.cc:3760`, seznam `docks`).
  **Nejlepší šablona pro nový dok = `AIDock`** (`src/gui/ai/AIDock.*`,
  `setupAIDock()` `MainWindow.cc:3701`): subclass `Dock`, `new` +
  `addDockWidget()`, přidat do `docks`, slot `visibilityChanged`.
- **Editor**: `EditorInterface` (`src/gui/Editor.h`), impl
  `ScintillaEditor` (QScintilla). Vkládání textu na řádek/sloupec:
  `activeEditor->setCursorPosition(line, col)` (0-based) + `insert(text)`,
  nebo `insertOrReplaceText(...)`. `MainWindow::jumpToLine(line,col)`
  (`MainWindow.cc:3444`).
- **Preview (F5)**: `on_designActionPreview_triggered()` →
  `actionRenderPreview()` → `csgRender()` (`MainWindow.cc:1824-1876`).
  **Render (F6)**: `on_designActionRender_triggered()` → `cgalRender()`
  (async `CGALWorker`, `MainWindow.cc:1941-1971`).

## Výběr objektu, mapování na zdroj, měření (základ pro kvóty)

- **Picking objektu → řádek zdroje UŽ FUNGUJE** (pravé tlačítko):
  GPU color-picking kóduje `AbstractNode::idx` do RGB.
  - `MouseSelector` (`src/gui/MouseSelector.{h,cc}`, shadery
    `MouseSelector.vert/.frag`), `select(renderer,x,y)` vrátí idx.
  - `QGLView::pickObject(QPoint)` (`QGLView.cc:648`).
  - `MainWindow::rightClick` (`MainWindow.cc:2087`) →
    `rootNode->getNodeByID(index, path)` → menu s `modinst->location()`.
  - `MainWindow::setSelection(index)` (`MainWindow.cc:2246`) posadí kurzor
    na `location.firstLine()/firstColumn()`.
  - Index se propaguje do geometrie: `CSGLeaf::index == AbstractNode::idx`
    (`CSGTreeEvaluator.cc:238`), na leaf je i barva a bbox.
- **Měření (výběr 3D bodů/hran)**: `SelectedObject`
  (`src/core/Selection.h`, `SELECTION_POINT`/`SELECTION_LINE`, `p1/p2`),
  `QGLView::selectPoint`/`findObject`, `MainWindow::leftClick`
  (`MainWindow.cc:2051`), `Measurement` (`src/gui/Measurement.*`).
  **Vyžaduje F6 render** (mesh). To je základ pro klikání konců kvót.
- **Bounding box na úrovni objektu**: `CSGLeaf::bbox` (world-space,
  `CSGNode.cc:142`), `CSGProduct(s)::getBoundingBox` (`CSGNode.cc:267-305`),
  `PolySet::getBoundingBox` (`PolySet.cc:82`). `BoundingBox` =
  `Eigen::AlignedBox<double,3>` (`linalg.h:30`).

## Poznámky pro implementaci
- Nový neviditelný „marker" uzel (kamera, později kotva kvóty) musí být
  ošetřen v `GeometryEvaluator`/`CSGTreeEvaluator` tak, aby vracel prázdnou
  geometrii (jako prázdná grupa) — nesmí rozbít preview/render/export/měření.
- World-pozice markeru = průchod stromem `rootNode` s akumulací
  `TransformNode::matrix`.

## Testování

Vše přes **CTest** z build adresáře. Dokumentace: `doc/testing.md`.
Spouštění: `ctest -j8`, `ctest -R <regex>`, konfigy `Default`/`Heavy`/
`Examples`/`Bugs`/`All`. Render/GUI testy potřebují display → v headless
`xvfb-run ctest`. Logy `build/Testing/Temporary` (+ `index.html`), skutečné
výstupy `build/tests/output/*`, zlaté (expected) `tests/regression/*`.

**Tři vrstvy:**
1. **Regresní / cmdline** (`tests/CMakeLists.txt`, funkce `add_cmdline_test`
   v `tests/cmake/TestFunctions.cmake:393`; driver
   `tests/test_cmdline_tool.py`). Spustí openscad na `.scad` a porovná výstup:
   - `dump` → CSG strom `.csg`, `astdump` → `.ast`, `echo` → `.echo`
     (textový diff), `export-*` → přesný exportovaný soubor.
   - `render-*` / `preview-*` → PNG přes obrazové porovnání
     (`tests/image_compare.py`, venv s numpy/Pillow; fallback ImageMagick/
     `diffpng`).
   - Vstupy `tests/data/scad/**` (soubory v `3D/features/` se auto-globnou
     do dump/echo/render/preview skupin).
2. **Unit testy — Catch2 v3.** Soubory `*_test.cc` **co-located u zdroje**,
   glob **jen** `src/utils/*_test.cc` a `src/gui/*_test.cc`
   (`CMakeLists.txt:1574-1579`) → linkuje se do `OpenSCADUnitTests`,
   auto-discover (`catch_discover_tests`). Přímé spuštění:
   `./OpenSCADUnitTests "*fráze*"` nebo `-# #jmeno_test`. Příklady:
   `src/utils/vector_math_test.cc`, `src/gui/Measurement_test.cc`.
   ⚠️ Test pro `src/core/` (např. `CameraNode`) se do globu nechytí — dát
   logiku do `src/utils/` nebo glob rozšířit.
3. **GUI testy — Qt Test.** `src/guitests/` (`TestMainWindow`,
   `TestTabManager`, `TestModuleCache`), spouští `OpenSCAD
   --run-all-gui-tests` (`src/openscad.cc`), build option
   `-DENABLE_GUI_TESTS=ON`. Nový: přidat `TestXxx.{h,cc}`, zaregistrovat v
   `CMakeLists.txt:1540-1552` a v `runAllTest()`.

**Přidání testu** (`doc/testing.md:95`): `.scad` do `tests/data/`, registrace
v `tests/CMakeLists.txt`, `TEST_GENERATE=1 ctest -R <test>` vygeneruje zlatý
soubor → **ručně ověřit** → `ctest -R <test>` musí projít.

**Precedenty pro kamery** (kopírovatelné vzory):
- `dump`/`echo` s `--camera`: `test-view-variables.scad`,
  `tests/CMakeLists.txt:1108-1118`.
- obrazové: `openscad-cam*` nad `camera-tests.scad`,
  `tests/CMakeLists.txt:1120-1150`.
