"""story/field/text.md: examine lines and NPC names, and their ban lists.

Examine text is story, so it lives in the story tree, not in the maps and not in the
engine. check_field_text enforces two sentences at most, no unknown token, and nothing on
the BAN: lists — which it READS from story/v3/STYLE.md and SMELLS.md rather than copying,
so a word banned there is banned wherever it is written.

Must never do: keep its own copy of a banned word.
Public: load_bans, banned_hits, sentence_count, load_field_text, check_field_text.
Imports: field, md, names, paths.
"""
import re
from .field import FIELD_NAME_MAX, SMELLS, TEXT_MAX_SENTENCES, TODO_RE, V3_STYLE
from .md import h2_sections, kv_lines, squash
from .names import DOUBLE_ARTICLE, NAMES_FILE, detok
from .paths import FIELD, FIELD_TEXT, ROOT



# ── field text: what the world says when you look at it ──

def load_bans():
    """[(phrase, whole word?, file)] for every quoted phrase on a 'BAN:' line in the v3 lists.

    Those lists are written to be grepped — each rule is one `BAN:` line and the greppable part of it
    is in quotes — so this reads them rather than copying them. A rule with nothing quoted is a
    structural one ("ALL-CAPS dialogue") and is left to a human."""
    out, seen = [], set()
    for path in (V3_STYLE, SMELLS):
        if not path.exists():
            continue
        for line in path.read_text().splitlines():
            m = re.match(r"^BAN:\s*(.+)$", line.strip())
            if not m:
                continue
            rest = re.split(r"\s+[\u2014-]\s+", m.group(1))[0]     # the advice after the dash is not the rule
            words = re.match(r"^the words?\s+(.*)$", rest, flags=re.I)
            quotes = re.findall(r'"([^"]{2,})"', words.group(1) if words else rest)
            bare = re.fullmatch(r'\s*(?:"[^"]+"[\s,/]*(?:and\s*)?)+\s*(?:\([^)]*\))?\s*', rest)
            for phrase in quotes:
                one = re.fullmatch(r"[\w']+", phrase) is not None
                if one and not (words or bare):
                    continue                             # a common word quoted inside a prose rule
                if phrase.lower() not in seen:
                    seen.add(phrase.lower())
                    out.append((phrase, one, path.relative_to(ROOT.parent)))
    return out


def banned_hits(text):
    """[(phrase, which list)] for every banned phrase in a piece of text."""
    hits = []
    for phrase, word, where in load_bans():
        pat = rf"\b{re.escape(phrase)}\b" if word else re.escape(phrase)
        if re.search(pat, text, flags=re.I):
            hits.append((phrase, where))
    return hits


def sentence_count(text):
    return len([s for s in re.split(r"[.!?]+(?:\s|$)", text.strip()) if s.strip()])


def load_field_text():
    """{id: {...}} from story/field/text.md: the line itself, and the speaker name if it carries one.

    `- name:` is the name the game prints over the box — an NPC's display name is story, the same as
    the line is — and takes tokens like everything else. `- what:` is the writer's own note and is
    never exported."""
    out = {}
    if not FIELD_TEXT.exists():
        return out
    for ident, body in h2_sections(FIELD_TEXT.read_text()).items():
        if "." not in ident:
            continue                                     # the file's own prose headings
        raw = next((squash(l) for l in body.splitlines()
                    if l.strip() and not re.match(r"^\s*- [\w ]+?:", l)), "")
        kv = kv_lines(body)
        unknown, unnamed = [], []
        out[ident] = {**kv, "raw": raw, "text": detok(raw, unknown, unnamed),
                      "name_raw": kv.get("name", ""), "name": detok(kv.get("name", ""), unknown, unnamed),
                      "unknown": unknown, "unnamed": unnamed}
    return out


def check_field_text():
    """(errors, warnings) for story/field/text.md: ids, length, tokens, and the banned lists."""
    err, warn = [], []
    if not FIELD_TEXT.exists():
        return err, [f"{FIELD_TEXT.relative_to(ROOT.parent)} not found; the field has no examine text"]
    maps = {p.stem for p in (FIELD / "maps").glob("*.map")}
    entries = load_field_text()
    if not entries:
        warn.append(f"{FIELD_TEXT.name}: no '## <map>.<id>' entries")
    for ident, e in sorted(entries.items()):
        where = f"{FIELD_TEXT.name}: {ident}"
        if not re.fullmatch(r"[a-z0-9_]+\.[a-z0-9_]+", ident):
            err.append(f"{where}: an id is '<map>.<thing>', lower case, digits and underscores only")
            continue
        mp = ident.split(".")[0]
        if maps and mp not in maps:
            warn.append(f"{where}: no story/field/tmaps/{mp}.tmap, so nothing can trigger this line")
        if not e["raw"]:
            err.append(f"{where}: no text under the heading")
            continue
        for tok in e["unknown"]:
            err.append(f"{where}: unknown name token {{{{{tok}}}}}: it has no row in "
                       f"{NAMES_FILE.relative_to(ROOT.parent)}")
        if e["name"]:                                    # a display name is a name, not a sentence
            if len(e["name"]) > FIELD_NAME_MAX:
                warn.append(f"{where}: '- name: {e['name']}' is {len(e['name'])} characters; the box "
                            f"draws about {FIELD_NAME_MAX}")
            m = DOUBLE_ARTICLE.search(e["name"])
            if m:
                err.append(f"{where}: '- name:' reads '{m.group(0)}' after name substitution")
        if TODO_RE.match(e["raw"]):
            warn.append(f"{where}: still a placeholder; the game will show it in brackets")
            continue                                     # the rest is for text somebody has written
        n = sentence_count(e["text"])
        if n > TEXT_MAX_SENTENCES:
            err.append(f"{where}: {n} sentences, max {TEXT_MAX_SENTENCES}")
        for phrase, src in banned_hits(e["text"]):
            err.append(f"{where}: '{phrase}' is on the banned list in {src}")
        m = DOUBLE_ARTICLE.search(e["text"])
        if m:
            err.append(f"{where}: '{m.group(0)}' after name substitution: a token's value never carries "
                       f"its own article")
    return err, warn
