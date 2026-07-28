# STATE — Fáze 15 `view()` (přepínatelné pohledy)

Návrh a rozhodnutí: viz `ROADMAP.md` → „Fáze 15". Tento soubor drží **aktuální
stav implementace** (co je hotové/ověřené, co zbývá, kde jsem skončil).

## Sémantika (rozhodnuto)
- `view("jméno"){ … }` = strukturální kontejner; **re-instanciace** při přepnutí
  (neaktivní blok = 0 dětí = neviditelný), NE filtr viditelnosti.
- Aktivní = jeden uzel stromu pohledů; render = společné + předci-a-on-sám;
  **akumulace dolů** (vnější ⊂ vnitřní), geometrie i anotace stejně.
- Default = **root** (jen společné = dnešní chování); auto-posun na 1. potomka
  jen když je root prázdný.
- Mechanika: speciální proměnná **`$view` = vektor stringů** = zbývající aktivní
  cesta od tohoto místa dolů. `view("x")` matchuje, když `$view[0]=="x"`; pak
  instanciuje děti s `$view := $view[1:]`. Default `[]` → nic nematchuje → root.
- Anotace se scopují **strukturálně podle obalujícího `view()` bloku** (řádkový
  rozsah z AST, path-aware), žádný `view=` klíč. Enumerace pohledů z **AST**
  (rootFile scope), ne z instanciovaného stromu → funguje i pro neaktivní/vnořené
  a přes `include`. v1: jen literální `view("jméno")`.

## Kroky
- [x] **15A** builtin `view()` + `ViewNode` (transparent container) + registrace +
      `$view` default + NodeVisitor + CMake. Test: dump/echo `view-node` ZELENÉ.
      Ověřeno: default root = jen společné; `-D '$view=["a"]'` aktivuje větev;
      vnoření akumuluje dolů; prázdný view graceful (STL/PNG OK).
- [x] **15B** aktivní cesta → re-instanciace. HOTOVO. `$view`=vektor stringů;
      `RenderVariables.viewPath` injekce. Test `dump-view-active` zelený.
- [x] **15C** `collectViews` (AST) → combo přepínač na editor-toolbaru
      (`setupViewSwitcher`/`refreshViewSwitcher`/`onViewSwitcherChanged`), skrytý
      bez view. Build+smoke(Xvfb) OK; živé kliknutí = user visual check.
- [x] **15D** scoping anotací: `src/gui/ViewScope.{h,cc}` (pure, unit-test
      `ViewScope_test.cc` 26 assertions) — brace-matching bloky, ownerPath,
      isPrefix. `refreshDrawingViews` filtruje kóty/labely + per-view `// @sheet`.
- [x] **15E** write-back: `insertAnnotationLine` píše `// @dim`/`// @label`/
      `// @sheet` dovnitř aktivního bloku (root→EOF); `onDrawingSheetSettingsChanged`
      replace sheet vlastněný aktivním pohledem.
- [x] **15F** headless: `--model-view A` / `--model-view A --model-view B` /
      `--all-model-views`, soubor na pohled (tag ve jméně; single nechává -o).
      Ověřeno na STL exportech (single/list/all/nested).

## Klíčové soubory (hotové)
- `src/core/ViewNode.{h,cc}` — `ViewNode` + `builtin_view` (re-instanciace) +
  `collectViews(SourceFile*)` (AST walk, path-aware, literální jména).
- `src/core/Builtins.cc` — extern+registrace+`$view` default (prázdný Vector).
- `src/core/NodeVisitor.h` — `Visitor<ViewNode>` forward na AbstractNode.
- `src/core/RenderVariables.{h,cc}` — `viewPath` → `$view`.
- `src/openscad.cc` — `--model-view`/`--all-model-views`, `CommandLine.modelViews/
  allModelViews`, view-spec smyčka v `cmdline()`.
- `src/gui/MainWindow.{h,cc}` — `viewSwitcher`/`activeViewPath`, `setRenderVariables`
  nastavuje viewPath, `csgRender()` volá `refreshViewSwitcher()`.
- `tests/data/scad/misc/view-node.scad`, `view-active.scad` + reg. v tests/CMakeLists.

## DŮLEŽITÉ zjištění pro 15D/E
- Per-view **geometrie výkresu i kamer jede zadarmo**: obojí projektuje `rootNode`,
  který teď reflektuje aktivní pohled. 15D/E jsou už jen o **anotacích**:
  scoping `// @sheet` per-view a psaní kót dovnitř bloku. Dnes globální = kóta se
  kreslí, když existuje její `dimref` v aktivním stromu (funkční základ).
