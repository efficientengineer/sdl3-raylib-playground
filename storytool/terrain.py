"""Parsing the terrain, decal and plain-tile entries of a tileset's tiles.md.

TILES2 (D20): a terrain is one seamless swatch whose edges are a generated 1-bit mask, a
decal is a small cut-out the game hash-scatters, and anything else is a tile or a stamp.
This module only reads the entries; baking the masks is masks.py and cutting the swatches
is tset_cut.py.

Must never do: touch an image.
Public: parse_terrain, parse_decal, parse_passmask, set_terrains, set_decals.
Imports: md, tset.
"""
import re
from .md import die
from .tset import EDGE_STYLES, SWATCH_MAX, SWATCH_MIN, parse_wh



# ── the new tiles.md keys ──

def parse_terrain(ident, kv, e, where):
    """`- kind: terrain`: a swatch, a priority, an edge style, optionally a border band."""
    e["priority"] = int(kv.get("priority", kv.get("fringe", 0)))          # `fringe:` was the old name
    style = (kv.get("edge_style") or "ragged").strip().lower()
    if style == "shore":
        style = "bank"                                                   # the proposal's older word
    if style not in EDGE_STYLES:
        die(f"{where}: '## {ident}' has '- edge_style: {style}'; "
            f"use one of {', '.join(sorted(EDGE_STYLES))}")
    e["edge_style"] = style
    sw = (kv.get("swatch") or "3x3").strip()
    w, h = parse_wh(sw, f"{where} '{ident}' swatch")
    if not (SWATCH_MIN <= w <= SWATCH_MAX and SWATCH_MIN <= h <= SWATCH_MAX):
        die(f"{where}: '## {ident}' has '- swatch: {sw}'; each side is {SWATCH_MIN}..{SWATCH_MAX} tiles")
    e["swatch"] = (w, h)
    e["border"] = None
    if kv.get("border"):
        f = kv["border"].split()
        if len(f) != 2:
            die(f"{where}: '## {ident}' has '- border: {kv['border']}'; "
                f"write 'border: <terrain-or-colour> <width in tiles>'")
        try:
            e["border"] = (f[0], float(f[1]))
        except ValueError:
            die(f"{where}: '## {ident}' border width '{f[1]}' is not a number")
    e["drift"] = float(kv.get("drift", 0.0))
    e["cycle"] = (kv.get("cycle") or "").strip() or None
    return e


def parse_decal(ident, kv, e, where):
    """`- kind: decal`: what it may sit on, and how the hash scatter is shaped."""
    on = [s.strip() for s in (kv.get("on") or "").split(",") if s.strip()]
    if not on:
        die(f"{where}: '## {ident}' is a decal with no '- on: <terrain>[, ...]' line")
    e["on"] = on
    e["density"] = float(kv.get("density", 0.4))
    e["cluster"] = float(kv.get("cluster", 4))
    e["sizes"] = [float(v) for v in (kv.get("sizes") or "0.8 1.0 1.25").split()]
    e["flip"] = "h" in (kv.get("flip") or "").lower()
    e["edge_bias"] = {}
    for part in (kv.get("edge_bias") or "").split(","):
        f = part.split()
        if len(f) == 2:
            try:
                e["edge_bias"][f[0]] = float(f[1])
            except ValueError:
                die(f"{where}: '## {ident}' has edge_bias '{part.strip()}', not '<terrain> <factor>'")
        elif part.strip():
            die(f"{where}: '## {ident}' has edge_bias '{part.strip()}', not '<terrain> <factor>'")
    e["tiles"] = float(kv.get("size_tiles", 0.75))
    return e


def parse_passmask(text, ident, where):
    """`- pass: NESW` — the sides of a tile that may be walked THROUGH, as the open letters.

    A counter you talk across, a ledge you drop off one way, a wall you walk along: `- solid:` says
    whether the tile blocks at all, this says which of its four sides do."""
    text = (text or "").strip().upper()
    if not text or text in ("ALL", "NESW"):
        return "NESW"
    if text in ("NONE", "-"):
        return ""
    if not re.fullmatch(r"[NESW]+", text) or len(set(text)) != len(text):
        die(f"{where}: '## {ident}' has '- pass: {text}'; write the open sides as letters from "
            f"N, E, S, W (each at most once), or 'none'")
    return "".join(c for c in "NESW" if c in text)


def set_terrains(entries):
    """The set's terrains in draw order: priority low first, then by name. Ties never overlay."""
    t = [i for i, e in entries.items() if e.get("kind") == "terrain"]
    return sorted(t, key=lambda i: (entries[i]["priority"], i))


def set_decals(entries):
    return sorted(i for i, e in entries.items() if e.get("kind") == "decal")
