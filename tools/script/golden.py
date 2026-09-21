#!/usr/bin/env python3
"""The refactor contract: run every story_prompt.py command in a throwaway copy of the
repo and record exactly what came out.

  tools/script/golden.py record <out.json> [--legacy old_story_prompt.py]
                                              run the suite, write the report (--legacy swaps in a
                                              pre-refactor single-file copy of the tool, so a
                                              before/after pair is taken over the SAME story files)
  tools/script/golden.py diff <a.json> <b.json>  compare two reports, exit 1 on any difference

The suite runs in a COPY of the working tree under /tmp, so nothing here touches the
repo: commands that rewrite story/packages, story/out or src/*.h do it over there.
A report holds, per command, the exit code and the normalised stdout/stderr, plus a
sha256 of every file under the generated trees afterwards. Byte-identical reports
before and after a refactor is the whole test.

Normalisation: absolute paths of the temp checkout become {ROOT}, and anything that
looks like a wall-clock duration is dropped, because those legitimately vary.
Stdlib only, like the tool it tests.
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve()
REPO = HERE.parent.parent.parent

# Every command the tool offers that produces files or stdout. Order matters: the
# ones that write run after the ones that only read, so a reader never sees a
# half-written tray.
SUITE = [
    ["shots"],
    ["--help"],
    ["nosuchcommand"],
    ["names"],
    ["stats"],
    ["stats", "--all"],
    ["script", "01"],
    ["check", "--all"],
    ["brief"],
    ["tmap", "check", "--all"],
    ["palette", "check"],
    # scene-level packages and previews (story/out)
    ["sheet", "story/scenes/0110_the_yard.md"],
    ["preview", "story/scenes/0110_the_yard.md"],
    ["refsheet", "Bron"],
    ["expressions", "Bron"],
    ["walker", "Bron"],
    ["sprites", "burr"],
    ["tmap", "preview", "halm"],
    # the tray, then the real cutters over the archived returns
    ["packages"],
    ["packages", "--all"],
    ["portraits"],
    ["ingest", "--force"],
    # exports last: they read what ingest produced
    ["export"],
    ["palette", "colormap"],
    ["palette", "check"],
    ["check", "--all"],
]

# Trees whose every file is hashed after the suite.
HASH_TREES = [
    "story/packages", "story/out", "story/panels", "story/portraits",
    "story/field/manifest.md", "story/field/tilesets", "story/refs",
    "story/palette", "src/cutscene_data.h", "src/field_text.h",
    "story/v3",
]

DURATION = re.compile(r"\b\d+(?:\.\d+)?\s*(?:ms|s|sec|secs|seconds)\b")
FLOATY = re.compile(r"\b\d+\.\d{3,}\b")


def norm(text, root):
    text = text.replace(str(root), "{ROOT}")
    text = DURATION.sub("{DUR}", text)
    return text


def copy_repo(dst):
    def ignore(d, names):
        drop = set()
        for n in names:
            if n in (".git", "build", "build-android", "node_modules", "__pycache__"):
                drop.add(n)
        return drop
    shutil.copytree(REPO, dst, ignore=ignore, symlinks=True)


def hash_tree(root):
    out = {}
    for rel in HASH_TREES:
        p = root / rel
        if p.is_file():
            out[rel] = hashlib.sha256(p.read_bytes()).hexdigest()
        elif p.is_dir():
            for f in sorted(p.rglob("*")):
                if f.is_file():
                    key = str(f.relative_to(root))
                    out[key] = hashlib.sha256(f.read_bytes()).hexdigest()
    return out


def record(out_path, legacy=None):
    tmp = Path(tempfile.mkdtemp(prefix="golden-"))
    root = tmp / "repo"
    copy_repo(root)
    if legacy:                      # run a pre-refactor copy of the tool over today's story files
        shutil.rmtree(root / "storytool", ignore_errors=True)
        shutil.copyfile(legacy, root / "story_prompt.py")
    report = {"commands": [], "files": {}}
    env = dict(os.environ, PYTHONHASHSEED="0", PYTHONDONTWRITEBYTECODE="1")
    for cmd in SUITE:
        r = subprocess.run([sys.executable, "story_prompt.py"] + cmd, cwd=root,
                           capture_output=True, text=True, env=env)
        report["commands"].append({
            "cmd": cmd,
            "code": r.returncode,
            "out": norm(r.stdout, root),
            "err": norm(r.stderr, root),
        })
        print(f"  {' '.join(cmd) or '(none)'} -> {r.returncode}", file=sys.stderr)
    report["files"] = hash_tree(root)
    Path(out_path).write_text(json.dumps(report, indent=1, sort_keys=True) + "\n")
    shutil.rmtree(tmp, ignore_errors=True)
    print(f"{out_path}: {len(report['commands'])} commands, {len(report['files'])} files")


def diff(a_path, b_path):
    a = json.loads(Path(a_path).read_text())
    b = json.loads(Path(b_path).read_text())
    bad = 0
    for x, y in zip(a["commands"], b["commands"]):
        label = " ".join(x["cmd"])
        for key in ("code", "out", "err"):
            if x[key] != y[key]:
                bad += 1
                print(f"DIFF {label}: {key}")
                if key != "code":
                    import difflib
                    for line in list(difflib.unified_diff(
                            x[key].splitlines(), y[key].splitlines(),
                            "before", "after", lineterm=""))[:40]:
                        print("   " + line)
                else:
                    print(f"   {x[key]} -> {y[key]}")
    if len(a["commands"]) != len(b["commands"]):
        bad += 1
        print("DIFF: command count")
    keys = sorted(set(a["files"]) | set(b["files"]))
    for k in keys:
        if a["files"].get(k) != b["files"].get(k):
            bad += 1
            print(f"DIFF file {k}: {a['files'].get(k, '-')[:12]} -> {b['files'].get(k, '-')[:12]}")
    print("IDENTICAL" if not bad else f"{bad} differences")
    return 1 if bad else 0


def main():
    args = sys.argv[1:]
    if len(args) >= 2 and args[0] == "record":
        legacy = args[3] if len(args) == 4 and args[2] == "--legacy" else None
        record(args[1], legacy)
    elif len(args) == 3 and args[0] == "diff":
        sys.exit(diff(args[1], args[2]))
    else:
        print(__doc__.strip())
        sys.exit(2)


if __name__ == "__main__":
    main()
