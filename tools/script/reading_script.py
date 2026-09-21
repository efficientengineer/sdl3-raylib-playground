#!/usr/bin/env python3
"""Moved: this is now `./story_prompt.py script [chapter]` (storytool/script.py), so it shares the
tool's scene parser, name table and reveal rule instead of keeping its own copies. Kept as a wrapper
because the old path is in people's shell history."""
import subprocess, sys
from pathlib import Path

sys.exit(subprocess.call([sys.executable, str(Path(__file__).resolve().parents[2] / "story_prompt.py"),
                          "script"] + sys.argv[1:]))
