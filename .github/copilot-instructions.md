# SGDK/Genesis · KI-Regelwerk

> Projektspezifische Details → [.github/copilot-instructions-project.md](.github/copilot-instructions-project.md)

## 1. KI-VERHALTENSREGELN
- Vor jeder Aufgabe beide Regeldateien lesen und anwenden:
    - `.github/copilot-instructions.md` (abstrakt)
    - `.github/copilot-instructions-project.md` (projektkonkret)
- Abstrakte Regeln (`.github/copilot-instructions.md`) werden nicht geändert,
    außer bei expliziter Nutzeranweisung.
- Bei Projektänderungen muss `.github/copilot-instructions-project.md`
    im selben Arbeitsgang auf Konsistenz geprüft und bei Bedarf aktualisiert werden.
- Keine Lobformulierungen, Bestätigungen oder Begründungen
- Risiken/Probleme: knapp erwähnen
- Kein „Soll ich...?" — direkt ausführen
- Mehrstufig: nach Abschluss einer Stufe nur den nächsten Schritt nennen
- Code im Chat: immer **„Datei – Funktion – ab Zeile ##"** voranstellen
- Code: niemals mit `...` oder `// ...` kürzen — Funktion immer vollständig zeigen
- Nach größerer Änderung: Verifikation auf weggelassene Funktionalität, ggf. nachbessern ohne Aufforderung
- Nach Codeänderungen Build selbstständig ausführen, Terminal-Fehler analysieren, automatisch korrigieren.
- Erst bei erfolgreichem Build beenden; bei Build-Fehlern nicht vorzeitig stoppen.
- Build-Priorität für dieses Projekt: immer Debug-Build (`tetr-vibe-debug.gen`).

## 2. PLATTFORM
- CPU: M68000 · 16/32-bit · Big-Endian · ~7.67 MHz
- Framework: SGDK · C99 · m68k-elf-gcc
- Audio-Treiber: XGM2 (`Z80_DRIVER_XGM2`) ausschließlich
- Debug: `KLog` `KLog_U1` `KLog_U2` `KLog_U2_` `kprintf`
- VERBOTEN: `malloc` `free` `printf` `float` `double`

## 3. CODEQUALITÄT
- Kein Over-Engineering; keine Single-Use-Abstraktionen
- Docblocks nur für neue/geänderte Funktionen
- Kein `MEM_alloc` im Update-Loop
- Kein Float
- Alignment: u16→gerade Adresse, u32→4-Byte-Adresse (Bus-Error-Risiko auf 68k)
- Big-Endian: explizit bei Speicherzugriff / SRAM / DMA
- `SPR_update()`: exakt einmal/Frame nach allen SPR-Aufrufen, vor `SYS_doVBlankProcess`
- DMA bevorzugen für große Tile-Uploads; CPU für kleine Korrekturen
- BG_A/BG_B-Zuordnung: fest pro Projekt, nie wechseln

## 4. HEADER-KONVENTIONEN
- `#pragma once` obligatorisch
- Alle `typedef struct union enum #define` ausschließlich in Headers
- Exportierte Funktion → JSDoc: `@brief @param @return @note @warning`
- `static inline` mit Body im Header erlaubt

## 5. DATENSTRUKTUREN
Feldfolge: `u32 → u16 → u8 → bool` (minimiert Padding)

Attribute:
- `__attribute__((packed))` → SRAM/Serialisierung (kein Padding; Unaligned-Access auf 68k langsam)
- `__attribute__((aligned(2)))` → Standard SGDK
- `__attribute__((aligned(4)))` → DMA-Puffer
- `static inline` → Hot-Path (kein Call-Overhead, Code wird direkt eingefügt)

Typedef-Tag immer in Header, nie in `.c` — **PFLICHT: `typedef struct Foo { ... } Foo;` — nie `typedef struct { ... } Foo;` (anonyme Structs sind verboten)**:
```c
typedef struct Foo { u32 a; u16 b; u8 c; u8 _pad; } Foo;
typedef union Bar { u32 raw; struct { u16 hi; u16 lo; }; } Bar;
```

Flags statt bool-Arrays:
```c
#define F_ACTIVE  (1U << 0)
#define SET_FLAG(v,f)    ((v) |=  (f))
#define CLEAR_FLAG(v,f)  ((v) &= ~(f))
#define TOGGLE_FLAG(v,f) ((v) ^=  (f))
#define GET_FLAG(v,f)    ((v) &   (f))
```

## 6. NAMENSKONVENTIONEN
- Typen/Enums: `PascalCase` (z.B. `GameState`)
- Makros/Konstanten: `UPPER_SNAKE_CASE`
- Funktionen: `lower_snake_case`
- Static-`.c`-Vars: `lowerCamelCase`; nie exportieren
- Globale Exports: Modulpräfix (z.B. `config` `currentState` `joyState`)
- Flag-Makros: `(1U<<n)` oder `(1UL<<n)` für u32

