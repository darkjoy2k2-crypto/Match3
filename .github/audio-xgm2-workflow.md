# Generative XGM2 Music Engine v3.5 — SGDK / Mega Drive
# Handout: Finales Arrangement-Zielbild (Hülsbeck-Hymne + PCM-Samples)
# Stand: 2026-04-12

## 1. Architektur & Stil
* **Stil:** Cinematic Technoid-orchestral (Chris Hülsbeck) – 64-Bar-Hymnen.
* **Melodie-Basis:** Heroisches Motiv aus MIDI-Vorlage (Start: C4 -> G4 -> Eb4). KEINE Tetris-Abwandlung.
* **Pipeline:** Python (MIDI-Logic) -> VGM (PCM-Embedded) -> xgm2tool -> XGM2 -> C-Header.
* **Integration:** Direkte ROM-Einbindung via Hex-Arrays (kein rescomp für Musik).

## 2. Hardware-Layout (Optimiert für PCM-Drums)

### YM2612 (FM) — 6 Kanäle
| Ch | Rolle         | Technik                                      |
|----|---------------|----------------------------------------------|
| 0  | Lead-Melodie  | Hymnische Lead-Stimme (MIDI-basiert), Alg. 4 |
| 1  | Software-Echo | Kopie Ch0 (+5 Frames Delay, TL +10)          |
| 2  | Orch. Pads    | Harmonische Schichtung / Big-Band Stabs      |
| 3  | Drive-Bass    | Dynamische Bassline, synchron zu PCM-Kick    |
| 4  | Orch. Layer 1 | Frei für Brass-Stabs / tiefe Streicher       |
| 5  | Orch. Layer 2 | Frei für Pauken / zusätzliche Harmonien      |

### SN76489 (PSG) — 3 Square + 1 Noise
| Ch | Rolle         | Technik                                      |
|----|---------------|----------------------------------------------|
| 0  | Hülsbeck Arp 1| 50Hz Chord-Tracking (1 Note/Frame)           |
| 1  | Hülsbeck Arp 2| Counter-Arpeggio / Melodische Kaskaden       |
| 3  | Noise Layer   | Weißes Rauschen parallel zu PCM-Hi-Hat       |

### PCM (XGM2-Mixer) — Sample-IDs (Source: /samples/*.wav)
| ID   | Datei       | Rolle                                      |
|------|-------------|--------------------------------------------|
| 0x01 | kick.wav    | Hard-Hitting PCM Kick                      |
| 0x02 | snare.wav   | Snare Backbeat                             |
| 0x03 | hihat.wav   | Metallische 16tel Hi-Hats                  |
| 0x04 | tom.wav     | Orchestrale Tom-Fills am Phrasen-Ende      |

## 3. PAL-Timing & Loop-Präzision

### Anti-Drift (Floating-Point Accumulation)
* **Regel:** Berechne Frames absolut: `int(step * (3000 / BPM))`.
* **Ziel:** Verhindert BPM-Drift über 64 Takte (besonders kritisch bei 60-70 BPM).

### Zero-Latency Loop
* **Fix:** Der letzte Frame (`Total_Frames - 1`) darf keinen WAIT-Befehl im VGM enthalten.
* **Effekt:** Nahtloser Übergang ohne rhythmischen Stolperer zurück zu Frame 0.

## 4. Das 64-Bar Arrangement (Narrative Struktur)
1. **Bars 01–08 (Intro):** Sphärische PSG-Arpeggios und leise FM2-Pads.
2. **Bars 08–24 (The Hymn):** Das Hauptthema aus der MIDI-Vorlage (C-Dorisch). Voller PCM-Beat.
3. **Bars 24–40 (Variation B):** Modulation nach Eb-Dur. Bassline spielt Oktav-Galopp.
4. **Bars 40–48 (Techno Bridge):** Fokus auf PSG-Textur. FM-Kanäle gedämpft. Rein technoid.
5. **Bars 48–56 (Heroic Solo):** Virtuose Lead-Variante (eine Oktave höher) mit 16tel-Läufen.
6. **Bars 56–64 (Grand Finale):** Maximale Schichtung. Hymne + Brass-Layering auf FM 4/5.
7. **Loop:** Nahtloser Übergang zurück zum atmosphärischen Intro.

## 5. Invarianten (Hard Constraints)
1. **Alignment:** Jedes XGM2-Array benötigt `__attribute__((aligned(256)))`.
2. **Drum-Source:** Percussion läuft rein über PCM; FM 4/5 bleiben frei für Orchestrierung.
3. **Wait-Logic:** In `render_vgm_v2` muss gelten: `if frame < render_frames - 1: writer.wait_frames(1)`.
4. **Echo-Delay:** Festgelegt auf 5 Frames (100ms) für maximale räumliche Tiefe.
5. **PSG-Arps:** Müssen strikt den Harmonien folgen; keine willkürlichen Intervalle.

## 6. Build & Deployment
* **Generator:** `py/sound/build_sf_style_pack_xgm2.py`
* **C-Header:** `inc/music_data.h` (Definition der `MUSIC_*_SIZE` Makros)
* **C-Source:** `src/music_data.c` (Definition der Arrays mit Alignment)