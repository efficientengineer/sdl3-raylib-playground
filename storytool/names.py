"""The {{TOKEN}} name table (DECISIONS.md D13) and the `names` command.

story/v3/NAMES.md says what every proper noun is called this week; detok() substitutes
before any text is used, so the game and ChatGPT never see braces. A token with no row is
a validation error, and a token whose value is a note rather than a name reads as a plain
phrase. DOUBLE_ARTICLE lives here because "the the" is a substitution fault, not a scene
fault.

Must never do: rewrite a story file. The files stay tokenised forever.
Public: load_names, token_phrase, tokens_in, detok, token_of_name, names_table,
tokenised_files, cmd_names, TOKEN_RE, DOUBLE_ARTICLE. Imports: md, paths.
"""
import re
import sys
from .md import die
from .paths import CAST, FIELD_DOCS, FIELD_TEXT, ROOT, SCENES, STATE_FILES



# ───────────────────────── Name tokens (DECISIONS.md D13) ─────────────────────────
#
# Every proper noun in the story text is a token in double braces — {{HERO}}, {{HOME_TOWN}} — and
# story/v3/NAMES.md is the one table that says what each token is currently called. Renaming a
# character is a one-line edit there. The raw files stay tokenised forever; every command that
# *consumes* text (check, brief, sheet, packages, preview, export, stats) substitutes first, so the
# game and ChatGPT only ever see real names.
#
# A token whose table entry is descriptive rather than a name — '*(descriptive)*', '*(unnamed yet)*'
# — has no name yet, so the text reads a plain phrase built from the token ({{SEA_WALL}} -> "the sea
# wall"). A token that is not in the table at all is a validation error: nothing silently ships with
# braces in it.

NAMES_FILE = ROOT / "v3" / "NAMES.md"


TOKEN_RE = re.compile(r"\{\{([A-Z][A-Z0-9_]*)\}\}")


_NAMES = None                                       # {TOKEN: name or None}, loaded once


_ARTICLED = []                                      # table values that arrived with an article on them


def _name_cell(cell):
    """The 'current name' column of a NAMES.md row, or None when it is a note rather than a name.

    A value never carries its own article. The text says 'the {{STAIR}}', so substitution puts the
    value in verbatim and the sentence supplies the article; a value of 'the stair' would read 'the
    the stair'. A leading article in the table is stripped here and reported by `names`."""
    c = re.sub(r"^\*+|\*+$", "", cell.strip().strip("`").strip()).strip()
    c = c.strip('"').strip("“”").strip()
    if not c or re.fullmatch(r"\(.*\)", c):
        return None
    m = re.match(r"^(the|a|an)\s+(.*)$", c, flags=re.I)
    if m:
        _ARTICLED.append(c)
        return m.group(2)
    return c


def load_names():
    """{TOKEN: current name or None} from every markdown table row in story/v3/NAMES.md."""
    global _NAMES
    if _NAMES is None:
        _NAMES = {}
        if NAMES_FILE.exists():
            for line in NAMES_FILE.read_text().splitlines():
                if not line.strip().startswith("|"):
                    continue
                cells = [c.strip() for c in line.strip().strip("|").split("|")]
                m = re.fullmatch(r"`?\{\{([A-Z][A-Z0-9_]*)\}\}`?", cells[0]) if cells else None
                if m and len(cells) >= 2:
                    _NAMES[m.group(1)] = _name_cell(cells[1])
    return _NAMES


def token_phrase(tok):
    """What a token with no name in the table reads as: {{SEA_WALL}} -> 'sea wall', with no article.

    The text writes the article itself ('the {{SEA_WALL}}'), exactly as it does for a named token."""
    return re.sub(r"^the\s+", "", tok.lower().replace("_", " "))


def tokens_in(text):
    return TOKEN_RE.findall(text or "")