## 7. SPEICHER
- StateUnion: alle zustandsspezifischen Daten; exklusiv, nie zwei Slots gleichzeitig aktiv
- State-Wechsel: Pointer umschalten; kein `MEM_free` auf Union-Member
- Init-Pflicht: `memset(sctx, 0, sizeof(StateUnion))` vor jedem neuen State
- Static/globale Puffer für frame-kritische Daten; kein `MEM_alloc` im Update-Loop

## 8. ARITHMETIK
Priorität: **Bitshift > Multiplikation > Division**
```
val/2    → val>>1
val*2    → val<<1
val*10   → (val<<3)+(val<<1)
idx(x,y) → x+((y<<3)+(y<<1))   // Board-Index, Breite=10
```
- Kein Float (68k ohne FPU); Fixkomma oder Bitshift
- 32-Bit-Operationen: `mulu(a,b)` `divu(a,b)` `divs(a,b)` aus SGDK maths.c
- Hot-Path-Berechnungen als Header-Makro oder `__asm__` inline (Kommentar obligatorisch)
- Dimensionen bevorzugt Power-of-2: 8 16 32 64 128 256 512 1024

## 9. FRAME-LIFECYCLE & STATE-VERTRAG
Hook-Folge (obligatorisch): `init → init_draw → update → update_draw → cleanup`

State-Wechsel:
1. `old.cleanup()`
2. `memset(sctx, 0, sizeof(StateUnion))`
3. `new.init()` → `new.init_draw()`

Frame-Reihenfolge:
1. `JOY_readJoypad` — einmal, zentral in main
2. `state.update()` — Logik, Timer, Transitionen
3. `state.update_draw()` — visuelle Updates
4. `SPR_update()` — einmal
5. `SYS_doVBlankProcess()` — einmal

Trennungsregeln:
- `update`: kein VDP-Massenupdate, kein Redraw
- `update_draw`: keine Spiellogik, keine Input-Auswertung
- `cleanup`: nur state-lokal; kein globales Reinit

`update_input` optional: einführen wenn ≥3 States denselben Input-Prepass teilen.

## 10. PERSISTENZ (SaveManager + Serializable-Shadow)
Schichten:
- `Serializable __attribute__((packed))` → SRAM
- `RuntimeConfig` → RAM only, nie persistieren
- `GlobalConfig`: `union { Serializable serializable; struct{...}; }` + `RuntimeConfig runtime`

Ownership: nur `save_manager` liest/schreibt SRAM via `SRAM_enable/disable readByte writeByte`

Header-Layout: `SAVE_MAGIC(u32)` + `SAVE_VERSION(u32)` an festen Offsets `ADDR_MAGIC ADDR_VERSION`

Compile-Guard:
```c
typedef char sz_check[(ADDR_OPTIONS + sizeof(Serializable) <= SRAM_WINDOW) ? 1 : -1];
```

Serialize/Deserialize: `u8* ptr = (u8*)&config.serializable; for(u16 i=0; i<sizeof(Serializable); i++)`

Nach Save: `save_verify()` obligatorisch (Byte-Vergleich RAM vs SRAM)

Save-Flow: eigener `STATE_SAVE` mit `SramOp` Enum `{NONE INIT LOAD SAVE}` + `preferredState` für Rücksprung

Boot: MAGIC+VERSION prüfen → bei Match `save_load()`, sonst `save_clear()` (Defaults zentralisiert)

Version-Bump: jede Layoutänderung → `SAVE_VERSION` erhöhen

Nicht in Serializable: Timer, Cursor, Animationen, KI-Zwischenstände, Runtime-Flags, abgeleitete Caches

## 11. VRAM-BUDGET & TILE-MANAGEMENT
- `SPR_init()` reserviert 420 Tiles direkt vor Font-Bereich
- Tile-Indizes zentral in `gfx.h` / `sprite.h` als `#define TILE_x_BASE n`
- State-weite Tiles: einmalig laden in `gfx_load_tiles(u16 offset)` beim Gameplay-Init
- Nach großem Upload: `VDP_waitDMACompletion()` obligatorisch
- Kein mehrfacher großer Upload/Frame ohne zwischengeschaltetes `VDP_waitDMACompletion()`
- Große Uploads nur in Init-Phase oder eigenen Übergangs-States, nie im Update-Loop
- Standard für Tilemap-Writes: `VDP_setTileMap*(..., DMA)`

## 12. SPRITE-ENGINE & PARTIKEL
Ownership:
- `sprites_init()`: einziger Einstieg für SPR-Engine-Init
- `sprites_update()`: einziger Schreiber für Position/Animation
- `SPR_update()`: einmal in main nach `sprites_update()`
- Kein direkter `SPR_*`-Aufruf außerhalb Sprite-Manager

GameSprite Struct:
```c
typedef struct {
    Sprite* vdpSprite;
    s16 x, y, offsetX, offsetY;
    u16 frame, animation, animTimer, stateTimer, attr;
    s16 animDir;
    u8  type, padding;
} GameSprite;
```

- Cleanup: `SPR_setPosition(spr, -128, -128)`, kein `SPR_end`
- DustParticle: statisches Array `dustParticles[DUST_SLOT_COUNT]` · `active` Flag · TTL in `sprites_update()`
- Clipping: Board-Grenzen `DUST_BOARD_MIN_X MAX_X MIN_Y` obligatorisch

