"""A scene file: parse it, and say whether it is legal.

parse_scene() reads story/scenes/NNNN_slug.md into the dict every other module passes
around (meta, panels, dialogue, pages); validate() applies the composition rules from
rules.py and returns errors and warnings. speaker_sides() fixes who takes the left and
right of the dialogue box, and panel_file() names a panel's cut image.

Must never do: print, exit, or write a file — it returns problems and lets the caller
decide. Style text and thresholds come from style.py and rules.py, never from here.
Public: parse_scene, validate, token_problems, speaker_sides, portrait_file,
backdrop_panel, name_matches, names_in, acting_for, panel_text, scene_context,
scene_paths, panel_file. Imports: cast, md, names, paths, rules.
"""
from pathlib import Path
import re
from .cast import alias_map, is_narrator, resolve_name, speaker_warnings
from .md import die, existing, h2_sections, kv_lines, slug, squash
from .names import DOUBLE_ARTICLE, NAMES_FILE, detok, load_names, token_phrase
from .paths import PANELS, PORTRAITS, ROOT, SCENES, expr_portrait_path
from .rules import ANCHOR_SCALES, EXPR_IDS, GAZE_WORDS, MAX_ACTING_WORDS, MAX_PANEL_WORDS, MOODS, PAGES_MAX, PANELS_MAX, PANELS_MIN, PUNCH_SCALES, SCENE_KINDS, SCENE_MAX_PANELS, STYLE_WORDS, TEXT_WORDS



def token_problems(scene):
    """(errors, warnings) for the {{TOKEN}}s in a scene file, after substitution."""
    err = [f"unknown name token {{{{{t}}}}}: it has no row in {NAMES_FILE.relative_to(ROOT.parent)}. "
           f"Add it to the table there, or fix the spelling; never write the bare name in the scene."
           for t in scene.get("unknown_tokens", [])]
    warn = [f"{{{{{t}}}}} has no name in {NAMES_FILE.relative_to(ROOT.parent)}, so the text reads "
            f"'{token_phrase(t)}' and the sentence must supply the article"
            for t in scene.get("unnamed_tokens", [])]
    # The sentence owns the article, the token never does. 'the {{STAIR}}' with a table value of
    # 'the stair' would reach the screen as 'the the stair', so it is an error, not a warning.
    where = [(f"dialogue line {n}", text) for n, (_, text) in enumerate(scene["dialogue"], 1)]
    where += [(f"panel {n}", desc) for n, (_, desc) in enumerate(scene["panels"], 1)]
    where += [(f"the '- {k}:' line", v) for k, v in scene["meta"].items() if k != "characters"]
    where += [("the '## Beat' paragraph", scene["beat"])]
    for what, text in where:
        m = DOUBLE_ARTICLE.search(text or "")
        if m:
            err.append(f"{what} reads '{squash(text[max(0, m.start() - 20):m.end() + 20])}' after name "
                       f"substitution: two articles in a row. A token's value never carries 'the' or 'a' "
                       f"— the sentence writes it. Fix the line, or the value in "
                       f"{NAMES_FILE.relative_to(ROOT.parent)}.")
    return err, warn


