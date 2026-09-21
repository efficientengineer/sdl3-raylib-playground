"""Assembling a field-art package: the attachments, the rules block, the write.

write_field_package is what every template command (sprites, walker, expressions) ends
with: the template PNG, the prompt built from STYLE.md's locked blocks plus the per-slot
descriptions, and the template rules. palette_attachment puts the master swatch last on
every sheet, with the one line telling the generator to use only those colours.

Must never do: write a prompt sentence that is not either generated from data or quoted
from STYLE.md.
Public: field_rendering, field_attachments, palette_attachment, field_status,
write_field_package, field_after, TEMPLATE_RULES. Imports: canvas, field, md,
package_io, palette, paths, png.
"""
import json
from .canvas import draw_template
from .field import SLOT_BORDER
from .md import existing
from .package_io import package, pkg_path, return_lines
from .palette import MASTER_SWATCH
from .paths import OUT, ROOT
from .png import png_size, write_png



# ── the package ──

def field_rendering(blocks):
    """The rendering block for field art: `field_rendering` if STYLE.md has one, else `rendering`.

    They differ in one sentence. The cutscene block asks for checkerboard dithering in skies, walls
    and shadows, which was right when a panel was an RGB image of its own; under D19 every field
    image is one 256-colour palette and flat cel tones, and a dithered wall becomes two colours
    fighting on a tile that then repeats across a map. The panel blocks are untouched."""
    return blocks.get("field_rendering") or blocks["rendering"]


def field_attachments(style, template, ref, warn):
    """[(path, note)] in attach order: the template, the style reference, then a character sheet."""
    out = [(template, "TEMPLATE. Redraw this exact image with every numbered slot filled in and "
                      "everything else left untouched. It is the canvas, not a reference.")]
    # The field's style reference is OUR OWN approved test sheet, not the third-party screenshot the
    # cutscene path uses: the owner picked that look (flat luminous colour, big simple shapes, soft
    # painted edges, sparse detail, cool blue-violet shadows) off a sheet we generated, so the safest
    # thing to hand the generator is the sheet it already agreed to.
    sp = existing(style["refs"].get("style_map")) or existing(style["refs"].get("style"))
    if sp:
        out.append((sp, "STYLE reference. " + (style["refs"].get("style_note_map")
                                               or style["refs"].get("style_note", ""))))
    else:
        warn.append(f"no style reference at {style['refs'].get('style')}; the prompt text carries the style alone")
    if ref:
        out.append(ref)
    pa = palette_attachment(warn)
    if pa:
        out.append(pa)
    return out


PALETTE_NOTE = ("the game's COLOUR PALETTE, one material ramp a row: use only these colours.")


def palette_attachment(warn=None):
    """The master swatch, attached LAST to every package whose art shares the field's palette (D19).

    Last on purpose: it is not a reference for what to draw, it is the set of colours to draw it in,
    and a generator takes the last image as the most recent instruction. A package drawn before this
    existed keeps working — the slots do not move, only the prompt gains a line."""
    if not MASTER_SWATCH.exists():
        if warn is not None:
            warn.append(f"no palette swatch at {MASTER_SWATCH.relative_to(ROOT.parent)}; "
                        f"run: ./story_prompt.py palette build")
        return None
    return (MASTER_SWATCH, PALETTE_NOTE)


TEMPLATE_RULES = (
    "TEMPLATE RULES: Return the whole template at the same size and proportions as the image I "
    "attached. Every white slot border and every slot number stays exactly where it is, the same size "
    "and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is "
    "left as it is: the margins, the gutters between the slots, and the background behind the numbers. "
    "Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the "
    "slots. No text, no labels, no captions, no arrows, no colour swatches, no signature."
)


def field_status(kind, slots, out_file=None):
    """'What exists already' for a field package: one line per file the cut will write."""
    if out_file:
        p = ROOT.parent / out_file
        return [f"- `{out_file}` — " + (f"**exists**, {png_size(p)[0]}x{png_size(p)[1]}" if p.exists() else "missing")]
    seen, L = set(), []
    for s in slots:
        if s["out"] in seen:
            continue
        seen.add(s["out"])
        p = ROOT.parent / s["out"]
        L.append(f"- slot {s['n']} `{s['out']}` — " + ("**exists**" if p.exists() else "missing"))
    have = sum(1 for l in L if "exists" in l)
    return [f"{have} of {len(L)} files in this package have been cut already.", ""] + L


def write_field_package(kind, name, canvas_label, W, H, bg, slots, prompt, attach, after, out_file=None,
                        status=(), digit=None, fill=None):
    OUT.mkdir(exist_ok=True)
    template = pkg_path(name, "template.png")
    write_png(template, W, H, 3, draw_template(W, H, bg, slots, digit, fill))
    manifest = pkg_path(name, "sheet.json")
    manifest.write_text(json.dumps({
        "template": name, "kind": kind, "canvas": [W, H], "canvas_label": canvas_label,
        "background": list(bg), "border": SLOT_BORDER, "out": out_file, "slots": slots, "prompt": prompt,
        **({"slot_fill": list(fill)} if fill else {}),
    }, indent=2) + "\n")
    md = pkg_path(name, "chatgpt.md")
    md.write_text(package(f"{kind} template {name}", attach, prompt, after, status=status))
    return template, manifest, md


def field_after(manifest, lines, makes="the files listed above"):
    return lines + [""] + (return_lines(makes) or
                           ["When it passes, download the image and cut it up:", "", "```",
                            f"./story_prompt.py cut {manifest.relative_to(ROOT.parent)} ~/Downloads/<file>.png",
                            "```"])