## 13. PLANE-AUFTEILUNG & BACKGROUND-MANAGER
- `BG_A`: Gameplay/HUD — dynamische Tile-Updates
- `BG_B`: Background-Manager exklusiv (Menü, Starfield, Parallax)
- Sprites: über beiden Planes (`DEPTH_FOREGROUND..DEPTH_BACKGROUND`)
- Aufteilung fest; kein Wechsel innerhalb eines Projekts

Background-Manager API: `menu_bg_init` `menu_bg_set_mode(BG_MODE_x)` `menu_bg_set_mode_instant` `menu_bg_update`

Modi: `BG_MODE_NONE MENU SPACE RIISTAR CLUB ...`
- Nur `menu_bg_*` schreibt auf BG_B
- State setzt Modus über `menu_bg_set_mode()` in `state_init()`
- Palette-Freeze: `menu_bg_set_palette_frozen(TRUE)` vor Fade-Out; `FALSE` nach Fade-In
- Komplexe Modi können in eigene States ausgelagert werden (5-Hook-Vertrag bleibt, BG_B-Ownership bleibt)

## 14. FADE-STRATEGIE
```
State-Fade : 30 Fr (PAL-korrigiert) · Haupt-Transitions (TITLE↔GAME)
BG-Fade    : 15-30 Fr · nur Background-Mode-Wechsel
No-Fade    : 0 Fr · Sub-States, Dialoge, Pausen
```

State-Fade-Flow (verbindlich):
1. `PAL_fadeOut` + `menu_bg_set_palette_frozen(TRUE)`
2. `old.cleanup()` + `menu_bg_set_mode_instant(BG_MODE_NONE)`
3. `new.init()` + `new.init_draw()`
4. `PAL_fadeIn` + `menu_bg_set_palette_frozen(FALSE)`

- Sub-States: `VDP_clearPlane` oder Text-Clear, kein Vollscreen-Fade
- Color-Ramps: vorberechnet in RAM · `PAL_doFadeStep()` pro Frame · `GET_TICKS()` für PAL
- Fades nicht in `state_update()` — in `main` oder dedizierten Fade-Manager
- `state_update()` setzt nur Flag: `currentState = STATE_X`

## 15. SGDK API — ERLAUBT
**VDP Text/BG:** `drawText` `drawTextBG` `drawTextEx` `clearText` `clearTextArea` `clearTextAreaBG` `setTextPlane` `setTextPalette` `setTextPriority`
**VDP Tilemap:** `setTileMapXY` `setTileMapEx` `setTileMapDataRow` `setTileMapDataRowEx` `setTileMapDataRectEx` `fillTileMapRect` `clearTileMap` `clearPlane` `drawImageEx`
**VDP Tiles:** `loadTileData` `loadTileSet` `loadFont` `fillTileData` `waitDMACompletion` `setEnable` `setScrollingMode` `setHorizontalScroll` `setVerticalScroll` `setHorizontalScrollTile` `setVerticalScrollTile` `TILE_ATTR` `TILE_ATTR_FULL`
**VRAM:** `alloc` `free` `createRegion` `clearRegion` `getFree`
**SPR:** `init` `initEx` `reset` `end` `addSprite` `setPosition` `setHFlip` `setPriority` `setDepth` `setAnim` `setFrame` `setAutoAnimation` `defragVRAM` `update` `isInitialized`
**PAL:** `setColor` `setColors` `setPaletteColors` `setPalette` `getColor` `initFade` `doFadeStep` `fade`
**JOY:** `readJoypad(JOY_1/JOY_2)` · `BUTTON_UP DOWN LEFT RIGHT A B C START`
**XGM2:** `Z80_loadDriver(Z80_DRIVER_XGM2,w)` `playPCM` `play` `stop` `isPlaying` `SOUND_PCM_CH_AUTO`
**SRAM:** `enable` `disable` `readByte` `writeByte`
**SYS:** `doVBlankProcess` `IS_PAL_SYSTEM` `GET_TICKS(n)` `getNextPow2`
**Debug:** `KLog` `KLog_U1` `KLog_U2` `KLog_U2_` `kprintf`

## 16. SGDK API — VERBOTEN
```
VDP_waitVSync              → SYS_doVBlankProcess
VDP_drawImage              → VDP_drawImageEx
VDP_drawBitmap / BMP_*     → entfernt aus SGDK
SPR_addSpriteSafe          → SPR_addSprite
SND_PCM_startPlay/stopPlay → XGM2_playPCM / XGM2_stop
XGM_startPlay/stopPlay/playPCM → XGM2_*
JOY_getJoypadValue         → JOY_readJoypad
JOY_setButtonActionHandler → manuell in update-loop
SYS_setVInt                → SYS_setVBlankCallback
allocateTileSet/TileMap/Map → VRAM_alloc
VDP_setPlaneSize           → VDP_init
GFX_*                      → VDP_* direkt
malloc / free              → MEM_alloc / statische Puffer
printf                     → kprintf / sprintf + KLog
```
