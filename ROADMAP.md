# ROADMAP — rozšíření OpenSCADu (kamery + technické výkresy)

Plán dvou funkcí definovaných ve zdrojovém kódu modelu. Orientaci v kódu
najdeš v `CLAUDE.md`.

**Cíl / motivace:** dnes obojí řeší autor externími nástroji (render kamer
mimo OpenSCAD; technická dokumentace ve FreeCADu / openscad branch), což je
neohrabané — při každé změně modelu se musí vše dělat znovu. Obě funkce mají
žít v `.scad` zdrojáku a aktualizovat se se změnou modelu.

**Pořadí (rozhodnuto):** nejdřív **Fáze 1 (kamery)** jako uzavřená
použitelná věc, pak **Fáze 2 (výkresy + kvóty)** po krocích.

**Vývojová větev:** `master`. Kompatibilita se stable = neškodné varování
stačí (viz níže).

**Názvosloví:** práci dělíme na **fáze** (Fáze 0 baseline, Fáze 1 kamery,
Fáze 2 výkresy); uvnitř fáze jsou očíslované **kroky**. Ke každé fázi patří
i její **testy** — bez zelených testů není fáze hotová.

---

## Stav implementace

- **Fáze 0 (baseline)** — ✅ **HOTOVO**. Build masteru, `ctest` baseline zelený
  (2721 testů). Pozn.: hrstka `preview-*`/`render-*` image testů je flaky pod
  softwarovým GL (llvmpipe) při `-j`, v `-j1` projdou — nejde o regresi.
- **Fáze 1 (kamery)** — ✅ **HOTOVO** (větev `cam_and_drawings`, 5 commitů nad
  `master`, viz níže). Builtin `camera()`, neviditelná geometrie, sběr kamer,
  převod matice→pohled, offscreen render snímků (s barvami z `color()`), dok
  „Cameras" + Save PNG, drátěné frustumy v 3D, `marker=`, skrytí frustumu v F6.
  Unit + regresní (`dump/echo-camera-node`) testy zelené; ověřeno i vizuálně v
  GUI.
  - Commity: `f934423bd` jádro+math+testy · `e58cdc857` engine+dok+F5 ·
    `a88628d89` frustum · `859caac72` `marker=` · `b359bb7ab` skrytí frustumu v F6.
  - Volitelné navazující (mimo kroky 1–9): CLI `--export-cameras` (headless
    dávkový render z kamer + odemkne render regresní test), GUI test doku.
