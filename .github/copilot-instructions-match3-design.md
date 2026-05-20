# MATCH-3 · SPIELDESIGN-SPEZIFIKATION

## 1. GENRE-GRUNDLAGEN (Match-3 / Tile-Matching)
Referenz: Bejeweled, Candy Crush Saga, Puzzle Quest

Kernprinzip:
- Rechteckiges Board mit farbigen Gems (Edelsteinen)
- Spieler tauscht zwei benachbarte Gems (horizontal/vertikale Nachbarn) aus
- Match = 3 oder mehr gleiche Gems in einer Reihe oder Spalte
- Gematchte Gems verschwinden; Gems darüber fallen nach unten (Gravity)
- Leere Felder oben werden mit neuen zufälligen Gems aufgefüllt
- Cascade (Kettenreaktion): entstehende neue Matches werden automatisch gelöst

## 2. BOARD & GEM-TYPEN

Board:
- Größe: 8×8 (64 Zellen) — ideal für Genesis-Auflösung und CPU-Budget
- Index-Makro: `col + (row << 3)` (Breite = Power-of-2 → Bitshift statt Multiplikation)
- Zellen: `u8 board[64]`; 0 = leer, 1..6 = Gem-Farbe, 7..= Spezial-Gems

Gem-Farben (6 Standard):
1. ROT
2. BLAU
3. GRÜN
4. GELB
5. LILA
6. ORANGE

Spezial-Gems (entstehen durch größere Matches):
- **Reihen-Bombe** (Match-4 horizontal): löscht gesamte Reihe
- **Spalten-Bombe** (Match-4 vertikal): löscht gesamte Spalte
- **Farb-Bombe** (Match-5 / L/T-Shape): löscht alle Gems einer Farbe vom Board
- **Explosion** (L- oder T-Shape Match-5): löscht 3×3-Umfeld

## 3. SPIELMODI

### 3a. ENDLESS (Endlosmodus)
- Keine Zeitbegrenzung, kein Move-Limit
- Score-Attack: so viele Punkte wie möglich
- Level steigt ab Punkteschwellen → Hintergrundmodus wechselt
- Spiel endet NICHT — Spieler kehrt mit START zum Titel zurück

### 3b. TIMED (Zeitangriff)
- Countdown-Timer (z. B. 90 Sekunden)
- Jeder Match verlängert Zeit leicht (+1s für Match-3, +2s für Match-4+)
- Bei Timer = 0 → GAMEOVER
- Ziel: maximaler Score vor Ablauf

### 3c. MOVES (Züge-Limit)
- Festes Move-Budget (z. B. 30 Züge)
- Ziel: Score-Schwelle erreichen
- Bei 0 Zügen und Score < Ziel → GAMEOVER (FAIL)
- Bei Score ≥ Ziel → Übergang zu nächstem Level / SUCCESS

## 4. SCORING

Basis:
- Match-3: 30 Punkte
- Match-4: 90 Punkte (× 3)
- Match-5+: 200 Punkte (+100 je weiterer Gem)
- Spezial-Gem aktiviert: +150 Punkte

Multiplikatoren:
- Cascade-Bonus: × Kettenglied (× 1, × 2, × 3, …)
- Combo: aufeinanderfolgende Matches ohne Lücke → × Combo-Zähler (max × 8)

Highscore: Top 10 in SRAM gespeichert (Name 3 Buchst. + Score u32)

## 5. INPUT / CURSOR-STEUERUNG

Cursor:
- D-Pad bewegt Cursor über Board (8×8)
- Cursor markiert eine Zelle
- `A` oder `C`: Swap mit Nachbar in zuletzt gedrückter Richtung
  - Alternativ: `A` zum Wählen der ersten Zelle, dann D-Pad → Swap ausführen
- Ungültiger Swap (kein Match entsteht): Gems kehren zurück (kurze Rückkehr-Animation)
- `B`: Auswahl aufheben / Pause

D-Pad-Repeat (DAS-ähnlich):
- Initial-Delay: 12 Frames
- Repeat-Delay: 4 Frames

## 6. SPIELFLUSS / STATES

```
TITLE → GAME → GAMEOVER → HIGHSCORE → TITLE
                    ↕ (nach Highscore)
              OPTIONS (aus TITLE oder GAME-Pause)
```

TITLE:
- Blinktext „PRESS START"
- Kurzes Menü: ENDLESS / TIMED / MOVES / OPTIONS / HIGHSCORE
- Idle → Highscore-State nach 600 Frames

GAME:
- Modus wird aus TITLE-Menü-Auswahl gesetzt (RuntimeConfig)
- Board-Init: zufällig befüllen, keine initialen Matches erlaubt (Pre-Check-Schleife)
- Update-Cycle: Input → Swap → Match-Check → Cascade-Loop → Board-Refill → Draw

GAMEOVER:
- zeigt Score, Modus, Spielername
- `START`/`A` → prüfe Highscore → HIGHSCORE-State

HIGHSCORE:
- Top-10-Liste
- nach Exit → TITLE

OPTIONS:
- FLAG_MUSIC, FLAG_SOUND, FLAG_BG, FLAG_DEBUG toggelbar
- `START` → Save + Return TITLE
- `B` → Return TITLE ohne Save

SAVE:
- Standard Save-Flow (SRAM_INIT / SRAM_LOAD / SRAM_SAVE)
- `preferredState` steuert Rücksprung

