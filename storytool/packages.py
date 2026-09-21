"""The art tray: story/packages/, the owner's whole loop.

(Re)builds the folder tree — cast/<name>/refsheet|expressions|walker, chNN/sprites,
chNN/scenes/<scene> — each with prompt.md, sheet.json, template.png, package.json and
RETURN_HERE.md, plus the README that is the ordered to-do. A package that has been drawn
is FROZEN: once a returned image sits in it, its id list, slot boxes and template stay
exactly as they were, and new ids go to a fresh sheet beside it.

Must never do: rewrite or delete a returned.png, or re-shuffle a drawn sheet.
Public: cmd_packages, write_package, pkg_status, fingerprint and the fingerprint helpers,
prune_packages, write_packages_readme. Imports: canvas, cast, expressions, field,
manifest, md, package_io, paths, playlist, rules, scenes, sheet, sprites, stats,
walkers.
"""
import hashlib
import json
import sys
from .canvas import try_layout_cells
from .cast import alias_map, load_cast, resolve_name
from .expressions import cmd_expressions
from .field import SPRITE_SLOTS_MAX, field_entries, sprite_art
from .manifest import footprint_of
from .md import existing, slug, squash
from .package_io import RETURNED, RETURN_SUFFIXES, in_package
from .paths import FIELD_DIRS, FIELD_DOCS, PORTRAITS, ROOT, expr_portrait_path
from .playlist import PACKAGES, all_scenes, chapter_label
from .rules import EXPR_IDS
from .scenes import panel_file
from .sheet import cmd_refsheet, cmd_sheet
from .sprites import cmd_sprites, sprite_frames
from .stats import refsheet_priority, run_quiet
from .walkers import cmd_walker



PKG_META = "package.json"          # the machine index in every package folder; `ingest` reads it


INGEST_KEYS = ("cut_fingerprint",)  # keys `ingest` writes, which a `packages` rebuild must not erase


def cast_in_play(scenes, cast):
    """Cast handles that the selected scenes actually use, in the order they first turn up."""
    aliases, order = alias_map(cast), []
    for _, sc in scenes:
        for written in sc["characters"] + [w for w, _ in sc["dialogue"]]:
            handle = resolve_name(written, cast, aliases)
            if handle and handle not in order:
                order.append(handle)
    return order


def split_field_ids(ids, sizes):
    """Group ids into sheets that lay out with usable slots. ([[id...]], [(id, why it never fits)]).

    An id may want several slots — a building wants three — so an entry in `sizes` is either one
    (w, h) box or a list of them, and a group is never split through the middle of an id."""
    boxes = [s if isinstance(s, list) else [s] for s in sizes]
    groups, bad, cur, cur_boxes = [], [], [], []
    for i, bs in zip(ids, boxes):
        best, why = try_layout_cells(bs)
        if best is None:
            bad.append((i, why))
            continue
        if cur and try_layout_cells(cur_boxes + bs)[0] is None:
            groups.append(cur)
            cur, cur_boxes = [], []
        cur.append(i)
        cur_boxes += bs
    if cur:
        groups.append(cur)
    return groups, bad


def frozen_packages(here, dirname, kind, ids):
    """[(folder, ids)] for packages of this kind that have already been drawn, so their slots are fixed.

    Once the owner has generated a sheet, its template and its slot boxes are the only thing that can
    cut the image they got back. Adding an id to `tiles.md` must therefore never re-shuffle that
    package: the drawn ones keep exactly the ids they were built with, and everything new goes into a
    fresh `tiles_2`. A package counts as drawn when a returned image is sitting in it, or when every
    file it makes is already on disk."""
    out = []
    for d in sorted(here.glob(f"{dirname}*")) if here.is_dir() else []:
        mf = d / PKG_META
        if not mf.exists():
            continue
        try:
            meta = json.loads(mf.read_text())
        except ValueError:
            continue
        was = meta.get("ids") or []
        if meta.get("kind") != kind or not was or any(i not in ids for i in was):
            continue                                     # not ours, or an id it drew has since gone
        if pkg_returned(d) or (meta.get("outputs") and
                               all((ROOT.parent / o).exists() for o in meta["outputs"])):
            out.append((d.name, was))
    return out


