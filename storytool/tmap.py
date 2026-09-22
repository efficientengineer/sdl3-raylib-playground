"""Reading and validating story/field/tmaps/<map>.tmap.

The plain-text map format in TILES.md. check_tmap validates the legend against the
tileset, every stamp's footprint and its `+` cells, overlaps, the spawn and every exit or
door target standing on a non-solid tile, every message/npc id against story/field/text.md,
and every exit's target against the .tmap files or the README's `## todo` list.

Must never do: edit a map.
Public: tmap_path, parse_tmap, check_tmap, height_grid, tmap_todo, tmap_solid_grid,
colormap_table_names, and the per-section checks. Imports: field, fieldtext, md, palette,
paths, playlist, png, tileset, tset.
"""
import json
import re
from .field import ATLAS_CELL, field_entries, sprite_art
from .fieldtext import load_field_text
from .md import die, h2_sections, slug
from .palette import COLORMAP_JSON
from .paths import FIELD_DIRS, FIELD_DOCS, FIELD_TEXT, ROOT
from .playlist import PLAYLIST, played_stems
from .png import read_png
from .tileset import tileset_entries
from .tset import ATLAS_COLS, TMAPS, atlas_path, tileset_doc



# ── tmaps ──

TRIGGER_KINDS = {"exit": 4, "door": 4, "message": 1, "npc": 3, "zone": 1, "trap": 1, "light": 2,
                 "scene": 1, "fight": 1, "pickup": 2, "goal": 1, "sprite": 1}


# 'light <radius> <level> [flicker]' is a point light as a trigger box; the same thing written as a
# 'lamp: x y radius level [flicker]' meta line is what the maps actually use. Both are checked, and
# both are looked up in the colormap's point-light table (`lamp`), not in the map's ambient table.
#
# The chapter-one four (2026-09-21, asked for by the engine side):
#   scene  <scene_id>              play a cutscene. Checked against story/playlist.md's scene stems.
#   fight  <encounter_id>          start a battle. Checked against FIGHT_IDS below.
#   pickup <item_id> <text_id>     a takeable. The text id is checked like `message`'s.
#   goal   <Words_with_underscores>  the on-screen goal line, six words at most.
#   sprite <sprite_id>            draw story/field/sprites/<id>.png as an upright billboard here.
# Any trigger line (an `npc` included) may carry a bare trailing `nightsight`: hidden and
# non-interactive until the party has night sight. A `fight` draws its encounter id as a sprite by
# itself, so it never needs a `sprite` line beside it.
FIGHT_IDS = {"post", "arm", "swing",          # hart_yard, the three training machines
             "lid",                            # halm, the grain yard
             "burr", "burr3",                  # the hill path (one burr; three burrs)
             "lantern", "fleece", "klee"}      # the high pasture, and its boss


ITEM_IDS = {"bench_part",                     # the part carried to the guild counter on day one
            "loot_yard", "loot_teaching_find", "loot_hill", "loot_pasture"}   # LOOT.md


GOAL_MAX_WORDS = 6


def tmap_path(mp):
    return TMAPS / f"{slug(mp)}.tmap"


def strip_comment(line):
    return re.sub(r"\s{2,}#.*$", "", line).rstrip()


def parse_tmap(mp):
    path = tmap_path(mp)
    if not path.exists():
        die(f"{path.relative_to(ROOT.parent)} not found")
    secs = h2_sections(path.read_text())
    for need in ("meta", "legend", "ground"):
        if need not in secs:
            die(f"{path.name}: no '## {need}' section (see WORLD.md, 'Maps')")
    meta, metas = {}, {}
    for l in secs["meta"].splitlines():
        l = strip_comment(l)
        if ":" in l:
            k, v = l.split(":", 1)
            k, v = k.strip().lower(), v.strip()
            meta[k] = v
            metas.setdefault(k, []).append(v)       # 'lamp:' is written once per point light
    legend = {}
    for l in secs["legend"].splitlines():
        l = strip_comment(l)
        if not l.strip():
            continue
        ch, rest = l[0], l[1:].strip().split()
        if not rest:
            die(f"{path.name}: legend line '{l}' names no tile")
        legend[ch] = rest[0]
    grid = lambda key: [l.rstrip("\n") for l in secs.get(key, "").splitlines()
                        if l.strip() and not l.lstrip().startswith("#")]
    lines_of = lambda key: [strip_comment(l).strip().split()
                            for l in secs.get(key, "").splitlines()
                            if strip_comment(l).strip() and not l.lstrip().startswith("#")]
    trig = lines_of("triggers")
    return {"path": path, "meta": meta, "metas": metas, "legend": legend, "ground": grid("ground"),
            "objects": grid("objects"), "height": height_grid(secs), "triggers": trig,
            # TILES2 (D20). Both are optional and both are LINE LISTS, never grids: a grid of the
            # same width as the map is a thing an author has to keep aligned, and neither of these
            # is dense enough to be worth that. `## decals` places one by hand where the scatter
            # cannot be trusted to (the flowers on a grave); `## flips` names the top-left cell of
            # a stamp placement that is drawn mirrored. A map with neither reads exactly as before.
            "decals": lines_of("decals"), "flips": lines_of("flips")}


