"""The markdown the story files are written in, and two path helpers.

A story file is headings and `- key: value` lines; this module turns that into dicts,
and holds die() — the one way the tool exits with a message. squash/slug are the text
normalisers every other module shares, so they cannot drift.

Must never do: know what a scene, a tile or a character is. It parses shape, not meaning.
Public: die, squash, h2_sections, h3_entries, kv_lines, section, existing, slug.
Imports: paths.
"""
import re
import sys
from .paths import ROOT



def die(msg):
    sys.exit(f"story_prompt: {msg}")


def squash(text):
    return re.sub(r"\s+", " ", text).strip()


def h2_sections(text):
    """{'h2 title lowercased': body} for a markdown document."""
    out, name, buf = {}, None, []
    for line in text.splitlines():
        m = re.match(r"^## (?!#)(.+)$", line)
        if m:
            if name is not None:
                out[name] = "\n".join(buf)
            name, buf = m.group(1).strip().lower(), []
        elif name is not None:
            buf.append(line)
    if name is not None:
        out[name] = "\n".join(buf)
    return out


def h3_entries(body):
    """[(id, body)] for each ### heading inside an H2 body."""
    parts = re.split(r"^### (.+)$", body, flags=re.M)
    return [(parts[i].strip(), parts[i + 1]) for i in range(1, len(parts), 2)]


def kv_lines(body):
    return {m.group(1).strip().lower(): m.group(2).strip()
            for m in re.finditer(r"^- ([\w ]+?):\s*(.+)$", body, flags=re.M)}


def section(sections, prefix, where):
    for name, body in sections.items():
        if name.startswith(prefix):
            return body
    die(f"{where}: missing '## {prefix}...' section")


def existing(path_text):
    if not path_text:
        return None
    p = (ROOT.parent / path_text).resolve()
    return p if p.exists() else None


def slug(text):
    return re.sub(r"[^a-z0-9_]", "", (text or "").strip().lower())