def next_package_name(dirname, used):
    if dirname not in used:
        return dirname
    n = 2
    while f"{dirname}_{n}" in used:
        n += 1
    return f"{dirname}_{n}"


def pkg_returned(d):
    """The image the owner saved in a package folder, newest first, or None."""
    files = [p for p in d.iterdir() if p.stem == RETURNED and p.suffix.lower() in RETURN_SUFFIXES] \
        if d.is_dir() else []
    return max(files, key=lambda p: p.stat().st_mtime) if files else None


def pkg_state(d, meta):
    """(status word, files cut, files the package makes) — the same answer for README and prompt.md."""
    outs = [ROOT.parent / o for o in meta.get("outputs", [])]
    have = [o for o in outs if o.exists()]
    ret = pkg_returned(d)
    if ret:
        stale = len(have) < len(outs) or any(o.stat().st_mtime < ret.stat().st_mtime for o in have)
        if stale:
            return "**image waiting** — run ingest", len(have), len(outs)
    if outs and len(have) == len(outs):
        return "done", len(have), len(outs)
    if have:
        return f"{len(have)}/{len(outs)} cut", len(have), len(outs)
    return "to generate", 0, len(outs)


def write_return_here(d, meta):
    rel = d.relative_to(ROOT.parent)
    outs = "\n".join(f"- `{o}`" for o in meta.get("outputs", [])) or "- (nothing listed)"
    if meta.get("kind") == "screen":
        (d / "RETURN_HERE.md").write_text(f"""# {meta['title']} — save every image here

One chat, one message per image: the map, then the walkable mask. `prompt.md` in this folder has the
prompts in order and each one says which name to save under.

1. `{RETURNED}.png` — the top-down map
2. `{RETURNED}_walk.png` — the walkable mask (green on black)
3. `{RETURNED}_over.png` — the overhead mask, **only** if `prompt.md` has a third message

`ingest` needs every image `prompt.md` asks for and refuses until they are all here. Then, from the repository root:

```
./story_prompt.py ingest {rel}
```

Add `--debug` for a picture of what the tool understood. That writes:

{outs}

Nothing here is ever deleted, so a bad cut is redone by fixing and rerunning, and a regeneration is
just saving the new image over the old one and running `ingest` again.
""")
        return
    (d / "RETURN_HERE.md").write_text(f"""# {meta['title']} — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `{rel}/{RETURNED}.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest {rel}
```

That cuts the image into:

{outs}

`{RETURNED}.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
""")


def write_package(d, meta, builder, args):
    """Build one package into its folder, then drop the machine index and the return note.

    A folder that already holds a returned image is FROZEN: its `template.png` and `sheet.json` are
    put back exactly as they were after the rebuild, so the image the owner generated can always be
    cut again. Only the prose — prompt.md, RETURN_HERE.md — is refreshed. Without this, a change to a
    template's layout (the walk sheet going from 16 frames to 9) would silently invalidate every
    image already sitting in the tray."""
    d.mkdir(parents=True, exist_ok=True)
    frozen = {}
    if pkg_returned(d):
        for f in ("template.png", "sheet.json"):
            if (d / f).exists():
                frozen[f] = (d / f).read_bytes()
    with in_package(d, meta.get("title")):
        ok, msg = run_quiet(builder, args)
    for f, blob in frozen.items():
        (d / f).write_bytes(blob)
    if not ok:
        return False, msg or "no package written"
    # KEEP what `ingest` wrote. `meta` is rebuilt from the source files every run and knows nothing
    # about what has been cut, so writing it straight over package.json erased `cut_fingerprint` and
    # the staleness check could never fire: `packages` would quietly forget, every single run, the
    # one fact it needs to tell the owner their art is out of date.
    kept = {}
    if (d / PKG_META).exists():
        try:
            kept = {k: v for k, v in json.loads((d / PKG_META).read_text()).items()
                    if k in INGEST_KEYS}
        except ValueError:
            pass
    (d / PKG_META).write_text(json.dumps({**meta, **kept, "dir": str(d.relative_to(ROOT.parent))},
                                         indent=2) + "\n")
    write_return_here(d, meta)
    return True, ""