def parse_scene(path):
    raw = path.read_text()
    unknown, unnamed = [], []
    text = detok(raw, unknown, unnamed)              # the file stays tokenised; everything downstream sees names
    title = re.search(r"^# (.+)$", text, flags=re.M)
    head = text.split("\n## ", 1)[0]
    secs = h2_sections(text)
    panels, acting, pages, page = [], [], [], 0
    for raw in secs.get("panels", "").splitlines():
        if re.match(r"^\s*-{3,}\s*$", raw):                     # "---" starts a new page: the screen clears
            page += 1 if panels and pages[-1] == page else 0
            continue
        m = re.match(r"^\d+\.\s*([\w-]+)\s*\|\s*(.+)$", raw)
        if m:
            panels.append((m.group(1), m.group(2).strip()))
            acting.append({})
            pages.append(page)
            continue
        m = re.match(r"^\s+- ([^:]+):\s*(.+)$", raw)           # indented "- Name: where they look, expression, body"
        if m and panels:
            acting[-1][m.group(1).strip().lower()] = m.group(2).strip()
    dialogue, reveals, moods, exprs = [], [], [], []
    # "- Speaker (expr) [panel] {mood}: text". The expression comes first, right after the speaker,
    # and a line that carries one may have no text at all: a reaction beat, an empty box and a face.
    for m in re.finditer(r"^- ([^:\[{(\n]+?)\s*(?:\((\w+)\))?\s*(?:\[(\d+)\])?\s*(?:\{(\w+)\})?\s*:[ \t]*(.*)$",
                         secs.get("dialogue", ""), flags=re.M):
        dialogue.append((m.group(1).strip(), m.group(5).strip()))
        exprs.append(m.group(2).lower() if m.group(2) else None)
        reveals.append(int(m.group(3)) if m.group(3) else None)
        moods.append(m.group(4).lower() if m.group(4) else None)
    meta = kv_lines(head)
    kind = meta.get("type", "").lower() or "panels"
    return {
        "path": path, "stem": path.stem,
        "title": title.group(1).strip() if title else path.stem,
        "meta": meta,
        "characters": [c.strip() for c in meta.get("characters", "").split(",") if c.strip()],
        "beat": squash(secs.get("beat", "")),
        "panels": panels, "acting": acting, "pages": pages, "dialogue": dialogue, "reveals": reveals,
        "moods": moods, "exprs": exprs,
        "kind": kind,
        "narration": kind == "narration",
        "talk": kind == "talk",
        "map": meta.get("map", "").strip().lower(),
        "unknown_tokens": unknown, "unnamed_tokens": unnamed,
    }


def speaker_sides(scene, cast=None):
    """{speaker lowercased: 0 left / 1 right}. The first distinct speaker takes the left, the
    second the right, later ones alternate; a speaker keeps their side for the whole scene.

    The Narrator has no portrait, so it takes no side and does not use up one of the two ends.
    A character called by an alias in one line and by their handle in another keeps one side."""
    sides, order = {}, {}
    aliases = alias_map(cast) if cast else {}
    for who, _ in scene["dialogue"]:
        key = who.strip().lower()
        if is_narrator(who):
            sides[key] = 0
            continue
        ident = (resolve_name(who, cast, aliases) if cast else None) or key
        if ident not in order:
            order[ident] = len(order) % 2
        sides[key] = order[ident]
    return sides


def portrait_file(speaker, cast=None):
    """story/portraits/<handle>.png for a speaker, or None when there is none.

    A speaker written as an alias uses the portrait of the handle it resolves to; the Narrator never
    has one (the game draws that line as a box with no name and no portrait)."""
    if is_narrator(speaker):
        return None
    key = (resolve_name(speaker, cast) if cast else None) or speaker.strip().lower()
    p = PORTRAITS / f"{re.sub(r'[^a-z0-9_-]', '', key)}.png"
    return p if p.exists() else None


def backdrop_panel(scene):
    """(scene stem, panel number, panel file Path) for a talk scene's '- backdrop: stem:N', or None.

    Returns the file even when it has not been generated yet; the caller decides whether to warn."""
    spec = scene["meta"].get("backdrop", "").strip()
    m = re.match(r"^([\w-]+)\s*:\s*(\d+)$", spec)
    if not m:
        return None
    other = SCENES / f"{m.group(1)}.md"
    if not other.exists():
        return (m.group(1), int(m.group(2)), None)
    src, n = parse_scene(other), int(m.group(2))
    if not 1 <= n <= len(src["panels"]):
        return (m.group(1), n, None)
    return (m.group(1), n, panel_file(src, n, src["panels"][n - 1][0]))