# '## height' — the cell's surface height in VOXELS above the map base, base 36: '0'-'9' is 0..9 and
# 'a'-'z' is 10..35. A voxel is half a walk cell. '.' or a space leaves the cell unauthored and
# procedural, exactly as before, and a map with no '## height' section at all behaves as it always
# did. '~' is a bend — the mean of its authored orthogonal neighbours — parsed but not yet authored.
# `base: <n>` in '## meta' is the map floor in voxels, default 4.
#
# The size of a step between two authored cells is DELIBERATELY not checked: an authored cell is
# exempt from the engine's two-voxel neighbour clamp, so a cliff is legal and hill_path depends on
# one. Stamps and trigger boxes are flattened by the engine to their top-left cell's height.
HEIGHT_CHARS = "0123456789abcdefghijklmnopqrstuvwxyz"


HEIGHT_FREE = ". "                    # leave it to the engine's noise


HEIGHT_BEND = "~"


HEIGHT_MAX = len(HEIGHT_CHARS) - 1    # 'z'


def height_grid(secs):
    """'## height' as written, keeping blank rows: a space means procedural, so a row of them is data.

    The other grids drop blank lines, which is right for them and wrong here — an all-procedural row
    written as spaces would silently vanish and every row below it would shift up a cell. Comment
    lines still go, and the blank lines that pad the section top and bottom are trimmed."""
    rows = [l.rstrip("\n") for l in secs.get("height", "").splitlines()
            if not l.lstrip().startswith("#")]
    while rows and not rows[0].strip():
        rows.pop(0)
    while rows and not rows[-1].strip():
        rows.pop()
    return rows


def check_map_height(m, name, w, h):
    """The optional '## height' grid and the 'base:' meta key."""
    err, rows = [], m.get("height") or []
    base = (m["meta"].get("base") or "").strip()
    if base and not (base.isdigit() and 0 <= int(base) <= 64):
        err.append(f"{name}: 'base: {base}' is the floor depth in voxels under height 0; "
                   f"it must be a whole number 0 to 64")
    if not rows:
        return err                                    # no height section: all procedural, as today
    if len(rows) != h:
        err.append(f"{name}: '## height' has {len(rows)} rows, 'size:' says {h}")
    for y, line in enumerate(rows):
        line = line.ljust(w) if len(line) < w else line     # a row may stop at its last real cell
        if len(line) > w:
            err.append(f"{name}: '## height' row {y} is {len(line)} chars, 'size:' says {w}")
        for x, c in enumerate(line[:w]):
            if c not in HEIGHT_CHARS and c not in HEIGHT_FREE and c != HEIGHT_BEND:
                err.append(f"{name}: '## height' cell {x},{y} is '{c}'; it must be 0-9 or a-z "
                           f"(base 36, 0..{HEIGHT_MAX} voxels above the map base), '.' or a space "
                           f"to leave it procedural, or '{HEIGHT_BEND}' for a bend")
    return err


def tmap_todo():
    """Map names that are allowed to be missing: '## todo' in story/field/tmaps/README.md."""
    p = TMAPS / "README.md"
    if not p.exists():
        return set()
    body = h2_sections(p.read_text()).get("todo", "")
    return {slug(w) for l in body.splitlines() for w in re.findall(r"`([a-z0-9_]+)`", l)}


