# MATCH-3-SGDK · PROJEKTINDEX FÜR KI

## 0. AUTORITÄTSREIHENFOLGE
- Wahrheit: `inc/states/states.h` → `src/main.c` → Modul-Header in `inc/` → Modul-Implementierungen in `src/`
- README ist **nicht** autoritativ
- Für Architekturfragen: Code > Kommentare > README
- Für Typen/Enums/Flags/Offsets: immer Header zuerst

## 1. ORIENTIERUNG & SUCHE
Repo-Map (wächst mit dem Projekt — nach dem ersten Build aktualisieren):
- `src/main.c` → Boot, State-Machine, Fades, Main-Loop, `config`, `sctx`, globale State-Tabelle
- `inc/states/states.h` → `GameState`, alle Context-Typen, `StateUnion`, `Serializable`, `RuntimeConfig`, `GlobalConfig`, Flags
- `src/states/save_manager.c` + `inc/states/save_manager.h` → SRAM-Lesen/Schreiben, `STATE_SAVE`, Magic/Version, Verify
- `src/sound_manager.c` + `inc/sound_manager.h` → XGM2-only Audio, Sound-Flags
- `src/sprite.c` + `inc/sprite.h` → zentrale Sprite-Engine, GameSprite, Partikel
- `src/menu_bg.c` + `inc/menu_bg.h` → BG_B-Manager, Modi, Parallax, Palette-Freeze
- `src/gfx.c` + `inc/gfx.h` → projektspezifische Tiles, Font/Palette-Init
- `src/states/game.c` → Gameplay-State-Orchestrierung
- `src/states/game/game_logic.c` → Board-Mutation, Match-Erkennung, Gravity, Combos, Spezialeffekte
- `src/states/game/game_controls.c` → Cursor-Input, Swap, D-Pad-Repeat
- `src/states/game/game_view.c` → Board-Rendering, HUD, Score-Anzeige
- `res/*.h` / `res/*.res` → Ressourcen-Compiler-Outputs / SGDK-Ressourcen

Suchstrategie:
1. Typ/Enum/Flag? → `inc/states/states.h`
2. State-Übergang/Fade/Main-Loop? → `src/main.c`
3. Persistenz? → `src/states/save_manager.c`
4. Board-Logik/Match-Erkennung? → `src/states/game/game_logic.c`
5. Rendering? → `src/states/game/game_view.c`
6. Input? → `src/states/game/game_controls.c`
7. Audio? → `src/sound_manager.c`
8. Sprites/Partikel? → `src/sprite.c`
9. BG_B/Parallax/Freeze? → `src/menu_bg.c`

## 2. BUILD/RUN
- Build-Workspace: `c:\Users\peter\Documents\_Genesis\Match3\Build`
- Tasks vorhanden:
  - `Genesis: Debug & Run (Gens KLog)`
  - `Genesis: Release & Run (BlastEm)`
  - `clean`
  - `Tile Check`
  - `Transform_csv`
  - `smartfix`
  - `py Colors`
- SGDK über `%GDK%`
- Output-ROMs:
  - `out/tetr-vibe-debug.gen`
  - `out/tetr-vibe.gen`
- Pflicht-Policy für KI nach Codeänderungen:
  - Immer Task `Genesis: Debug & Run (Gens KLog)` ausführen (inkl. Emulator-Start).
  - Build-Output im Terminal auf Fehler prüfen.
  - Bei Fehlern: Ursache beheben, Task `Genesis: Debug & Run (Gens KLog)` erneut ausführen; Schleife bis Build und Start erfolgreich.
  - Arbeit erst nach erfolgreichem Debug-Build und Emulator-Start abschließen.

## 3. STATE-MASCHINE / BOOT / KERNEL

State-Hook-Vertrag (obligatorisch für jeden State):
`init → init_draw → update → update_draw → cleanup`

