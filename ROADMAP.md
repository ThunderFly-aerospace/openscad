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
- **Fáze 2 (výkresy + kvóty)** — 🔶 směr odsouhlasen, detaily doladit; sdílí
  infrastrukturu sběru marker-uzlů s kamerami (`dimref`).
- **Fáze 3 (OBJ export s barvami / `.mtl`)** — 🔶 naplánováno. Malé uzavřené
  vylepšení: `export_obj.cc` dnes barvy zahazuje; dopsat `.mtl`, aby Blender
  načetl per-region barvy nativně jako materiály. (3MF už barvy exportuje a
  Blender ≥ 4.1 ho čte nativně — to je hotová cesta; OBJ+MTL je robustní
  textová alternativa.)

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

## Fáze 2 — technické výkresy a kvóty  🔶 směr odsouhlasen, detaily doladit

### Zadání
Nárys / bokorys / půdorys (ortografické pohledy) + **kreslení kvót** klikáním.
Kvóty se musí zapsat do `.scad` **co nejblíž objektu, ze kterého vznikly**, a
**aktualizovat se se změnou zdrojáku**. Dnes autor používá FreeCAD openscad
branch, kde po vytvoření modelu už nejde editovat — to je hlavní bolest.

### Datový model (návrh autora + zjednodušení)
- **Kotevní bod** = jeden konec kvóty, definovaný v kódu **u objektu**,
  u jehož stěny/hrany/rohu má viset.
- **Kvóta** = dvojice kotevních bodů; někde je **seznam dvojic**.
- **Zjednodušení:** kotevní bod ať je **stejný typ neviditelného marker-uzlu
  jako kamera** (např. `dimref("A")`). Když je zapsán ve stejném
  `translate()/rotate()` jako objekt, **pohne se s objektem automaticky**
  (sdílená transformace). World-pozice se sbírá **stejným kódem jako kamery**
  → proto stavíme kamery první.
- **Hodnota kvóty se počítá až při renderu** = vzdálenost world-pozic dvou
  bodů → aktualizuje se sama. Popisek buď spočítaný, nebo ručně přepsaný.
- **Seznam dvojic** = komentáře, např. `// @dim A B popisek offset dir`.

### Hranice automatiky (klíčové k doladění)
- Posun/rotace objektu → bod jede s ním zadarmo. ✅
- Změna **velikosti** objektu (bod na pohybující se stěně) → funguje jen
  když je bod vyjádřený **stejnou proměnnou** jako rozměr
  (`translate([w,0,0]) dimref("A")`). **Automatická re-detekce geometrického
  prvku po libovolné editaci obecně NEJDE** — trackujeme přes transformace a
  proměnné, ne přes re-detekci geometrie.

### Round-trip (editace)
- *Vytvoření:* klik dvou prvků ve výkresu (přes stávající měřicí picking) →
  GUI vloží dva `dimref(...)` poblíž objektů + řádku `// @dim A B ...`.
- *Úprava odsazení/pozice popisku:* tažením → GUI přepíše parametry řádky
  `// @dim ...`.
- *Posun objektu v kódu:* nic — markery jedou s ním, hodnota se přepočítá.

### Otevřené otázky k doladění (než začneme)
- Jak přesně `dimref` vložit „na správné místo se souřadnicí relativní k
  objektu" — nejtěžší UX část.
- Formát a umístění seznamu `// @dim` (u objektu vs. dedikovaný blok).
- Vykreslení kvót: 2D overlay v doku výkresu (projekce world-bodů do
  aktuálního ortografického pohledu; šipky + text v Qt), bez zápisu do
  geometrie.
- Hidden-line / silueta pro „opravdový" technický výkres — pravděpodobně až
  pozdější fáze.

### Hrubé kroky (A/B/C)
- **A:** dok „Výkres" s ortografickými pohledy (nárys/bokorys/půdorys,
  reuse ortho kamery). Jen prohlížení.
- **B:** čtení `// @dim` + `dimref` markerů, přepočet hodnot z geometrie,
  2D overlay kvót v pohledu.
- **C:** interaktivní tvorba kvót (klik prvků → zápis do zdroje) + stabilní
  ukotvení a heuristiky umístění.

### Testy (součást fáze)
- **Unit (Catch2)** — přepočet hodnoty kvóty z geometrie, sběr `dimref`
  (sdílí kód s kamerami), parsování `// @dim`.
- **Regresní `dump`/`echo`** — `dimref` je prázdný uzel, nerozbije CSG strom.
- **GUI test** (`src/guitests/`, styl `TestMainWindow`) — round-trip:
  vložit `dimref`/`@dim` → upravit → ověřit text v editoru. Vzor pro picking
  konců kvót: `src/gui/Measurement_test.cc`.

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
| Ukotvení kvót | marker-uzly (`dimref`) + seznam `// @dim`, přepočet z geometrie |
| Barevný export | 3MF (hotová cesta, Blender ≥ 4.1 nativně) + dopsat OBJ `.mtl` |