def tmap_solid_grid(m, entries, err):
    """(w, h, solid[y][x], stamp tops) after laying the ground and every stamp down."""
    w, h = (int(x) for x in (m["meta"].get("size") or "0 0").split()[:2]) if m["meta"].get("size") \
        else (0, 0)
    solid = [[False] * w for _ in range(h)]
    tops = []
    for y, line in enumerate(m["ground"][:h]):
        for x, c in enumerate(line[:w]):
            name = m["legend"].get(c)
            e = entries.get(name)
            if e and e["solid"].lower() in ("yes", "true", "1"):
                solid[y][x] = True
    claimed = {}
    for y, line in enumerate(m["objects"][:h]):
        for x, c in enumerate(line[:w]):
            if c in (".", "+", " "):
                continue
            name = m["legend"].get(c)
            e = entries.get(name)
            if not e:
                continue
            cw, chh = e["foot"]
            if x + cw > w or y + chh > h:
                err.append(f"stamp '{name}' at {x},{y} is {cw}x{chh} and runs off the map")
                continue
            rows = re.split(r"[\s/]+", e["solid"].strip()) if set(e["solid"]) <= set("#./ ") \
                and "#" in e["solid"] else []
            hit, notplus = [], []                # one message a stamp, not one a cell
            for r in range(chh):
                for cc in range(cw):
                    key = (x + cc, y + r)
                    if key in claimed:
                        hit.append(claimed[key])
                    claimed[key] = name
                    if (r or cc) and m["objects"][y + r][x + cc] != "+":
                        notplus.append(f"{x + cc},{y + r}")
                    blocked = e["solid"].lower() in ("yes", "true", "1")
                    if rows and r < len(rows) and cc < len(rows[r]):
                        blocked = rows[r][cc] == "#"
                    if blocked:
                        solid[y + r][x + cc] = True
            if hit:
                err.append(f"stamp '{name}' at {x},{y} ({cw}x{chh}) overlaps {', '.join(sorted(set(hit)))}")
            if notplus:
                err.append(f"stamp '{name}' at {x},{y} is {cw}x{chh}, so cell(s) "
                           f"{', '.join(notplus[:6])}{' ...' if len(notplus) > 6 else ''} must be '+' "
                           f"in '## objects'")
            tops.append((x, y, name, e))
    orphan = [f"{x},{y}" for y, line in enumerate(m["objects"][:h])
              for x, c in enumerate(line[:w]) if c == "+" and (x, y) not in claimed]
    if orphan:
        err.append(f"{len(orphan)} cell(s) marked '+' that no stamp's footprint covers: "
                   f"{', '.join(orphan[:8])}{' ...' if len(orphan) > 8 else ''}")
    return w, h, solid, tops


def colormap_table_names():
    """The tables story/palette/colormap.json holds, so a map's `light:` can be checked against them."""
    if not COLORMAP_JSON.exists():
        return []
    try:
        return [t["table"] for t in json.loads(COLORMAP_JSON.read_text()).get("tables", [])]
    except (ValueError, KeyError):
        return []


def check_map_light(m, name, w, h):
    """`light: <table> <level>` and every `lamp: x y radius level [flicker]` in a map's '## meta'.

    The engine (src/TILEFIELD_NOTES.md, "Light, as implemented") reads these; the tool used to ignore
    any meta key it did not know, so a table name with a typo or a lamp standing outside the map went
    to the phone and showed up as a black screen or nothing at all. Light is a map's *ambient plus its
    point lights*, both looked up in `story/palette/colormap.png`, so this is the one place that can
    tell the two files are still talking about the same tables."""
    err, tables = [], colormap_table_names()
    lit = (m["meta"].get("light") or "").split()
    if lit:
        if lit[0] not in tables and tables:
            err.append(f"{name}: 'light: {' '.join(lit)}' names table '{lit[0]}', which "
                       f"{COLORMAP_JSON.relative_to(ROOT.parent)} does not hold "
                       f"({', '.join(tables)}). Rebuild it, or fix the line.")
        try:
            lv = float(lit[1]) if len(lit) > 1 else 1.0
            if not 0.0 <= lv <= 1.0:
                err.append(f"{name}: 'light:' level {lv} is outside 0..1 (1.0 is full light)")
        except ValueError:
            err.append(f"{name}: 'light: {' '.join(lit)}' — the level is not a number")
    for raw in m.get("metas", {}).get("lamp", []):
        f = raw.split()
        if len(f) < 4:
            err.append(f"{name}: 'lamp: {raw}' is not 'x y radius level [flicker]'")
            continue
        try:
            x, y, r, lv = (float(v) for v in f[:4])
        except ValueError:
            err.append(f"{name}: 'lamp: {raw}' has a non-numeric field")
            continue
        if not (0 <= x < w and 0 <= y < h):
            err.append(f"{name}: lamp at {f[0]},{f[1]} is outside the {w}x{h} map")
        if r <= 0:
            err.append(f"{name}: lamp at {f[0]},{f[1]} has radius {r}; it would light nothing")
        if not 0.0 <= lv <= 1.0:
            err.append(f"{name}: lamp at {f[0]},{f[1]} has level {lv}, outside 0..1")
        if len(f) > 4 and f[4] != "flicker":
            err.append(f"{name}: 'lamp: {raw}' — the only word after the level is 'flicker'")
    return err