# ── the inputs fingerprint: what makes a package STALE ──
#
# A package is stale when the art in it was cut from inputs that have since changed: a character's
# `look` line rewritten, a scene's panels rewritten, a sprite's slot list changed. Guessing that
# from mtimes is unreliable (every `packages` run rewrites the prompt), so the fingerprint of the
# inputs is written into package.json when the package is BUILT, and again under `cut_fingerprint`
# when `ingest` successfully cuts it. Different -> stale. This is computed, never guessed.
#
# Packages cut before fingerprints existed have no `cut_fingerprint`, and there is no honest way to
# recover one. LEGACY_STATUS is the ruling on those few, by hand, from the facts.

LEGACY_STATUS = {
    # ("refsheet", "bron") WAS ruled stale here — the old red-haired armoured design against a `look`
    # line that is now green-haired in a quilted ochre training vest. The owner redrew it through the
    # tray on 2026-09-21 and it matches; the ruling is gone and its `cut_fingerprint` is stamped, so
    # it reads `done` and is computed from here on like everything else.
    ("walker", "falke"): ("stale", "cut from the PREVIOUS reference sheet — the red-haired armoured "
                                   "design. Falke's sheet has since been redrawn, so this one is "
                                   "now the wrong character"),
    ("walker", "ottilie"): ("stale", "cut from the reference sheet as it was before the weapon came "
                                     "off her `look` line"),
    ("refsheet", "lyra"): ("keep", "exists and is good — only a weapon was taken off the `look` "
                                   "line. Regenerate ONLY if you want the mace gone from the "
                                   "full-body panel; the portrait is unaffected"),
}


def fingerprint(*parts):
    """A short stable hash of whatever produced a package. Order matters; whitespace does not."""
    h = hashlib.sha256()
    for part in parts:
        h.update(squash(str(part)).encode())
        h.update(b"\x00")
    return h.hexdigest()[:16]


def cast_fingerprint(c):
    """A character's design, as the refsheet, expression and walker prompts see it."""
    return fingerprint(c["display"], c.get("look", ""), c.get("look_v3_day_one", ""),
                       c.get("asymmetric", ""))


def scene_fingerprint(sc):
    """A shot sheet's inputs: the staging, the beat, and every panel with its acting lines."""
    meta = sc.get("meta", {})
    return fingerprint(meta.get("staging", ""), sc.get("beat", ""), meta.get("location", ""),
                       meta.get("characters", ""),
                       *[f"{sid}|{desc}" for sid, desc in sc["panels"]])


def sprite_fingerprint(ids, entries):
    """A sprite sheet's inputs: exactly the slots it asks for, in order."""
    return fingerprint(*[f"{i}|{entries[i]['desc']}|{entries[i].get('footprint', '')}"
                         f"|{entries[i].get('frames', '1')}|{entries[i].get('frame2', '')}"
                         f"|{entries[i].get('frame3', '')}" for i in ids])