## 7. BOARD-LOGIK (Implementierungsrichtlinien)

Match-Erkennung:
- Nach jedem Swap: horizontale + vertikale Scan über gesamtes Board
- Floodfill-ähnlich: alle zusammenhängenden Matches einer Gruppe markieren
- Ergebnis: `u8 matchMask[64]` Bitfeld (oder separates Flag-Array)

Cascade:
```
while (matches_exist) {
    mark_matches();
    remove_marked();
    apply_gravity();       // Gems fallen nach unten
    refill_from_top();     // neue Gems oben
}
```

Gravity: Spaltenweise, von unten nach oben iterieren

Refill: pseudozufällig (`u16 rng`-Lfsr), Farbe 1..6 gleichverteilt

Pre-Check beim Board-Init: nach Befüllen auf vorhandene Matches prüfen → ggf. Shuffle-Schleife (max. 100 Iterationen)

Deadlock-Erkennung:
- Nach jedem Refill prüfen ob noch gültige Swaps existieren
- Kein gültiger Swap: Board neu mischen (mit kurzer Shuffle-Animation)

## 8. ANIMATION / VISUELLES

Sprite-Gems: 16×16 Pixel pro Gem, 6 Farben × 2 Frames (Idle-Blink)
Board-Offset: zentriert auf BG_A, ca. Pixel 32,16 (je nach Layout)
Cursor: Highlight-Rahmen (eigene Tile-Variante oder Sprite)
Match-Anim: Gems blinken 8 Frames, dann verschwinden
Cascade-Anim: kurze Verzögerung vor Gravity (16 Frames)
Swap-Anim: 8-Frame-Slide

Partikel: Gem-Burst beim Löschen — 4 Dust-Partikel je gelöschtem Gem (reuse DustParticle-Pool)

Score-Popup: kurzes Fly-Up-Text-Sprite nach Match (+30 / +90 / COMBO! etc.)

## 9. PERSISTENZ (SRAM)

Zu speichern:
- `highscores[10]` → Name (3× u8) + Score (u32) → je 7 Byte → 70 Byte
- `playerName[4]` (null-terminiert)
- `flags` (u16): MUSIC/SOUND/BG/DEBUG
- `currentScore` (u32) — nur für Continue-Funktion (Endless)

SAVE_MAGIC: projektspezifisch setzen (z. B. `0x4D433300` = „MC3\0")
SAVE_VERSION: bei Layoutänderung erhöhen

## 10. AUDIO-KONZEPT

Musik:
- 1 Endlos-BGM für TITLE
- 1 Gameplay-BGM (kann je Level-Phase wechseln)
- 1 Kurzer Jingle für GAMEOVER

SFX (Placeholder-IDs; vor Implementierung finalisieren):
- `SND_SWAP`       → Gem-Swap
- `SND_MATCH`      → Match-3 Auflösung
- `SND_MATCH_BIG`  → Match-4/5 Auflösung
- `SND_CASCADE`    → Kettenreaktion
- `SND_SPECIAL`    → Spezial-Gem aktiviert
- `SND_CURSOR`     → Cursor-Bewegung (leise)
- `SND_INVALID`    → Ungültiger Swap
- `SND_LEVEL_UP`   → Level-Aufstieg
- `SND_GAME_OVER`  → Game Over Jingle
- `SND_MENU_SEL`   → Menü-Auswahl

## 11. VRAM-PLAN (grobe Schätzung)

- SGDK Font: ~256 Tiles (reserviert oben)
- SPR_init: 420 Tiles davor
- Gem-Tiles: 6 Farben × 2 Frames × 1 Tile (16×16) = 12 Tiles
- Spezial-Gems: 4 Typen × 1 Tile = 4 Tiles
- Cursor-Tile: 1-2 Tiles
- BG_B (Starfield/Space): ~64 Tiles
- HUD-Tiles: ~10 Tiles
- Gesamt Spielinhalt: ~100 Tiles — gut im Budget

Index-Basis für Gem-Tiles: `TILE_GEM_BASE` = `TILE_USER_INDEX`

## 12. OFFENE ENTSCHEIDUNGEN (zu klären vor Implementierung)
- Swap-Mechanik: Zwei-Schritt (Wählen + Richtung) vs. Drag (Gem halten + D-Pad)
- Ob Match-4/5 Spezialeffekt sofort oder erst beim nächsten Swap aktiviert
- Anzahl Spielmodi im ersten Release (Empfehlung: Endless + Timed)
- Farb-Palette der Gems (Genesis-Farbraum: 512 Farben, 4 Paletten à 16)
- BG_A-Hintergrund für GAME-State (derzeit BG_MODE_SPACE als Platzhalter)

## 13. SPIELVERLAUF / FORTSCHRITTSLOG
- Schritt 1 abgeschlossen: Raster-basierte Sand/Frucht-Simulation stabilisiert und beschleunigt.
- Umgesetzt: Top-Down-Conway-Scan, Anti-Return-Sperre gegen unmittelbares Ping-Pong, Row-Lock-Marker für volle Unterreihen.
- Umgesetzt: Spawn-Fill auf nutzbare Top-Reihe, SGDK-FrameLoad-Anzeige aktiv, Timer-Text nur bei Zeitänderung.
- Umgesetzt: Check-Anzeige unter dem Timer (Zellprüfungen pro Frame) zur Laufzeitkontrolle.