def check_map_decals(m, name, w, h, entries):
    """`## decals` (x y id [flip]) and `## flips` (x y) — the two optional D20 sections."""
    err = []
    for f in m.get("decals", []):
        if len(f) < 3:
            err.append(f"{name}: '## decals' line '{' '.join(f)}' is not 'x y <id> [flip]'")
            continue
        try:
            x, y = int(f[0]), int(f[1])
        except ValueError:
            err.append(f"{name}: '## decals' line '{' '.join(f)}' has a non-numeric cell")
            continue
        if not (0 <= x < w and 0 <= y < h):
            err.append(f"{name}: decal '{f[2]}' at {x},{y} is outside the {w}x{h} map")
        e = entries.get(f[2])
        if not e:
            err.append(f"{name}: '## decals' names '{f[2]}', which has no entry in "
                       f"{tileset_doc(slug(m['meta'].get('tileset', ''))).name}")
        elif e["kind"] != "decal":
            err.append(f"{name}: '## decals' names '{f[2]}', which is a {e['kind']}, not a decal")
        if len(f) > 3 and f[3] != "flip":
            err.append(f"{name}: '## decals' line '{' '.join(f)}' — the only word after the id is 'flip'")
    objs = m.get("objects") or []
    for f in m.get("flips", []):
        if len(f) != 2:
            err.append(f"{name}: '## flips' line '{' '.join(f)}' is not 'x y' (a stamp's top-left cell)")
            continue
        try:
            x, y = int(f[0]), int(f[1])
        except ValueError:
            err.append(f"{name}: '## flips' line '{' '.join(f)}' has a non-numeric cell")
            continue
        if not (0 <= x < w and 0 <= y < h) or y >= len(objs) or x >= len(objs[y]):
            err.append(f"{name}: '## flips' names {x},{y}, which is outside the {w}x{h} map")
            continue
        ch = objs[y][x]
        ident = m["legend"].get(ch)
        if ch in (".", "+", " ") or not ident:
            err.append(f"{name}: '## flips' names {x},{y}, where '## objects' has no stamp")
            continue
        e = entries.get(ident)
        if e and not e.get("flip"):
            err.append(f"{name}: '## flips' mirrors '{ident}' at {x},{y}, and that entry has no "
                       f"'- flip: h' line. A building must never be mirrored: every roof and wall "
                       f"face in the set is lit from the upper left, so a mirrored one is lit from "
                       f"the wrong side. Mirror nature, not architecture.")
    return err


