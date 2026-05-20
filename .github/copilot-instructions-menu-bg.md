# MENU_BG · KI-REGELWERK (PROJEKTSPEZIFISCH)

## 0. AUTORITÄT
- Primär: `src/menu_bg.c`
- API: `inc/menu_bg.h`
- Asset-Quellen: `bg.h`
- Gameplay-Koordinaten für Riistar-Reaktion: `inc/states/game/game_core.h`
- Globalflags/Fadesystem: `inc/states/states.h`, `src/main.c`

## 1. PUBLIC API (VERTRAG)
- `menu_bg_init()`
- `menu_bg_set_mode(u8 mode)`
- `menu_bg_set_mode_instant(u8 mode)`
- `menu_bg_set_active(bool active)` [Legacy-Wrapper]
- `menu_bg_update()`
- `menu_bg_get_base_color(void)`
- `menu_bg_set_palette_frozen(bool frozen)`
- `menu_bg_set_intensity(u8 intensity)`
- `menu_bg_riistar_set_stack_top(s16 topRow)`
- `menu_bg_riistar_pulse(u8 pulseType)`

## 2. MODE-ENUM
- `BG_MODE_NONE`
- `BG_MODE_MENU`
- `BG_MODE_SPACE`
- `BG_MODE_RIISTAR`
- `BG_MODE_CLUB`

## 3. KONSTANTEN (KANONISCH)
- Tile-Base:
  - `TILE_MENU_BLOCK_BASE`
  - `TILE_STAR_BASE`
  - `SPRITE_VRAM_RESERVE_TILES`
- Motion:
  - `BG_SPEED_BASE`
  - `BG_ACCEL`
  - `F16_0`
- Riistar/Club Spring:
  - `RIISTAR_SPRING_K`
  - `RIISTAR_DAMPING`
  - `RIISTAR_PULSE_SOFT`
  - `RIISTAR_PULSE_SOFTDROP`
  - `RIISTAR_PULSE_HARDDROP`
- Club:
  - `CLUB_SCROLL_STEP`
  - `CLUB_SCROLL_MAX_X`
  - `CLUB_SCROLL_Y_OFFSET`
- Colors:
  - `COL_GRAY`
  - `COL_YELLOW`
  - `COL_RED`
  - `MENU_BG_COLOR`
- Space:
  - `STARFIELD_BASE_SP`

## 4. GLOBALE STATISCHE DATEN (IST-ZUSTAND)
Allokationen in aktueller Implementierung:
- `scroll_x`, `sub_y`
- `curr_dx`, `curr_dy`
- `target_dx`, `target_dy`
- `change_timer`
- `is_active`
- `bg_mode`
- `pending_bg_mode`
- `fade_level`
- `is_fading`, `is_flashing`
- `has_pending_mode_switch`
- `palette_frozen`
- `current_intensity`
- `flash_level`
- `riistar_group_scroll[5]`
- `riistar_hscroll_lines[240]`
- `riistar_map_row_scroll[32]`
- `bg_has_line_parallax`
- `bg_dynamic_palette`
- `club_map`
- `club_scroll_x`
- `club_scroll_diff`
- `club_vertical_center`
- `club_vertical_max`
- `club_vertical_offset`
- `club_debug_scroll_px`
- `riistar_vertical_center`
- `riistar_vertical_amplitude`
- `riistar_vertical_max`
- `riistar_y_offset`
- `riistar_y_velocity`
- `riistar_speed_scale`
- `riistar_stack_top_row`
- `riistar_group_sizes[5]`
- `riistar_group_speeds[5]`
- `target_palette[8]`

## 5. FUNKTIONEN (INTERN)
- `apply_current_mode_state()`
- `draw_riistar_background()`
- `draw_club_background()`
- `reset_riistar_scroll()`
- `update_riistar_scroll_lines(s16 verticalScrollPx)`
- `update_palette_fade()`
- `update_star_tiles(u8 intensity)`
- `draw_stars_dynamic(u8 intensity)`
- `draw_menu_shapes()`
- `draw_club_debug_info()`

## 6. FUNKTIONSVERHALTEN
### 6.1 `menu_bg_init()`
- Setzt PAL0 Base-Color auf `MENU_BG_COLOR`
- Baut 7 Tetromino-Blocktiles in `TILE_MENU_BLOCK_BASE + [0..6]`
- Initialisiert Space-Star-Tiles via `update_star_tiles(1)`