def pkg_status(d, meta):
    """(word, why, cut, total) — the one status the README, the prompt and ArtTray all read.

    'done' / 'stale' / 'image waiting' / 'N/M cut' / 'to generate', and for stale a one-line reason,
    which is the whole point: the owner must never be told to redo something without being told why.
    """
    label, have, total = pkg_state(d, meta)
    if label != "done":
        return label, "", have, total
    # `meta` is rebuilt from the source files on every run, so it carries the fingerprint of the
    # inputs AS THEY ARE NOW and never a `cut_fingerprint` — that one is written by `ingest` and
    # lives only in package.json. Read it from there, or this comparison is always None and every
    # drawn package reports `done` for ever.
    want, got = meta.get("fingerprint"), pkg_cut_fingerprint(d)
    if want and got:
        return ("done" if want == got else "stale",
                "" if want == got else "the description it was drawn from has changed since",
                have, total)
    ruling, why = LEGACY_STATUS.get((meta.get("kind"), slug(meta.get("handle") or "")), (None, ""))
    if ruling == "stale":
        return "stale", why, have, total
    if ruling == "keep":
        return "exists", why, have, total
    return "done", "", have, total


def pkg_cut_fingerprint(d):
    """The fingerprint `ingest` last cut this package at, off disk. None if it has never been cut."""
    mf = d / PKG_META
    if not mf.exists():
        return None
    try:
        return json.loads(mf.read_text()).get("cut_fingerprint")
    except ValueError:
        return None


def record_cut(d, meta):
    """`ingest` stamps the fingerprint it cut at, so the next change to the inputs shows as stale."""
    mf = d / PKG_META
    if not mf.exists() or not meta.get("fingerprint"):
        return
    body = json.loads(mf.read_text())
    body["cut_fingerprint"] = meta["fingerprint"]
    mf.write_text(json.dumps(body, indent=2) + "\n")


# ── the tray ──

# Packages the tool no longer generates but must never delete: the valley tileset sheets. Their
# outputs (atlas.png, the decals, the swatches) are read by the voxel field every frame, and these
# folders hold the only templates that can cut the archived returns in story/sheets/ again if the
# palette is ever refitted. They are frozen: drawn, done, and off the to-do list.
FROZEN_ROOTS = ("tilesets",)


def is_frozen(d):
    rel = d.relative_to(PACKAGES).parts
    return bool(rel) and rel[0] in FROZEN_ROOTS