`GameState` (Stand: Projektbeginn — bei neuen States hier ergänzen):
- `STATE_NONE`
- `STATE_TITLE`
- `STATE_GAME`
- `STATE_GAMEOVER`
- `STATE_HIGHSCORE`
- `STATE_OPTIONS`
- `STATE_SAVE`

State-Mapping (`src/main.c:initStateMachine`):
- `TITLE` → `title_*`
- `GAME` → `game_*`
- `GAMEOVER` → `gameover_*`
- `HIGHSCORE` → `highscore_*`
- `OPTIONS` → `options_*`
- `SAVE` → `saving_*`

State→Context:
- `TITLE` → `TitleContext`
- `GAME` → `GameContext`
- `HIGHSCORE` → `HighscoreContext`
- `OPTIONS` → `OptionsContext`
- `SAVE` → `SaveContext`
- `GAMEOVER` → kein eigener Context in `StateUnion`

State→BG-Modus:
- `TITLE` → `BG_MODE_MENU`
- `OPTIONS` → `BG_MODE_MENU`
- `HIGHSCORE` → `BG_MODE_MENU`
- `GAMEOVER` → `BG_MODE_MENU`
- `SAVE` → `BG_MODE_MENU`
- `GAME` → `BG_MODE_SPACE` (oder projekteigener Modus, noch zu definieren)

Boot (`src/main.c`):
1. `JOY_init()`
2. `sctx = MEM_alloc(sizeof(StateUnion))`
3. `memset(sctx,0,sizeof(StateUnion))`
4. PAL/NTSC Bildschirmhöhe setzen
5. Font laden, Paletten schwarz
6. `initHighscores()`
7. `initStateMachine()`
8. `menu_bg_init()`
9. `start_boot_sequence()` → Boot-Fade auf BG
10. `SOUND_init()`
11. `SOUND_playMusic()`
12. Start in `STATE_SAVE` mit `config.sramop = SRAM_INIT`

Frame-Loop (`src/main.c`):
1. `joyState = JOY_readJoypad(JOY_1)`
2. Bei State-Wechsel: Fade-Out → `cleanup` → `memset(sctx)` → `init` → `init_draw` → Fade-In
3. `states[currentState].update()`
4. `states[currentState].draw()`
5. `menu_bg_update()`
6. `update_ui_fade_freeze()`
7. `lastJoyState = joyState`
8. `SYS_doVBlankProcess()`

Fade-Policy:
- Fullscreen-Fade bei TITLE↔GAME
- UI-Fade für Menü-/Substates
- `menu_bg_set_palette_frozen(TRUE/FALSE)` schützt BG-Palette während UI-Fade
- `game` macht eigenen Start-Fade; main überspringt globalen Fade-In beim Eintritt in `STATE_GAME`

State-Wechsel-Sequenz (verbindlich):
1. `PAL_fadeOut` + `menu_bg_set_palette_frozen(TRUE)`
2. `old.cleanup()` + `menu_bg_set_mode_instant(BG_MODE_NONE)`
3. `new.init()` + `new.init_draw()`
4. `PAL_fadeIn` + `menu_bg_set_palette_frozen(FALSE)`

## 4. SPEICHER / GLOBALCONFIG / SAVE
`StateUnion` einmalig allokiert, danach nur wiederverwendet
- State-Daten niemals separat freigeben
- Jeder State bindet lokalen `ctx = &sctx-><slot>`
- State-Wechsel setzt komplette Union auf 0

`GlobalConfig`:
- Persistenter Teil: `Serializable serializable`
- Anonymer Mirror-Struct für Direktzugriff
- Nicht persistent:
  - `RuntimeConfig runtime`
  - `GameState preferredState`
  - `SramOp sramop`

Systemflags (`config.flags`):
- `FLAG_SOUND`
- `FLAG_MUSIC`
- `FLAG_BG`
- `FLAG_DEBUG`