### 6.2 `menu_bg_set_mode()`
- Wenn identischer Mode ohne Pending-Switch: no-op
- Wenn Pending-Switch aktiv:
  - gleicher Zielmode wie `bg_mode` → Pending abbrechen
  - sonst nur `pending_bg_mode` überschreiben
- Wechsel aus `BG_MODE_NONE`:
  - sofort aktivieren ohne Out-Fade
  - `fade_level = 1.0`
  - `apply_current_mode_state()`
- Wechsel aus aktivem Mode:
  - `pending_bg_mode = mode`
  - `has_pending_mode_switch = true`
  - `is_active = false`
  - `is_fading = true`

### 6.3 `menu_bg_set_mode_instant()`
- Bricht Pending-State komplett ab
- Bei `BG_MODE_NONE`:
  - deaktiviert, löscht BG_B, setzt PAL0 auf schwarz
- Sonst:
  - aktiviert sofort mit `fade_level = 1.0`
  - `apply_current_mode_state()`

### 6.4 `menu_bg_set_active(bool)`
- Legacy-Wrapper:
  - `true` → `BG_MODE_MENU`
  - `false` → `BG_MODE_NONE`

### 6.5 `menu_bg_set_intensity(u8)`
- Nur aktiv in `BG_MODE_SPACE`
- Clamp auf `[1..10]`
- Trigger-Flash bei Resetpfad (`level==1` nach hoher Intensität)
- Aktualisiert `target_dy`, Star-Tiles, Star-Verteilung und ggf. Palette

### 6.6 `menu_bg_update()`
- Läuft wenn aktiv oder Fade/Flash noch ausläuft
- Mode-spezifisches Motion-Update
- Scrolling-Ausgabe an VDP/MAP
- Flash-Decay (`flash_level -= 0.04`)
- Fade-In/Out-Progress (`±0.05`)
- Bei Fade-Out-Ende + Pending:
  - `bg_mode = pending_bg_mode`
  - `apply_current_mode_state()` oder kompletter OFF-Zustand

### 6.7 `menu_bg_riistar_set_stack_top(s16)`
- Akzeptiert `-1` oder `[0..19]`
- Berechnet `riistar_speed_scale` aus Fill-Height (`BOARD_HEIGHT - topRow`)
- Clamp auf `[0..1]`

### 6.8 `menu_bg_riistar_pulse(u8)`
- Nur für `BG_MODE_RIISTAR` oder `BG_MODE_CLUB`
- Nur wenn aktiv
- Puls-Impulse auf `riistar_y_velocity`:
  - type1 soft
  - type2 softdrop
  - type3 harddrop

## 7. MODE-SPEZIFISCHE LOGIK
### 7.1 MENU
- Zufalls-Tetrominohintergrund (`draw_menu_shapes`)
- Smooth drift via `target_dx/target_dy` und `BG_ACCEL`
- Zielrichtung zyklisch neu (`change_timer`)

### 7.2 SPACE
- Sternhintergrund (`draw_stars_dynamic`)
- Intensität 1..10 steuert Sternlänge, Anzahl, vertikales Drift-Target
- Kanalbasierte Farbampel (grau/gelb/rot)
- optionaler Weißflash

### 7.3 RIISTAR
- lädt `riistar_bg_tileset` + `riistar_bg_tilemap`
- berechnet sicheren Tile-Base mit Sprite-Reserve
- line-parallax aktiv (`HSCROLL_LINE`)
- gruppenbasierte Horizontalscroll-Lagen (5 Gruppen)
- federndes Vertikaloffset (`riistar_y_offset`, `riistar_y_velocity`)
- gameplay-getriebene Geschwindigkeitskopplung (`riistar_speed_scale`)

### 7.4 CLUB
- lädt `club_bg_tileset` + `MAP_create(club_bg_map,...)`
- `MAP_scrollTo` mit fix32-X und federndem Y
- horizontal ping-pong scroll
- Debug-Overlay in BG_A (`draw_club_debug_info`)
- line-parallax aus (`HSCROLL_PLANE`)

### 7.5 NONE
- Hintergrund aus
- Plane gelöscht
- Palette-Basis schwarz

## 8. PALETTE-/FADE-VERTRAG
- `palette_frozen == true` blockiert `update_palette_fade()` vollständig
- CLUB nutzt eigene Palette und überspringt Runtime-Fadepfad
- MENU/SPACE/RIISTAR skalieren Zielpalette mit `fade_level`
- `is_flashing` hat Vorrang vor normaler Farbe im SPACE-Mode
- Fade-Rampen sind framebasiert in `menu_bg_update` integriert