def detok(text, unknown=None, unnamed=None):
    """Substitute every {{TOKEN}} with its current name. Unknown tokens are left alone and collected.

    A token that opens a sentence is capitalised, so a descriptive phrase ('the stair') still reads
    as prose wherever the writer put it. A real name is already capitalised, so this is a no-op there."""
    if not text or "{{" not in text:
        return text
    names = load_names()

    def sub(m):
        tok = m.group(1)
        if tok not in names:
            if unknown is not None and tok not in unknown:
                unknown.append(tok)
            return m.group(0)
        word = names[tok]
        if word is None:
            word = token_phrase(tok)
            if unnamed is not None and tok not in unnamed:
                unnamed.append(tok)
        before = text[:m.start()].rstrip(" \t")
        if not before or before[-1] in ".!?:;\"'“”(\n":
            word = word[:1].upper() + word[1:]
        return word
    return TOKEN_RE.sub(sub, text)


def token_of_name(name):
    """The token a written name currently stands for, or None. Used to explain a missing alias."""
    key = name.strip().lower()
    return next((t for t, n in load_names().items() if n and n.lower() == key), None)


def names_table():
    """The token table as markdown, for a brief or a package: token, what it reads as."""
    names = load_names()
    if not names:
        return f"\n(no table found at {NAMES_FILE.relative_to(ROOT.parent)})\n"
    L = ["", "| token | reads as |", "| --- | --- |"]
    L += [f"| `{{{{{t}}}}}` | {n if n else token_phrase(t) + ' _(no name yet)_'} |"
          for t, n in names.items()]
    return "\n".join(L) + "\n"


# Files whose text is tokenised. Everything here is substituted when it is consumed, never rewritten.
def tokenised_files():
    out = (sorted(SCENES.glob("*.md")) + [CAST, ROOT / "playlist.md", FIELD_TEXT] + list(STATE_FILES)
           + list(FIELD_DOCS.values()))
    return [p for p in out if p.exists()]


def cmd_names(args):
    """Print the token table, and every token used in the story that has no row in it."""
    names = load_names()
    if not names:
        die(f"{NAMES_FILE.relative_to(ROOT.parent)} not found or has no token table")
    used = {}
    for path in tokenised_files():
        for tok in set(tokens_in(path.read_text())):
            used.setdefault(tok, []).append(path.name)
    print(f"{NAMES_FILE.relative_to(ROOT.parent)} — {len(names)} token(s)\n")
    print(f"{'token':<20}{'reads as':<34}used in")
    print("-" * 78)
    for tok, name in names.items():
        where = used.get(tok, [])
        reads = name if name else f"{token_phrase(tok)}  (no name yet)"
        note = f"{len(where)} file(s)" if where else "unused"
        print(f"{'{{' + tok + '}}':<20}{reads:<34}{note}")
    missing = {t: w for t, w in used.items() if t not in names}
    print()
    if _ARTICLED:
        print(f"{len(_ARTICLED)} value(s) in the table carry their own article and were read without it: "
              f"{', '.join(sorted(set(_ARTICLED)))}. The text writes 'the {{{{TOKEN}}}}', so a value of "
              f"'the x' would reach the screen as 'the the x'. Drop the article in the table.",
              file=sys.stderr)
    if missing:
        print(f"{len(missing)} token(s) used but not in the table — every one of these is a "
              f"validation error:", file=sys.stderr)
        for tok in sorted(missing):
            files = sorted(set(missing[tok]))
            print(f"  {{{{{tok}}}}}  in {', '.join(files[:6])}"
                  + (f" and {len(files) - 6} more" if len(files) > 6 else ""), file=sys.stderr)
        print(f"\nAdd a row for each to {NAMES_FILE.relative_to(ROOT.parent)}, or fix the spelling in the "
              f"scene. Never write the bare name in the text.", file=sys.stderr)
        sys.exit(1)
    unused = [t for t in names if t not in used]
    print("every token used in the story has a row in the table.")
    if unused:
        print(f"not used anywhere yet: {', '.join('{{%s}}' % t for t in unused)}")


DOUBLE_ARTICLE = re.compile(r"\b(the|a|an)\s+(the|a|an)\b", flags=re.I)
