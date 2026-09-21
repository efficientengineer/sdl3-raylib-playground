"""story/field/manifest.md: every field art id ever asked for, and whether it exists.

Regenerated on every template run. Humans and agents read it; the engine does not.
kind_rows/report_field are the shared "what did this sheet ask for" reporting the
template commands end with.

Must never do: delete a row whose file is on disk.
Public: dedupe_ids, package_name, manifest_path, footprint_of, kind_rows, report_field,
read_manifest_rows, field_art_exists, write_field_manifest. Imports: field, package_io,
paths, png.
"""
import re
import sys
from .field import BUILDING_FACES, CELL_PX, DEFAULT_FOOTPRINT, FACE_PX, WALK_OUT_H, WALK_OUT_W, WALK_ROWS, WALK_STEPS, field_entries
from .package_io import pkg_path
from .paths import FIELD, FIELD_DIRS, FIELD_DOCS, FIELD_MANIFEST, ROOT
from .png import png_size



def dedupe_ids(ids, warn):
    out = []
    for i in ids:
        if i in out:
            warn.append(f"'{i}' listed twice; one slot is enough")
        else:
            out.append(i)
    return out


def package_name(kind, ids):
    head = "-".join(ids[:3])
    return f"{kind}_{head}" + (f"-plus{len(ids) - 3}" if len(ids) > 3 else "")


def manifest_path(name):
    return pkg_path(name, "sheet.json")


def footprint_of(i, entry, warn):
    m = re.fullmatch(r"\s*(\d+)\s*x\s*(\d+)\s*", entry.get("footprint", ""))
    if m:
        return (int(m.group(1)), int(m.group(2)))
    warn.append(f"'{i}' has no '- footprint: WxH' line; "
                f"drawn at {DEFAULT_FOOTPRINT[0]}x{DEFAULT_FOOTPRINT[1]} cells")
    return DEFAULT_FOOTPRINT


def kind_rows(kind, slots, package):
    seen, rows = set(), []
    for s in slots:
        if s["id"] in seen:
            continue
        seen.add(s["id"])
        target = (" / ".join(f"{f} {FACE_PX[f][0]}" for f in BUILDING_FACES) if kind == "building"
                  else f"{s['target'][0]}x{s['target'][1]}")
        rows.append({"id": s["id"], "kind": kind, "target": target, "package": package})
    return rows


def report_field(rows, warn, md, template, what):
    write_field_manifest(rows)
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    print(f"wrote {template.relative_to(ROOT.parent)}  ({what})")
    print(f"      {md.relative_to(ROOT.parent)}  — attach the template first, then paste the prompt")
    print(f"      {FIELD_MANIFEST.relative_to(ROOT.parent)} updated")


# ── the manifest ──

def read_manifest_rows():
    """The rows already in story/field/manifest.md: every id ever requested keeps its package name."""
    rows = {}
    if not FIELD_MANIFEST.exists():
        return rows
    for line in FIELD_MANIFEST.read_text().splitlines():
        if not line.startswith("|"):
            continue
        cells = [c.strip().strip("`") for c in line.strip().strip("|").split("|")]
        if len(cells) < 4 or cells[0] in ("id", "") or set(cells[0]) <= set("-: "):
            continue
        rows[(cells[1], cells[0])] = {"id": cells[0], "kind": cells[1], "target": cells[2], "package": cells[3]}
    return rows


def field_art_exists(kind, ident):
    """Is there a file on disk for this id?"""
    d = FIELD_DIRS.get(kind)
    return bool(d) and (d / f"{ident}.png").exists()


def write_field_manifest(new_rows=()):
    """Regenerate story/field/manifest.md: every field art id, plus whether its file is on disk.

    Two kinds now (D22/D23): the walkers and the billboard sprites. Tiles, props and building faces
    left with the systems that read them. The engine does not read this file; people and agents do."""
    rows = read_manifest_rows()
    declared = []
    walkers = field_entries(FIELD_DOCS["walker"], "walker") if FIELD_DOCS["walker"].exists() else {}
    try:
        sprites = field_entries(FIELD_DOCS["sprite"], "sprite") if FIELD_DOCS["sprite"].exists() else {}
    except SystemExit:
        sprites = {}
    for i in walkers:
        declared.append({"id": i, "kind": "walker", "package": "-",
                         "target": f"{WALK_OUT_W * len(WALK_STEPS)}x{WALK_OUT_H * len(WALK_ROWS)}"})
    for i, e in sprites.items():
        cw, chh = footprint_of(i, e, [])
        n = int((e.get("frames") or "1").strip() or 1)
        declared.append({"id": i, "kind": "sprite", "package": "-",
                         "target": f"{cw * CELL_PX * n}x{chh * CELL_PX}"
                                   + (f" ({n} frames)" if n > 1 else "")})
    for r in declared:                                   # a declared id keeps whatever package asked for it
        key = (r["kind"], r["id"])
        r["package"] = rows.get(key, {}).get("package", "-")
        rows[key] = r
    for r in new_rows:
        rows[(r["kind"], r["id"])] = dict(r)
    live = {(r["kind"], r["id"]) for r in declared} | {(r["kind"], r["id"]) for r in new_rows}
    for key in [k for k in rows if k not in live and not field_art_exists(*k)]:
        del rows[key]                                    # an id its doc has dropped, with nothing on disk

    L = ["# story/field/manifest.md — every field art id", "",
         "Generated by `./story_prompt.py walker|sprites|cut`. The engine does not read it; people and",
         "agents do. `package` is the last template sheet that asked for the id, `-` if none has yet.",
         "Sizes are the target the pipeline writes; the last column is what is actually on disk.", ""]
    for kind, title, note in (
            ("walker", "Walkers", f"alpha, rows {', '.join(WALK_ROWS)} x columns "
                                  f"{', '.join(WALK_STEPS)} of {WALK_OUT_W}x{WALK_OUT_H} frames"),
            ("sprite", "Field sprites", f"alpha billboards, {CELL_PX} px to a map cell, anchored "
                                        f"bottom-centre; several frames become one strip plus a .json")):
        group = sorted((r for (k, _), r in rows.items() if k == kind), key=lambda r: r["id"])
        L += [f"## {title} — {note}", "", "| id | kind | target | package | file |",
              "| --- | --- | --- | --- | --- |"]
        for r in group:
            f = FIELD_DIRS[kind] / f"{r['id']}.png"
            size = png_size(f) if f.exists() else None
            state = f"yes, {size[0]}x{size[1]}" if size else "missing"
            L.append(f"| `{r['id']}` | {r['kind']} | {r['target']} | `{r['package']}` | {state} |")
        if not group:
            L.append("| — | | | | |")
        L.append("")
    FIELD.mkdir(parents=True, exist_ok=True)
    FIELD_MANIFEST.write_text("\n".join(L))