- **Fáze 2 (výkresy + kvóty)** — ✅ **HOTOVO** (rozsah A + B + `kvota=` cukr;
  zatím jen **délková kvóta dvou bodů**; interaktivní vkládání kotev klikáním do
  holého povrchu „C" i průměr/úhel = pozdější rozšíření).
  - **2A** (commit `dff6172c8`): dok „Drawing" se třemi ortho pohledy
    (nárys/bokorys/půdorys), `render_ortho_view` (OpenCSG, barvy zachovány),
    obnova po F5, Save Sheet.
  - **2B-1** (`4fa85cb64`): `dimref("name", kvota=…)` uzel + `collectDimrefs`
    (neviditelný, prázdná geometrie); dump/echo testy.
  - **2B-2** (`17e2f866a`): parsování `// @dim A B [proj=…] [label=…]` +
    `kvota=` sugar; přepočet délky z world-pozic; 2D overlay (body + kóty)
    projekcí world→pixel přes GL matice renderu.
  - **2B-3** (`21ae9d742`): klik dvou bodů v pohledu → zápis `// @dim` (projekce
    dle pohledu). Vše ověřeno vizuálně v GUI.
  - **2C-1/2** (`d6bb8dce8`): odsazené kóty — `offset=` v `@dim` (mm v rovině
    pohledu), vynášecí čáry + šipky; **tahatelné myší** v 2D → po puštění se
    `offset=` zapíše zpět do zdroje. Ortho render s okrajem (fov=75) + clamp
    labelu, ať odsazené kóty nezmizí za okrajem.
  - **2C-3** (`e8a19803a`): dimref body (modré křížky) + čáry kót (červené) i
    v **hlavním 3D pohledu** (jako frustumy kamer; skryté v F6).
- **Fáze 3 (OBJ export s barvami / `.mtl`)** — ✅ **HOTOVO** (commit `dfd3a8b86`).
  `export_obj.cc` seskupí facy podle barvy → sesterský `.mtl`
  (`newmtl`/`Kd`/`d`), odkaz `mtllib`. Bezbarvý model + stdout zůstávají
  **bit-identické**. Regresní test `export-obj_color-export` zelený.
  **Ověřeno v Blenderu 4.3.2** (headless import): OBJ+MTL → 1 mesh, 3 materiály
  se správnými barvami včetně průhlednosti (α=0.5). 3MF cesta funguje na
  oficiálních buildech blender.org ≥ 4.1 (Debianní `blender` dfsg balík 3MF
  addon nemá zabalený).
- **Fáze 4 (navazující)** — ✅ **HOTOVO** (větev `cam_and_drawings`, commity
  `Phase 4A/4B/4C/4D`). 4A CLI `--export-cameras`; 4B měřicí režimy +
  duplicitní kotvy `A#i` + ⌀/R + úhel + chain/running; 4C vektorový výkres
  (silueta+crease, hidden-line přes CPU depth buffer, SVG+DXF); 4D zoom/pan +
  pravý klik skok do zdroje. Ověřeno vizuálně v GUI; relevantní regrese
  (dimref/camera/obj dump-echo) zelené.

- **Fáze 14 (`bend()`)** — ✅ **HOTOVO** (2026-07-23). Builtin `bend(at, r, angle,
  xlim, ylim, zlim)` ohýbá i importovaná STL (nelineární mapa pod úrovní meshe,
  což knihovny jako BOSL2 neumí). Analytická C1 mapa, dělení trojúhelníků jen
  v pásmu ohybu a na rovinách masky, detekce trhlin a minimálního poloměru,
  zachované barvy facy. **14B**: zbytek modelu se klasifikuje podle **souvislosti
  meshe** — co visí na ohýbaném konci, jede tuze s ním (široká plocha za úzkým
  žebrem), co visí na pevné části, stojí; nespojené skořepiny podle vlastní
  polohy. 26 testů (dump/echo, obrazové na obou backendech, export-stl,
  11 unit testů) zelených.

- **Fáze 15 (`view()` — pohledy)** — ✅ **HOTOVO** (2026-07-27; 15A–F). Jádro
  jazyka + GUI přepínač + headless dávka + strukturální scoping anotací.
  Přepínatelné „pohledy" = pojmenované stavy modelu (`view("jméno"){ … }`):
  jedna aktivní cesta ve **stromu pohledů**, re-instanciace při přepnutí,
  akumulace **dolů** (vnější geometrie i anotace platí i uvnitř). Default =
  **root** (jen společné, dnešní chování).
  - **HOTOVO 15A/B**: builtin `view()` + `ViewNode` (průhledný kontejner) +
    speciální proměnná `$view` (vektor stringů = zbývající aktivní cesta); děti se
    instanciují jen na aktivní cestě, s převázáním `$view := tail`. Testy
    `dump/echo-view-node` (default root) + `dump-view-active`
    (`--model-view assembly/exploded`) zelené; vnoření/akumulace ověřeny.
  - **HOTOVO 15C**: GUI přepínač (combo na editor-toolbaru, strom s odsazením),
    skrytý bez `view()`; výběr → recompile s aktivní cestou → **preview, kamery
    i výkres se přepočtou z re-instanciovaného stromu** (per-view geometrie
    „zadarmo", protože obojí projektuje `rootNode`).
  - **HOTOVO 15F**: headless `--model-view A` (single), opakované/`A/B` (seznam),
    `--all-model-views` (všechny) → soubor na pohled (tag ve jméně). Ověřeno na
    STL exportech.
  - **HOTOVO 15D/E**: strukturální scoping anotací podle obalujícího `view()`
    bloku. `src/gui/ViewScope.{h,cc}` (pure, unit-test `ViewScope_test.cc`, 26
    assertions): brace-matching → rozsahy bloků, `ownerPath(řádek)`, `isPrefix`
    (viditelnost = owner je prefix aktivní cesty), `insertLine` (cíl zápisu).
    `refreshDrawingViews` filtruje kóty/labely podle owner-prefixu a vybírá
    **nejhlubší `// @sheet` na aktivní cestě** (per-view list). Klikané `// @dim`/
    `// @label` a `// @sheet` se píšou **dovnitř aktivního bloku** (root → EOF).
    Bez `view()` = chování identické jako dřív.

- **Fáze 16 (CLI kamery per-view + CLI výkres)** — ✅ **HOTOVO** (2026-07-27; 16A+16B).
  - **16A**: `--export-cameras` respektuje výběr pohledů — `--model-view A`
    vyrenderuje i kamery uvnitř `view("A")`, `--all-model-views` projede všechny
    (tag pohledu ve jméně). `export_cameras` přepsán na smyčku přes pohledy,
    sdílený `resolveViewSpecs` s `cmdline`.
  - **16B**: CLI export **technického výkresu** vybraných pohledů —
    `--export-drawing <prefix>` (+ `--drawing-format svg|pdf|png|dxf`). Recykluje
    celý GUI pipeline (běží headless pod offscreen `QApplication`), respektuje
    `--model-view`/`--all-model-views`. Přidán i **PNG** export (i do GUI doku).
    Ověřeno (SVG/PNG, single/all); regrese zelené.

---

## Fáze 0 — baseline (než sáhneme na kód)  ✅ HOTOVO

Cíl: mít jistotu, na čem stavíme, a referenci pro „nerozbilo se nic navíc".

1. Založit **git branch** pro vývoj (nesahat na `master` napřímo).
2. **Přeložit nezměněný `master`** (viz `README.md`; případně doladit
   závislosti/GL).
3. **Projet celou testovací sadu** z build adresáře:
   `ctest -j8` (render/GUI testy přes `xvfb-run ctest`, viz `doc/testing.md`).
4. **Zaznamenat výsledek** (co je zelené / co selhává už teď) do poznámky —
   to je baseline. Existující `dump`/`echo`/`render` testy musí zůstat
   stejné i po všech našich změnách (naše neviditelné uzly nesmí změnit
   žádný stávající výstup).

Bez úspěšné Fáze 0 nezačínáme psát kód.

---

## Fáze 1 — kamery v modelu  ✅ HOTOVO (viz Stav implementace výše)

### Zadání
Umístit do modelu jednu či více kamer s parametry (rozlišení, ohnisko).
Na stisk klávesy / z menu se otevře okno, kam se vyrenderuje snímek z každé
kamery **včetně barev z `color()`** (což řeší, že Blender barvy z STL
nepřečte). Jinak kamera nic nedělá a je neviditelná v exportu/renderu.

### Rozhodnutí
- **Definice = builtin modul `camera(...)`** (ne komentář) — umožňuje
  standardní pozicování přes `translate()/rotate()`.
- **Objektiv = ohnisko + senzor**: `camera(focal=35, sensor=24, width=1920,
  height=1080)`. FOV se dopočítá: `fov_v = 2·atan(sensor/(2·focal))`
  (`sensor` = svislý rozměr senzoru v mm, default 24 = full-frame).
  Volitelně povolit i přímé `fov=` jako alternativu.
- **Obnova snímků = automaticky po každém preview (F5)**, ale renderovat
  jen když je dok kamer viditelný.
- **Marker v hlavním 3D pohledu = ANO**: u každé kamery vykreslit
  **drátěný čtyřboký jehlan (frustum)** ukazující pozici, směr pohledu a FOV.
- **Kompatibilita se stable**: `camera()` je tam neznámý modul → vypíše
  `Ignoring unknown module 'camera'`, model se vyrenderuje identicky.
  Do modelu NEdávat `module camera(){}` (přebilo by builtin).

### Konvence
- Kamera kouká podél lokálního **−Z**, up = **+Y** (upřesnit/ověřit při
  implementaci; vyřešit roll z transformační matice).

### Kroky implementace
1. **Jádro:** `CameraNode : AbstractNode` (`src/core/CameraNode.{h,cc}`),
   bez geometrie. Pole: `focal, sensor` (nebo `fov`), `pixel_width/height`,
   `name`, `projection`. Šablona `ColorNode`.
2. **Registrace:** `register_builtin_camera()` + zápis v `Builtins.cc`.
3. **Prázdná geometrie:** ošetřit `CameraNode` v `GeometryEvaluator` /
   `CSGTreeEvaluator`, aby vracel prázdno (nesmí rozbít preview/render/
   export/měření).
4. **Sběr kamer:** `collectCameras(node, parentMatrix)` — průchod `rootNode`,
   akumulace `TransformNode::matrix`, u `CameraNode` uložit world-matici +
   parametry.
5. **Převod:** z world-matice odvodit eye/center/up → nastavit `Camera`
   (příp. přidat `Camera::setFromMatrix(...)`). Ohnisko→FOV dle vzorce výše.
6. **Render snímku:** offscreen přes **preview renderer** (`OpenCSGRenderer` /
   `prepare_preview` z `export_png.cc`) do FBO → `QImage` (zachytí barvy).
7. **Dok kamer:** `CameraViewDock` (šablona `AIDock`): seznam/taby kamer,
   náhled, tlačítko **Uložit PNG**. Registrace v `setupDocks()`.
8. **Marker v 3D:** v `QGLView` po vykreslení scény nakreslit drátěný
   frustum pro každou kameru (z dat `collectCameras`).
9. **Napojení obnovy:** po dokončení preview (`csgRender`) přesbírat kamery,
   překreslit markery vždy, snímky jen když je dok viditelný.

### Testy (součást fáze — bez zelených testů není hotovo)
- **Unit (Catch2)** — nejcennější, deterministické, bez GL: převod
  `focal+sensor → fov`; `collectCameras` (strom translate→rotate→camera →
  ověřit world-matici/pozici); `Camera::setFromMatrix`. Uložit jako
  `*_test.cc`; **pozor:** glob bere jen `src/utils/*_test.cc` a
  `src/gui/*_test.cc` (`CMakeLists.txt:1574`) → čistou logiku dát do
  `src/utils/`, nebo glob rozšířit.
- **Regresní `dump`/`echo`** — `.scad` s `camera(...)` ověří, že se parsuje a
  vytvoří **prázdný uzel** (nerozbije CSG strom). Vzor už v repu:
  `dump-cli-view-variables-7` / `echo-cli-view-variables-7`
  (`tests/data/scad/misc/test-view-variables.scad`,
  `tests/CMakeLists.txt:1108-1118`). Soubor pod
  `tests/data/scad/3D/features/` se auto-globne do dump/echo/render/preview.
- **Regresní `render` (PNG)** — `camera()` vyrenderovaná normálně je v
  exportu **neviditelná** (frustum je jen preview overlay). Vzor:
  `openscad-cam*` nad `camera-tests.scad` (`tests/CMakeLists.txt:1120-1150`).
- **Regenerace zlatých souborů:** `TEST_GENERATE=1 ctest -R <test>` →
  **ručně ověřit** → `ctest -R <test>`.
- **Nerozbití:** porovnat plnou `ctest` proti baseline z Fáze 0 — stávající
  dump/echo/render nesmí zčervenat.

### Rizika
- (3) korektní ignorování `CameraNode` v geometrii;
- (6) offscreen render sdílený s hlavním GL kontextem;
- (5) roll / orientace při převodu 4×4 matice na `Camera`.

---

## Fáze 2 — technické výkresy a kvóty  ✅ HOTOVO (A + B + cukr; viz Stav výše)

### Zadání
Nárys / bokorys / půdorys (ortografické pohledy) + **délkové kvóty mezi dvěma
body**. Kvóty žijí v `.scad` a **aktualizují se se změnou zdrojáku**. Dnes autor
používá FreeCAD openscad branch, kde po vytvoření modelu už nejde editovat — to
je hlavní bolest.

**Rozsah pro teď:** implementujeme **A + B (+ cukr)**. Zatím **jen délková kvóta
dvou bodů**. Interaktivní vkládání nových kotev klikáním do holého povrchu (C)
**zatím neděláme** (viz níže). Průměr/poloměr/úhel apod. = **pozdější rozšíření**.
Dojem autora: **B s cukrem může být samo o sobě dost dobré.**

### Datový model (ROZHODNUTO)
- **`dimref("jmeno")`** = pojmenovaný **neviditelný marker-uzel** (stejný typ
  jako kamera). Zapsaný ve stejném `translate()/rotate()` jako objekt se s ním
  **hýbe automaticky** (sdílená transformace). World-pozice se sbírá **stejným
  kódem jako kamery** (obdoba `collectCameras` → `collectDimrefs`).
- **Kvóta se odkazuje na DVA pojmenované `dimref`** (ne „stejný název = pár"):
  `// @dim A B popisek proj=… offset=… dir=…`. Důvody: jedna kotva jde
  **znovupoužít** ve víc kótách i ve víc projekcích; veškerá **prezentace**
  (popisek, offset, směr, **projekce**) je na jednom editovatelném místě.
- **Projekce je pole kvóty** (`proj=front|side|top`), ne kotvy — kvóta se typicky
  kreslí **jen v jedné** projekci.
- **Hodnota se počítá až při renderu** = vzdálenost world-pozic dvou bodů →
  aktualizuje se sama. Popisek buď spočítaný, nebo ručně přepsaný.
- **Sběr kvót je na JEDNOM místě** (jeden collector), který vrací kvóty ze
  **dvou zdrojů**: (a) explicitní `@dim` řádky, (b) `kvota=` cukr (níže). Díky
  tomu je cukr jen další větev — přidatelný bez přepisu overlay/editoru.

### `kvota=` cukr (additivní, plánováno hned v B)
- Zkratka pro **minimum psaní bez klikání**: `dimref(kvota="sirka")` na dvou
  uzlech → collector **automaticky** vyrobí kvótu `sirka` mezi nimi (interní
  názvy dimref si dogeneruje). Rozloží se (desugar) na základní model.
- **Vznik** přes zkratku (defaultní prezentace); **doladění** offsetu/projekce
  v doku dopíše explicitní `// @dim sirka …` (nebo parametry) jako override.
- Je to vědomě „stejný `kvota=` → pár" — jako **cukr** to vadí nemůže: kdo
  potřebuje sdílet kotvu do víc kót, sáhne po explicitním `@dim`.

### Vykreslení + výběr (jádro B — proč je snadné)
- Kvóta = **2D overlay** v doku výkresu: projekce world-bodů do aktuálního ortho
  pohledu, šipky + text v Qt. **Žádný zápis do geometrie.**
- **Výběr existujících `dimref` v 2D je snadné:** world-pozice známe (collector)
  → dopředná projekce do 2D → nakreslit **klikací puntík s názvem** → klik dvou
  puntíků sestaví kvótu (zápis `@dim`). Je to jen dopředná projekce **známých**
  bodů — žádná re-detekce geometrie. Alternativa: výběr jmen ze seznamu.
- Drobnosti (ne blokery): překryv projekcí (dva dimref na stejné 2D místo →
  řešit v jiné projekci / seznamem), popisky puntíků pro jednoznačnost.

### Hranice automatiky
- Posun/rotace objektu → bod jede s ním zadarmo. ✅
- Změna **velikosti** objektu (bod na pohybující se stěně) → funguje jen když je
  bod vyjádřený **stejnou proměnnou** jako rozměr (`translate([w,0,0])
  dimref("A")`). **Automatická re-detekce geometrického prvku po libovolné
  editaci obecně NEJDE** — trackujeme přes transformace a proměnné, ne přes
  re-detekci geometrie.

### Kroky (A/B)
- **A:** dok „Výkres" s ortografickými pohledy (nárys/bokorys/půdorys, reuse
  ortho kamery z Fáze 1). Jen prohlížení.
- **B:** `dimref` uzel + `collectDimrefs`; čtení `// @dim` (+ `kvota=` cukr);
  přepočet délky z world-pozic; **2D overlay kvót**; **výběr dimref v 2D** →
  zápis/úprava `@dim`; formulářový/klikací editor kvót v doku.
- **„C" (interaktivní vkládání nových `dimref` klikáním do holého povrchu) —
  ZRUŠENO** (rozhodnutí autora). Kotvy píše uživatel ručně ve 3D kódu; klikáním
  se jen sestavují kvóty z existujících dimref (to je součást B).

### Testy (součást fáze)
- **Unit (Catch2)** — přepočet délky kvóty z world-pozic; `collectDimrefs`
  (sdílí kód s kamerami); parsování `// @dim` i `kvota=` desugar.
- **Regresní `dump`/`echo`** — `dimref` je prázdný uzel, nerozbije CSG strom
  (vzor: `dump/echo-camera-node`).
- **GUI test** (`src/guitests/`, styl `TestMainWindow`) — round-trip: vybrat
  dva dimref → vložit `@dim` → upravit offset/projekci → ověřit text v editoru.

### Fáze 2E — konfigurace doku + editace kót (🔶 rozpracováno)
Požadavky autora, seskupené podle náročnosti:

**Rychlé (per-kóta atributy + přepínače doku):**
- **2E-1 ✅** (`dcb1144bb`) **label šablona + přesnost + šipky** — `label="… %f …"`
  (dosadí délku, `prec=N` přesnost), `arrows=in|out`. Uvozovkovaný label.
  Ověřeno („šířka 40.0 mm").
- **2E-3 ✅** (`37740ed4e`) **posun labelu** — `laboff=du,dv`, tažení labelu →
  zápis zpět. Ověřeno.
- **2E-2 ✅** (`b620023ac`) **vypnout referenční tečky** — checkbox „Reference
  points" skryje dimref tečky+jména (labely kót zůstanou). Ověřeno.
- **2E-4 ✅** (`cb7d36fc9`) dropdown pro koincidentní body — klik na místo s víc
  dimref → menu s výběrem (A/H1). Ověřeno.
- Pozn.: editace atributů jde vždy i přímo v `@dim` řádku.
- **2E-5 🔶 UI editace kóty** — dostupné z projekcí **dvojklikem na kótu nebo
  její label** → malý dialog: šablona labelu (`%f`), přesnost, šipky in/out,
  projekce. Zapisuje tytéž `@dim` tokeny; ruční editace v kódu zůstává.

**Střední (režimy doku):**
- **✅ Barevný vs. čárový (černobílý) režim** (`fcb9db24b`) — checkbox „Line
  drawing (b&w)": barvy `color()` potlačeny (bílé plochy přes technické
  color-scheme) + viditelné hrany. `render_ortho_view` je teď soběstačný.
  Pozn.: opraven dangling pointer (GLView drží color-scheme přes pointer →
  scheme musí být statické) + inicializace edge shaderu offscreen.

**✅ Export v měřítku (SVG + PDF)** (`286bc3e67`):
- Náhled v doku zůstává responsivní; měřítko je vlastnost exportu. Spinbox
  „Scale 1:N" + tlačítko „Export SVG/PDF…".
- Výstup vektor **SVG i PDF**, jednotky **mm**, bez omezení papírem. Geometrie
  v měřítku (model-mm : výkres-mm), **popisky/šipky pevné pt velikosti** (pravý
  vektorový `<text>`, čitelné při jakémkoli měřítku). Model jako vložený raster
  ve správné mm velikosti, overlay (čáry/šipky/text) vektor přes QPainter na
  `QSvgGenerator`/`QPdfWriter`. Ověřeno: SVG 504×1513 mm + PDF %PDF-1.4.

---

## Fáze 4 — navazující (✅ HOTOVO)

Implementováno na větvi `cam_and_drawings` (commity `Phase 4A/4B/4C/4D`):
- **4A** — CLI `--export-cameras <prefix>`: headless dávkový render všech
  `camera()` do `<prefix>_<name>.png` (reuse `render_camera_preview`, zachová
  barvy `color()`). Samostatná cesta v `openscad.cc`, bez `-o`.
- **4B** — kóty: `measure=direct|planar|h|v` (4 režimy vůči projekci, `h`/`v`
  osově zarovnané); index-kvalifikované duplicitní kotvy `A#i` + varování +
  self-correcting re-klik; **⌀/R** (`dimref(d=/r=)` + `@dim … kind=diameter|
  radius angle=`); **úhel** (`@dim angle V A B`, exaktní 3D); **chain/running**
  série kót.
- **4C** — vektorový výkres: `FeatureEdges` (silueta = per-hrana front/back-
  facing, crease dle dihedrálu, boundary; welding vrcholů), **hidden-line přes
  CPU depth buffer**, výstup **SVG i DXF** (tlačítko „Line drawing (vector)…"
  v doku). Facety hladkých ploch mizí; okluze se řeší z-bufferem.
- **4D** — UX doku: **zoom (kolečko) + pan (tažení)** per-view přes
  `userZoom/userPan`; **pravý klik = skok do zdroje** (kóta → `@dim` řádek,
  dimref kotva → `dimref()` řádek), levý zůstává na tvorbu/tahání kót.

Detailní návrh jednotlivých bodů níže.

### 4A — CLI `--export-cameras <prefix>`
Headless dávkový render snímků ze všech `camera()` v modelu (autorův reálný
workflow + odemkne render regresní test). Engine hotový (`render_camera_preview`);
doplnit option do `CommandLine` a do `do_export`/cmdline cesty v `openscad.cc`
(pozn.: `CommandLine` má `const&` členy, init na ~2 místech). Výstup PNG
`<prefix>_<name>.png` per kamera. Otestovat cmdline testem.

### 4B — další typy kót
- **Poloměr / průměr** — hodnotu NELZE brát z geometrie (válec = N-úhelník dle
  `$fn`, kružnice ve finální meshi není). Proto **hodnota ze zdroje**:
  **parametr na `dimref`** — `dimref("A", d=5)` / `r=2.5` (klidně z proměnné,
  OpenSCAD vyhodnotí). Střed = world-pozice dimref. Display (směr/otočení,
  projekce, offset, popisek) na `@dim` řádku: `// @dim A kind=diameter proj=top
  angle=45`. Label „⌀5"/„R2.5". (Ne nový uzel — parametr na dimref = 1 collector.)
- **Úhel** — nepotřebuje uloženou hodnotu: počítá se ze **tří přesných dimref**
  (vrchol + 2 konce), `// @dim angle V A B` = úhel ve V mezi VA a VB. Exaktní.
- **Postupná / řetězená kóta** — layout nad víc délkovými kótami: řetězené
  (konec-na-konec na jedné čáře) nebo od společné základny (running). Zobrazovací
  režim nad uspořádaným seznamem dimref v jednom směru.

**Robustnost — duplicitní jména dimref (cyklus + modul):**
- `dimref` ve **for-cyklu** (typicky v hloubi modulu, který se cyklí nahoře) →
  víc uzlů se **stejným jménem** na různých pozicích. `byName[jméno]=pos` se
  přepíše → `@dim A B` tiše vezme **poslední**, klik spojí špatnou instanci.
  `str("A", i)` NEfunguje, když dimref **nevidí** proměnnou cyklu (je v modulu).
- **Identita vs. reference:** každá instance MÁ jednoznačnou identitu (`idx`,
  world-pozice); kolize je jen v ploché textové referenci `@dim A B`.
- **Dvě komplementární stability** (dokonale stabilní ref NEEXISTUJE — instance
  looped dimref jsou efemérní):
  - **index** (pořadí ve stromě): odolný vůči **geometrii** (translate/rotate/
    scale nemění pořadí), rozbije ho **strukturální** změna (rozsah cyklu).
  - **pozice** (global): odolná vůči **struktuře**, rozbije ji **geometrie**.
  - Geometrické úpravy jsou mnohem častější → **volíme INDEX**.
- **Řešení (rozhodnuto): reference = index mezi stejnojmennými** ve stromovém
  DFS pořadí: `// @dim A#2 B#0`. Kóta se tvoří **klikem** na konkrétní puntík
  (známe přesnou instanci → zapíšeme její index). **Sebeopravné:** když se cyklus
  změní a kóta skočí jinam, uživatel **klikne znovu** → přepíše se index
  („upravím kvoty" = re-klik). Pořadí deterministické z `collectDimrefs`.
- Alternativy pro speciální případy: **id-parametr** modulu
  (`widget(id=i){ dimref(str("A",id)); }`) když modul píšeš = nejstabilnější;
  **radiální/průměrová** kóta v cyklu žádné jméno nepotřebuje — dimref se
  **auto-vykreslí na každé instanci** (řada děr → okótuj každou).
- **Tooling:** `collectDimrefs` **hlásí varování** při duplicitním jménu na víc
  pozicích (viditelné upozornění místo tichého chybného chování).

**Sémantika měření — 4 režimy (relativně k projekci):**
Autor to trefil líp než světové osy: měření je vztažené k **projekční rovině**.
`@dim` dostane `measure=`:
  - `measure=direct` (default) — **pravá 3D vzdálenost** `|b-a|`, nezávislá na
    pohledu, čára mezi body (aligned). *(dnešní chování)*
  - `measure=planar` — **vzdálenost v projekční rovině** `|proj(b)-proj(a)|`
    (ignoruje hloubku daného pohledu).
  - `measure=h` — **horizontální** složka v pohledu (vodorovná kóta).
  - `measure=v` — **vertikální** složka v pohledu (svislá kóta).
- **2/3/4 jsou vázané na `proj=`** (h/v mapují na světové osy dle pohledu: nárys
  h=X v=Z, půdorys h=X v=Y, bokorys h=Y v=Z). Platí `h² + v² = planar²`.
- Řeší „vzdálenost dvou ploch" (dva body na protilehlých stěnách kvádru, mezi
  nimi hrana není): zvol pohled + `h`/`v` kolmé ke stěnám → odstup stěn (ne šikmá
  3D vzdálenost). `h`/`v`/`planar` se kreslí **osově zarovnané** s vynášecími
  čárami z bodů na kótovací čáru, jak je ve výkresech zvykem.

### 4C — vektorové projekce (technický výkres)
Finální geometrie jsou polygony → projekce hran do 2D je **přesný vektor**.
Cíl = čistý, kompaktní vektorový výkres (ne obří SVG). **Nekreslit plochy**
(painter's algorithm = tisíce polygonů, artefakty), kreslit jen správné **hrany**:

**Výběr hran (per pohled):**
- **Silueta = per-hrana front/back-facing test** (NE `projection()` obrys!):
  hrana sdílená dvěma fasetami je silueta, když jedna faseta míří ke kameře a
  druhá od ní (`sign(nA·view) ≠ sign(nB·view)`). Zachytí i **vnitřní pohledově
  závislé kontury** — např. oblý obrys válce stojícího před kvádrem (kvádr sahá
  za něj, takže to není vnější obrys, ale hrana se má nakreslit). **Toto je
  hlavní zdroj siluet**; počítá se pro každý ze tří pohledů zvlášť.
- **Crease/feature hrany** = ostrý dihedrální zlom mezi sousedními plochami
  (úhel > prahu). Pohledově nezávislé. Tessellační fasety hladkých ploch (bok
  válce) NEjsou crease → **automaticky zmizí** (proto čistý výkres). Horní/dolní
  kružnice válce (bok↔víko) crease jsou → nakreslí se.
- `projection()` jen jako **bonus** na vnější obrys celé scény, ne hlavní zdroj.

**Viditelnost (hidden-line) přes GPU depth buffer (místo analytického HLR):**
1. Offscreen render modelu **jen do depth bufferu** (rychlé, skoro to umíme).
2. Pro každou vybranou hranu jdi po její 2D projekci a **sampluj depth**: kde je
   hrana nejblíž (hloubka ≈ buffer) → **viditelný úsek** → vektorová čára; za
   plochou → zahoď. Okluzi řeší GPU.

**Výstup:** kompaktní SVG/DXF — jen viditelné feature+silhouette hrany.

**Oblouky NEŘEŠÍME** — OpenSCAD jede přes úsečky, výkres taky (schodovitá silueta
válce je při rozumném `$fn` OK). Jediná výjimka, kde hodnota jde ze zdroje, je
**poloměrová/průměrová kóta** (4B, `dimref(d=/r=)`).

### 4D — UX doku Výkres
- **Zoom + pan v pohledech** — kolečko = zoom, tažení = pan v každém ortho
  pohledu (monitor není nekonečný). Přidat uživatelskou zoom/pan transformaci do
  `mapToWidget` (fitScale/fitOffset) → overlay, kóty, tažení i klikání se posunou
  konzistentně. Volitelně „maximalizovat jeden pohled".
- **Klik → skok do zdrojáku** (jako 3D pravý klik: idx→`getNodeByID`→
  `modinst->location()`→řádek). Konvence: **pravý klik = skok**, levý = tvorba
  kóty (aby se nepraly). Dvě úrovně:
  - Pravý klik na **dimref** → jeho řádek (triviální, máme lokaci uzlu). *(hned)*
  - Pravý klik na **geometrii** → řádek objektu pod kurzorem. Vyžaduje offscreen
    **idx-buffer** (druhý pass jako `MouseSelector`) + sampling. *(víc práce)*

---

## Fáze 3 — OBJ export s barvami (`.mtl`)  🔶 naplánováno

### Motivace
Při vývoji chybí spolehlivý **barevný export do Blenderu**. STL barvy ztrácí,
Blender je z STL nepřečte. Zjištěno (viz níže „Jak jsou barvy uložené"):
- **3MF už barvy exportuje** (per-trojúhelník, `basematerials`/color group) a
  **Blender ≥ 4.1 ho importuje nativně** → to je hotová, doporučená cesta.
- **OBJ** má data, ale `export_obj.cc` je **zahazuje** — nepíše `.mtl`. Blender
  přitom OBJ+MTL nativně čte. Proto: dopsat `.mtl` do OBJ exportu jako robustní
  textovou alternativu (1 Blender materiál na 1 OpenSCAD barvu).

### Jak jsou barvy uložené (podklad)
- Barva je **per-region** — `color()` obarví celý podstrom
  (`GeometryEvaluator.cc:488` `setColor`); Manifold ji trackuje per source-object
  přes „original ID" (`ManifoldGeometry.h:81`), CSG operace ji přenášejí.
- Při exportu se zploští na **per-face**: `PolySet` má paletu `colors` +
  `color_indices` (jeden index na face) — `PolySet.h:26-27`.
- **Žádné per-vertex barvy, žádné UV, žádná textura** — model = sada plochých
  barevných oblastí. → **Textura je špatný nástroj** (vyžadovala by UV unwrap a
  rastrovala by ostré hranice); správná reprezentace = per-face / materiál.

### Rozhodnutí
- Cíl = **rozšířit stávající `src/io/export_obj.cc`** o generování `.mtl`
  (žádný nový formát, žádný zásah do geometrie).
- 1 `newmtl` na 1 barvu palety; facy seskupené podle `color_index` s `usemtl`.

### Kroky implementace
1. **`.mtl` obsah:** z `PolySet` paletu `colors` → `newmtl color_N` s `Kd r g b`;
   u průhledných z alfy `d`/`Tr`. Facy s `color_index == -1` → default materiál.
2. **`.obj` napojení:** na začátek `mtllib <basename>.mtl`; facy **seřadit /
   seskupit podle `color_index`**, před každou skupinu vypsat `usemtl color_N`.
3. **Kam `.mtl`:** vedle `.obj` (název odvodit z výstupní cesty). Ošetřit zápis
   na **stdout** (bez cesty) — buď `.mtl` vynechat + varovat, nebo dohodnutý
   název; doladit při implementaci.
4. **Zpětná kompatibilita:** bezbarvý model musí dát **bitově stejný `.obj`**
   jako dnes (žádné `mtllib`/`usemtl`, pokud `color_indices` prázdné) — jinak
   zčervenají stávající golden soubory.

### Testy (součást fáze)
- **Regresní `export-obj`** (stávající) — bezbarvý model beze změny (viz krok 4).
- **Nový regresní** — `.scad` s `color()` → ověřit vygenerovaný `.obj`
  (`mtllib`/`usemtl`) i `.mtl` (`newmtl`/`Kd`). Zlaté soubory
  `TEST_GENERATE=1` → ručně ověřit → zelené.
- Unit není nutný (čistě serializace).

### Rizika
- Minimální — nový výstup, geometrie netknutá. Hlavní pozor: (4) bit-stabilita
  bezbarvého OBJ a (3) chování při stdout.

---

## Stav rozhodnutí
| Téma | Rozhodnutí |
|---|---|
| Větev | `master` |
| Pořadí | kamery → výkresy |
| Kompatibilita stable | varování stačí |
| Definice kamery | builtin `camera()` |
| Objektiv | ohnisko + senzor |
| Obnova snímků | auto po F5, jen když je dok vidět |
| Marker kamery v 3D | ano, drátěný frustum |
| Ukotvení kvót | pojmenované `dimref` + kvóta odkazuje na dva; `// @dim A B …` |
| Zápis kvót bez klikání | `kvota=` cukr (auto-pár) + výběr dimref v 2D náhledu |
| Rozsah Fáze 2 | A + B + cukr; jen délková kvóta; C (klikání kotev) a průměr = později |
| Projekce kvóty | pole kvóty (`proj=…`), typicky jen jedna projekce |
| Barevný export | 3MF (hotová cesta, Blender ≥ 4.1 nativně) + dopsat OBJ `.mtl` |

---

## Fáze 5 — Drawings jako vektorový WYSIWYG editor (redesign) + bugfixy

**Rozhodnutí (2026-07-04):** Barvy jsou ve výkresech k ničemu (navíc rastru
chybí hrany, nevypadá dobře). **Barevný raster zůstává jen pro kamery** (ten je
prakticky hotový). **Drawings dělají výhradně čárový (vektorový) model** a dok
se stává **editorem exportu** (WYSIWYG): co vidím, to se vyexportuje.

### Model chování (dle uživatele)
- **Scale je nastaven a je vidět** — geometrie v model-mm, ale text a tloušťky
  čar jsou **absolutní mm**, takže jejich velikost *vůči geometrii* odpovídá
  měřítku (1:2 → text relativně 2× větší). Zvětšení/zmenšení scale mění jen
  relativní velikost textu/čar a fyzický rozměr exportu.
- **Offsety kvót/labelů v mm** platí pro tento „výkres v měřítku".
- **Zoom/pan** je jen navigace nad tím (nemění export). Texty se se zoomem
  vizuálně zvětšují/zmenšují (kreslí se v mm-prostoru).
- Dok **prakticky slouží jako editor exportu** — polohování kvót a labelů.
  (Uživatel si zatím není jistý, co je „správně" → potřebuje to vidět.)

### Důsledek: body 1 a 2 splynou
- **Pixelace (bod 1) zmizí sama** — vektor se neškáluje z rastru 600×600, žádný
  tichý hi-res re-render není potřeba.
- **WYSIWYG (bod 2)** — dok kreslí přesně to, co export; kvóty + scale jsou
  součástí jednoho vektorového listu.

### Architektura
1. **Sdílený projektor** (`export_line_drawing.cc`): vytáhnout per-view 2D
   projekci (FeatureEdges + DepthBuffer HLR + projekce do os pohledu) do
   znovupoužitelného API, které konzumuje **dok i souborový export** (jeden
   zdroj pravdy → WYSIWYG). Kvóty (svět 3D z `resolveDrawing`) promítat do téže
   2D roviny jako hrany.
2. **`OrthoViewWidget`** (evoluce, ne přepis): místo `drawImage` kreslit
   **úsečky hran** v model-mm; overlay kvót/kotev už v tomto prostoru pracuje.
   Pera/fonty/šipky zadané v mm (÷ scale) a mapované transformací → škálují se
   zoomem. Odpadá `QImage`/`pxPerMm` jako rastr; zůstává fit + zoom + pan.
3. **`MainWindow::refreshDrawingViews`**: místo `render_ortho_view` (raster)
   vyhodnotit **mesh** (znovupoužít F6 geometrii, jinak `GeometryEvaluator`),
   spočítat 2D hrany per view + promítnout kvóty, předat vektorová data do doku.
   Pozor na cenu mesh evaluace (F6) — dělat jen když je dok vidět.
4. **Export** (SVG/PDF/DXF): ze stejného 2D listu → garantované WYSIWYG;
   respektovat **scale** (geometrie × scale, text/čáry absolutní mm).
5. **Úklid UI**: z drawings pryč přepínač barevný/čárový (vždy čárový);
   barevný raster jen v cestě kamer.

### Bugfixy v této fázi (samostatné, bezpečné)
- **Bod 5 — auto-refresh po nové kvótě**: po vložení `// @dim` hned
  `refreshDrawingViews()` (kvóta je komentář, geometrie se nemění). **Hotovo.**
- **Bod 3 — chybějící hrany siluety**: `FeatureEdges::select` dostal
  grazing-toleranci (facety skoro tečné k pohledu / soused rovnoběžný s projekcí
  drží konturu bez děr). **Hotovo** (ověřeno simulací, netříští edge-count).
- **Bod 4 — pravý klik → skok do zdroje**: reprodukovat v běžící appce a
  opravit (drátování vypadá správně, příčina se najde za běhu).

### Refactor (hotový, neutrální)
- `resolveDrawing()` — řešení kotev/kvót do světa vytaženo mimo render loop
  (sdílí dok overlay i vektorový export).

---

## Fáze 6 — Standardní pohledy, normovaný layout (ISO-E) a řezy

Rozšíření výkresů z pevných 3 pohledů na plnohodnotný technický list: až 6
zapínatelných standardních pohledů rozmístěných podle promítací metody, plus
**řezy (sections)** definované markerem v kódu.

### Rozhodnutí (2026-07-05)
- **Směr projekce se NErozvolňuje** — pro jiný úhel si uživatel model otočí.
  Ortografie (oko v nekonečnu) → „oko uvnitř tělesa" nenastává.
- **Promítání ISO-E** (1. kvadrant / evropská metoda) jako default; přepínač
  E/A v doku.
- **Výkres zůstává čárový** (ČSN EN ISO 128: barva nemá normovaný význam).
  Obarvení/šedá jen jako nenormativní pomůcka na obrazovce, ne v defaultním
  exportu → problém „výplně" tímto uzavřen (nepřidávat do exportu).
- **Řezná rovina markeru:** díváš se ve směru lokální **+Z** (šipky = +Z),
  zachová se půlka „za" rovinou; parametr `flip` překlopí.
- **Export:** default **jeden složený list**; checkbox „samostatné soubory"
  vysype každý pohled/řez do vlastního souboru. Stejná primitiva.

### A. 6 zapínatelných pohledů + normovaný layout
- `LineDrawingView`: rozšířit standardní sadu na 6 (Front, Back, Left, Right,
  Top, Bottom) — pořád přes indexy os + `depthSign` (žádný obecný směr).
  Back = −Y, Left/Right = ∓X, Bottom = −Z.
- **Výběr pohledů:** checkboxy v doku (default dle ISO-E: Front + Top + Right).
- **Layout engine** (sdílený dok + soubor. export): kanonické pozice v mřížce
  3×3 kolem Frontu podle ISO-E se **zarovnanými osami** (Top přesně pod Front ve
  stejném X; boky ve stejné výšce jako Front). Řez = dlaždice mimo mřížku.
  Tímto se sjednotí dnešní dva rozdílné layouty (dok stack vs. knihovní 2×2).
- **Export:** jeden list vs. samostatné soubory (checkbox); obojí ze stejného
  seznamu dlaždic.

### B. Marker řezu v kódu (`section()`)
- Nový **`SectionNode`** (šablona: `DimrefNode`/`CameraNode`): `VISITABLE`,
  prázdná geometrie (v `NodeVisitor.h` jako empty group → neviditelný v
  preview/render/exportu/měření). Builtin `section(name="A", flip=false,
  hatch=…)` v `Builtins.cc`; `collectSections(root)` prochází strom s akumulací
  `TransformNode::matrix`.
- **Rovina z markeru:** počátek = world translace; normála = world rotace ×
  lokální +Z. Umístění/orientace čistě transformacemi (`translate`/`rotate`),
  jako u kamer/dimref → přežije editace, skriptovatelné.

### C. Kreslení řezu
- V `refreshDrawingViews`/exportu pro každý `section`:
  1. `PolySet → ManifoldUtils::createManifoldFromPolySet → Manifold::TrimByPlane(
     normal, offset) → PolySet` (uřízne a **zavíčkuje** řeznou plochu; ověřit
     znaménko strany, aplikovat `flip`).
  2. **Táž pipeline** `FeatureEdges` + HLR v rámu řezu (right/up z markeru,
     dir = +Z) → hrany řezového pohledu.
  3. **Řezné víčko:** plochy s normálou ≈ normála roviny a ležící v rovině →
     **vyšrafovat 45°** (tenká plná, rozteč default ~3 mm drawing-mm, clip na
     polygon víčka). (Obrys víčka lze i z `projection(cut=true)` —
     `manifold->slice()` / `CGALUtils::project`, `GeometryEvaluator.cc:850`.)
  4. Dlaždice **„Řez A–A"** na listu.
- **Annotace v ostatních pohledech:** kde je rovina *hranou* (obsahuje směr
  daného pohledu), nakreslit tlustou čerchovanou čáru přes bbox + **šipky**
  (směr +Z) + **písmena „A"** na koncích.

### D. Polish / testy
- Parametry: `flip`, rozteč/úhel šraf; **cache řezu** podle (mesh hash, rovina),
  počítat jen když je dok vidět (boolean navíc je drahý).
- **Unit testy** (`src/gui/LineDrawing_test.cc` styl): krychle půl na půl →
  čtvercové vyšrafované víčko + uzavřený obrys; 6 směrů promítají správně;
  ISO-E pozice dlaždic; `TrimByPlane` sanity (objem/normála víčka).

### Pořadí (inkrementálně)
A (pohledy + layout + export) → B (marker) → C (řez + annotace) → D (polish).
A je samostatně užitečné a nízkorizikové; B–C jsou jádro řezů.

### Rizika
- Znaménko/orientace u `TrimByPlane` — ověřit empiricky.
- Rozpoznání koplanárního víčka (tolerance polohy/normály).
- Cena booleanu na každý refresh → cache.
- DXF: šrafy jako `LINE` segmenty (ok), UTF-8 text „⌀/°" (staré čtečky).

### Stav (2026-07-05): HOTOVO A–D
- **A** — 6 standardních pohledů se znaménkem os (`standardViews()`), dok v ISO-E
  mřížce se zaškrtávacími pohledy, export ISO-E + „samostatné soubory". ✅
- **B** — `section()` marker (`SectionNode`, empty group), `collectSections()`. ✅
- **C** — `projectSection()` (TrimByPlane cut + projekce + 45° šrafy), dlaždice
  „Řez A–A" v doku i exportu, čerchovaná čára řezu se šipkami + písmeny v ostatních
  pohledech (kde je rovina hranou). Ověřeno v GUI. ✅
- **D** — unit testy (6 směrů + zrcadlení, section cut/hatch, kept-half sign) +
  cmdline regrese `dump/echo-section-node`. ✅
- **Zbývá jako optimalizace:** cache řezu (boolean běží při každém refreshi doku).

## Fáze 7 — Jeden výkres, jeden zoom (unifikace canvasu doku)  🔶 naplánováno

**Motivace.** Dok má být WYSIWYG exportu (viz Fáze 5). Dnes ho ale tvoří N
nezávislých `OrthoViewWidget`, každý s vlastním fit/zoom/pan — a právě ten
per-dlaždicový zoom WYSIWYG rozbíjí (dok vypadá jinak než export). Přitom
**složený list už existuje** — jen se používá pouze pro export.

**Klíčové zjištění.** Export (`DrawingViewDock.cc:999`, `1198–1245`) už skládá
všechny pohledy na jeden list: `Placement{v, ox, oy}` v drawing-mm, jedno sdílené
`scale`, first-angle rozvržení (sloupce/řádky/okraje/mezery). Interaktivní dok je
jediné místo, kde se místo toho používá `QGridLayout` z per-view widgetů.

**Export jednotlivých pohledů zůstává.** Režim „Separate files"
(`separateFilesCheck`, ř. 1213–1223) běží přes tentýž `renderToFile(places,…)` a
je nezávislý na tom, jak dok zobrazuje → Fáze 7 se ho **nedotýká**.

### A. Sdílený layout engine
- Vytáhnout výpočet ISO-E rozvržení (dnes v `exportDrawing`, ř. 1224–1245) do
  sdílené funkce `sheetLayout(tiles, scale) → {places, sheetWmm, sheetHmm}`.
- Používají ji: (1) složený export (beze změny chování), (2) nový canvas.
- Větev „separate files" beze změny.

### B. `DrawingSheetWidget` — jeden canvas
- Nahradí `QGridLayout` z `OrthoViewWidget` jedním widgetem, který drží: seznam
  `Placement` (`OrthoViewData` + `ox,oy`), jedno `scale`, jedno `fitScale/fitOffset`
  (fit celého listu), jedno `userZoom/userPan`.
- `paint`: pro každý `Placement` translace na jeho pozici + vykreslení
  edges/hatch/dims/angles/sectionLines přes společné `scale` — **recyklovat dnešní
  per-view kreslení**, parametrizované offsetem (přesně jak už dělá `map()` v exportu).
- Kolečko-zoom + pan tažením prázdna → celý list.

### C. Interakce přes hit-test dispatch
- Na mouse event: najít `Placement` pod kurzorem (bbox test), mapovat
  widget → view-local mm daného pohledu (offset + scale + zoom/pan), pak spustit
  **tytéž handlery** co dnes `OrthoViewWidget`: tažení kvóty/labelu, rotace ⌀/R,
  klik na kotvu, pravý klik → skok, dvojklik → editor.
- Signály (`dimensionOffsetChanged`, …) nesou `viewIndex` jako dnes — určí se
  z toho, který `Placement` byl zasažen.
- Per-view checkboxy → relayout listu (přepočítat `sheetLayout`); prázdné/nezaškrtnuté
  nezabírají místo (plyne z layoutu).

### D. Polish / testy
- `showAnchors`, scale spinbox beze změny (globální nad celým listem).
- Volitelně gesto „zaměřit pohled" (dvojklik do prázdna pohledu → přizoomovat na
  jeho bbox) — nice-to-have, ne nutnost; jinak zoom do detailu = přiblížit+posunout.
- Test: layout pozice canvasu == export (regrese sdílené funkce); hit-test mapuje
  klik ve složeném listu na správný pohled + bod.

### Pořadí
A (sdílený layout) → B (canvas paint) → C (interakce) → D (polish).

### Rizika
- Interakce je hlavní objem — hit-test + přemapování transformace na per-view
  offset. Dobře ohraničené (handlery se recyklují 1:1).
- Souřadnice canvasu musí sedět 1:1 s exportem → garantuje sdílená `sheetLayout`.
- Zoom/pan stav přežít refresh geometrie (uchovat při `setViews`).

## Fáze 8 — Detail (výřez ve větším měřítku)  🔶 naplánováno (staví na Fázi 7)

Norma ČSN EN ISO 128-34 / ISO 5455: oblast v mateřském pohledu orámovat **tenkou
kružnicí** + **velké písmeno**; zvětšenina zvlášť, nadepsaná „**Detail A (5:1)**".

**Rozhodnutí (2026-07-07):**
- **Ne** 3D marker `detail()` — detail je z definice vázaný na konkrétní pohled;
  „view-nezávislost" 3D koule je jen zdánlivá. Detail patří do rodiny `@dim`.
- **Anotace `// @detail`** (jako `@dim`), vzniká **nakreslením kružnice nad
  dlaždicí** (naklikat → materializovat). Návrh:
  `// @detail A parent=front cx=12.4 cy=8.1 r=6 scale=5`
- `parent` = **standardní pohled nebo řez** (`front/back/left/right/top/bottom`
  nebo `section<jméno>`). **Detail z detailu nepodporujeme.**
- Střed = **`cx,cy` ve view-local mm** (holé souřadnice; **drift** při posunu
  modelu je akceptované riziko uživatele — případný upgrade na world-kotvu později).
- **Měřítko volitelné** (nezávislé na měřítku výkresu), v titulku „(5:1)".
- **Sirotek = měkký:** když `parent` (řez) zmizí, detail se **přeskočí + varování**
  — stejný vzor jako nevyřešená `@dim` (`MainWindow.cc:4489,4532` dělají `continue`).
  Detail nad standardním pohledem nikdy neosiří (těch 6 pohledů je fixních).
- Nová dlaždice = hrany (a šrafy) parenta **oříznuté na kružnici** (úsečka × kruh
  nad `OrthoViewData.edges`), vykreslené s `vd.scale = scale`; přistane na
  sjednoceném listu z Fáze 7.

### Stav (2026-07-07): HOTOVO Fáze 7 i 8
- **Fáze 7** — `DrawingSheetWidget` nahradil grid `OrthoViewWidget`ů: jeden list,
  jeden fit+zoom/pan, sdílený `computeSheetLayout()` (dok i složený export;
  „separate files" beze změny). Interakce přes `placementAt()` hit-test, signály
  nesou `viewIndex`. Commit `a2fa889ad`. Ověřeno headless (Xvfb :99 + screenshot):
  všechny vybrané pohledy na jednom listu v ISO-E, tituly, řezová čára + „Section
  A–A", kvóty.
- **Fáze 8** — `// @detail parent= cx= cy= r= scale=`: tenká kružnice + písmeno na
  rodiči (`OrthoDetailMark`), zvětšená dlaždice „Detail A (n:1)" (ořez úsečka×kruh),
  per-tile `drawScale` na canvasu i v exportu, tlačítko „New detail" (rubber-band →
  `detailRequested` → `onDetailRequested` zapíše řádek). Parent = std. pohled nebo
  řez; sirotek měkce přeskočen (warning). Commit `61ed51edb`. Ověřeno headless:
  nakreslení kroužku zapsalo `@detail`, značka „A" na Frontu + dlaždice „Detail A
  (2:1)".
- **Zbývá jako drobnost:** drift `cx,cy` při posunu modelu (akceptováno); volitelně
  world-kotva středu; PNG „Save Sheet" grabuje canvas v aktuálním zoomu.

## Fáze 9 — rozvržení listu: mezery + ruční umístění dlaždic  ✅ HOTOVO (2026-07-07)
- **Mezery/oddělení:** `gapMm` 12→16, extra `blockGapMm` před blokem řezů/detailů
  (řádky ≥3) v `computeSheetLayout`; dok i export. Commit `3f8c7c763`.
- **Ruční umístění řezu/detailu do volného místa** (rozhodnuto A = tažení + zápis
  do zdroje): `OrthoViewData.movable/placeKey/hasAt/atMm`; dlaždice s `at=` plave na
  dané sheet-mm pozici (mimo mřížku, list se zvětší), ostatní zůstávají v ISO-E.
  Canvas táhne pohyblivou dlaždici (`viewIndex==-1`) → `tilePlaced(placeKey,x,y)` →
  MainWindow zapíše `at=x,y` do `// @detail` řádku, resp. do/nový `// @section <name>
  at=x,y`. Commit `f7211e8b7`. Ověřeno headless: přetažení „Section A–A" do prázdného
  sloupce zapsalo `// @section A at=92.686,87.173` a dlaždici tam přesunulo.

## Fáze 10 — labely (3D text callouty) + barevná výplň výkresů  ✅ HOTOVO (2026-07-11)

Dvě funkce (rozhodnutí uživatele 2026-07-11):

### 10A — Labely (3D text s vodicí čárou, ve výkresech i kamerách)
- **Marker `label("A", "text")`** — nový neviditelný uzel (`src/core/LabelNode.{h,cc}`,
  vzor `DimrefNode`): jméno + text, prochází transformacemi, prázdná grupa v
  `NodeVisitor`, `collectLabels()`. Registrace `Builtins.cc` + CMake. `text` default =
  jméno. Regrese `dump/echo-label-node`.
- **Komentář `// @label A [proj=front|…|cam:<jméno>] [laboff=dx,dy] [text="…"] [leader=on|off]`**
  — parsuje `resolveLabels()` (v `MainWindow.cc`, vzor `resolveDrawing`/`@dim`). `proj`
  určuje, kde se label vypíše (prázdné = všechny ortho pohledy; `cam:<jméno>` = konkrétní
  kamera); `laboff` = posun textu od promítnutého bodu (mm ve výkresu, px v kameře; dx=vpravo,
  dy=nahoru). Mezi textem a bodem se kreslí **leader**. Duplicity jmen `A#i` jako dimref.
- **Výkresy:** `OrthoLabel` v `OrthoViewData`; projekce v per-view loopu (gate `proj==key`),
  kreslení v `DrawingSheetWidget::paintPlacement` (černý text + leader + tečka na kotvě) i ve
  vektorovém exportu (SVG/PDF/DXF). Label markery = zelené klikací tečky (gate „Reference points").
  **Plná parita s kótami:** tažení textu → `laboff=` zpět (`onLabelTextMoved`), pravý klik skok,
  dvojklik editor (`onLabelDoubleClicked`, text/proj/leader/delete), klik na zelenou kotvu →
  `// @label` (`onLabelAnchorClicked`).
- **Kamery:** `CameraViewDock` přepsán z pasivního `QLabel` na `CameraImageView` (letterbox +
  overlay + tažení). `MainWindow::refreshCameras` promítá labely `proj=cam:<jméno>` přes GL
  matice snímku (`FboSnapshotView.modelview/projection`, MVP ručně) na pixely; kreslí text+leader,
  „Save PNG" ukládá i s overlayem. **Plná parita vč. tvorby:** `label()` markery se promítají i
  na snímek jako **zelené klikací tečky** (i bez `@label` řádku); klik → `// @label <jméno>
  proj=cam:<jméno kamery>` (`onCameraLabelAnchorClicked`). Tažení/skok/editor přeposláno na tytéž
  sloty (laboff v px). **Pozor:** kamera se jmenuje `camera(name="main")` (pojmenovaně), ne
  pozičně — jinak je jméno prázdné a `proj=cam:main` nematchne.

### 10B — Barevná výplň výkresů (živě v doku + SVG/PDF, hrany černé)
- `projectViewFaces()` v `export_line_drawing.{h,cc}`: per-face barvy z `PolySet.color_indices`
  /`colors`, řazeno painter's algoritmem (nejdál první), nebarevné plochy přeskočeny (zůstanou bílé).
- Checkbox **„Fill colors"** v doku → `OrthoViewData.fills`, kreslení **pod** hrany (dok i
  SVG/PDF export). DXF zůstává čárový. Unit test `[fill]` (řazení + přeskočení nebarevných).

Ověřeno headless (Xvfb :99, screenshoty): labely v Front/Top s leadery + per-projekce gating,
barevná výplň červená/zelená s černými hranami, label „top face" na snímku kamery `main`.
Unit (190 assertions) + 56 deterministických regresí (vč. `label-node`) zelené.

**Zbývá jako drobnost / možné rozšíření:** bug detekce viditelných hran (siluety) — samostatně,
uživatel ho hledá.

## Fáze 11 — použitelnost doku Drawing: uvolnit kóty + výkon nad velkými meshi  ✅ 11A–C HOTOVO (2026-07-20)

Motivace (uživatel, 2026-07-20): nad dvěma naimportovanými STL plošného spoje je dok
Drawing „hodně zamrzlý", a popisky kót se odmítají posunout mimo viditelnou plochu doku.

### 11A — zrušit clamp popisek na viewport  ✅
**Bug.** `DrawingSheetWidget::paintPlacement` napevno vtlačí box popisky do obdélníku
widgetu: `DrawingViewDock.cc:373-375` (lambda `drawLabel`, lineární i radiální kóty) a
`DrawingViewDock.cc:508-510` (úhlové kóty):
```cpp
const double bx = std::clamp(box.x(), 1.0, width()  - box.width()  - 1.0);
const double by = std::clamp(box.y(), 1.0, height() - box.height() - 1.0);
```
Pozůstatek z éry per-view `OrthoViewWidget`ů (každý pohled vlastní widget, bez zoom/pan),
kde clamp držel popisku viditelnou. Na jednom canvasu Fáze 7 to znamená, že se popisky při
panu/zoomu **lepí na okraj doku** místo aby odjely s výkresem. Vedlejší efekt: tažení popisky
k okraji se vizuálně zastaví, ale do zdroje se zapíše nezclampované `labAlongPx/labPerpPx`
→ po refreshi „poskočí". Vektorový export clamp nemá → dok je dnes ne-WYSIWYG.

**Oprava:** clamp na obou místech odstranit. Malý nezávislý zásah, dělá se první.

### 11B — cache projekce (největší výkonová výhra)  ✅
**Diagnóza.** Každá editace kóty/labelu/dlaždice zapíše zdroj a zavolá
`refreshDrawingViews()` (`MainWindow.cc:4003, 4033, 4074, 4191, 4339, 4359, 4406`), která
v `MainWindow.cc:5249` volá `projectViewEdges()` **znovu pro každou dlaždici**. Ta
(`export_line_drawing.cc:311`) je nejdražší článek celého řetězce:
`FeatureEdges::select` přes všechny facy + postavení `DepthBuffer` (rasterizace celého
meshe) + HLR vzorkování **až 400 vzorků na hranu**. Pro 6 pohledů + řezy + detaily nad
PCB meshem to jsou vteřiny — a spustí se to při **puštění myši po každém posunu kóty**.

Komentář na `MainWindow.cc:5136-5139` tvrdí, že opakované refreshe jsou levné, protože
`GeometryEvaluator` hitne cache. To platí pro **mesh**, ale **ne pro projekci** — ta dnes
žádnou cache nemá.

**Návrh:** cache `projectViewEdges`/`projectViewFaces`/`projectSection` klíčovaná na
(identita geometrie, směr pohledu, `scale`, fill flag, crease angle) → `edges/hatch/fills`.
Změna `// @dim` / `// @label` / posun dlaždice geometrii nemění → cache hit → editace kót
je **okamžitá nezávisle na velikosti meshe**. Odhad: ~80 % vnímaného zamrznutí.

### 11C — levné výhry v paintu  ✅
Nezávislé na 11B, lokální změny v `paintPlacement`:
- `painter.drawLines(QVector<QLineF>)` v jedné dávce místo N volání `drawLine` na hranu.
- Mapování nechat na `painter.setTransform()` místo dvou `QPointF` transformací na hranu.
- Cull hran mimo viditelný obdélník (při zoomu je většina hran mimo plátno).
- Vypnout antialiasing po dobu tažení / panu.

### 11D — vrstvení + pixmap cache  ⬜ rozhodne se později
Návrh uživatele: statická vrstva (fills/hatch/edges/section lines) do `QPixmap`,
invalidovaná jen při změně geometrie / zoomu / velikosti widgetu; kóty a labely se kreslí
živě nad ni. Přidání/editace kóty pak překresluje jen tenkou vrstvu.

Upřesnění k návrhu:
- **Pan nepotřebuje re-render vůbec** — nemění měřítko, je to čistý blit s offsetem.
- **Zoom** — okamžitě škálovaný (rozmazaný) blit + přerender, jak uživatel navrhl. Ale
  přerender **ne ve vlákně**: `QPainter` nad `QPixmap` není thread-safe a `places` by
  chtěla zámek. Stačí odložit přerender `QTimer`em ~150 ms po ustání kolečka — vizuálně
  týž efekt, o řád méně rizika.

**Rozhodnutí odloženo:** 11D sáhne i do `paintPlacement` a hit-testu, takže je to
podstatně větší zásah než 11A–C. Nejdřív udělat 11A–C a **změřit**; je dost možné, že
vrstvení pak nebude potřeba. Rozhodnout až podle naměřeného stavu.

### Pořadí
11A (clamp) → 11B (cache projekce) → 11C (paint) → změřit → rozhodnout o 11D.

### Stav implementace (2026-07-20)
- **11A** — clamp odstraněn na **třech** místech, ne dvou: `drawLabel` (lineární +
  radiální kóty), úhlové kóty, a **textové labely `// @label`** (ten se v původní
  analýze přehlédl). Ověřeno headless: po panu listu kóty odjedou s pohledem místo
  aby se zastavily na okraji doku.
- **11B** — `MainWindow::DrawingProjCache`: `projectViewEdges` / `projectViewFaces`
  per index pohledu, `projectSection` per (rovina, flip, hatch, fills). Klíč = objekt
  geometrie, který si cache **drží přes `shared_ptr`**, takže se novému meshi nemůže
  vrátit stará položka recyklovanou adresou. Řezy v `std::deque` (ne `vector`) —
  `cachedSection` vydává reference dovnitř kontejneru a pozdější vklad je nesmí
  invalidovat.
- **11C** — hrany/šrafy/výplně jdou přes `QTransform` dlaždice + dávkové `drawLines`
  (žádné mapování po úsečkách, u plně viditelné dlaždice **nulová kopie**); cosmetic
  pen drží tloušťky v device px; culling na viditelný obdélník jen když je dlaždice
  částečně mimo; antialiasing vypnutý po dobu tažení (a `update()` po puštění, aby
  se poslední snímek dokreslil vyhlazeně).

## Fáze 12 — kóty a labely v řezech (+ chybějící data v detailech)  ✅ HOTOVO (2026-07-20)

Dnes **řezová dlaždice kóty nepodporuje**. Kóty se počítají jen ve smyčce přes 6
standardních pohledů, gated na `proj == key`; `normalizeProj` (`MainWindow.cc:3835`,
použité při parsování `// @dim … proj=` na `4745`) rozeznává jen
`front/back/left/right/top/bottom`. Řezy se staví v samostatné smyčce
(`MainWindow.cc:5412-5446`), která plní jen `fills/edges/hatch/title` — `vd.dims`,
`vd.anchors` ani `vd.labels` se tam nikdy nenaplní. Napsat `proj=sectionA` proto dnes
nic nechytí.

### Rozhodnutí (2026-07-20)

**Řezy zůstávají obecné (libovolná rovina), NEomezujeme je na směry os.** Zvažováno a
zamítnuto, protože:
- Obecnost není volba, kterou lze vzít zpět: `section()` nemá parametr orientace
  (`SectionNode.cc:49` bere jen `name`, `flip`, `hatch`) — rovina vzniká **výhradně
  z akumulované transformace rodičů** (`rotate([0,45,0]) … section("A")`), stejně jako
  u `camera()` a `dimref()`. „Omezit na osy" by znamenalo aktivně detekovat neosovou
  world matici a hlásit chybu / snapovat = práce navíc, ne ušetřená, plus breaking change.
- Nic by to nezjednodušilo: `projectSection` už si lokální rám počítá
  (`export_line_drawing.cc:461`, `const Transform3d L = worldFrame.inverse()`), takže
  projekce kotvy je `L * anchorPos` → vezmi `(x, y)`. Osově zarovnaná varianta by ušetřila
  jeden řádek.
- Šikmý řez sestavou je jeden z hlavních důvodů, proč řezy mít.

**Kotvy mimo rovinu řezu = kolmý průmět do roviny.** Žádné nové pravidlo — je to přesně
to, co dělají ortho pohledy dnes: `MainWindow.cc:4476-4477` vybere `hAxis`/`vAxis` a
třetí souřadnici zahodí, což je kolmý průmět do roviny pohledu. U řezu je směr pohledu
totožný s normálou roviny, takže „zahodit z v lokálním rámu `L`" je tatáž operace.

**Otevřené k rozhodnutí při implementaci:** kotva ležící v **odříznuté** polovině se
kolmým průmětem taky promítne do dlaždice → kóta na prvek, který v řezu není vidět.
V ortho pohledu tenhle případ nenastává (model je celý). Varianty: (a) nechat být —
konzistentní s tím, že kotvy se promítají i na zakryté prvky; (b) varovat; (c) skrýt.
Přiklonit se pravděpodobně k (a).

### Práce
1. **`normalizeProj` rozšířit o `section<jméno>`** — precedens existuje, `// @detail
   parent=` to už umí (lookup přes `sectionKey`, `MainWindow.cc:5488-5491`).
2. **Projekce kotev/kót/labelů do rámu řezu** v section smyčce (`MainWindow.cc:5412`):
   `L * anchorPos` → `(x, y)` → `toLocal()` (tentýž posun `minSx/maxSy`, co už používají
   hrany na ř. 5424), plus `flip` handling. Naplnit `vd.dims`, `vd.anchors`, `vd.labels`.
3. **Interakce zdarma:** dlaždice řezu už jde přes týž `placementAt()` hit-test a signály
   nesou `viewIndex` — tažení/skok/editor by měly fungovat bez dalšího zásahu, jakmile
   jsou data v `vd`.

### Stav implementace (2026-07-20)
Ověřeno headless (Xvfb :99): řez „Section A–A" ukazuje kótu `// @dim L R
proj=sectionA measure=h` (=60) i label `proj=sectionA`, nescopovaná kóta zůstala jen
ve standardních pohledech; tažení kóty v řezu zapsalo `offset=21.56` zpět do zdroje;
dvojklik otevřel editor s **Projection = sectiona**; barevná výplň v řezu funguje.

Nad rámec původního plánu bylo nutné dodělat:
- **`fillAnnotations(vd, key, takeUnscoped, proj)`** — projekce kotev/labelů/kót
  vytažena ze smyčky standardních pohledů do sdílené lambdy. `takeUnscoped` rozlišuje,
  co znamená chybějící `proj=`: pro 6 standardních pohledů „všechny" (dokumentované
  chování), pro řezy **ne** — jinak by každá nescopovaná kóta spadla i do každého řezu.
- **`OrthoViewData.annotated` + `projKey`** — hit-test (`hitDim`/`hitLabel`/kotvy) byl
  gated na `viewIndex >= 0`, takže by kóty v řezu šly vykreslit, ale ne chytit. Nově
  gate na `annotated`, což drží dlaždice **detailu** dál jako jen zobrazovací.
- **Signály `anchorClicked` / `labelAnchorClicked` / `detailRequested` nesou `projKey`
  místo `viewIndex`.** Bez toho by klik na kotvu v řezu vyrobil kótu s `proj=front`
  (viewIndex −1 → `clamp(…,0,5)` → 0), což je horší než nic. Vedlejší zisk: detail lze
  teď nakreslit i nad řezem (`parent=section<jméno>`).
- **Dropdown projekcí v editoru kót** doplněn o řezy modelu + fallback „(not on the
  sheet)" pro `proj=`, který na listu není.

### Bugfix (2026-07-24): `measure=h/v/planar` v řezu počítalo špatnou hodnotu

Nahlásil uživatel: v řezu se kóta **nakreslí správně**, ale **hodnota je špatně**;
jinde je to v pořádku.

Příčina: `measureAxes()` (`MainWindow.cc`) mapovala `proj` na **indexy world os**
a znala jen šest standardních pohledů. Řez má `proj = "section<jméno>"`, což
neodpovídá žádné větvi a **tiše propadlo na default = nárys (XZ)** — takže se
`h`/`v` v řezu měřily podél world X/Z bez ohledu na natočení roviny řezu. Čára
byla správně, protože ta jde jinou cestou (`fillAnnotations` promítá přes
`info.world.inverse()`).

Indexy os to principiálně vyjádřit nemohly: řez je **libovolný rámec v prostoru**
(screen x = jeho lokální X, y = lokální Y). Nahrazeno `measureDirs()`, které
vrací dva světové směry — pro řez sloupce 0 a 1 z `info.world` (normalizované,
kdyby transformace nesla měřítko), jinak jednotkové osy jako dosud. `dimLength()`
počítá `|d·hDir|`; pro standardní pohledy je to bitově totéž co dřív
(`d.dot(Unit(k)) == d[k]`). Rámce řezů se sbírají přes `collectSections()`
a předávají do `resolveDrawing()`.

Postižené bylo i `measure=planar` a **směr radiálních kót** (`angle=`) v řezu —
opraveno spolu s tím. `measure=direct` chybu nemělo (3D vzdálenost na rámci
nezávisí).

Ověřeno headless v GUI: řez `rotate([90,0,90]) section("A")` má h = world Y;
kóta `measure=h` mezi body lišícími se jen v Y o 15 ukazuje **15** (před opravou
by dala 0). 107 regresních testů (dimref/camera/label/section/bend) zelených.

### Vedlejší bug (Fáze 10B), opravit spolu s tím
Dlaždice **detailu** (`MainWindow.cc:5481-5508`) přebírá z rodiče jen `edges` a `hatch` →
detail nemá ani kóty, ani **barevnou výplň**, i když ji rodič má. `fills` se neořezávají
`clipToCircle` (jsou to polygony, ne úsečky — chce Sutherland-Hodgman na kruh, nebo
clip přes `QPainterPath`).

**Opraveno 2026-07-20:** výplně rodiče se ořezávají `QPainterPath::intersected()` s
kružnicí detailu (kružnice se aproximuje polygonem, přesný ořez by znamenal křivé
hrany) a kreslí se ve stejném painter's pořadí jako u rodiče. Kóty se do detailu
**nedědí** — dlaždice detailu zůstává zobrazovací (`annotated=false`); kótovat detail
zvlášť je samostatné rozhodnutí, ne bugfix.

## Fáze 13 — vícevrstvé zobrazení výkresu + nastavitelné rozestupy  ✅ HOTOVO (2026-07-21)

Zpětná vazba z reálného projektu (2 naimportované STL plošňáku, 2026-07-21): Fáze 11
(cache projekce + levný paint) použitelnost **nezachránila**. Dva problémy:

1. **Bug — klik na kotvu v řezu nejde.** Body kotev se zobrazí, ale levý klik do řezové
   dlaždice spustí **posun dlaždice**, ne tvorbu kóty. Příčina: pořadí hit-testů v
   `DrawingSheetWidget::mousePressEvent` — kontrola „táhni pohyblivou dlaždici"
   (`DrawingViewDock.cc:695`) je PŘED „klik na kotvu" (`:704`), a řezy jsou `movable`.
   Ve standardních pohledech se to nestane (nejsou `movable`).
2. **Výkon — tažení kóty i posun řezu je pomalé.** Kóta se nedrží kurzoru, skočí až po
   puštění, pak se vše překreslí. Příčina: každý snímek tažení (`mouseMoveEvent` →
   `update()` → `paintEvent`) překreslí VŠECHNU geometrii všech dlaždic. 11C zlevnilo
   jeden paint, ale u velkého meshe je i jeden kompletní repaint delší než interval myši
   → Qt snímky slučuje a zahazuje. Potvrzuje, že 11B/11C nestačí a je nutné vrstvení.

Rozhodnuto rozdělit do **dvou kroků** (13A rendering, pak 13B layout); oba naplánovány
teď, implementují se po sobě.

### 13A — vrstvy + pixmap cache (per-dlaždice)  ✅ HOTOVO (2026-07-21)

Commit `dc5944b76`. Bug (pořadí hit-testů) opraven a ověřen headless (klik na kotvu
v řezu zapsal `// @dim TOP R proj=sectiona`). Vrstvení: geometrie dlaždice do vlastní
cache `QPixmap` (`DrawingSheetWidget::TileLayer`, klíč `tileId`), `paintPlacement`
rozdělen na `renderTileGeometry` (do pixmapy) + `paintTileOverlay` (živě). Zoom
dvoufázově přes `zoomSettleTimer` (150 ms); scaled blit sedí s overlayem, protože
`ratio*builtScale == k`. Cache přežije `setSheet` editace kóty, dropne se na změnu
meshe přes `drawingGeomVersion` (MainWindow bumpuje spolu s projekční cache). Ověřeno
headless: tažení kóty drží kurzor, pan i zoom zarovnané, bez vizuální regrese.

Rozhodnutí (uživatel, 2026-07-21): **každá dlaždice — pohled, řez i detail — je vlastní
vrstva (pixmapa).** Posun dlaždice po listu pak není zvláštní případ, je to „pan jedné
dlaždice": změní se její origin, list se přeskládá blitováním. Sjednotí to všechny
operace:

| operace | co se děje | cena |
|---|---|---|
| skládání listu | blit každé dlaždice na její pozici | — |
| posun dlaždice (řez/detail) | změň origin jedné, přeblituj | okamžité |
| pan | posuň blity všech | okamžité |
| tažení kóty/labelu | živé kóty nad hotovými pixmapami | okamžité |
| zoom | škálovaný (rozmazaný) blit, přerender po ~150 ms | okamžité + doostření |
| změna geometrie (F5/F6) | přerender pixmap | jednorázově |

Práce:
1. **Rozdělit `paintPlacement`** na `renderTileGeometry` (výplně + šrafa + hrany → do
   pixmapy) a `paintTileOverlay` (titulek, kóty, úhly, řezové čáry, detail-značky,
   labely, kotvy → kreslí se živě nad blit).
2. **Cache pixmap per dlaždice** (klíč = identita dlaždice), s uloženým měřítkem, při
   kterém byla postavená. Invalidace jen při: změně geometrie, změně zoomu, expozici
   nové oblasti panem.
3. **`paintEvent`**: pro každou viditelnou dlaždici zajisti aktuální pixmapu → blit na
   pozici → živý overlay.
4. **Zoom dvoufázově**: `QTimer` ~150 ms; během točení kolečka škálovaný blit, po ustání
   přerender + swap.

Háček (ne rozhodnutí, jen omezení): při velkém zoomu je pixmapa dlaždice mnohem větší
než plátno → rasterizovat jen `bbox ∩ (plátno + okraj)`. Důsledek: tažení dlaždice hodně
daleko při velkém zoomu dorenderuje kraj se zpožděním. Při fit-zoomu (kdy se dlaždice
reálně přerovnávají) se celá vejde → plynulé. Paměť ohraničená.

**Bug s klikem na kotvu v řezu** se opraví zvlášť a jako první (nezávislé): otestovat
kotvu / label-marker PŘED tažením dlaždice. Kotva má malý poloměr zásahu (12 px), klik
mimo tečky spadne dál na tažení dlaždice — správná priorita.

### 13B — nastavitelné rozestupy + `// @sheet` (staví na 13A)  ✅ HOTOVO (2026-07-21)

Standardní pohledy zůstávají v pevném ISO-E rozvržení, mění se jen **rozestup mezi nimi**.
Řezy se dál tahají (`at=` do zdroje) — beze změny. Rozhodnutí uživatele (2026-07-21):

- **Číselné pole „rozestup (mm)"** u zaškrtávátek pohledů. Nahradí dnešní pevný `gapMm`
  (16) uživatelskou hodnotou → jde do `computeSheetLayout`.
- **Tlačítko „auto"**: z rozložení kót spočítá rozestup tak, aby mezi sousedními
  dlaždicemi zůstala cílová mezera (default 10 mm).

Dopřesnění (odsouhlaseno):
1. **Co „auto" měří:** skutečný nakreslený rozsah dlaždice **včetně přesahu kót a
   labelů**, ne jen bbox geometrie — jinak kóty pořád přetečou do souseda. Rozsah =
   geometrie + kótovací čáry + popisky; mezera se počítá mezi těmito rozšířenými rozsahy.
2. **Jedna hodnota, ne H/V zvlášť** (pro začátek): „auto" nastaví tu, co je potřeba na
   těsnějším směru. Případné rozdělení později.

**`// @sheet` nese CELÝ stav listu** (rozhodnutí uživatele 2026-07-21, AskUserQuestion —
vybráno gap + views + scale + fill). Jeden komentář, sheet-level, drží WYSIWYG (dok ==
export) a přežije reload:

```
// @sheet gap=12 scale=2 views=front,top,right fill=on
```

- **`gap=`** rozestup v mm (13B jádro).
- **`views=`** které z 6 standardních pohledů jsou zaškrtnuté (nápad uživatele — dnes jen
  UI stav doku, po uložení cestuje se souborem).
- **`scale=`** měřítko listu (dnes spinbox „Scale", neperzistentní).
- **`fill=`** barevná výplň on/off (dnes checkbox, neperzistentní).

Důsledek: přepnutí zaškrtávátka pohledu / změna scale / fill / gap **zapíše do zdroje**
(jako řezy/kóty), a `refreshDrawingViews` čte `// @sheet` a nastaví ovládací prvky doku
podle něj. Pozor na zpětnou vazbu: při nastavování ovládacích prvků ze zdroje **blokovat
signály**, jinak smyčka. Chybí-li `// @sheet`, platí dnešní defaulty; první změna řádek
vytvoří (vzor `// @section … at=`).

### 13C — neblokující přestavba pixmap (staví na 13A, po 13B)  ✅ HOTOVO (2026-07-21)

Zpětná vazba (uživatel, 2026-07-21): „když běží překreslení, nedá se tahat" — jednorázová
přestavba pixmap (po F5 / doostření zoomu / přepnutí pohledu) běží synchronně na UI vlákně
a na velkém meshi UI na chvíli zamrzne. Ovládání je jinak „výrazně lepší". Řešení:
- **Inkrementální render** (jedna dlaždice za cyklus event-loop, `QTimer(0)`), nebo
- **worker vlákno** (`QImage` v threadu → `QPixmap` na UI po dokončení).
Během přestavby ukazovat starou (scaled) pixmapu, jak už dělá zoom. Rozhodnuto řešit
**až po 13B a změřit** — možná stačí to, co je.

### Pořadí
Bug (hit-test) ✅ → 13A (vrstvy/pixmapy) ✅ → měření uživatelem ✅ → 13B (rozestupy +
`// @sheet`) ✅ → 13C (neblokující render) ✅. Celá Fáze 13 hotová 2026-07-21, ověřeno
headless; čeká na měření uživatele na reálném projektu.

---

## Fáze 14 — `bend()`: ohyb geometrie (flex PCB)  ✅ HOTOVO (2026-07-23)

### Motivace

Uživatel má STL flexibilního PCB, které je exportované **ploché**. Do modelu ho
potřebuje dostat ohnuté. Dnes to nejde nijak: OpenSCAD umí jen afinní
transformace (`multmatrix`), a knihovní řešení (BOSL2 `skin`/`path_sweep`)
pracují jen nad geometrií, kterou si samy vygenerují — **importované STL
deformovat neumí**. Vestavěný uzel pod úrovní meshe to umí.

### Zvažovaná alternativa (zamítnuto)

Původní úvaha byla obecný `deform(from=[…], to=[…])` — dva stejně dlouhé
seznamy bodů, geometrie uvnitř konvexního obalu se mapuje na obal výstupní.
Zamítnuto: pro N > 4 bodů afinní mapa neexistuje, takže „lineárně" musí být
buď po částech lineární (Delaunay tetraedralizace + barycentrické souřadnice),
nebo klec se zobecněnými barycentrickými souřadnicemi, nebo RBF/TPS — každá
volba má jiné chování a uživatel by musel trefovat body. Pro reálnou úlohu
(ohnout flex) je analytický ohyb jednodušší, přesnější a lépe ovladatelný.

### Zadání (ROZHODNUTO 2026-07-23)

```scad
bend(at = 10, r = 2, angle = 90, xlim = [undef, 60], ylim = [-6, 6], zlim = [-1, 5])
  children();
```

- **Orientace natvrdo**: deska leží v XY podél +X, ohyb kolem osy **Y**,
  neutrální vlákno **z = 0**, kladný `angle` zakřivuje k **+Z**. Jiná orientace
  se řeší obalujícím `rotate()`. Volitelná osa **zamítnuta** — měnila by
  význam `ylim`/`zlim` a byla by matoucí.
- **`at`** — x dělicí roviny, **`r`** — poloměr neutrálního vlákna (> 0),
  **`angle`** — úhel ohybu ve stupních (0 = identita).
- **`xlim`/`ylim`/`zlim`** — nepovinné rozsahy `[lo, hi]`, `undef` na kterékoli
  pozici = neomezeno. Vrchol musí projít **všemi** zadanými limity, jinak se ho
  ohyb **vůbec nedotkne** (identita). Limity se vyhodnocují ve **vstupních**
  (tj. aktuálních, už možná ohnutých) souřadnicích.
- **`$fa`/`$fs`/`$fn`** řídí počet segmentů oblouku přes `CurveDiscretizer`
  (stejná sémantika jako u `cylinder`).
  ⚠️ **Pozor na default `$fs = 2`**: u malých poloměrů je to vazbová podmínka
  a ohyb vyjde hrubý — `r=2, angle=90` dá s defaulty jen 2 segmenty a `$fa` na
  tom nic nezmění (min() vybere hrubší z obou, přesně jako u `cylinder`).
  Změřeno na desce 30×10×0.4: defaulty 84 trojúhelníků, `$fs=0.2` i `$fn=32`
  444. Pro reálný flex tedy nastavovat `$fn` nebo `$fs`.

### Mapa (jedna formule, C1 spojitá)

`Θ` = `angle` v radiánech, `R = sign(Θ)·r`, `L = r·|Θ|`, střed `C = (at, 0, R)`.

```
u = x - at
u ≤ 0  → θ = 0,    s = u        (pevná část, identita)
u ≥ L  → θ = Θ,    s = u - L    (tuhá zóna za obloukem)
jinak  → θ = u/R,  s = 0        (oblouk)

bent = C + Ry(-θ) · (s, y, z - R)
```

Ověření: pro `u ≤ 0` vyjde přesně `(x,y,z)`; pro `u = L` se obě větve
shodují. Vlákna nad neutrální rovinou se natahují, pod ní stlačují — to je
fyzikálně správné chování flexu a uživatel to tak chce.

Koncový rámec oblouku je `translate([at + r·sin(angle), 0, r - r·cos(angle)])
rotate([0, -angle, 0])` — jeho inverze slouží uživateli ke srovnání výsledku
zpět do XY před dalším ohybem.

### Řetězení (model uživatele)

Uživatel **nezanořuje bendy přímo do sebe**; mezi ně vkládá `rotate`/`translate`,
aby další ohýbaná část zase ležela v XY:

```scad
bend(…)                      // 3. ohyb
  rotate(…) translate(…)     // srovnat do XY
    bend(…)                  // 2. ohyb
      rotate(…) translate(…)
        bend(…)              // 1. ohyb
          import("pcb.stl");
```

Důsledek: **`bend` je bezstavový**, pracuje jen v aktuálních souřadnicích,
nepamatuje si žádnou rozvinutou délku a nezáleží na pořadí zanoření. Limity
pak řeší reálné situace: `ylim` vybere úzký výběžek, `xlim` shora usekne tuhou
zónu, `zlim` vyloučí to, co předchozí ohyby už vyzvedly do výšky a co náhodou
zasahuje do X-rozsahu aktuálního ohybu.

### Diskretizace

Ploché STL má obdélník ze dvou trojúhelníků — bez dělení by se ohyb neprojevil.
Řezné roviny (Sutherland–Hodgman na trojúhelnících, půlky se vějířovitě
triangulují):

1. **roviny masky** (`xlim`/`ylim`/`zlim`, jen konečné meze) — na všechny
   trojúhelníky; díky tomu je případná trhlina **čistý otvor s rovnou hranou**
   místo natažené blány,
2. **roviny pásma** `x = at + k·L/N`, `k = 0…N` (`N` z `CurveDiscretizer`) —
   jen na trojúhelníky, které mají aspoň jeden vrchol uvnitř masky.

Trojúhelníky, které rovinu neprotínají, projdou beze změny → dělení je
automaticky lokální, mimo pásmo nic nepřibude.

Dělicí bod se počítá z **kanonicky uspořádané** dvojice vrcholů (lexikograficky),
aby dva sousední trojúhelníky sdílející hranu dostaly bit-identický bod a
`PolySetBuilder` (Reindexer) je spojil → mesh zůstane manifold.

Barvy facy (`color_indices`/`colors`) se při dělení dědí.

### Kontroly

- **Trhliny**: hrana, jejíž jeden vrchol projde maskou a druhý ne, **a zároveň**
  se ten průchozí vrchol reálně posune (v oblasti `x < at` je mapa identita, ty
  hrany se nepočítají). Hlásí se **Warning**, ne Error — roztržený model může
  být záměr a tvrdá chyba by byla otravná; ale mlčet nelze, protože nemanifoldní
  mesh položí následné booleany v Manifoldu s nesrozumitelnou chybou jinde.
  ```
  WARNING: bend(): region boundary cuts through material (247 edges, max gap 3.8)
  ```
- **Minimální poloměr ohybu**: `r ≤ max(z)` na vnitřní straně → mapa se převrátí
  a mesh se sám prosekne. Warning, fyzikálně = „minimum bend radius" flexu.
- `r ≤ 0` → Error, prázdná geometrie. `angle == 0` → identita bez dělení.
- 2D děti → Warning „bend() requires 3D geometry", děti projdou beze změny.

### Zapojení do enginu

- `GeometryEvaluator::visit(BendNode)` podle vzoru `visit(RenderNode)`
  (`GeometryEvaluator.cc:637`): `applyToChildren(UNION)` →
  `PolySetUtils::getGeometryAsPolySet()` → `bendPolySet()`.
- `CSGTreeEvaluator::visit(BendNode)` podle vzoru `visit(RenderNode)`
  (`CSGTreeEvaluator.cc:302`): nelineární mapa se do CSG stromu nevejde
  (`CSGLeaf` nese 4×4 matici), takže se podstrom **zmaterializuje jako jeden
  leaf** — `bend()` se v F5 chová jako `render()`.
  **Cena je u cílového případu nulová**: dítě je jediný `import()`, tedy už
  hotový PolySet, žádné booleany se nedělají.

### Kroky implementace

1. `src/core/BendNode.{h,cc}` — uzel, `toString()` (musí vypsat `at/r/angle` i
   limity, jinak by se dva různé ohyby srazily v `GeometryCache`),
   `register_builtin_bend()` podle vzoru `color()`.
2. Zapojení: `NodeVisitor.h`, `Builtins.cc`, `node_clone.cc`, `CMakeLists.txt`.
3. `src/geometry/bend.{h,cc}` — řezání rovinou, mapa, statistika trhlin.
4. `GeometryEvaluator` + `CSGTreeEvaluator`.
5. Testy.

### Testy  ✅ 23 testů, všechny zelené

- `tests/data/scad/misc/bend-node.scad` → `dump-bend-node`/`echo-bend-node`:
  dump nese všechny parametry včetně limitů a `$fn/$fa/$fs` (nutné kvůli
  `GeometryCache`), echo chytá varování pro vadný a prázdný rozsah.
- `tests/data/scad/3D/features/bend-tests.scad` — auto-globnuto do
  `dump`/`render-{cgal,manifold}`/`preview-*`/`throwntogether-*`/`render-csg-cgal`
  (5 případů: 90°, záporný úhel, `ylim` na jednom ze dvou výběžků, řetězení,
  `angle=0`). Zlaté PNG ověřeny okem.
- `tests/data/scad/stl/bend-export.scad` v `EXPORT_STL_TEST_FILES` → `export-stl`,
  `-stdout`, `-manifold`, `export-binstl`, `-stdout`. **Oba backendy dávají
  bajtově identický výstup**, takže sdílený zlatý soubor drží.
- `src/geometry/bend_test.cc` (8 Catch2 testů): identita pro `angle=0`, bbox a
  vodotěsnost po ohybu, lokálnost dělení (deska 30 a 60 dá stejný počet
  trojúhelníků), maska mimo materiál = identita, detekce i nedetekce trhliny,
  příznak minimálního poloměru, záporný úhel.
  Glob v `CMakeLists.txt` bere jen `src/utils/*_test.cc`, proto je soubor
  přidaný **explicitně** přes `list(APPEND TEST_SOURCES …)` — širší glob by
  vtáhl i mrtvý `src/geometry/linear_extrude_test.cc`.

### Co odhalily testy

Unit test „maska mimo materiál" našel reálnou chybu: v pevné části (`u ≤ 0`)
mapa počítala `at + (x - at)`, což se v plovoucí čárce **nevrací bit po bitu**.
Vrchol na hranici masky pak dostal o jeden ulp jinou souřadnici než jeho soused
z nemaskované strany, `Reindexer` je nespojil a mesh se tiše otevřel — a to bez
varování, protože posun ~1e-16 je pod prahem detekce trhlin. Opraveno: pro
`u ≤ 0` se vrací vstupní bod beze změny.

### Regrese

Celá sada `xvfb-run ctest -j6`: 5 selhání, všechna z hrstky známých flaky
image testů pod llvmpipe (viz Fáze 0) — v `-j1` projdou 100 %. Nic navíc.

---

### 14B — klasifikace zbytku podle souvislosti  ✅ HOTOVO (2026-07-23)

**Problém, který 14A neuměla.** Maska říká „co se ohýbá", a všechno ostatní
zamrzne. Jenže typický flex je **úzké žebro mezi dvěma širokými plochami**:
maskou `ylim` vybereš šířku žebra, čímž z masky vypadne i široká plocha na
druhém konci, která je fyzicky připojená k ohýbanému konci a musí odjet s ním.
Ve 14A zůstala stát a deska se roztrhla.

**Zamítnuto: ruční `carry_*` oblasti.** Byly by to tři parametry navíc a
uživatel by musel oblast trefovat. Nápad uživatele (2026-07-23) je lepší.

**Zvolené řešení — souvislost meshe.** Žádný nový parametr:

1. Trojúhelníky **uvnitř masky** dostanou štítek podle `x`: `fixed` (`x < at`),
   `arc`, `carried` (`x > at+L`).
2. Trojúhelníky **mimo masku** se flood-fillem přes **sdílené hrany** (ne přes
   sdílený vrchol) pospojují do skupin.
3. Skupina zdědí osud maskovaného materiálu, kterého se dotýká: připojená před
   dělicí rovinou → stojí, připojená za obloukem → jede tuhým pohybem.

Tuhý pohyb je `BendMap::rigid()` — **záměrně tentýž výraz** jako větev `u ≥ L`
v `operator()`, takže vezený materiál a ohnutý konec se napojí bit po bitu a
šev nevzniká. To je hlavní důvod, proč je celé to schéma použitelné: carry
oblast se nemusí trefovat přesně.

**Tři patologie, každá se hlásí:**

| případ | chování |
|---|---|
| skupina se dotýká **fixed i carried** (most kolem ohybu) | žádný tuhý pohyb nesedí → zamrazit + Warning |
| skupina visí na **oblouku** | utrhne se → Warning |
| skupina se nedotýká **ničeho** (samostatná skořepina — součástka, co na desce jen leží) | posoudit podle **vlastní polohy**: celá za pásmem → jede, jinak stojí |
| nespojená skupina leží **napříč** ohybem | tatáž nejednoznačnost jako most → zamrazit + počítá se do `conflict` |

Plus vždy jednořádkové shrnutí (`message_group::NONE`), ať je vidět, co se
stalo: `bend(): 4 part(s) outside the bend region: 2 fixed, 2 carried along
(0 of them connected to nothing, placed by position)`. `fixed + carried` vždy
dá dohromady `groups`; `conflict`/`on_arc`/`isolated` jsou ortogonální poznámky.

**Robustnost.** Graf souvislosti stojí nad **svařenými** pozicemi
(`Grid3d<int>(GRID_FINE)`, kontroluje i sousední buňky), takže vlásečnicové
trhlinky z konverze STEPu mesh nerozpadnou. Výstupní geometrie se svařováním
nemění, používá se jen ke klasifikaci. Ověřeno, že `import_stl.cc` staví mesh
přes `PolySetBuilder`/`Reindexer`, tj. **importované STL má vrcholy sjednocené**
(round-trip: kvádr = 12 trojúhelníků nad 8 vrcholy, 0 nespárovaných půlhran).

**Pořadí kroků (důležité).** Klasifikace běží **po** řezech maskou, ale **před**
řezy pásma. Kdyby se pásmo řezalo dřív, vznikly by na hranici masky T-junctions
(ohýbaný trojúhelník rozříznut, jeho nemaskovaný soused ne) a hrana by v grafu
zmizela → skupina by se špatně naseedovala. Detekce trhlin běží na stejném
edge-mapu a je nově třístavová.

**Co to nenahrazuje.** `zlim` zůstává. Maska rozhoduje, co se **deformuje**;
souvislost rozhoduje, co se stane se **zbytkem**. Výběžek vyzvednutý předchozím
ohybem leží prostorově uvnitř maskovacího kvádru, takže bez `zlim` by se pořád
deformoval.

**Výkon.** Analýza souvislosti nestojí nic měřitelného: deska se 149 004 facy
(`import` STL) trvá **1,55 s s maskou i bez ní** — čas žere import, tesselace a
export, ne ohyb. Není důvod ji dělat volitelnou.

**Testy:** +3 unit testy (žebro mezi dvěma plochami: 4 skupiny, 2 stojí, 2 jedou,
0 trhlin, vodotěsné; most přes ohyb: 2 konflikty; nespojená skořepina před
pásmem / za pásmem / napříč) — celkem 11 unit testů, 51 assertions. Do
`bend-tests.scad` přibyl šestý případ (žebro) a zlaté PNG jsou přegenerované
a ověřené okem. Pomocník `makePlate()` v testu staví plát na **sdílené mřížce
vrcholů** — kvádry poskládané vedle sebe nejsou hranově spojené a testovaly by
nesmysl.

**Regrese.** Celá sada `-j6`: 1 selhání (`render-dxf_text-metrics`), sériově
projde — opět jen llvmpipe flakyness, a jiný test než minule.

---

### 14C — `glue`: soudržnost přes mezeru (nespojité importované STL)  ✅ HOTOVO (2026-08-18)

**Problém.** 14B rozhoduje o osudu materiálu mimo masku podle **souvislosti
meshe** — flood-fill přes sdílené hrany, svařené `Grid3d(GRID_FINE)`, tj.
tolerance ~1e-6 mm. Reálné importované STL ale často spojité **není**: součástky
jsou samostatné shelly, které na desce jen stojí, a konverze z jiného formátu
umí těleso rozpadnout na kusy, které se dotýkají, ale nesdílí jediný vrchol.
Takový kus dnes vyjde jako `groups_isolated`, rozhodne se podle **vlastní
x-polohy**, a když leží napříč pásmem, zamrzne — zatímco deska pod ním odjede.
Model se rozjede.

**Zamítnuto: zespojitit mesh (svařit / doplnit mostíky / union).** Neřešilo by
to nic navíc — po spojení by flood-fill kusy stejně jen sloučil do jedné skupiny,
což je přesně výsledek, který dá proximita. Zato by to stálo mnohem víc:
u překrývajících se shellů je „zespojitit" booleovský union (nad ~150k facy
s koplanárními plochami z cizího STL řádově minuty a časté selhání na
self-intersections), u shellů s mezerou by šlo o **generování nové geometrie**
(kudy mostík vede? jaký průřez?), která by se do exportu dostala jako materiál,
co tam fyzicky není, a při ohybu by se sama deformovala. Hlavně by to porušilo
vlastnost, na které 14B stojí: **svařování slouží jen ke klasifikaci, výstupní
mesh se nemění o jediný vrchol.** Kdo union opravdu chce, ať ho udělá mimo
`bend()` — je to ortogonální operace.

**Zvolené řešení — proximita jako druhá relace soudržnosti.** Nový nepovinný
parametr:

```scad
bend(at = 10, r = 2, angle = 90, ylim = [-6, 6], glue = 0.1) import("pcb.stl");
```

- `glue` = vůle v jednotkách modelu; **default 0 = přesně dnešní chování**
  (nulová regrese, stávající zlaté soubory drží).
- Metrika **vrchol vs. trojúhelník**: kusy patří k sobě, když některý vrchol
  jednoho leží blíž než `glue` k trojúhelníku druhého. Vrchol-vrchol by
  nestačilo — typický případ je součástka stojící uprostřed velkého trojúhelníku
  desky, kde nejbližší vrchol desky je v rohu, metry daleko.
- Proximita plní **tytéž** `TOUCH_FIXED/ARC/CARRIED` bity a `unite()` skupin jako
  dnešní hranová relace, takže veškerá logika 14B (dědění osudu, konflikty,
  izolované kusy) platí beze změny.
- **Konflikt řeší stejně jako dnes**: kus blízko stojící i cestující strany =
  most přes ohyb, žádný tuhý pohyb nesedí → zamrazit + Warning. Nedělá se
  „bližší strana vyhraje", protože by to tiše zakrylo reálný problém.
- Trhliny se dál počítají **jen na hranách** (`findTears`). Kusy spojené
  proximitou nejsou sdílenou hranou spojené, takže se „neroztrhnou" — jen se
  rozjedou, a tomu právě `glue` předchází.

**Klasifikace poběží vždy**, i bez masky (dnes ji `if (params.hasMask())`
přeskočí). Ověřeno, že je to bezpečné: bez masky vyjde `inRegion[i] = true` pro
všechno, takže nevznikne žádná skupina a klasifikace nemá co změnit — přibude
jen `findTears`, tj. diagnostika navíc, ne jiná geometrie.

**Prostorový index.** Vrcholy do uniform gridu s buňkou `h = max(glue, medián
délky hrany)`; pro každý trojúhelník se projdou buňky protínající jeho o `glue`
nafouklý bbox a spočítá se přesná vzdálenost bod–trojúhelník. Buňka odvozená od
velikosti trojúhelníků, ne od `glue`, je podstatná: s buňkou `glue` by deska
100×100 mm a `glue = 0.01` znamenala 10^8 buněk. Při `glue = 0` se celý krok
přeskočí, takže dnešní modely neplatí nic.

**Pozn. k pořadí.** Proximita běží na stejném místě jako hranový flood-fill —
tedy **po** řezech maskou a **před** řezy pásma (viz 14B, jinak T-junctions).

### Co odhalila implementace

**Kontaktní bod, ne centroid.** Unit test „vertex against face" (deska jako
jediný `cube`, tj. horní stěna ze dvou obřích trojúhelníků) odhalil, že
`touchOf` rozhoduje z **centroidu** region-trojúhelníku — a protože klasifikace
běží *před* řezy pásma, jeden facet může přesahovat celý oblouk. Součástka
stojící uprostřed takového facetu pak zdědila `TOUCH_FIXED` z jedné poloviny a
`TOUCH_CARRIED` z druhé → falešný konflikt a zamrznutí. Proximita proto zónu
bere z **x kontaktního bodu** (`touchAt(x)`), ne z celého trojúhelníku. Hranová
cesta centroid dál používá; u sdílené hrany je nepřesnost menší (soused je vždy
přímo vedle), ale je to tentýž latentní problém — viz „Známé zbytky" níže.

**Prostorový index musí vynechat region×region.** První verze indexovala všechny
vrcholy a testovala každý trojúhelník proti všem — jenže drtivá většina dvojic
je „deska proti desce", tedy oba v regionu, a ty se stejně zahodí. Index se
proto staví dvakrát (všechno / jen mimo region) a trojúhelník **uvnitř** regionu
se dotazuje jen na ten druhý. Měřeno na 83 084 trojúhelnících: `glue = 0`
0,72 s, `glue = 0.2` **1,71 s před** touto úpravou a **1,18 s po ní**. Cena
proximity je tedy ~0,45 s/83 k trojúhelníků a platí se jen se zapnutým `glue`;
`glue = 0` přeskočí celý krok a je bit po bitu jako dřív.

### Testy  ✅

- `src/geometry/bend_test.cc`: +5 testů (16 celkem, 76 assertions) — součástka
  ležící na desce napříč koncem oblouku (bez glue zamrzne, s glue jede a hlásí
  `on_arc`), měření vrchol×plocha na desce ze dvou obřích trojúhelníků, dva
  shelly jednoho dílu s vlásečnicovou mezerou, most přes ohyb (konflikt zůstává),
  a že `glue` nedosáhne dál, než smí (mezera 0,5 vs. `glue = 0.2`).
- `tests/data/scad/misc/bend-node.scad`: `glue` v dumpu jen když je v použití
  + varování pro záporné `glue`.
- `tests/data/scad/3D/features/bend-glue-tests.scad` (**nový soubor**, aby se
  nemusela přegenerovat zlatá PNG stávajícího `bend-tests`) — 4 případy,
  auto-globnuto do `dump`/`render-*`/`preview-*`/`throwntogether-*`/
  `render-csg-cgal`.
- Ověřeno naživo: bez `glue` bbox končí na x = 24 (součástka leží na místě),
  s `glue = 0.2` na x = 17 (odjela s deskou).
- Plná `xvfb-run ctest -j6`: 7 selhání, všechna sériově projdou = známá llvmpipe
  flakyness, žádné se netýká `bend`. Unit testy 292 assertions / 33 testů.

### Známé zbytky (neřešeno v 14C)

- **Hranová cesta pořád rozhoduje z centroidu.** `touchOf` u sdílené hrany může
  u velmi hrubého meshe ukázat na materiál mimo místo dotyku. Oprava by byla
  vzít x sdílené hrany; nechalo se být, protože by změnila zlaté soubory 14B
  a u hranově spojeného meshe je odchylka malá.
- **Trhliny se počítají jen na hranách.** Kusy držené `glue` nejsou hranově
  spojené, takže se do `torn_edges` nezapočítají — rozjet se ale nemohou,
  protože sdílejí zónu.
- `EPS = 1e-9` je pořád absolutní, ne relativní k velikosti modelu.

## Fáze 15 — `view()`: přepínatelné pohledy (stavy modelu) + per-view výkresy/kamery  🔶 naplánováno

### Zadání / motivace
Dnes existuje **právě jedna implicitní scéna** = celý `rootNode`; výkres i
kamery projektují/renderují celý strom (`MainWindow::refreshDrawingViews`,
`refreshCameras`). Autor ale potřebuje z **jednoho zdrojáku** ukazovat a měřit
**různé komponenty a stavy sestavy**: samostatnou součástku s jejími kótami,
pak přepnout na celek s jinými kótami, případně „složeno / rozloženo". Cíl:
zavést **pojmenované pohledy**, které jdou přepínat v GUI, a v **headless**
z nich dávkově generovat PNG (kamery) i technické výkresy.

### Rozhodnutí (probráno 2026-07-27)
- **Dvě kolmé osy, ne jeden seznam.** „Pohled" = *co* se zobrazuje (výběr/stav).
  *Jak* se na to díváš (kamera vs. výkres) zůstává druhá osa → dosavadní dvě
  okna `CameraViewDock` a `DrawingViewDock` **zůstávají oddělená**, jen reagují
  na společný přepínač aktivního pohledu. Ke komponentě lze mít 3D i výkres.
- **Syntaxe = strukturální modul `view("jméno"){ … }`**, ne hromada `if
  ($view==…)`. Čitelnější a — hlavně — anotace uvnitř bloku k němu **strukturálně
  patří** (viz scope níže). Prakticky je to cukr: aktivní pohled zapne, ostatní
  vypne.
- **Sémantika = re-instanciace, NE filtr viditelnosti.** Neaktivní `view()`
  vrací prázdnou grupu (jako dnešní markery kamera/řez, viz CLAUDE.md), přepnutí
  = re-evaluace stromu. **Žádný nový průřezový pojem viditelnosti** v
  CSG/Geometry/exportu/pickingu/měření — neaktivní blok prostě není ve stromu.
  (Zavržena tag-varianta „jedna evaluace + filtr" právě kvůli té invazivnosti.)
- **Zvládá i „jinou polohu/stav", ne jen show/hide.** Komponenta se v každém
  bloku napíše s vlastní transformací; `dimref()` uvnitř se instanciuje s
  **maticí toho bloku**, takže kóty sedí per-pohled samy.

### Sémantický model (6 pravidel)
1. **Geometrie mimo všechny `view()` = společná, vždy viditelná.** Žádný `view()`
   v modelu → přesně **dnešní chování** (vše vidět, jeden implicitní pohled) →
   zpětná kompatibilita zdarma.
2. **Pohledy tvoří strom** (sourozenci = alternativy, děti = zjemnění).
   **Aktivní = jeden uzel.** Render = **společné + všechny `view()` bloky, které
   jsou předek-nebo-on-sám aktivního uzlu**; sourozenci a nezvolené větve ne.
3. **Akumulace dolů** („co je vně, je i uvnitř"): vnější obsah teče do vnitřního
   pohledu. Př.: `view("assembly"){ housing(); view("exploded") …lid; }` — při
   aktivním `exploded` je vidět `housing` **i** rozložené `lid`. Opačný směr by
   ukázal víko ve dvou polohách = nesmysl. **Geometrie i anotace akumulují
   stejně.**
4. **Default = root** (jen společné). **Když je root prázdný** (celý model je
   uvnitř bloků) → auto-posun na **prvního potomka**, jinak root.
5. **Klikaná kóta/label → dovnitř nejhlubšího aktivního bloku** (před jeho
   zavírací `}`). Root/žádný pohled → **na konec souboru jako dnes**. Přesun kóty
   mezi pohledy = přesun jejího `// @dim` řádku.
6. **Přepínač = strom** (root pojmenovat `(vše)`/`(společné)`). **Skrytý úplně,
   pokud v modelu není žádný `view()`** → nulová změna UI pro stávající modely.

### Scope anotací = strukturálně podle obalu (řeší i „třetí otázku")
Anotace (`// @dim`, `// @sheet`, `// @detail`, `// @label`) se k pohledu vážou
tím, že jejich **řádek leží uvnitř rozsahu `view()` bloku** — **žádný `view=`
klíč**. Mechanika: `view("x")` je reálný `ModuleInstantiation` se scopem, z AST
známe `[firstLine, lastLine]` (po `}`); sběrače/`CommentParser` jen zařadí každou
anotaci do pohledu, do jehož rozsahu spadá (vnoření → nejhlubší). `// @sheet`
(dnes **jediný globální**) se tím stává **per-view**. Seznam pohledů + jejich
anotace se dají získat **jen z parsování zdrojáku** (bez evaluace); geometrii
konkrétního pohledu až evaluací s daným aktivním pohledem.

> Pozn.: zápis „dovnitř" potřebuje blokovou formu `{ }`. U `view("x") child();`
> bez závorek není kam vložit → write-back blok **auto-obalí** do `{ }`, nebo si
> řekneme, že pohled nesoucí anotace musí být blokový.

### Kroky implementace
- **15A — builtin `view()` marker.** ✅ HOTOVO. `ViewNode` (průhledný kontejner,
  `src/core/ViewNode.{h,cc}`), registrace v `Builtins.cc`, `$view` default,
  `NodeVisitor` forward na `AbstractNode`. Neaktivní = 0 dětí = prázdná grupa
  (žádný speciál v evaluatorech netřeba). **Testy:** `dump/echo-view-node` zelené.
- **15B — stav „aktivní pohled" + re-instanciace.** ✅ HOTOVO (jádro). `$view`
  = vektor stringů (zbývající cesta); `view("x")` matchuje `$view[0]`, instanciuje
  děti s `$view := tail` (control-style builtin, `Children(inst->scope, childCtx)`).
  Injekce přes `RenderVariables.viewPath`. **Testy:** `dump-view-active` zelený;
  vnoření + akumulace dolů ověřeny. (Auto-posun na 1. potomka při prázdném rootu =
  odloženo, kosmetika.)
- **15C — strom pohledů + přepínač v GUI.** ✅ HOTOVO. `collectViews(SourceFile*)`
  z **AST** (ne z uzlů → vidí i neaktivní/vnořené a přes `include`). Combo na
  editor-toolbaru (`setupViewSwitcher`/`refreshViewSwitcher`/`onViewSwitcherChanged`),
  odsazený strom, skrytý bez `view()`; výběr → `actionRenderPreview()` s aktivní
  cestou → preview+kamery+výkres se přepočtou. Smoke-test headless (Xvfb) bez
  pádu; živé kliknutí ověří uživatel. (Formální Qt-Test = odloženo.)
- **15D — bucketování anotací podle obalu.** ✅ HOTOVO. `src/gui/ViewScope.{h,cc}`
  (pure, testovatelné): `resolveBlocks` (brace-matching, path-aware přes
  `getFullpath`), `ownerPath`, `isPrefix`. `refreshDrawingViews`: per-view
  `// @sheet` (nejhlubší owner-prefix aktivní cesty) + filtr kót/labelů podle
  owner-prefixu (kvota= dims mají sourceLine -1 → owner root → vždy). **Testy:**
  `ViewScope_test.cc` (26 assertions) zelené; regrese beze změny. Rozšířit `collectSections/
  Dimrefs/Cameras` + čtení `// @…` o **zařazení do pohledu podle rozsahu řádků**
  (nejhlubší obal). `// @sheet` se řeší per-view. Výkres kreslí anotace
  aktivního pohledu + zděděné od předků. **Testy:** cmdline echo — anotace v
  bloku patří jen jemu; anotace u předka se dědí do potomka.
- **15E — write-back dovnitř bloku.** ✅ HOTOVO. `MainWindow::insertAnnotationLine`
  vloží `// @dim`/`// @label` (a `// @sheet`) **před `}` aktivního bloku**
  (`ViewScope::insertLine`), s odsazením; root nebo neblokový `view()` → EOF
  (fallback = původní chování). `onDrawingSheetSettingsChanged` navíc **replace**
  `// @sheet` vlastněného přesně aktivním pohledem. **Testy:** `ViewScope_test.cc`
  (insertLine cíluje `}`, single-statement → -1); GUI smoke bez pádu. Živý
  round-trip ověří uživatel. (Auto-obal jednořádkového `view()` = odloženo,
  fallback na EOF.)
- **15F — headless dávka (dva režimy výběru).** ✅ HOTOVO. Pro každý zvolený
  pohled se nastaví `$view` a znovu vyhodnotí strom, pak proběhne stávající
  export (STL/PNG/…; kamery přes `--export-cameras`, výkres 4C jedou stejnou
  cestou). **Výběr pohledů** (volba `--model-view`, protože `--view` je obsazené
  viewport-optionami):
  - **A) všechny pohledy** — `--all-model-views` projede celý strom `view()`,
    soubor na pohled.
  - **B) zvolený seznam** — explicitní podmnožina, opakovaný
    `--model-view A --model-view B`; vnořený pohled jako cesta
    `--model-view assembly/exploded`.
  Tag pohledu se vloží do jména výstupu (`out.stl` → `out_A.stl`,
  `out_assembly_exploded.stl`); u **jediného** `--model-view` se `-o` nechává
  beze změny. Bez `--model-view*` → root jako dnes. **Testy:** `dump-view-active`
  (`--model-view` mění strom) zelený; ověřeno ručně: single/seznam/all/nested
  STL exporty.

### Headless (klíčová hodnota) — dva režimy výběru
> **Pozn.:** volba se jmenuje `--model-view` (a `--all-model-views`), protože
> `--view` už je obsazené (viewport display options: axes/edges/…). Tag pohledu
> se vkládá do jména výstupu (`out.svg` → `out_assembly.svg`); u **jediného**
> explicitního `--model-view` se `-o` nechává beze změny.

Z jednoho `.scad` (dnes ověřeno na geometrických exportech; kamera/vektor jedou
stejnou cestou přes aktivní `$view`):
- **A) všechny pohledy** naráz: `openscad model.scad --all-model-views -o out.stl`
  → `out_<pohled>.stl` pro každý `view()` (vč. vnořených jako `out_a_b.stl`).
- **B) zvolený seznam** pohledů: `openscad model.scad --model-view pcb
  --model-view housing -o out.stl` → `out_pcb.stl`, `out_housing.stl`. Vnořený
  pohled jako cesta: `--model-view assembly/exploded`.

Staví přímo na 4A (kamery) a 4C (vektor) — před evaluací každého pohledu se jen
nastaví `$view` a znovu se vyhodnotí strom.

### Zpětná kompatibilita
Model bez `view()` = jeden root = vše společné = **beze změny** (přepínač
skrytý, kóty na EOF, náhled ukazuje vše). Existující výkresy/kamery/`// @sheet`
fungují jako dřív (spadají do rootu).

### Import a více souborů (`import` / `include` / `use`)
Čtyři případy, každý jinak:
1. **`import("díl.stl")` (mesh):** žádná zvláštní interakce — obyčejná geometrie
   ve `view()` bloku nebo ve společné části; neaktivní pohled ji neimportuje
   (viz perf/mesh-cache).
2. **`include <lib.scad>`:** vloží cizí top-level kód → jeho `view()` bloky se
   stanou součástí stromu pohledů. **Nutné:** (a) **scope anotací path-aware** —
   `// @dim` patří do bloku jen když **stejný soubor** i rozsah řádků; bucket-klíč
   = `(path, řádek)` (`Location` už `path` nese). (b) **anotace nepřekračují
   hranici souboru** do obalujícího pohledu (`// @dim` uvnitř includovaného
   souboru se nezařadí do `view()` bloku v hlavním souboru). Pozn.: ověřit, že
   `include` **uvnitř** `view(){ }` bloku gramatika dovolí.
3. **`use <lib.scad>`:** jen definice, top-level `view()` se nevykoná; pohled
   vznikne jen instanciací modulu (→ případ 4).
4. **`view()` uvnitř modulu / smyčky (i z cizího souboru):** vzniká až evaluací,
   statický scan zdrojáku ho nevidí; navíc vnořený neaktivní `view()` se
   neinstanciuje → **nejde naráz enumerovat celý vnořený strom**. **Rozhodnutí
   pro v1:** pohledy musí být **literální `view("jméno")` bloky** (i přes
   `include`), enumerované scanem zdrojáku (path-aware). Modulem/smyčkou
   generované pohledy s počítaným jménem = **mimo v1** (geometricky fungují, když
   je jejich jméno aktivní, ale v přepínači se neukážou). Plná enumerace přes
   zvláštní průchod = **později**.

### Rizika
- **Výkon re-evaluace** u těžkých importů (STL 149k facy, Fáze 14) → přepínání
  může trhat. **Mesh-cache importu napříč re-evaluacemi = odloženo** (rozhodnuto
  neřešit teď); pokud bude problém, řešit až měřením.
- **Write-back do bloku** je křehčí než append na EOF (najít správnou `}`,
  auto-obal jednořádkového `view()`). Dobře ohraničené, ale hlídat okrajové
  případy (prázdný blok, komentář těsně před `}`).
- **Prázdný root** nesmí uživatele překvapit prázdným náhledem → auto-posun na
  prvního potomka (pravidlo 4).

### Odloženo (forward-compat, ne teď)
Dlouhodobě „SCAD-like" varianta: anotace jako **reálné child-moduly** uvnitř
`view()` (`dim("a","b", offset=…)` místo `// @dim`) — lokace i world-matice z
AST zadarmo, odpadá re-parsing zdrojáku a řádkové bucketování. `view()` kontejner
je právě **schůdek** k tomu; migrace = přepis `comment_parser.y` + sběru =
samostatný velký projekt, **až později**.

## Fáze 16 — CLI: kamery per-view + technický výkres do souboru

### Zadání / motivace
Z jednoho `.scad` chce autor v **headless/CLI** vygenerovat pro vybrané pohledy
(`--model-view` / `--all-model-views`): (A) **snímky kamer** a (B) **technický
výkres** (ortho pohledy + kóty + řezy + detaily + list) do **SVG / PDF / PNG**
(příp. DXF). Doplňuje headless dávku z Fáze 15 o vizuální výstupy.

### 16A — CLI kamery per-view  ✅ HOTOVO (2026-07-27)
`export_cameras` (`src/openscad.cc`) přepsán z jediné evaluace na **smyčku přes
pohledy** (`resolveViewSpecs`, sdílené s `cmdline`): pro každý zvolený pohled
přenastaví `$view`, re-instanciuje strom a vyrenderuje jeho `camera()` markery
(`render_camera_preview` → offscreen GL). `--export-cameras out_` respektuje
`--model-view A` (kamery uvnitř `view("A")`) i `--all-model-views` (tag pohledu ve
jméně: `out__A_hero.png`). Bez výběru = root jako dřív. Ověřeno; regrese zelené.

### 16B — CLI technický výkres  ✅ HOTOVO (2026-07-27)

**Jak to dopadlo (implementace).** Místo riskantní extrakce 300řádkového
`refreshDrawingViews` zvolena **nízkoriziková cesta k GUI**: CLI **recykluje celý
existující pipeline** tím, že spustí `MainWindow` **headless** pod offscreen
`QApplication` (jako gui-testy) a nechá dok postavit data:
- **16B-1 (bezpečná extrakce)**: `renderToFile`+`Placement` vytaženy do file-scope
  `renderSheetToFile` + přidán **PNG** (QImage); nová public statická
  `DrawingViewDock::exportDrawingSheet(...)` a `exportToPath(path)` (export bez
  dialogu). `exportDrawing` doku je teď tenký wrapper → GUI beze změny chování.
- **16B-3 (řízení)**: `MainWindow::exportDrawingHeadless(modelViews, all, prefix,
  ext)` — pro každý pohled nastaví `$view`, zavolá `actionRenderPreview()`
  (**synchronní**: `compile`→`compileDone`→`csgRender`→`refreshDrawingViews`), pak
  `dock->exportToPath()`. `gui()` dostal param `CliDrawingExport*` a přes
  `QTimer::singleShot` export spustí a ukončí appku. Nová CLI volba
  `--export-drawing <prefix>` (+ `--drawing-format`, default svg) v `openscad.cc`
  routuje do `gui()` (guard `#ifndef OPENSCAD_NOGUI` — v headless-noQt hlásí chybu).
- **16B-2 (extrakce build-strany)**: NEBYLA potřeba — headless běh doku ji
  obešel. `refreshDrawingViews` zůstal netknutý → **nulové riziko regrese** GUI.

**Runtime.** Potřebuje Qt (plný build) + offscreen GL/QPA: `xvfb-run openscad
model.scad --export-drawing out --all-model-views` → `out_<pohled>.svg`. Ověřeno:
SVG (25 polyline + text), PNG (325×280), single i `--all-model-views`.

--- (původní plán níže, ponechán jako záznam) ---

**Problém.** Celý výkresový pipeline je dnes **jen v GUI a přes Qt**:
- **stavba listu**: `MainWindow::refreshDrawingViews` (~300 ř.) — projekce
  geometrie (silueta + HLR), rozřešení kót/labelů (`resolveDrawing`/`resolveLabels`),
  řezy, detaily, `// @sheet`, per-view scoping (Fáze 15D) → `OrthoViewData` na
  pohled; provázané s členy `MainWindow`/`DrawingViewDock`.
- **export**: `DrawingViewDock::exportDrawing` → lambda `renderToFile` (~240 ř.) —
  kreslení primitiv přes **QPainter** → SVG (`QSvgGenerator`) / PDF (`QPrinter`),
  + vlastní **DXF** writer. Layout listu (ISO-E) je `sheetLayout`.

**Qt je OK.** `QSvgGenerator`/`QPainter`/`QPrinter` běží **i bez displeje**
(offscreen), takže CLI výkres poběží v plném (Qt) buildu bez X. Headless-noQt
variantu jsme opustili (úspora ~20 MB nestála za to), takže Qt v CLI nevadí.

**Kroky (bezpečně, každý build+test, GUI průběžně nesmí regresovat):**
- **16B-1** — vytáhnout `renderToFile` + `sheetLayout` do **ne-widget** funkcí
  (nový `src/gui/DrawingExport.{h,cc}` nebo `src/io/`):
  `renderDrawingSheet(places, sheetWmm, sheetHmm, path, format)`, kde `format ∈
  {SVG,PDF,DXF,PNG}`. **Přidat PNG** (přes `QImage`+`QPainter`, DPI parametr).
  `DrawingViewDock::exportDrawing` pak jen zavolá tuto funkci → neutrální refaktor.
- **16B-2** — vytáhnout stavbu listu z `refreshDrawingViews` do
  `buildDrawingSheet(rootNode, source, viewOptions, activeViewPath, sheetSettings)
  → {places, sheetW, sheetH}`, odvázané od `MainWindow`/doku (dnešní členy
  `viewPxPerMm` apod. si spočítat lokálně). GUI `refreshDrawingViews` i CLI pak
  volají tentýž builder. **To je jádro rizika** (velká funkce, hodně stavu).
- **16B-3** — CLI `--export-drawing <prefix>` (+ `--drawing-format svg|pdf|png|dxf`,
  jinak dle přípony) pod **offscreen `QApplication`** (`QT_QPA_PLATFORM=offscreen`):
  parse+`collectParameters`, smyčka přes `resolveViewSpecs` (sdíleno s 16A), pro
  každý pohled `buildDrawingSheet` → `renderDrawingSheet` → `<prefix>_<pohled>.<ext>`.
  Volitelně `--drawing-separate` (režim „každý pohled do svého souboru", už existuje
  v doku).

**Testy:** cmdline — `--export-drawing` na modelu s `dimref`/`section` vyprodukuje
neprázdný SVG (a PNG > pár px barev); `--all-model-views` soubor na pohled; layout
== GUI export (sdílená `sheetLayout`/`renderDrawingSheet`). Pozor na offscreen Qt
v CI (`xvfb`/`-platform offscreen`).

**Rizika.** 16B-2 je hlavní objem — odvázání od GUI stavu. Ověřovat, že dok kreslí
i exportuje **beze změny** (regrese vizuálně + porovnáním exportů před/po). SVG/PDF
přes QPainter je bez displeje OK; PNG rasterizace taky (QImage), GL netřeba.

## Fáze 17 — per-view vlajky exportu pro CI  ✅ HOTOVO (2026-08-12)

### Zadání / motivace
GitHub Action nad repem s modelem má vygenerovat **jen vybranou podmnožinu**
výstupů: u některých pohledů snímky kamer („jak se zařízení skládá“), u jiných
technický výkres (celek i komponenty), u dalších jen model (komponenty pro tisk).
Volba patří do modelu, ne do CI skriptu, aby se workflow nemusel měnit s každým
novým pohledem.

### Řešení
Tři nezávislé volitelné vlajky na `view()`, všechny default `false`:

```scad
view(name = "assembly", camera_export = true)                        { … }
view(name = "celek",    drawings_export = true)                      { … }
view(name = "pcb",      drawings_export = true, model_export = true) { … }
view(name = "debug")                                                 { … }  // nic
```

CLI `--default-model-views` vybere pohledy označené pro **právě běžící** export
(`--export-cameras`→`camera_export`, `--export-drawing`→`drawings_export`,
`-o`→`model_export`), vždy s tagem pohledu ve jménech souborů. Nic označeno =
varování + žádný výstup, ne chyba (CI matice běží nad všemi modely).
`--list-model-views[=text|json]` vypíše pohledy s vlajkami pro matrix job.

Vlajky se čtou dvěma cestami: při instanciaci (`Parameters`, vidí i spočítanou
hodnotu, jde do `toString()`) a z AST v `collectViews()` — tam **jen literál**
`true`/`false`, protože AST-walk nevyhodnocuje výrazy. Výběr pohledů pro export
používá AST cestu (funguje i pro neaktivní pohledy).

Detaily implementace, ověření a šablona workflow (`doc/ci/render-views.yml`) viz
`state.md` „Fáze 17“.

### 17B — enumerace pohledů napříč scope  ✅ HOTOVO (2026-08-12)

`view()` je **label, ne identita**: každý blok s daným jménem je aktivní, když je
jméno na aktivní cestě (modul volaný víckrát, iterace `for`, dva bloky stejného
jména → union). Instanciace to tak dělala od Fáze 15; doplněna byla **enumerace**
(`collectViews`), na které stojí `--list/all/default-model-views` i GUI přepínač:
prochází se i těla volaných uživatelských modulů (včetně `use <>` přes
`SourceFileCache`, s scope-chainem definice a guardem proti rekurzi) a obě větve
`if/else`; konzumenti používají `uniqueViewPaths()`, takže jedna cesta = jeden
výstupní soubor. Detaily v `state.md` „17B".

---

## Fáze 18 — `slicer_modifier()`: modifikátory PrusaSliceru v exportu 3MF  ✅ HOTOVO (2026-10-15)

### Zadání / motivace

Modifikátory sliceru (oblast s jinou výplní, víc perimetrů kolem díry, blokátor
podpor…) dnes autor naklikává v PrusaSliceru ručně po každém exportu. Mají žít
v `.scad` a cestovat s modelem v `.3mf`.

```scad
cube([40, 40, 10]);
slicer_modifier(fill_density = "80%", perimeters = 5)
    translate([15, 15, -1]) cylinder(d = 12, h = 12);
slicer_modifier(type = "support_blocker") translate([0, 0, 10]) cube(5);
```

### Formát (ověřeno ve zdrojácích PrusaSliceru 2.9.2, `src/libslic3r/Format/3mf.cpp`)

- Části objektu = rozsahy trojúhelníků `firstid`/`lastid` (včetně) v **jedné
  spojené `<mesh>`**; `<components>` se na části nepřevádí. Vrcholy každé části
  musí tvořit souvislý blok (importér bere vrcholy min..max rozsahu).
- Popis částí v příloze `Metadata/Slic3r_PE_model.config`:
  `<object id><volume firstid lastid><metadata type="volume" key value/>…`;
  atribut `type` (`object`/`volume`) je povinný.
- Typ části `volume_type`: `ModelPart`, `ParameterModifier`, `NegativeVolume`,
  `SupportEnforcer`, `SupportBlocker` (od Slic3r PE 1.42); zapisuje se i
  legacy `modifier="1"`, jako to dělá PrusaSlicer.
- Nastavení = vlastní klíče PrusaSliceru jako text (`set_deserialize`).
  **Neznámý klíč shodí načtení celého souboru** (`UnknownOptionException`).
- `Metadata/Slic3r_PE.config` (profil) se **nezapisuje** — soubor pak není
  „projekt“, nepřepíše uživateli presety, jen se vycentruje na podložku.
- Souřadnice: vrcholy v souřadnicích objektu, transformace buildu identita →
  stačí světové souřadnice z OpenSCADu.
- Barvy (`basematerials`/`colorgroup`) PrusaSlicer ignoruje; filament jen
  klíčem `extruder`.
- Orca/Bambu Prusa formát čtou jen s hlavičkou `Application=PrusaSlicer…` a
  i pak zahodí vše kromě typu části a `extruder` → **mimo cíl**. Nová (nevydaná)
  větev PrusaSliceru čte config možná jen ze souborů označených jako
  PrusaSlicer ≤ 2.99.1 — **rozhodnuto: neimitovat** hlavičku, řešit až nastane.

### Rozhodnutí

- Název **`slicer_modifier`** (slovo „modifier“ v OpenSCADu už znamená `# % ! *`).
- `type = "modifier"` (default) | `"negative"` | `"support_blocker"` |
  `"support_enforcer"`; `name = "…"` (jinak `slicer_modifier N`).
- Nastavení = **pojmenované argumenty 1:1 na klíče PrusaSliceru**. Hodnoty:
  číslo → text (celé bez desetinné části), string doslova (`"80%"`), bool →
  `1`/`0`; jiné typy = varování + vynechat.
- **Vestavěný seznam platných klíčů** = `PrintRegionConfig` z PrusaSliceru
  2.9.2 + `extruder`, i s typem volby (`PrintObjectConfig` jen pro lepší
  varování — na modifikátoru nemá účinek). Neznámý klíč nebo hodnota špatného
  typu = varování a klíč se **nezapíše** (soubor se vždy načte). Nastavení na
  `negative`/`support_*` = varování + vynechat.
- Uzel je **marker** jako `camera()`: do geometrie (F6, STL, …) nepřispívá;
  v náhledu F5 se děti kreslí jako pozadí (`%`), aby bylo vidět kde modifikátor je.
- Exportuje se **jen 3MF** (v1 i v2 lib3mf — obě API umí přílohy). Bez
  `slicer_modifier` v modelu je výstup bit po bitu jako dosud.
- **Jeden objekt:** s modifikátory jde celý model do jednoho mesh objektu
  (každý top-level kus z lazy-union = vlastní `ModelPart`) + modifikátory jako
  další části. Přiřazení modifikátorů k více objektům = mimo v1.
- `view()`: nic extra — modifikátor uvnitř pohledu se exportuje s pohledem.
- **Mimo rozsah** (možné navázání): barvy `color()` → části s `extruder = N`
  v jednom exportu; `type = "part"`; per-object nastavení; Orca/Bambu.

### Kroky

1. **18A jádro jazyka:** `src/core/SlicerModifierNode.{h,cc}` (builtin, parsování
   libovolných pojmenovaných argumentů, validace klíčů, `toString` do dumpu),
   seznam klíčů `src/core/prusaslicer_keys.h`, registrace v `Builtins.cc`,
   `NodeVisitor` forward. `GeometryEvaluator`: prune → prázdno.
   `CSGTreeEvaluator`: děti jako pozadí. Sběr `collectSlicerModifiers(root)`
   (světová matice jako u kamer).
2. **18B export:** `ExportInfo::slicerVolumes` (vyhodnocená geometrie ve
   světových souřadnicích + typ + nastavení), plnění v CLI (`openscad.cc`),
   GUI exportu 3MF a „poslat do sliceru“ (`ExternalToolInterface`);
   `export_3mf_v1/v2`: jeden objekt, souvislé bloky vrcholů, rozsahy
   trojúhelníků, příloha `Slic3r_PE_model.config` (generátor XML sdílený,
   v `src/io/`).
3. **18C testy:** dump/echo `slicer-modifier` (+ varování neznámého klíče),
   unit test generátoru XML, regresní `export-3mf` s modifikátorem
   (`post_process_3mf` přidá obsah přílohy, je-li), stávající 3MF testy beze
   změny. Ověření v PrusaSliceru (pokud je k dispozici) / ručně uživatelem.

### Co odhalila implementace

- **Hodnoty ověřené proti skutečnému PrusaSliceru 2.9.2** (`apt install
  prusa-slicer`, CLI `--export-3mf` round-trip a `--export-gcode`):
  - špatný **bool / int** (`thin_walls="5"`, `perimeters="abc"`) shodí načtení
    celého souboru („Invalid value provided for parameter …“) → nestačí
    whitelist klíčů, validuje se i **typ** hodnoty podle typu volby
    (Bool/Int/Float/Percent/FloatOrPercent/Enum, tabulka v
    `SlicerModifierNode.cc`);
  - špatný **enum** PrusaSlicer jen nahradí výchozí hodnotou s varováním →
    stringy pro enumy se předávají beze změny;
  - **holé číslo u procentní volby je poměr**: `fill_density = 50` → 5000 %.
    U `Percent` voleb (`fill_density`, `ironing_flowrate`,
    `scarf_seam_start_height`) se k číslu i číselnému stringu doplní `%`.
- **Locale:** `snprintf` psal pod českým locale `2,5`; čísla se teď formátují
  v `std::locale::classic()`.
- Modifikátor se do slicování skutečně promítne (krychle 40×40×10 se středem
  `fill_density=100`: 3929 mm filamentu proti 2853 mm bez něj).

### Otevřené / mimo rozsah

- Regresní testy běží jen s lib3mf v1 (lokální Debian 1.8.1). Cesta pro v2
  (AppImage, lib3mf 2.4.1) je napsaná zrcadlově a prošla `-fsyntax-only`
  proti hlavičkám lib3mf 2.4.1; funkčně ji ověří první build AppImage.

### 18D — zobrazení: průnik s modelem v barvě  ✅ HOTOVO (2026-10-15)

**Rozhodnutí uživatele (2026-10-15).** OpenSCAD neví, *co* je v oblasti
modifikátoru jinak (infill, perimetry, extruder…), jen *kde*. Vyznačí proto
**část modelu uvnitř modifikátoru** (model ∩ M) jinou barvou; samotný
přečnívající tvar modifikátoru se běžně nekreslí. Celý tvar si uživatel ukáže
standardně přes **`#`** (highlight), jako negativní objem v `difference()`.

- Nový parametr **`color`** na `slicer_modifier` (stejné formy jako `color()`:
  jméno, `"#rrggbb"`, vektor). **Bez `color` se použije výchozí barva**, takže
  oblast je v náhledu vidět vždy.
- **Výchozí: vidět všude** (F5, snímky kamer, výkresy), **jde schovat**
  (upřesnění uživatele 2026-10-15, nahrazuje původní „kamery/výkresy jen
  s přepínačem“):
  - F5: přepínač v menu *View* („Show Slicer Modifiers“), default zapnuto;
  - `camera(..., show_modifiers = false)` schová ve snímku;
  - výkres: `// @sheet … modifiers=off` + checkbox v doku Drawing.
- **Náhled / snímky kamer (OpenCSG):** model zůstává beze změny; každá
  oblast je **vlastní sada CSG produktů** `model ∩ M` s přebarvenými listy
  (`CSGTreeEvaluator::getSlicerRegionNodes` → `CsgInfo::compileSlicerRegions`
  → `OpenCSGRenderer::setSlicerRegionProducts`), kreslená **po modelu
  s `glPolygonOffset`** (o krok blíž za každou další oblast → pozdější vyhrává
  v překryvu). Žádný booleovský výpočet. ThrownTogether oblasti nekreslí
  (bez CSG by přebarvil celý model). Dnešní kreslení celého tvaru jako pozadí
  (`%`) odpadlo; `%slicer_modifier` dál ukáže jen tvar jako pozadí.
- **Vektorový výkres** pracuje s meshem → **skutečný boolean** (Manifold,
  `colorSlicerRegions` v `io/slicer_volumes.cc`): `(model − M) ∪ (model ∩ M)`
  s druhou částí obarvenou (`setColor`), vnitřní stěny union odstraní.
  Výsledný mesh je v `DrawingProjCache::mesh` (klíč geometrie + přepínač).
  Vidět je ve stávající cestě „Fill colors“.
- F6 / export geometrie: beze změny (modifikátory do geometrie nepatří).
- Testy: dump `color`, obrazové `preview-*` s modifikátorem (barevný průnik,
  bez přečnívání) a s `#slicer_modifier` (celý tvar), snímek kamery s/bez
  `show_modifiers`, výkres s/bez `modifiers=off` (unit nad výplní).

#### Co odhalila implementace 18D

- **Přepis stromu na `(R − M) ∪ (R′ ∩ M′)` v OpenCSG nefunguje.** OpenCSG
  kreslí barvu produktu s `glDepthFunc(GL_EQUAL)` proti hloubce, kterou
  nechaly předchozí produkty. `R′` má plochy přesně v místě `R`, takže
  přebarvil celý model. Proto zvláštní produkty s polygon offsetem.
- Na hranách oblasti může být pár pixelů artefaktů polygon offsetu (vidět
  na hraně válce v testu `preview-*_slicer-modifier-preview`).
- **Výkres bez „Fill colors“ oblast nevyznačí:** hranice oblasti leží
  v rovině plochy (bez lomu), takže z ní projekce hran neudělá čáru. Případné
  „obrysy barevných hranic“ by změnily výkresy i pro `color()` — neřešeno.
- `ManifoldGeometry::toPolySet()` čte barevné schéma → unit test musí volat
  `PlatformUtils::registerApplicationPath(current_path)`.
- Testy: `dump/echo-slicer-modifier` (+ `color`), `dump/echo-camera-node`
  (+ `show_modifiers`), nový `3D/features/slicer-modifier-preview.scad`
  (auto-glob: dump/render/preview/throwntogether/render-csg), unit
  `colorSlicerRegions`. Ověřeno i v `build-lib3mf2`. Ručně ověřeno: snímek
  kamery s/bez `show_modifiers`, `--export-drawing` s/bez `modifiers=off`,
  GUI smoke (Xvfb). Přepínač *View → Show Slicer Modifiers* a checkbox
  v doku jsou bez automatického testu.