def name_matches(cast):
    """{written form: handle} for every name allowed to identify a character inside panel text.

    That is the '## handle' itself, and any alias that a name token currently resolves to. An alias
    is otherwise never matched in panel text, because an alias like 'Nine' would fire on 'the Nine
    Doors'. A token's current name is the exception, and has to be: with names tokenised (D13) it is
    the only form a writer ever types, and the table's names are proper nouns by construction."""
    out = {h: h for h in cast}
    for name in load_names().values():
        handle = name and resolve_name(name, cast)
        if handle:
            out[name.lower()] = handle
    return out


def names_in(text, cast):
    """Cast handles named in a piece of panel text, in the cast file's order."""
    found = []
    for written, handle in name_matches(cast).items():
        if handle not in found and re.search(rf"\b{re.escape(written)}\b", text, flags=re.I):
            found.append(handle)
    return found


def acting_for(scene, cast):
    """The scene's acting notes with every name resolved to a cast handle: [{handle: note}] per panel.

    A writer labels an acting line with the name in the panel above it — which, names being tokens, is
    whatever the token currently says. Everything downstream works in handles."""
    aliases = alias_map(cast)
    return [{(resolve_name(who, cast, aliases) or who.lower()): note for who, note in notes.items()}
            for notes in scene["acting"]]


def reveal_plan(scene):
    """Panel revealed by each dialogue line: the [n] tag, else the next unseen panel (0 when none are left)."""
    shown, plan, total = set(), [], len(scene["panels"])
    for tag in scene["reveals"]:
        n = tag or next((i for i in range(1, total + 1) if i not in shown), 0)
        if n:
            shown.add(n)
        plan.append(n)
    return plan


