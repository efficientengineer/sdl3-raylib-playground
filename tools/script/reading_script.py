#!/usr/bin/env python3
"""Rebuild story/v3/chapterNN_script.md: each scene's dialogue in play order with the PICTURE that is on
screen described at the moment it appears. Keeps whatever sits between scenes (the play notes)."""
import re, sys, glob
ROOT = "story"
names = {}
for l in open(f"{ROOT}/v3/NAMES.md"):
    m = re.match(r"\|\s*`\{\{(\w+)\}\}`\s*\|\s*([^|]+?)\s*\|", l)
    if m: names[m.group(1)] = m.group(2)
def sub(t): return re.sub(r"\{\{(\w+)\}\}", lambda m: names.get(m.group(1), m.group(1).lower().replace("_", " ")), t)

def scene_block(path):
    s = open(path).read()
    kind = (re.search(r"^- type:\s*(\w+)", s, re.M) or [None, "panels"])[1]
    panels, page_of, page = {}, {}, 1
    if "## Panels" in s:
        for l in s.split("## Panels")[1].split("## Dialogue")[0].splitlines():
            if l.strip() == "---": page += 1
            m = re.match(r"(\d+)\.\s*(\w+)\s*\|\s*(.*)", l)
            if m: panels[int(m.group(1))] = (m.group(2), m.group(3).strip()); page_of[int(m.group(1))] = page
    out, seen, cur_page = [], set(), 1
    loc = re.search(r"^- (?:location|backdrop):\s*(.*)", s, re.M)
    if kind != "panels" and loc: out.append(f"> *No pictures — a talk scene: {sub(loc.group(1))[:160]}*\n")
    for l in s.split("## Dialogue")[1].strip().splitlines():
        m = re.match(r"- (.+?)(?:\s*\((\w+)\))?((?:\s*\[\d+\]|\s*\{\w+\})*):\s?(.*)", l)
        if not m: continue
        who, expr, tags, text = m.group(1), m.group(2), m.group(3), m.group(4)
        t = re.search(r"\[(\d+)\]", tags or "")
        # the game's rule (story_prompt.py reveal_plan): the [n] tag, else the next unseen panel
        n = int(t.group(1)) if t else next((k for k in sorted(panels) if k not in seen), None)
        if n and n in panels and n not in seen:
            if page_of[n] != cur_page: out.append("> — *the page clears* —\n"); cur_page = page_of[n]
            out.append(f"> 🖼 *{sub(panels[n][1])}*\n"); seen.add(n)
        face = f" *({expr})*" if expr else ""
        out.append(f"**{sub(who)}**{face}: {sub(text)}  ")
    missing = [k for k in panels if k not in seen]
    if missing: out.append(f"\n> ⚠ panels never shown by a line: {missing}")
    return "\n".join(out)

chap = sys.argv[1] if len(sys.argv) > 1 else "01"
dst = f"{ROOT}/v3/chapter{chap}_script.md"
old = open(dst).read()
parts = re.split(r"(?m)^(## .*)$", old)
res = [parts[0]]
for i in range(1, len(parts), 2):
    head, body = parts[i], parts[i + 1]
    m = re.match(r"## (\d{4})", head)
    fs = glob.glob(f"{ROOT}/scenes/{m.group(1)}_*.md") if m else []
    if not fs: res += [head, body]; continue
    tail = re.search(r"(?ms)^\*[^*].*", body)            # the italic play note after the scene, if any
    res += [head, "\n\n" + scene_block(fs[0]) + "\n\n" + (tail.group(0).rstrip() + "\n\n" if tail else "")]
open(dst, "w").write("".join(res))
print("wrote", dst)
