"""`ingest`: the other half of the loop — cut every returned image waiting in the tray.

Walks story/packages/, finds every returned.* newer than what it makes, runs the right
cutter, and archives the raw image into story/sheets/ under the package's path first,
because returned.png itself is gitignored scratch. One line per package, a summary, and a
non-zero exit if any failed. The returned file is NEVER deleted, so a bad cut is redone by
fixing and rerunning.

Must never do: delete a returned image, or swallow a LOUD warning.
Public: to_png, archive_returned, ingest_one, cmd_ingest. Imports: cast, cutcmd, field,
keying, md, package_io, packages, palette, paths, playlist, portraits, stats.
"""
from pathlib import Path
import json
import subprocess
import sys
from .cast import load_cast
from .cutcmd import cmd_cut, cmd_slice
from .field import FIELD_KINDS
from .keying import LOUD
from .md import die, squash
from .package_io import RETURNED, RETURN_SUFFIXES
from .packages import PKG_META, pkg_returned, pkg_state, record_cut, stamp_status
from .palette import PAL_TIMING
from .paths import ROOT, SHEETS
from .playlist import PACKAGES
from .portraits import portrait_from_ref
from .stats import run_quiet



# ───────────────────────── Ingest: one command for every returned image ─────────────────────────

def to_png(src, dst):
    """Copy a returned image to dst as a PNG, converting through sips when it is not one already."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    if src.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n":
        dst.write_bytes(src.read_bytes())
        return
    try:
        subprocess.run(["sips", "-s", "format", "png", str(src), "--out", str(dst)],
                       check=True, capture_output=True)
    except (OSError, subprocess.CalledProcessError):
        die(f"{src.name} is not a PNG and sips could not convert it.")


def archive_returned(d, returned):
    """Keep the raw generation in story/sheets/, which is where raw generations live and is tracked.

    The package folder's own returned.png is gitignored scratch: it can be overwritten by the next
    attempt and thrown away with the folder. The archive copy is named for the package's path so a
    sheet can always be traced back to what asked for it, and re-cut years later."""
    rel = d.relative_to(PACKAGES) if d.is_relative_to(PACKAGES) else Path(d.name)
    out = SHEETS / (str(rel).replace("/", "-") + ".png")
    SHEETS.mkdir(exist_ok=True)
    if out.exists() and out.read_bytes() == returned.read_bytes():
        return None                                          # same image, already kept
    to_png(returned, out)
    for other in sorted(d.glob(f"{RETURNED}_*")):         # a screen returns three images, not one
        if other.suffix.lower() in RETURN_SUFFIXES:
            to_png(other, SHEETS / (str(rel).replace("/", "-") + other.stem[len(RETURNED):] + ".png"))
    return out


def ingest_one(d, meta, returned, extra=()):
    """Run the right cutter for one package. (ok, message)."""
    kind = meta.get("kind")
    if kind == "sheet":
        return run_quiet(cmd_slice, [str(d / "sheet.json"), str(returned)])
    # `tileset`, `swatch` and `decal` are the tile era's: no package generates them any more, but the
    # cutter stays so the frozen valley sheets in story/packages/tilesets/ can be cut again from the
    # archived returns if the palette is ever refitted (WORLD.md, "What the world is made of").
    if kind in FIELD_KINDS or kind in ("tileset", "swatch", "decal", "expressions"):
        return run_quiet(cmd_cut, [str(d / "sheet.json"), str(returned), *extra])
    if kind == "refsheet":
        handle = meta["handle"]
        try:
            to_png(returned, ROOT.parent / meta["ref"])          # the sheet itself is the deliverable
            cast = load_cast()
            if handle not in cast:
                return True, f"saved {meta['ref']}; no characters.md entry, so no portrait"
            ok, msg = portrait_from_ref(handle, cast[handle])    # then the dialogue-box portrait out of it
        except SystemExit as e:
            return False, squash(str(e.code))
        except Exception as e:
            return False, f"{type(e).__name__}: {e}"
        return (True, f"saved {meta['ref']}; {msg}") if ok else (False, f"saved {meta['ref']}, but {msg}")
    return False, f"unknown package kind '{kind}'"


def cmd_ingest(args):
    """Cut every image the owner has saved into story/packages/ since the last run."""
    force, extra, skip = "--force" in args, [], False
    for a in args:                                       # --fringe N is handed to the cutter as it stands
        if skip:
            extra.append(a)
            skip = False
        elif a in ("--fringe", "--palette"):
            extra.append(a)
            skip = True
        elif a in ("--nearest", "--debug", "--no-heal", "--neutral-main"):
            extra.append(a)
    roots = [Path(a).expanduser().resolve() for a in args
             if not a.startswith("--") and a not in extra] or [PACKAGES]
    metas = []
    for r in roots:
        if not r.exists():
            die(f"{r} not found. Give a package folder under {PACKAGES.relative_to(ROOT.parent)}/, "
                f"or no path at all to do them all.")
        metas += [r / PKG_META] if (r / PKG_META).exists() else sorted(r.rglob(PKG_META))
    if not metas:
        die(f"no package folders under {', '.join(str(r) for r in roots)}. Run: ./story_prompt.py packages")

    done, failed, waiting, fresh, loud = 0, 0, 0, 0, 0
    for mf in metas:
        d, meta = mf.parent, json.loads(mf.read_text())
        rel = d.relative_to(ROOT.parent) if d.is_relative_to(ROOT.parent) else d
        returned = pkg_returned(d)
        if not returned:
            waiting += 1
            continue
        status, have, total = pkg_state(d, meta)
        if not force and status == "done":
            fresh += 1
            continue
        kept = archive_returned(d, returned)
        LOUD.clear()
        ok, msg = ingest_one(d, meta, returned, extra)
        line = squash(msg) if msg else ""
        if ok:
            done += 1
            record_cut(d, meta)        # stamp what it was cut from, so a later edit reads as stale
            stamp_status(d, meta)      # and the status the tray and the README both read back
            after, have, total = pkg_state(d, meta)
            print(f"ok    {rel}  ({returned.name} -> {have}/{total} file(s)){'  ' + line if line else ''}"
                  + (f"  [kept {kept.relative_to(ROOT.parent)}]" if kept else ""))
        else:
            failed += 1
            print(f"FAIL  {rel}  {line}", file=sys.stderr)
        for w in LOUD:                                   # never let a cutter's warning vanish into the capture
            print(f"      ! {w}", file=sys.stderr)
        loud += len(LOUD)
    if PAL_TIMING:
        secs = sum(t for _, _, t in PAL_TIMING)
        worst = max(PAL_TIMING, key=lambda r: r[2])
        print(f"\npalette: {len(PAL_TIMING)} file(s) converted to indexed art in {secs:.1f}s "
              f"(slowest {worst[0]}, {worst[1] / 1e6:.2f} MP in {worst[2]:.1f}s)")
    print(f"\n{done} cut, {failed} failed, {fresh} already up to date, {waiting} still waiting for an image "
          f"({len(metas)} package(s))" + (f", {loud} warning(s) above" if loud else ""))
    if done:
        print("The game picks the new art up on the next ./fast_reload.sh (it runs portraits and export first).")
    if failed:
        sys.exit(f"story_prompt: {failed} package(s) failed. The returned images are untouched; fix and rerun.")