- Pro 15D: `collectViews` dává `ViewInfo{path, firstLine, lastLine, fullPath}` —
  bucketovat anotaci = najít nejhlubší `ViewInfo` se stejným `path` (souborem) a
  `firstLine<=řádek<=lastLine`. POZOR ověřit, že `mi->location().lastLine()`
  sahá po `}` (jinak dopočítat z max lastLine dětí).
- Pro 15E: dnes se `// @dim` appenduje na EOF (viz `onDrawingAnchorClicked` a spol.
  v MainWindow); změnit cíl vložení na konec aktivního `view()` bloku.

## Postup / poznámky
- Větev: `cam_and_drawings`. Build: `cmake --build build -j6`; testy
  `cd build && xvfb-run -a ctest -R <r> --output-on-failure`. NECOMMITOVÁNO
  (držím v pracovním stromu dle instrukce „commit jen na vyžádání").
- Vzory: `CameraNode`/`SectionNode` (marker), `BendNode` (kontejner+VISITABLE),
  `control.cc` `builtin_if`/`builtin_for` (signatura `(inst, context)` +
  `Children(inst->scope, childCtx).instantiate(node)`), `RenderVariables.cc`
  (injekce speciálních proměnných), `Builtins.cc:120-140` (defaulty `$vp*`).

## Fáze 16 — CLI kamery per-view + CLI výkres
- [x] **16A CLI kamery per-view/all** HOTOVO. `export_cameras` v `openscad.cc`
      přepsán na smyčku přes pohledy; sdílený `resolveViewSpecs` (dedup s
      `cmdline`). `--export-cameras` teď respektuje `--model-view`/`--all-model-views`
      (kamery uvnitř `view()` bloků; tag pohledu ve jméně u dávky). Ověřeno +36 testů.
- [x] **16B CLI výkres (SVG/PDF/PNG/DXF)** HOTOVO. Zvolena nízkoriziková cesta:
      CLI recykluje celý GUI pipeline přes **headless MainWindow** (offscreen
      QApplication), takže `refreshDrawingViews` zůstal netknutý (16B-2 nebyla třeba).
      - 16B-1: `DrawingViewDock.cc` — `renderToFile`→file-scope `renderSheetToFile`
        (+PNG), public `exportDrawingSheet(...)` a `exportToPath(path)`; `exportDrawing`
        = tenký wrapper. `.h` public metody.
      - 16B-3: `MainWindow::exportDrawingHeadless(modelViews, all, prefix, ext)`
        (synchronní `actionRenderPreview` per pohled → `dock->exportToPath`).
        `gui()` + `CliDrawingExport` (openscad_gui.{h,cc}) → `QTimer::singleShot`.
        `openscad.cc`: `--export-drawing`/`--drawing-format` routuje do `gui()`
        (guard `#ifndef OPENSCAD_NOGUI`).
      - Ověřeno pod xvfb: `--model-view a` → out_a.svg (25 polyline+text); PNG 325×280;
        `--all-model-views` → oba pohledy. Plná ctest zelená (9 flaky image, sériově OK).

## Log
- 2026-07-27: 15A/B/C/F HOTOVÉ a otestované (jádro jazyka + GUI přepínač +
  headless). Plná `ctest -j6` zelená (3 image faily = známá llvmpipe flakyness,
  sériově projdou).
- 2026-07-27: 15D/E HOTOVÉ. `src/gui/ViewScope.{h,cc}` + `ViewScope_test.cc`
  (26 assertions). `MainWindow`: filtr kót/labelů + per-view `// @sheet` v
  `refreshDrawingViews`; `insertAnnotationLine` (zápis do bloku); sheet-write
  replace vlastněný aktivním pohledem. Plná `ctest -j6` = 2 image faily, sériově
  projdou (llvmpipe). GUI smoke s anotacemi v bloku bez pádu. **CELÁ FÁZE 15
  HOTOVÁ.** Živý round-trip kót/přepínače ověří uživatel. NECOMMITOVÁNO.
  - Nezautomatizované (GUI-only, ověří uživatel): vizuál per-view kót/`// @sheet`
    v doku Drawing a že klikaná kóta fyzicky skončí ve správném bloku.