def validate(scene, style, cast):
    """Returns (errors, warnings). Errors block the build."""
    err, warn = token_problems(scene)
    shots, panels = style["shots"], scene["panels"]
    aliases = alias_map(cast)

    for n, mood in enumerate(scene["moods"], 1):
        if mood and mood not in MOODS:
            err.append(f"dialogue line {n}: unknown mood '{{{mood}}}'. Use one of: {', '.join(MOODS)}")
    # The expression tag: '- Name (biglaugh) [2] {hope}: text', and a tagged line may be textless.
    for n, ((who, text), expr) in enumerate(zip(scene["dialogue"], scene["exprs"]), 1):
        if expr and expr not in EXPR_IDS:
            err.append(f"dialogue line {n}: unknown expression '({expr})'. Use one of: {', '.join(EXPR_IDS)}")
        elif expr and is_narrator(who):
            err.append(f"dialogue line {n}: the Narrator has no portrait and takes no expression; "
                       f"drop the '({expr})' tag")
        elif expr:
            handle = resolve_name(who, cast, aliases)
            if not handle or not existing(cast[handle].get("ref")):
                warn.append(f"dialogue line {n}: '{who}' has no reference sheet, so there is no "
                            f"portrait_{slug(who)}_{expr}.png and the game falls back to no portrait")
            elif not expr_portrait_path(handle, expr).exists():
                warn.append(f"dialogue line {n}: {cast[handle]['display']}'s '{expr}' portrait has not "
                            f"been generated yet (./story_prompt.py expressions {cast[handle]['display']}); "
                            f"the game falls back to their main portrait")
        if not text and not expr:
            err.append(f"dialogue line {n} ('{who}') has no text. A line may only be empty when it "
                       f"carries an expression, which makes it a reaction beat: '- {who} (shock):'")
    if scene["kind"] not in SCENE_KINDS:
        err.append(f"unknown '- type: {scene['kind']}'. Use one of: {', '.join(SCENE_KINDS)}")
        return err, warn
    if scene["narration"] or scene["talk"]:
        kind = scene["kind"]
        if panels:
            err.append(f"a {kind} scene has no panels; remove '## Panels' or the 'type: {kind}' line")
        if not scene["dialogue"]:
            err.append(f"a {kind} scene needs at least one line under '## Dialogue'")
        if scene["talk"]:
            for n, r in enumerate(scene["reveals"], 1):
                if r is not None:
                    err.append(f"dialogue line {n}: a talk scene has no panels to reveal; drop the [{r}] tag")
            spec = scene["meta"].get("backdrop", "").strip()
            if spec:
                bd = backdrop_panel(scene)
                if not bd:
                    err.append(f"'- backdrop: {spec}' is malformed; write '<scene_stem>:<panel_number>'")
                elif bd[2] is None:
                    err.append(f"'- backdrop: {spec}' names no existing panel "
                               f"(check story/scenes/{bd[0]}.md and its panel count)")
                elif not bd[2].exists():
                    warn.append(f"backdrop panel {bd[2].name} has not been generated yet; the game shows black")
        warn += speaker_warnings(scene, cast, aliases)
        return err, warn

    if not scene["meta"].get("location"):
        err.append("missing '- location:' line")
    if len(panels) > SCENE_MAX_PANELS:
        err.append(f"R1 panel count: {len(panels)} panels in the scene, max {SCENE_MAX_PANELS} (one shot sheet)")
    if len(set(scene["pages"])) > PAGES_MAX:
        err.append(f"R1 page count: the scene has {len(set(scene['pages']))} pages; the limit is {PAGES_MAX}")
    unknown = [sid for sid, _ in panels if sid not in shots]
    for sid in unknown:
        err.append(f"R2 menu only: unknown shot '{sid}' (run: story_prompt.py shots)")
    if unknown or not panels:
        if not panels:
            err.append("R1 panel count: no panels")
        return err, warn                                   # remaining rules need valid shots

    for pg in sorted(set(scene["pages"])):                 # composition rules apply to each page on its own
        idx = [i for i, p in enumerate(scene["pages"]) if p == pg]
        where = f"page {pg + 1}" if len(set(scene["pages"])) > 1 else "the page"
        if not PANELS_MIN <= len(idx) <= PANELS_MAX:
            err.append(f"R1 panel count: {where} has {len(idx)} panels, need {PANELS_MIN}-{PANELS_MAX}")
        scales = [shots[panels[i][0]]["scale"] for i in idx]
        shapes = [shots[panels[i][0]]["shape"] for i in idx]
        for k in range(len(idx) - 1):
            if scales[k] == scales[k + 1]:
                err.append(f"R3 alternate scale: panels {idx[k]+1} and {idx[k+1]+1} are both '{scales[k]}' "
                           f"({panels[idx[k]][0]}, {panels[idx[k+1]][0]})")
        need = 2 if len(idx) <= 2 else 3
        if len(set(shapes)) < need:
            err.append(f"R4 vary shape: {where} has {len(set(shapes))} distinct shape(s) {sorted(set(shapes))}, need {need}")
        if len(idx) >= 3:
            if not ANCHOR_SCALES & set(scales):
                err.append(f"R5 anchor and punch: {where} has no anchor panel (wide, full, or medium)")
            if not PUNCH_SCALES & set(scales):
                err.append(f"R5 anchor and punch: {where} has no punch panel (close, extreme, or insert)")

    listed = []                                        # handles; a 'characters:' entry may be an alias
    for c in scene["characters"]:
        handle = resolve_name(c, cast, aliases)
        if handle is None:
            err.append(f"R6 known cast: '{c.lower()}' is not in characters.md")
        elif handle not in listed:
            listed.append(handle)
    mentioned = set()
    for n, (sid, desc) in enumerate(panels, 1):
        for c in names_in(desc, cast):
            mentioned.add(c)
            if c not in listed:
                err.append(f"R6 known cast: panel {n} names '{c}' but the characters: line does not list them")
        words = len(desc.split())
        if words > MAX_PANEL_WORDS:
            err.append(f"R7 short panels: panel {n} is {words} words, max {MAX_PANEL_WORDS}")
        low = desc.lower()
        for w in STYLE_WORDS:
            if re.search(rf"\b{re.escape(w)}\b", low):
                err.append(f"R8 no style leakage: panel {n} contains '{w}'")
        hit = next((w for w in TEXT_WORDS if re.search(rf"\b{re.escape(w)}\b", low)), None)
        if hit is None and re.search(r'["“][^"”]+["”]', desc):
            hit = "a quoted string"
        if hit:
            err.append(f"R9 no text: panel {n} asks for written words ({hit})")
    if not scene["meta"].get("staging"):
        err.append("R10 staging: missing '- staging:' line (who is on which side, facing which way, where the threat is)")
    shown = lambda c: cast[c]["display"] if c in cast else c.title()   # the name the writer actually types
    for n, ((sid, desc), notes) in enumerate(zip(panels, acting_for(scene, cast)), 1):
        named = names_in(desc, cast)
        for c in named:
            if c not in notes:
                err.append(f"R11 acting: panel {n} shows {shown(c)} but has no acting line "
                           f"('   - {shown(c)}: where they look, expression, body')")
        for c, note in notes.items():
            if c not in named:
                err.append(f"R11 acting: panel {n} has an acting line for '{shown(c)}' who is not named in the panel")
            if not any(g in note.lower() for g in GAZE_WORDS):
                err.append(f"R11 acting: panel {n}, {shown(c)}: say where the eyes point (looking at..., glaring toward...)")
            if len(note.split()) > MAX_ACTING_WORDS:
                err.append(f"R11 acting: panel {n}, {shown(c)}: {len(note.split())} words, max {MAX_ACTING_WORDS}")
            for w in STYLE_WORDS:
                if re.search(rf"\b{re.escape(w)}\b", note.lower()):
                    err.append(f"R8 no style leakage: panel {n} acting line for {shown(c)} contains '{w}'")
    for c in listed:
        if c not in mentioned:
            warn.append(f"'{shown(c)}' is listed but never named in a panel; their look will not be included")
    for n, r in enumerate(scene["reveals"], 1):
        if r is not None and not 1 <= r <= len(panels):
            err.append(f"dialogue line {n} reveals panel [{r}] but the scene has {len(panels)} panels")
    # The reveal rule catches writers out: an untagged line does NOT hold on the panel that is up,
    # it turns the page to the next unseen one. Say so, with the tag that would hold it.
    held = 0
    for n, (r, shown_n) in enumerate(zip(scene["reveals"], reveal_plan(scene)), 1):
        if r is None and n > 1 and shown_n and held and shown_n != held:
            warn.append(f"dialogue line {n} has no [n] tag, so it reveals panel {shown_n}; "
                        f"tag it [{held}] to hold on the panel already up")
        held = shown_n or held
    warn += speaker_warnings(scene, cast, aliases)
    return err, warn


