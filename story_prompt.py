#!/usr/bin/env python3
"""Story scene -> image prompt builder, and the art tray.

This file is the entry point every script and every habit already types; the tool itself
lives in storytool/ (see storytool/README.md for the module map). Keep it a shim: usage
text, commands and behaviour all belong in storytool/cli.py and the modules it calls.
Stdlib only.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from storytool.cli import USAGE as __doc_usage__, main   # noqa: E402

__doc__ = __doc_usage__

if __name__ == "__main__":
    main()
