"""story/characters.md: handles, aliases, looks, and who may speak.

A `## Name` heading is the handle a writer types; `- alias:` is the name shown on screen.
resolve_name() is the one place that maps a written name (token value, alias or handle)
back to a cast entry, which is what decides portraits, the left/right side and the
cast-membership check. `Narrator` is reserved and is always known.

Must never do: match an alias inside panel description text, unless it is a name token's
current value — that exception is the token table's and is implemented here.
Public: load_cast, alias_map, resolve_name, is_narrator, unknown_speakers,
speaker_warnings. Imports: md, names, paths, rules.
"""
import re
from .md import die, h2_sections, kv_lines
from .names import NAMES_FILE, detok, load_names, token_of_name
from .paths import CAST, ROOT
from .rules import LOOK_BANNED, NARRATOR



def load_cast():
    if not CAST.exists():
        die(f"{CAST} not found")
    cast, unknown = {}, []
    for name, body in h2_sections(detok(CAST.read_text(), unknown)).items():
        kv = kv_lines(body)
        if "look" in kv:
            bad = [w for w in LOOK_BANNED if re.search(rf"\b{w}\b", kv["look"], flags=re.I)]
            if bad:
                die(f"characters.md: {name.title()}'s look contains {bad}. Race, class, and beard words pull image "
                    f"models toward Western fantasy. Describe only what is visible (see 'Design direction').")
            # h2_sections lowercases; recover the display name from the look or title-case it
            cast[name] = {"name": name.title(), **kv,
                          "aliases": [a.strip() for a in kv.get("alias", "").split(",") if a.strip()]}
    if not cast:
        die("characters.md: no characters with a 'look' line")
    if unknown:
        die(f"characters.md: unknown name token(s) {', '.join('{{%s}}' % t for t in unknown)}. "
            f"Every token needs a row in {NAMES_FILE.relative_to(ROOT.parent)}.")
    claimed = {}
    for handle, c in cast.items():
        for a in c["aliases"]:
            key = a.lower()
            if key in cast:
                die(f"characters.md: {c['name']}'s alias '{a}' is already a '## ' handle")
            if key == NARRATOR:
                die(f"characters.md: '{a}' cannot be an alias; the game reserves it for boxes with no speaker")
            if key in claimed:
                die(f"characters.md: alias '{a}' is claimed by both {claimed[key]} and {c['name']}")
            claimed[key] = c["name"]
    for handle, c in cast.items():                   # what each character is called in the story right now
        c["display"] = c["name"]
    for name in load_names().values():               # a token's current name outranks the '## handle'
        handle = name and resolve_name(name, cast)
        if handle:
            cast[handle]["display"] = name
    return cast


def alias_map(cast):
    """{display name lowercased: handle} for every '- alias:' name in characters.md.

    An alias is a display name only: a dialogue speaker, a name on a scene's 'characters:' line, and
    the name the game prints over the box. Aliases are deliberately NOT matched inside panel
    descriptions, where an alias like 'Nine' would fire on 'the Nine Doors'; panel text matches the
    '## Handle' alone, which is why the handles are never common words."""
    return {a.lower(): handle for handle, c in cast.items() for a in c["aliases"]}


def resolve_name(name, cast, aliases=None):
    """The cast handle behind a written name: the handle itself, or one of its aliases. None if neither."""
    key = name.strip().lower()
    if key in cast:
        return key
    return (alias_map(cast) if aliases is None else aliases).get(key)


def is_narrator(speaker):
    return speaker.strip().lower() == NARRATOR


def unknown_speakers(scene, cast, aliases=None):
    """Speakers who are neither a handle, nor an alias, nor the Narrator: the one-off NPCs, in order."""
    out = []
    for who, _ in scene["dialogue"]:
        if not is_narrator(who) and not resolve_name(who, cast, aliases) and who not in out:
            out.append(who)
    return out


def speaker_warnings(scene, cast, aliases):
    """One warning per speaker the cast file does not know.

    A speaker written as a token resolves token -> name -> handle or alias, exactly as an alias does,
    so a name that came out of NAMES.md and still finds nobody means the alias line is missing rather
    than that this is a one-off NPC. Say which."""
    out = []
    for who in unknown_speakers(scene, cast, aliases):
        tok = token_of_name(who)
        if tok:
            out.append(f"dialogue speaker '{who}' is {{{{{tok}}}}} in {NAMES_FILE.relative_to(ROOT.parent)}, "
                       f"but no entry in characters.md carries it on an '- alias:' line, so the game draws "
                       f"no portrait for them")
        else:
            out.append(f"dialogue speaker '{who}' is not in characters.md (fine for one-off NPCs)")
    return out
