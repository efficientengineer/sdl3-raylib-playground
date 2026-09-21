"""The `stats` command, the reference-sheet priority order, and run_quiet.

Per chapter: scenes by type, panels, dialogue lines, optional scenes and which panel
scenes have art, then the totals and every speaker that is neither a handle, an alias nor
Narrator. run_quiet swallows a command's output so packages can call a builder without
printing it twice — LOUD warnings survive it on purpose.

Must never do: write a file.
Public: cmd_stats, refsheet_priority, run_quiet. Imports: cast, md, names, paths,
playlist, rules.
"""
import re
import sys
from .cast import alias_map, is_narrator, load_cast, resolve_name
from .md import h2_sections, squash
from .names import NAMES_FILE, token_of_name
from .paths import ROOT, SCENES
from .playlist import CAST_NOTES, all_scenes, chapter_label, chapter_of, is_optional, panel_art
from .rules import SCENE_KINDS



def cmd_stats(args):
    cast = load_cast()
    aliases = alias_map(cast)
    chapters, speakers, oneoffs, tokens = {}, {}, {}, {}
    scenes = all_scenes(args)
    skipped = len([p for p in SCENES.glob("*.md") if chapter_of(p.stem) is not None]) - len(scenes)
    print(f"{len(scenes)} scene file(s) in story/scenes/" if "--all" in args else
          f"{len(scenes)} scene(s) named in story/playlist.md" +
          (f"; {skipped} file(s) in story/scenes/ that no list names (add --all to count them)"
           if skipped else ""), end="\n\n")
    for ch, scene in scenes:
        for t in scene["unknown_tokens"]:
            tokens.setdefault(t, []).append(scene["stem"])
        c = chapters.setdefault(ch, {"kinds": {}, "panels": 0, "lines": 0, "optional": [], "art": []})
        c["kinds"][scene["kind"]] = c["kinds"].get(scene["kind"], 0) + 1
        c["panels"] += len(scene["panels"])
        c["lines"] += len(scene["dialogue"])
        if is_optional(scene):
            c["optional"].append(scene["stem"])
        if scene["kind"] == "panels" and scene["panels"]:
            have, total = panel_art(scene)
            if have:
                c["art"].append(f"{scene['stem']} {have}/{total}")
        for who, _ in scene["dialogue"]:
            speakers[who] = speakers.get(who, 0) + 1
            if not is_narrator(who) and not resolve_name(who, cast, aliases):
                oneoffs.setdefault(who, []).append(scene["stem"])

    kinds = sorted({k for c in chapters.values() for k in c["kinds"]}, key=lambda k: (k not in SCENE_KINDS, k))
    head = f"{'chapter':<14}{'scenes':>7}" + "".join(f"{k:>11}" for k in kinds) + \
           f"{'panels':>8}{'lines':>8}{'optional':>10}{'art':>6}"
    print(head)
    print("-" * len(head))
    tot = {"scenes": 0, "panels": 0, "lines": 0, "optional": 0, "art": 0, "kinds": {}}
    for ch in sorted(chapters):
        c = chapters[ch]
        n = sum(c["kinds"].values())
        print(f"{chapter_label(ch):<14}{n:>7}" + "".join(f"{c['kinds'].get(k, 0):>11}" for k in kinds) +
              f"{c['panels']:>8}{c['lines']:>8}{len(c['optional']):>10}{len(c['art']):>6}")
        tot["scenes"] += n
        for k in kinds:
            tot["kinds"][k] = tot["kinds"].get(k, 0) + c["kinds"].get(k, 0)
        for key in ("panels", "lines"):
            tot[key] += c[key]
        tot["optional"] += len(c["optional"])
        tot["art"] += len(c["art"])
    print("-" * len(head))
    print(f"{'total':<14}{tot['scenes']:>7}" + "".join(f"{tot['kinds'].get(k, 0):>11}" for k in kinds) +
          f"{tot['panels']:>8}{tot['lines']:>8}{tot['optional']:>10}{tot['art']:>6}")
    print()
    for ch in sorted(chapters):
        c = chapters[ch]
        if c["optional"]:
            print(f"{chapter_label(ch)} optional: {', '.join(c['optional'])}")
        if c["art"]:
            print(f"{chapter_label(ch)} panel art: {', '.join(c['art'])}")
    print(f"\ndistinct speakers: {len(speakers)}  "
          f"({len(speakers) - len(oneoffs)} cast or Narrator, {len(oneoffs)} one-off)")
    if oneoffs:
        print("one-off speakers (neither a handle, nor an alias, nor Narrator):")
        for who in sorted(oneoffs, key=lambda w: (-len(oneoffs[w]), w.lower())):
            tok = token_of_name(who)
            print(f"  {who:<16}{len(oneoffs[who]):>3} line(s) in {', '.join(sorted(set(oneoffs[who])))}"
                  + (f"   <-- {{{{{tok}}}}}, needs an '- alias: {who}' in characters.md" if tok else ""))
    if tokens:
        print(f"\n{len(tokens)} name token(s) with no row in {NAMES_FILE.relative_to(ROOT.parent)} "
              f"(each one is a validation error):", file=sys.stderr)
        for tok in sorted(tokens):
            print(f"  {{{{{tok}}}}}  in {', '.join(sorted(set(tokens[tok]))[:5])}", file=sys.stderr)


def refsheet_priority(cast):
    """[(handle, tier)] in the cast designer's order, then everyone that file does not rank.

    Parsed from story/notes/cast-designer-2.md '## 5. Reference sheet priority', whose items look
    like '1. Zeph - 2. Pip - ... 12. Tibb and Sela'. Anything unparsable is simply not ranked."""
    order, ranked = [], set()
    if CAST_NOTES.exists():
        body = next((b for name, b in h2_sections(CAST_NOTES.read_text()).items()
                     if "reference sheet priority" in name), "")
        parts = re.split(r"^\*\*(Tier [^*]+)\*\*\s*$", body, flags=re.M)         # [prose, tier, items, ...]
        for i in range(1, len(parts), 2):
            tier = squash(parts[i])
            for item in re.findall(r"\d+\.\s*([^·]+)", squash(parts[i + 1])):   # items split on the middot
                for word in re.findall(r"[A-Z][a-z]+", item):                    # '12. Tibb and Sela', notes and all
                    handle = word.lower()
                    if handle in cast and handle not in ranked:
                        ranked.add(handle)
                        order.append((handle, tier))
    return order + [(h, "unranked") for h in cast if h not in ranked]


def run_quiet(fn, args):
    """Run a command function with its output swallowed. (ok, message); die() is a failure, not the end."""
    import contextlib
    import io
    buf = io.StringIO()
    try:
        with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
            fn(args)
        return True, ""
    except SystemExit as e:
        why = [l for l in buf.getvalue().splitlines() if "ERROR" in l] or [squash(str(e.code or "rejected"))]
        return False, "; ".join(why)
    except Exception as e:                                      # one bad scene must not stop the run
        return False, f"{type(e).__name__}: {e}"
