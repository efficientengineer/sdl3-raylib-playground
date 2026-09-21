"""story/playlist.md: the play order, and the tool's definition of what counts.

A scene file that no list names is a draft — no package, no count, no export. `--all`
lifts that and takes story/scenes/ as it stands. chapter_of() reads the chapter out of a
four-digit stem, which is how stats and packages group.

Must never do: reorder or rewrite the playlist.
Public: chapter_of, chapter_label, playlist_lists, playlist_selection, in_playlist,
all_scenes, is_optional, panel_art, played_stems, EPILOGUE. Imports: md, names, paths,
scenes.
"""
import re
from .md import die, h2_sections, squash
from .names import detok
from .paths import ROOT, SCENES
from .scenes import panel_file, parse_scene



# ───────────────────────── Chapters, stats, and ChatGPT packages ─────────────────────────

EPILOGUE = 16                                       # chapter number the epilogue files are numbered in


PACKAGES = ROOT / "packages"                        # tracked; story/out/ is the scratch copy


CAST_NOTES = ROOT / "notes" / "cast-designer-2.md"  # holds the reference sheet priority order


def chapter_of(stem):
    """Chapter number from a scene file name, or None for the template (which is not a scene).

    The numbering is <chapter><scene> in four digits: 0105 is chapter 1, 1595 chapter 15, 16xx the
    epilogue. The intro files written before that scheme ('p01_prologue', '001'-'003b') are chapter 1."""
    if stem.startswith("000"):
        return None
    m = re.match(r"(\d+)", stem)
    if not m:
        return 1                                    # p01_prologue and anything else unnumbered
    return int(m.group(1)[:2]) if len(m.group(1)) >= 4 else 1


def chapter_label(ch):
    return f"ch{ch:02d}" + (" (epilogue)" if ch == EPILOGUE else "")


PLAYLIST = ROOT / "playlist.md"


def playlist_lists():
    """{section: [(stem, marker)]} for every '## ' list in playlist.md, in file order.

    Sections are 'intro' and 'chapter01'-'chapter16'; a marker is the '(optional)' or '(branch: ...)'
    note after the stem. This is the one list of scenes the game and the art pipeline care about:
    a scene file that no section names is a draft, however finished it looks."""
    if not PLAYLIST.exists():
        die(f"{PLAYLIST.relative_to(ROOT.parent)} not found")
    out = {}
    for name, body in h2_sections(detok(PLAYLIST.read_text())).items():
        # A list entry is '- <stem>' with an optional note after it: '(optional)', '(branch: "..." — ...)'.
        # A stem is a scene file name, so it starts with its number; prose bullets never match.
        rows = [(m.group(1), squash(m.group(2)))
                for m in re.finditer(r"^- ([\w-]+)[ \t]*(.*)$", body, flags=re.M)
                if re.match(r"^(\d|p\d)", m.group(1)) or (SCENES / f"{m.group(1)}.md").exists()]
        if rows:
            out[name] = rows
    return out


def playlist_selection():
    """(stems, numbers) named anywhere in playlist.md.

    Matching by number as well as by stem means a scene that has been renamed (0105_the_long_way_home
    -> 0105_the_water_run) is still recognised while the playlist catches up."""
    stems = {stem for rows in playlist_lists().values() for stem, _ in rows}
    nums = {m.group(1) for s in stems if (m := re.match(r"(\d+)", s))}
    return stems, nums


def in_playlist(stem, selection):
    stems, nums = selection
    m = re.match(r"(\d+)", stem)
    return stem in stems or (m is not None and m.group(1) in nums)


def all_scenes(args=()):
    """[(chapter, scene)] for the scenes under consideration, in file order, skipping the template.

    By default that is the scenes story/playlist.md actually names. Everything else in story/scenes/
    is a draft that no chapter plays: it is not counted, not given a package, and not exported.
    `--all` takes the folder as it stands, which is how a rejected draft is still measurable."""
    every = "--all" in args
    selection = None if every else playlist_selection()
    out = []
    for path in sorted(SCENES.glob("*.md")):
        ch = chapter_of(path.stem)
        if ch is None or (selection and not in_playlist(path.stem, selection)):
            continue
        out.append((ch, parse_scene(path)))
    return out


def is_optional(scene):
    return scene["meta"].get("optional", "no").lower() in ("yes", "true", "1")


def panel_art(scene):
    """(panels with art, panels in the scene) for a panel scene."""
    have = sum(1 for n, (sid, _) in enumerate(scene["panels"], 1) if panel_file(scene, n, sid).exists())
    return have, len(scene["panels"])


def played_stems():
    """Every scene stem any list in playlist.md names — what a `scene` trigger may point at."""
    return {stem for rows in playlist_lists().values() for stem, _ in rows}