def cap_sheets(ids, boxes, cap):
    """Split `ids` into as FEW sheets as the cap allows, then balance the slots across them.

    Greedy filling to the cap leaves a runt: eighteen slots at eight a sheet is 8, 8, 2, and the
    two-slot sheet wastes a whole generation on one creature drawn enormous. Taking the sheet count
    first and dividing evenly gives 6, 6, 6 instead."""
    total = sum(len(boxes[i]) for i in ids)
    sheets = max(1, -(-total // cap))
    want = -(-total // sheets)
    out, cur, used = [], [], 0
    for i in ids:
        if cur and used + len(boxes[i]) > want and len(out) < sheets - 1:
            out.append(cur)
            cur, used = [], 0
        cur.append(i)
        used += len(boxes[i])
    if cur:
        out.append(cur)
    return out


def cmd_packages(args):
    """Rebuild story/packages/ as the folder tree the owner works through.

    Five kinds and nothing else, because the voxel world (D22/D23) needs nothing else: the ground,
    the walls and the houses are palette colours, shader detail and rule-built geometry, with no
    generated art at all. What is left to draw is people and the things that move."""
    every = "--all" in args
    cast = load_cast()
    scenes = all_scenes(args)
    PACKAGES.mkdir(parents=True, exist_ok=True)
    index, failures, notes, built = [], [], [], set()

    def add(d, meta, builder, bargs, **row):
        ok, msg = write_package(d, meta, builder, bargs)
        built.add(d.resolve())
        if not ok:
            failures.append((str(d.relative_to(ROOT.parent)), msg))
        index.append({"dir": d, "meta": meta, "ok": ok, **row})

    # ── 1-3. the cast: a reference sheet, then the faces, then the walk sprite ──
    # A folder is named for what the character is called NOW, not for the '## handle' behind it,
    # because the maps name walkers the same way (`hart`, `falke`) and the owner reads the tree.
    handles = cast_in_play(scenes, cast) or ([h for h, _ in refsheet_priority(cast)] if every else [])
    if every:
        handles += [h for h, _ in refsheet_priority(cast) if h not in handles]
    # Only a character who SPEAKS needs the ten faces: the expression sheet is the dialogue box's,
    # and somebody who is only drawn in a panel never has a portrait on screen.
    aliases_now = alias_map(cast)
    speakers = {h for _, sc in scenes for w, _ in sc["dialogue"]
                if (h := resolve_name(w, cast, aliases_now))}
    for handle in handles:
        c = cast[handle]
        who, key, fp = c["display"], slug(c["display"]), cast_fingerprint(c)
        ref = c.get("ref") or f"story/refs/{handle}.png"
        add(PACKAGES / "cast" / key / "refsheet",
            {"kind": "refsheet", "handle": handle, "title": f"{who} — reference sheet", "ref": ref,
             "fingerprint": fp,
             "makes": f"`{ref}` and the dialogue-box portrait cut out of it",
             "outputs": [ref, str((PORTRAITS / f"{handle}.png").relative_to(ROOT.parent))]},
            cmd_refsheet, [handle], group="refsheet", who=who)
        if handle in speakers or every:
            outs = [str(expr_portrait_path(handle, e).relative_to(ROOT.parent)) for e in EXPR_IDS]
            meta = {"kind": "expressions", "handle": handle, "title": f"{who} — expression sheet",
                    "fingerprint": fp,
                    "makes": f"{len(EXPR_IDS)} dialogue-box faces in `story/portraits/`",
                    "outputs": outs}
            d = PACKAGES / "cast" / key / "expressions"
            if existing(ref):                # the prompt attaches the reference sheet, as the walker does
                add(d, meta, cmd_expressions, [who], group="expressions", who=who)
            else:
                index.append({"dir": d, "meta": meta, "ok": False, "group": "expressions", "who": who,
                              "blocked": "needs the reference sheet first"})
        walker_out = str((FIELD_DIRS["walker"] / f"{key}.png").relative_to(ROOT.parent))
        meta = {"kind": "walker", "handle": key, "title": f"{who} — walk sheet", "fingerprint": fp,
                "makes": f"`{walker_out}`, the 9-frame walk sprite sheet", "outputs": [walker_out]}
        d = PACKAGES / "cast" / key / "walker"
        if existing(ref):
            add(d, meta, cmd_walker, [who], group="walker", who=who)
        else:                                    # the walker prompt attaches the reference sheet
            index.append({"dir": d, "meta": meta, "ok": False, "group": "walker", "who": who,
                          "blocked": "needs the reference sheet first"})
    for ident, e in (field_entries(FIELD_DOCS["walker"], "walker")
                     if FIELD_DOCS["walker"].exists() else {}).items():
        owner = resolve_name(ident, cast)
        if owner:                                # the cast file has taken this name over since
            notes.append(f"story/field/walkers.md '## {ident}' is now {cast[owner]['display']} in "
                         f"characters.md; the cast entry wins and the walkers.md entry is ignored")
            continue
        out = str((FIELD_DIRS["walker"] / f"{ident}.png").relative_to(ROOT.parent))
        add(PACKAGES / "cast" / ident / "walker",
            {"kind": "walker", "handle": ident, "title": f"{ident} — walk sheet (no cast entry)",
             "fingerprint": fingerprint(ident, e.get("look", ""), e["desc"]),
             "makes": f"`{out}`, the 9-frame walk sprite sheet", "outputs": [out]},
            cmd_walker, [ident], group="walker", who=ident)

    # ── 4. the field sprites: every billboard the voxel world stands up ──
    ch_label = chapter_label(min([ch for ch, _ in scenes], default=1)).split()[0]
    try:
        sprites = field_entries(FIELD_DOCS["sprite"], "sprite") if FIELD_DOCS["sprite"].exists() else {}
    except SystemExit as exc:
        sprites = {}
        failures.append((str(FIELD_DOCS["sprite"].relative_to(ROOT.parent)), squash(str(exc.code))))
    if sprites:
        here = PACKAGES / ch_label
        ids = sorted(sprites)
        done, taken = frozen_packages(here, "sprites", "sprites", ids), set()
        for _, keep in done:
            taken |= set(keep)
        rest = [i for i in ids if i not in taken]
        warn = []
        boxes = {i: [footprint_of(i, sprites[i], warn)] * sprite_frames(i, sprites[i], warn)[0]
                 for i in rest}
        groups, bad = split_field_ids(rest, [boxes[i] for i in rest])
        groups = [c for g in groups for c in cap_sheets(g, boxes, SPRITE_SLOTS_MAX)]
        for i, why in bad:
            failures.append((f"sprite {i}", why))
        plan = list(done)
        for g in groups:                             # a new id never disturbs a sheet already drawn
            plan.append((next_package_name("sprites", {n for n, _ in plan}), g))
        for n, (folder, group) in enumerate(plan, 1):
            outs = [str(sprite_art(i).relative_to(ROOT.parent)) for i in group]
            add(here / folder,
                {"kind": "sprites", "ids": list(group),
                 "fingerprint": sprite_fingerprint(list(group), sprites),
                 "title": f"field sprites — {len(group)} billboard(s)"
                          + (f" ({n} of {len(plan)})" if len(plan) > 1 else ""),
                 "makes": ", ".join(f"`{o}`" for o in outs), "outputs": outs},
                cmd_sprites, list(group), group="sprites")

    # ── 5. one shot sheet per panel scene ──
    # Flat under the chapter: the voxel world has no per-map art any more, so filing a scene under
    # the map it happens to be played on bought nothing but two extra folders to click through.
    for ch, sc in [(ch, sc) for ch, sc in scenes if sc["kind"] == "panels" and sc["panels"]]:
        d = PACKAGES / chapter_label(ch).split()[0] / "scenes" / sc["stem"]
        outs = [str(panel_file(sc, n, sid).relative_to(ROOT.parent))
                for n, (sid, _) in enumerate(sc["panels"], 1)]
        add(d, {"kind": "sheet", "scene": sc["stem"], "title": f"{sc['stem']} — shot sheet",
                "fingerprint": scene_fingerprint(sc),
                "makes": f"{len(outs)} panel image(s) in `story/panels/`", "outputs": outs},
            cmd_sheet, [str(sc["path"])], group="scene", chapter=ch, scene=sc)

    orphans, frozen, removed = prune_packages(built)
    write_packages_readme(index, frozen, failures, notes, every, scenes, cast)
    print(f"wrote {len(built)} package folder(s) under {PACKAGES.relative_to(ROOT.parent)}/"
          + (f", removed {removed} legacy folder(s)" if removed else ""))
    for g, label in (("refsheet", "reference sheets"), ("expressions", "expression sheets"),
                     ("walker", "walk sheets"), ("sprites", "field sprite sheets"),
                     ("scene", "scene shot sheets")):
        n = sum(1 for r in index if r["group"] == g and r["ok"])
        blocked = sum(1 for r in index if r["group"] == g and r.get("blocked"))
        if n or blocked:
            print(f"  {n:>3} {label}" + (f"   ({blocked} waiting on a reference sheet)" if blocked else ""))
    if frozen:
        print(f"  {len(frozen):>3} frozen, already drawn (the valley tileset) — left exactly as they are")
    print(f"index: {(PACKAGES / 'README.md').relative_to(ROOT.parent)}   "
          f"(then: generate -> save as {RETURNED}.png -> ./story_prompt.py ingest)")
    if orphans:
        print(f"{len(orphans)} folder(s) no longer generated were kept because they hold a "
              f"{RETURNED} image; they are listed at the end of the README.", file=sys.stderr)
    if failures:
        print(f"\n{len(failures)} package(s) could not be built:", file=sys.stderr)
        for what, why in failures:
            print(f"  {what}: {why}", file=sys.stderr)


def prune_packages(built):
    """Drop package folders the tool no longer generates. ([kept orphans], [frozen], removed count).

    A folder under FROZEN_ROOTS is never touched. Everything else that this run did not write is a
    package of a system the game does not have any more — the old tiles, props, buildings, painted
    screens and painted views, the retired scenes' shot sheets, a cast member who has left the
    chapter — and it goes, `returned.png` and all. The raw generations those folders were cut from
    are archived in story/sheets/, and the whole pre-cleanup tree is on the `legacy-final` tag, so
    nothing here is the last copy of anything."""
    orphans, frozen, removed = [], [], 0
    for meta in sorted(PACKAGES.rglob(PKG_META)):
        d = meta.parent
        if is_frozen(d):
            frozen.append(d)
            continue
        if d.resolve() in built:
            continue
        for p in sorted(d.rglob("*"), reverse=True):
            p.unlink() if p.is_file() else p.rmdir()
        d.rmdir()
        removed += 1
    for legacy in PACKAGES.glob("*.chatgpt.md"):          # the flat layout this tree replaced
        legacy.unlink()
    for d in sorted(PACKAGES.rglob("*"), reverse=True):   # tidy the empty shells left behind
        if d.is_dir() and not any(d.iterdir()) and not is_frozen(d):
            d.rmdir()
    return orphans, frozen, removed


def write_packages_readme(index, frozen, failures, notes, every, scenes, cast):
    """The tray's front page: a count, then ONE ordered to-do, then the detail.

    It used to open with a 'short path' section and then repeat everything under six more headings,
    which meant a package appeared three times with three statuses. There is one list now, in the
    order the work unblocks itself, and every row says done, stale (and why), or to do."""
    rel = lambda d: str(d.relative_to(PACKAGES))
    link = lambda r: (f"[`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md)" if r["ok"]
                      else f"`{rel(r['dir'])}`")

    def row_status(r):
        if r.get("blocked"):
            return "blocked", r["blocked"]
        if not r["ok"]:
            return "broken", "could not be built — see the command output"
        word, why, have, total = pkg_status(r["dir"], r["meta"])
        return word, why or (f"{have} of {total} files cut" if word.endswith("cut") else "")

    GROUPS = [
        ("refsheet", "Reference sheets",
         "Do these first. The dialogue portrait, the ten faces, the walk sprite and every scene "
         "panel are all drawn from this one image."),
        ("expressions", "Expression sheets",
         "Ten head-and-shoulders faces a speaking character, one template sheet: `neutral`, `smile`, "
         "`laugh`, `biglaugh`, `concern`, `sorrow`, `annoyed`, `angry`, `shock`, `resolve`. A line "
         "picks one with `- Name (biglaugh): text`. Blocked until the reference sheet exists."),
        ("walker", "Walk sprites",
         "The nine-frame sheet the field animates (rows S, side, N; the engine mirrors the side "
         "row). Blocked until the reference sheet exists, because the walker must match it."),
        ("sprites", "Field sprites",
         "The voxel world's billboards: the creatures, the animals and the training machines, cut "
         "out against magenta and stood upright in the world. The ground, the walls and the houses "
         "need no art at all — they are palette colours, shader detail and rule-built geometry."),
        ("scene", "Scene shot sheets",
         "One sheet per panel scene: all of its panels as separate white-bordered rectangles on "
         "black, cut apart into `story/panels/`. Talk scenes need no art and are not listed. A "
         "scene with no art still plays: every panel is a placeholder box with its description."),
    ]
    ordered = [r for g, _, _ in GROUPS for r in index if r["group"] == g]
    todo = [r for r in ordered if row_status(r)[0] in ("to generate", "blocked", "stale")]
    waiting = [r for r in ordered if row_status(r)[0].startswith("**image")]
    done = [r for r in ordered if row_status(r)[0] in ("done", "exists")]
    stale = [r for r in ordered if row_status(r)[0] == "stale"]

    L = ["# story/packages — the art tray", "",
         f"**{len(todo)} to do"
         + (f" ({len(stale)} of them a redraw of art that has gone stale)" if stale else "")
         + f", {len(done)} already drawn and finished"
         + (f", {len(waiting)} waiting to be cut" if waiting else "") + ".**", "",
         "Generated by `./story_prompt.py packages`. Every folder below is one ChatGPT generation.",
         "Nothing here is edited by hand; rerun the command after any change to a scene file,",
         "`characters.md`, `story/field/sprites.md` or `STYLE.md`. Rerunning never touches an image",
         "you have saved.", "",
         "## The loop", "",
         "1. Open a folder and read `prompt.md`: the files to attach, in order, and the prompt to paste.",
         "2. Generate in ChatGPT, checking the result against the checklist at the end of `prompt.md`.",
         f"3. Save the image into that same folder as `{RETURNED}.png`.",
         "4. From the repository root: `./story_prompt.py ingest` — it finds every waiting image and",
         "   cuts each one into its panels, sprites or portraits, and says what it wrote.", "",
         "**Nothing goes to the phone unless you ask for it.**", "",
         "## Status words", "",
         "| word | what it means |", "| --- | --- |",
         "| `to generate` | nothing drawn yet |",
         "| `done` | every file exists and the description it was drawn from has not changed |",
         "| `stale` | the files exist but an input changed since — the row says which |",
         "| `exists` | drawn, and good enough; redo it only if the note says something you want |",
         "| `image waiting` | you saved a `returned.png`; run `ingest` |",
         "| `blocked` | waiting on the reference sheet above it |", "",
         f"Scenes come from `story/playlist.md`" + ("" if every else " — a scene file no chapter list "
         "names gets no package") + f"; {len(scenes)} scene(s) selected.", "",
         "## The queue", "",
         "In the order the work unblocks itself. Work down it.", ""]

    n = 0
    for group, heading, blurb in GROUPS:
        rows = [r for r in index if r["group"] == group]
        L += [f"### {len(L) and ''}{heading}", "", blurb, "",
              "| # | what | folder | status | why / next |", "| --- | --- | --- | --- | --- |"]
        for r in rows:
            n += 1
            word, why = row_status(r)
            what = r.get("who") or (r["scene"]["stem"] if r.get("scene") else r["meta"]["title"])
            after = why or ("—" if word in ("done", "exists")
                            else "rerun `packages` once the sheet is in" if word == "blocked"
                            else f"`ingest {rel(r['dir'])}`")
            L.append(f"| {n} | {what} | {link(r)} | {word} | {after} |")
        if not rows:
            L.append("| — | | | nothing in this group | |")
        L.append("")

    if frozen:
        L += ["## Frozen — already drawn, do not redo", "",
              "The valley tileset sheets. The voxel field reads their `atlas.png`, `decals/` and",
              "`swatches/` every frame, and these folders hold the only templates that could cut the",
              "archived returns in `story/sheets/` again if the palette is ever refitted. They are",
              "not regenerated, not pruned, and not on the queue above. **There is no work here.**", ""]
        L += [f"- `{rel(d)}`" for d in sorted(frozen)] + [""]

    if notes:
        L += ["## Notes", ""] + [f"- {x}" for x in notes] + [""]
    if failures:
        L += ["## Could not be built", "",
              "A validation error in a scene file or a field description, not a missing image.",
              "Fix the source file and rerun `packages`.", ""]
        L += [f"- `{what}` — {why}" for what, why in failures] + [""]
    (PACKAGES / "README.md").write_text("\n".join(L))
