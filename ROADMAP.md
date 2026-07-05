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
