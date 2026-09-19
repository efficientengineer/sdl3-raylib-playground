"""The asset catalogue and the game's real display geometry."""
import json
import re
from pathlib import Path

import imglib as I

ROOT = I.ROOT
ATLAS = ROOT / "story/field/tilesets/valley/atlas.png"
ATLAS_JSON = json.loads((ROOT / "story/field/tilesets/valley/atlas.json").read_text())
CELL = ATLAS_JSON["cell"]          # 128 px per logical 32 px tile -> 4x
COLS = ATLAS_JSON["cols"]

PHONE = (2400, 1080)
BIG = (2560, 1440)                 # the 1440p-tall stress case
FIELD_SCALE = {"phone": 3.0, "big": 4.0}     # device px per logical px (logical view 640x360)


def stage(screen):
    """star_logic.cpp draw_scene(), landscape branch: the 16:10 stage the panels sit on."""
    w, h = screen
    sh = h * 0.98
    sw = sh * 1.6
    if sw > w * 0.98:
        sw = w * 0.98
        sh = sw / 1.6
    return sw, sh


def panel_display(rect, screen):
    sw, sh = stage(screen)
    return max(1, round(sw * rect[2] / 100.0)), max(1, round(sh * rect[3] / 100.0))


def portrait_display(aspect, screen):
    """The dialogue-box portrait: box height minus the inset, width from the image aspect."""
    sw, sh = stage(screen)
    by0, by1 = sh * 0.765, sh * 0.985
    text_size = sh * 0.052
    u = text_size * 0.12
    inset = u * 2.0
    fh = (by1 - by0) - inset * 2.0
    fw = fh * aspect
    cap = (sw * 0.92) * 0.3
    if fw > cap:
        fw = cap
        fh = fw / aspect
    return max(1, round(fw)), max(1, round(fh))


# ---------------------------------------------------------------- panels
def panel_rects():
    """file -> landscape rect (x, y, w, h) in percent of the stage."""
    src = (ROOT / "src/cutscene_data.h").read_text()
    out = {}
    for m in re.finditer(r'\{\s*"([^"]+\.png)",\s*(\d+),\s*\{([^}]*)\},\s*\{([^}]*)\}', src):
        land = [float(v.strip().rstrip("f")) for v in m.group(3).split(",")]
        out[m.group(1)] = tuple(land)
    return out


PANEL_PICKS = [
    ("face close-up", "0110_the_board_p4_portrait_inset.png"),
    ("wide establishing", "0170_the_jar_p1_establishing_wide.png"),
    ("sky / gradient", "0150_the_road_west_p1_establishing_wide.png"),
    ("object insert", "0170_the_jar_p3_object_insert.png"),
    ("two-shot", "0110_the_board_p2_two_shot.png"),
    ("eyes slit", "0170_the_jar_p4_eyes_slit.png"),
]

PORTRAIT_PICKS = ["bron.png", "lyra.png", "hart.png"]

WALKER_PICKS = [("falke", 0, "S stand"), ("falke", 1, "W stand"),
                ("ottilie", 0, "S stand"), ("villager_a", 0, "S stand")]

# atlas stamps: id -> (index, cells w, cells h)
TILE_PICKS = ["grass", "paving"]
STAMP_PICKS = ["guild_hall", "house_a", "tree"]


def atlas_box(tid):
    t = ATLAS_JSON["tiles"][tid]
    idx = t["index"]
    col, row = idx % COLS, idx // COLS
    return col * CELL, row * CELL, t["w"] * CELL, t["h"] * CELL, t["w"], t["h"]


def load_tile(tid):
    x, y, w, h, cw, chh = atlas_box(tid)
    return I.load_crop(ATLAS, x, y, w, h), cw, chh


WALK_FW, WALK_FH = 256, 384        # frame size inside a 1024x1536 sheet


def load_walk(name, row, col):
    return I.load_crop(ROOT / f"story/field/walkers/{name}.png",
                       col * WALK_FW, row * WALK_FH, WALK_FW, WALK_FH)