def stamp_fill_warnings(m, name, entries, setname):
    """Warn when a solid stamp's art leaves too much of a solid tile see-through (owner's rule).

    A solid tile a player can see grass through looks walkable and is not, which is the single most
    annoying thing a tile map can do. Only checked where the art exists: a placeholder is exempt."""
    warn, seen = [], set()
    objs = m.get("objects") or []
    atlas = atlas_path(setname)
    if not atlas.exists():
        return warn
    try:
        aw, ah, ach, arows = read_png(atlas)
    except SystemExit:
        return warn
    cell = ATLAS_CELL
    for y, row in enumerate(objs):
        for x, ch in enumerate(row):
            ident = m["legend"].get(ch)
            e = entries.get(ident or "")
            if not e or e["kind"] != "tile" or ident in seen or e["solid"] == "no":
                continue
            seen.add(ident)
            rows_solid = tile_solid_rows(e)
            over = e.get("over_rows", 0)
            worst = (0.0, None)
            for r in range(e["h"]):
                if r < over:                                  # an `over` row is meant to be see-through
                    continue
                for c in range(e["w"]):
                    if not rows_solid[r][c]:
                        continue
                    n = e["index"] + ATLAS_COLS * r + c
                    ax, ay = (n % ATLAS_COLS) * cell, (n // ATLAS_COLS) * cell
                    if ay + cell > ah or ax + cell > aw or ach != 4:
                        continue
                    clear = sum(1 for yy in range(ay, ay + cell)
                                for xx in range(ax, ax + cell) if arows[yy][xx * 4 + 3] < 128)
                    pct = 100 * clear / (cell * cell)
                    if pct > worst[0]:
                        worst = (pct, (c, r))
            if worst[0] > 15:                                 # one line an entry, its worst tile
                c, r = worst[1]
                warn.append(f"{name}: '{ident}' is solid but its art leaves tile {c},{r} "
                            f"{worst[0]:.0f}% transparent — the player sees ground inside a tile "
                            f"they cannot walk on. Redraw that slot filling the footprint, or make "
                            f"the stamp smaller in tiles.")
    return warn


def tile_solid_rows(e):
    """[[bool]] per tile of a stamp, from `- solid: yes|no|##/.#`."""
    v = (e["solid"] or "no").strip()
    if v in ("yes", "true"):
        return [[True] * e["w"] for _ in range(e["h"])]
    if v in ("no", "false", ""):
        return [[False] * e["w"] for _ in range(e["h"])]
    rows = [r.strip() for r in v.split("/")]
    out = []
    for r in range(e["h"]):
        line = rows[r] if r < len(rows) else ""
        out.append([(c < len(line) and line[c] == "#") for c in range(e["w"])])
    return out


def check_tmap(mp):
    """(errors, warnings) for one .tmap against its tileset."""
    m, err, warn = parse_tmap(mp), [], []
    name = m["path"].name
    setname = slug(m["meta"].get("tileset", ""))
    if not setname:
        return [f"{name}: '## meta' has no 'tileset:' line"], []
    if not tileset_doc(setname).exists():
        return [f"{name}: tileset '{setname}' has no {tileset_doc(setname).relative_to(ROOT.parent)}"], []
    entries = tileset_entries(setname)
    if not m["meta"].get("size"):
        return [f"{name}: '## meta' has no 'size: W H' line"], []
    try:
        w, h = (int(x) for x in m["meta"]["size"].split()[:2])
    except ValueError:
        return [f"{name}: 'size: {m['meta']['size']}' is not 'W H'"], []
    for ch, tile in sorted(m["legend"].items()):
        if tile not in entries:
            err.append(f"{name}: legend '{ch}' is '{tile}', which has no entry in "
                       f"{tileset_doc(setname).relative_to(ROOT.parent)}")
    for key in ("ground", "objects"):
        rows = m[key]
        if key == "objects" and not rows:
            continue
        if len(rows) != h:
            err.append(f"{name}: '## {key}' has {len(rows)} rows, 'size:' says {h}")
        for y, line in enumerate(rows):
            if len(line) != w:
                err.append(f"{name}: '## {key}' row {y} is {len(line)} chars, 'size:' says {w}")
            for x, c in enumerate(line):
                if c in (".", "+", " ") and key == "objects":
                    continue
                if c not in m["legend"]:
                    err.append(f"{name}: '## {key}' cell {x},{y} is '{c}', which the legend does not name")
                elif key == "ground" and entries.get(m["legend"][c], {}).get("layer") != "ground":
                    warn.append(f"{name}: ground cell {x},{y} uses '{m['legend'][c]}', "
                                f"which is layer '{entries[m['legend'][c]]['layer']}'")
    hard = []
    w2, h2, solid, tops = tmap_solid_grid(m, entries, hard)
    err += [f"{name}: {e}" for e in hard]
    walkable = lambda x, y, what: (
        err.append(f"{name}: {what} at {x},{y} is outside the map") if not (0 <= x < w and 0 <= y < h)
        else err.append(f"{name}: {what} at {x},{y} stands on a solid tile") if solid[y][x] else None)
    sp = (m["meta"].get("spawn") or "").split()
    if len(sp) < 2:
        err.append(f"{name}: '## meta' has no 'spawn: x y [facing]' line")
    else:
        walkable(int(sp[0]), int(sp[1]), "spawn")
    err += check_map_light(m, name, w, h)
    err += check_map_height(m, name, w, h)
    err += check_map_decals(m, name, w, h, entries)
    warn += stamp_fill_warnings(m, name, entries, setname)
    text_ids = set(load_field_text())
    todo = tmap_todo()
    for t in m["triggers"]:
        if len(t) < 5:
            err.append(f"{name}: trigger '{' '.join(t)}' is not 'x y w h kind ...'")
            continue
        try:
            tx, ty, tw, th = (int(v) for v in t[:4])
        except ValueError:
            err.append(f"{name}: trigger '{' '.join(t)}' has a non-numeric box")
            continue
        kind, rest = t[4], t[5:]
        # `nightsight` is a bare trailing flag on any trigger: hidden until the party has night
        # sight. It is taken off before the arguments are counted, so it never looks like one.
        nightsight = False
        if len(rest) >= 2 and rest[-2].rstrip(":").lower() == "nightsight" \
                and rest[-1].lower() in ("yes", "true", "1"):
            rest, nightsight = rest[:-2], True             # 'nightsight: yes'
        elif rest and rest[-1].rstrip(":").lower() == "nightsight":
            rest, nightsight = rest[:-1], True             # the canonical bare trailing word
        if kind not in TRIGGER_KINDS:
            err.append(f"{name}: trigger kind '{kind}' is not one of {', '.join(sorted(TRIGGER_KINDS))}")
            continue
        if len(rest) < TRIGGER_KINDS[kind]:
            err.append(f"{name}: '{kind}' trigger at {tx},{ty} wants {TRIGGER_KINDS[kind]} argument(s), "
                       f"got {len(rest)}")
            continue
        if not (0 <= tx < w and 0 <= ty < h and tx + tw <= w and ty + th <= h):
            err.append(f"{name}: trigger box {tx},{ty} {tw}x{th} runs off the {w}x{h} map")
        if kind in ("exit", "door"):
            target = slug(rest[0])
            if not tmap_path(target).exists() and target not in todo:
                err.append(f"{name}: {kind} points at map '{target}', which has no "
                           f"{tmap_path(target).relative_to(ROOT.parent)} and is not listed under "
                           f"'## todo' in {(TMAPS / 'README.md').relative_to(ROOT.parent)}")
        elif kind == "message" and rest[0] not in text_ids:
            err.append(f"{name}: message id '{rest[0]}' has no '## {rest[0]}' entry in "
                       f"{FIELD_TEXT.relative_to(ROOT.parent)}")
        elif kind == "npc":
            if rest[2] not in text_ids:
                err.append(f"{name}: npc text id '{rest[2]}' has no entry in "
                           f"{FIELD_TEXT.relative_to(ROOT.parent)}")
            sprite = FIELD_DIRS["walker"] / f"{slug(rest[0])}.png"
            if not sprite.exists():
                warn.append(f"{name}: npc walker '{rest[0]}' has no {sprite.relative_to(ROOT.parent)} yet")
        elif kind == "scene":
            if rest[0] not in played_stems():
                err.append(f"{name}: scene trigger names '{rest[0]}', which no list in "
                           f"{PLAYLIST.relative_to(ROOT.parent)} plays")
        elif kind == "fight":
            if rest[0] not in FIGHT_IDS:
                err.append(f"{name}: fight id '{rest[0]}' is not one of {', '.join(sorted(FIGHT_IDS))}")
            elif not sprite_art(rest[0]).exists():       # a fight draws its encounter as a billboard
                warn.append(f"{name}: fight '{rest[0]}' has no "
                            f"{sprite_art(rest[0]).relative_to(ROOT.parent)} yet")
        elif kind == "pickup":
            if rest[0] not in ITEM_IDS:
                err.append(f"{name}: pickup item id '{rest[0]}' is not one of "
                           f"{', '.join(sorted(ITEM_IDS))}")
            if rest[1] not in text_ids:
                err.append(f"{name}: pickup text id '{rest[1]}' has no '## {rest[1]}' entry in "
                           f"{FIELD_TEXT.relative_to(ROOT.parent)}")
        elif kind == "sprite":
            known = field_entries(FIELD_DOCS["sprite"], "sprite") if FIELD_DOCS["sprite"].exists() else {}
            if rest[0] not in known:
                err.append(f"{name}: sprite '{rest[0]}' has no '## {rest[0]}' entry in "
                           f"{FIELD_DOCS['sprite'].relative_to(ROOT.parent)}")
            elif not sprite_art(rest[0]).exists():
                warn.append(f"{name}: sprite '{rest[0]}' has no "
                            f"{sprite_art(rest[0]).relative_to(ROOT.parent)} yet")
        elif kind == "goal":
            words = rest[0].split("_")
            if len(words) > GOAL_MAX_WORDS:
                err.append(f"{name}: goal line '{rest[0].replace('_', ' ')}' is {len(words)} words; "
                           f"{GOAL_MAX_WORDS} at most")
            if len(rest) > 1:
                err.append(f"{name}: goal takes ONE word-joined argument; write "
                           f"'{'_'.join(rest)}' as one token")
    return err, warn