## 9. VRAM-/MAP-OWNERSHIP
- `menu_bg` ist exklusiver Schreiber von BG_B
- CLUB hält `Map* club_map` und released diese beim Modewechsel
- Riistar/Club berechnen `tileBase` im sicheren User-Bereich
- `SPRITE_VRAM_RESERVE_TILES` wird in Tilebase-Berechnung berücksichtigt

## 11. OWNERSHIP & GRENZEN
- **EXKLUSIV:** `menu_bg` verwaltet nur `BG_B` (Background Plane B) — niemals `BG_A`
- **TABU:** Kein `VDP_setHorizontalScroll(..., BG_A, ...)`, keine `MAP_create(..., BG_A, ...)`
- `BG_A` bleibt vollständig unter Kontrolle des Spielstatus/Gameplay
- Union-Felder (`menuBg.mode.*`) dürfen **nur** in ihren jeweiligen Mode-Blöcken manipuliert werden
- Keine Mode-unabhängigen Writes auf Union-Branches (z. B. niemals `target_dx = F16_0` außerhalb MENU)
- **KRITISCH:** Bei Mode-Wechsel `memset(&menuBg.mode, 0, ...)` obligatorisch — Union hat keine "Destruktoren", alte Daten bleiben sonst!
- **Wichtig:** Felder, die über **mehrere Modi** hinweg benutzt werden, bleiben außerhalb der Union (shared state):
  - `target_dx`, `target_dy` — Drift-Ziele für MENU + SPACE
  - `scroll_x`, `sub_y` — gemeinsame Scroll-Ausgabe
  - `curr_dx`, `curr_dy` — gemeinsame Geschwindigkeit

## 12. UMBAUZIEL (UNION-BASIERT)
Pflichtziel für Refactor:
- Allgemeiner Zustand getrennt von mode-spezifischem Zustand
- mode-spezifische Daten in Union kapseln, z. B.:
  - MENU: `change_timer` (Mode-spezifisch — nur MENU benutzt es)
  - SPACE: `current_intensity`, `is_flashing`, `flash_level` (Mode-spezifisch — nur SPACE)
  - RIISTAR: Parallax-Arrays, Spring, Stack-Kopplung (Mode-spezifisch — nur RIISTAR)
  - CLUB: Map-Pointer, fix32-Scroll, Club-Y-Spring (Mode-spezifisch — nur CLUB)
- **Shared (außerhalb Union):** Felder, die über Modi hinweg gelten:
  - `target_dx`, `target_dy` — Drift-Ziele für MENU + SPACE
  - `scroll_x`, `sub_y`, `curr_dx`, `curr_dy` — gemeinsame Scroll-Ausgabe
  - `fade_level`, `is_fading`, `palette_frozen` — Fade-Kontrolle
  - `is_active`, `bg_mode`, `pending_bg_mode`, `has_pending_mode_switch` — State-Verwaltung

## 13. REFRACTOR-GUARDRAILS
- keine API-Änderung in `inc/menu_bg.h`
- visuelles Verhalten muss identisch bleiben
- Fadelogik und Pending-Mode-Transitions semantisch unverändert
- CLUB-Map-Lifecycle (`MAP_release`) beibehalten
- Riistar-Puls/Stack-API semantisch unverändert
- keine Änderung an State-Aufrufern außerhalb von `menu_bg.*`
- alle `typedef struct/union/enum` und `#define`-Konstanten ausschließlich in `inc/menu_bg.h` — nie in der `.c`-Datei
- **Ausnahme:** Bridge-Makros (`#define scroll_x (menuBg.scroll_x)` usw.), die auf die datei-interne `static MenuBgState menuBg` zeigen,
  dürfen in `menu_bg.c` verbleiben — sie referenzieren eine nicht-exportierte statische Variable und haben im Header keinen Kontext
- **Kritisch:** Union-Mitgliedschaft muss korrekt sein: Nur Fields, die **ausschließlich von einem Mode** verwendet werden, gehören in die Union
  - Kontra-Check: Wenn ein Feld in mehreren if(bg_mode == ...) Blöcken benutzt wird, **muss es shared sein**

## 14. VOLLSTÄNDIGKEITSCHECK (CHECKLIST)
- alle Public-API-Funktionen dokumentiert
- alle internen Funktionen dokumentiert
- alle aktuellen statischen Variablen benannt
- alle fünf Modi mit Ablauf beschrieben
- Palette/Fade/VRAM/Map-Lifecycle beschrieben
- Umbauziel + Guardrails definiert