def panel_text(style, cast, sid, desc, notes):
    """One panel's line for the image prompt: shot, content, then who looks where and feels what."""
    text = f"{style['shots'][sid]['prompt']} Content: {desc.rstrip('.')}."
    if notes:
        text += " Acting: " + " ".join(f"{cast[c]['display']}: {note.rstrip('.')}." for c, note in notes.items())
    return text


def scene_context(scene, span=""):
    """Story context for the image model: what is happening, where, and the fixed screen direction."""
    meta = scene["meta"]
    out = [f"SITUATION{span}: {scene['beat']}"] if scene["beat"] else []
    setting = f"SETTING{span}: {meta['location'].rstrip('.')}."
    if meta.get("mood"):
        setting += f" Mood: {meta['mood']}."
    out.append(setting)
    if meta.get("staging"):
        out.append(f"STAGING{span}, the same in every panel: {meta['staging'].rstrip('.')}.")
    return out


def scene_paths(args):
    if "--all" in args:
        return sorted(p for p in SCENES.glob("*.md") if not p.stem.startswith("000"))
    paths = [Path(a) for a in args if not a.startswith("--")]
    if not paths:
        die("give one or more scene files, or --all")
    for p in paths:
        if not p.exists():
            die(f"{p} not found")
    return paths


def panel_file(scene, n, sid):
    return PANELS / f"{scene['stem']}_p{n}_{sid}.png"