SRAM-Layout:
- `SAVE_MAGIC` (u32) + `SAVE_VERSION` (u32) an festen Offsets
- `ADDR_MAGIC = 0x00`
- `ADDR_VERSION = 0x04`
- `ADDR_OPTIONS = 0x10`
- SRAM-Fenster: 512 Bytes

Persistiert in `Serializable` (noch zu finalisieren mit Spieldesign):
- `playerName[4]`
- `flags`
- `highscores[10]`
- `currentScore`

Nicht persistent:
- alle State-Contexts
- alle Timer/Cursor/Animationen
- `runtime.*`, `preferredState`, `sramop`

Save-Flow:
- Boot: `STATE_SAVE + SRAM_INIT` → `save_init()` → `save_load()` oder `save_clear()`
- Save: beliebiger State setzt `config.sramop = SRAM_SAVE`, `config.preferredState = STATE_X`, `currentState = STATE_SAVE`
- `save_execute()` schreibt Header + gesamten gepackten `Serializable`-Block byteweise
- `save_verify()` Pflicht; Bytevergleich RAM vs SRAM

## 5. GETEILTE MANAGER
Audio:
- nur `SOUND_init()` lädt Treiber: `Z80_loadDriver(Z80_DRIVER_XGM2, 0)`
- `SoundEntry` aligned(2)
- `SOUND_play(event)` → `XGM2_playPCM(..., SOUND_PCM_CH_AUTO)`
- Sound nur wenn `FLAG_SOUND` gesetzt
- Musik nur wenn `FLAG_MUSIC` gesetzt
- `SOUND_playMusic()` startet `DEFAULT_MUSIC_ID`
- `SOUND_stopMusic()` → `XGM2_stop()`

Sprites:
- Autorität: `inc/sprite.h` + `src/sprite.c`
- zentrale Engine-Init in `sprites_init()`
- pro Frame `sprites_update()`
- finaler Flush in main via `SPR_update()`
- `GameSprite` Felder: `Sprite* vdpSprite`, `s16 x y offsetX offsetY`, `u16 frame animation animTimer stateTimer attr`, `s16 animDir`, `u8 type padding`
- Partikel: statisches Array, `active` Flag, TTL in `sprites_update()`
- Cleanup durch Offscreen-Position `(-128,-128)`, kein `SPR_end`

BG-Manager:
- `menu_bg` ist exklusiver Besitzer von `BG_B`
- Modi: `NONE MENU SPACE` (weitere projekteigene Modi nach Bedarf)
- `menu_bg_set_mode()` = normaler Wechsel
- `menu_bg_set_mode_instant()` = harter Sofortwechsel
- `menu_bg_set_palette_frozen()` = notwendig während UI-Fades

GFX:
- `gfx_load_tiles(u16 offset)` lädt projektspezifische Gem-/Board-Tiles
- `UI_init_fonts_and_palettes()` ist Standard-UI-Init vieler Menü-States
- `SPR_init()` reserviert 420 Tiles; `menu_bg.c` rechnet diese Reserve mit ein

Rendering-Autorität:
- Board/HUD/Score → `game_view.c`
- BG_B-Hintergründe → `menu_bg.c`
- Hardware-Sprites/Effekt-Sprites → `sprite.c`

## 6. KNOWN PATTERNS / NICHT UNGEFRAGT "AUFRÄUMEN"
- `src/main.c` kann explizite Prototypen als Build-Workaround enthalten; nicht blind entfernen
- `SOUND_playMusic()` ist bewusst stub-artig; nicht stillschweigend Track-Handling erfinden
- `res/*.h` und `res/*.res` als Ressourcenebene behandeln; nicht ohne Ressourcenpipeline umstrukturieren
- Bei Projektänderungen immer Autoritätsdatei pflegen: Typen in Header, Verhalten in zugehörigem Modul
- Spieldesign-Details → `.github/copilot-instructions-match3-design.md`