"""
gen_food_sprite.py
Wandelt Food.png in food-sprite.png (128x32px, column-major Tile-Reihenfolge) um.
Frucht n: base = foodBgTileBase + (n>>2)*16 + (n&3)*4  (alle 4 Tiles konsekutiv)
"""

import os
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(SCRIPT_DIR, "..", "res", "Food.png")
DST = os.path.join(SCRIPT_DIR, "..", "res", "food-sprite.png")

TILE  = 8   # 8x8 Pixel pro Tile
FRUIT = 16  # 16x16 Pixel pro Frucht (2x2 Tiles)
COUNT = 8   # Anzahl Fruchttypen

src = Image.open(SRC).convert("RGBA")

dst = Image.new("RGBA", (128, 2 * FRUIT), (0, 0, 0, 0))

for t in range(COUNT):
    sx = t * FRUIT
    sy = 0
    dx = (t & 3) * (FRUIT * 2)
    dy = (t >> 2) * FRUIT

    tl = src.crop((sx,        sy,        sx + TILE,  sy + TILE))
    tr = src.crop((sx + TILE, sy,        sx + FRUIT, sy + TILE))
    bl = src.crop((sx,        sy + TILE, sx + TILE,  sy + FRUIT))
    br = src.crop((sx + TILE, sy + TILE, sx + FRUIT, sy + FRUIT))

    dst.paste(tl, (dx,          dy))
    dst.paste(bl, (dx + TILE,   dy))
    dst.paste(tr, (dx,          dy + TILE))
    dst.paste(br, (dx + TILE,   dy + TILE))

dst.save(DST)
print(f"Gespeichert: {DST}  ({COUNT} Fruechte, 128x{2 * FRUIT}px)")
