#!/usr/bin/env python3
"""Claude Code Stop hook: push this turn's assistant text to the in-game Messages tab."""
import json, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEV_MSG = os.path.join(HERE, "dev_msg.sh")
MAX_CHUNK = 480

def is_human(entry):
    if entry.get("type") != "user":
        return False
    c = entry.get("message", {}).get("content")
    if isinstance(c, str):
        return True
    return isinstance(c, list) and not any(b.get("type") == "tool_result" for b in c)

def main():
    try:
        hook = json.load(sys.stdin)
        path = hook.get("transcript_path")
        if not path or not os.path.exists(path):
            return
        entries = []
        with open(path) as f:
            for line in f:
                try:
                    entries.append(json.loads(line))
                except ValueError:
                    pass
        start = 0
        for i, e in enumerate(entries):
            if is_human(e):
                start = i
        texts = []
        for e in entries[start:]:
            if e.get("type") != "assistant":
                continue
            for b in e.get("message", {}).get("content", []) or []:
                if b.get("type") == "text" and b.get("text", "").strip():
                    texts.append(b["text"].strip())
        chunks = []
        for t in texts:
            for para in t.split("\n\n"):
                para = " ".join(para.split())
                while para:
                    chunks.append(para[:MAX_CHUNK])
                    para = para[MAX_CHUNK:]
        for c in chunks:
            subprocess.run([DEV_MSG, c], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=20)
    except Exception:
        pass

if __name__ == "__main__":
    main()
