"""Writing a package: loose in story/out/, or into a folder of the art tray.

The same builders serve both shapes. in_package() is a context manager that redirects
pkg_path() at a tray folder and fixes the closing instruction; PKG_DIR is rebound by it,
so anything that reads PKG_DIR — package_label() — has to live in this module.

Must never do: know what kind of package is being written.
Public: attachments, pkg_path, in_package, package_label, return_lines, package,
PKG_FILES, RETURNED, RETURN_SUFFIXES. Imports: md, paths.
"""
from .md import existing
from .paths import OUT, ROOT



def attachments(style, cast, names, warn):
    """[(path, instruction)] in attach order: style reference first, then characters."""
    out = []
    sp = existing(style["refs"].get("style"))
    if sp:
        out.append((sp, "STYLE reference. " + style["refs"].get("style_note", "")))
    else:
        warn.append(f"no style reference at {style['refs'].get('style')}; the prompt text carries the style alone")
    for c in names:
        rp = existing(cast[c].get("ref"))
        if rp:
            out.append((rp, f"CHARACTER reference for {cast[c]['display']}. Keep the face, hair, outfit, and "
                            f"colors identical to this image in every panel {cast[c]['display']} appears in. Use it only "
                            f"for the character's design: ignore its background colors and its three-panel layout, "
                            f"and do NOT copy its calm neutral expression or head angle. Expressions come from the acting notes."))
        else:
            warn.append(f"no reference image for {cast[c]['display']} at {cast[c].get('ref')}; "
                        f"run: story_prompt.py refsheet {cast[c]['display']}")
    return out


# ── writing a package into a folder of story/packages/ ──
#
# The same builders serve both shapes. Loose in story/out/ (one writer, one scene at a time) the
# files keep their <name>.chatgpt.md / .sheet.json / .template.png names and the closing instruction
# is the matching `slice` or `cut` command. Inside a story/packages/ folder (the owner's tray, walked
# top to bottom) the names are fixed — prompt.md, sheet.json, template.png — and the closing
# instruction is always the same one: save the download here as returned.png, then run `ingest`.

PKG_DIR, PKG_TITLE = None, None                    # the package folder being written, and its heading


PKG_FILES = {"chatgpt.md": "prompt.md", "sheet.json": "sheet.json", "template.png": "template.png"}


RETURNED = "returned"                              # the file name the owner saves ChatGPT's image under


RETURN_SUFFIXES = (".png", ".jpg", ".jpeg", ".webp", ".heic", ".gif", ".tif", ".tiff")


def pkg_path(name, suffix):
    """Where one of a package's artifacts goes."""
    return (PKG_DIR / PKG_FILES[suffix]) if PKG_DIR is not None else (OUT / f"{name}.{suffix}")


def in_package(directory, title=None):
    """Context manager: write the next package into this folder, under the fixed file names."""
    import contextlib

    @contextlib.contextmanager
    def ctx():
        global PKG_DIR, PKG_TITLE
        was, PKG_DIR = (PKG_DIR, PKG_TITLE), directory
        PKG_TITLE = title
        directory.mkdir(parents=True, exist_ok=True)
        try:
            yield directory
        finally:
            PKG_DIR, PKG_TITLE = was
    return ctx()


def package_label(name):
    """What the manifest calls the package that asked for an id: the folder inside the tray if there
    is one, because that is what the owner opens; otherwise the loose story/out/ name."""
    return str(PKG_DIR.relative_to(ROOT.parent)) if PKG_DIR is not None else name


def return_lines(what):
    """The closing instruction inside a package folder: one file name, one command, always the same."""
    if PKG_DIR is None:
        return None
    rel = PKG_DIR.relative_to(ROOT.parent)
    return [f"Save ChatGPT's image into **this folder** as `{RETURNED}.png` — the whole path is",
            f"`{rel}/{RETURNED}.png`. A .jpg or .webp works too; the tool converts it.", "",
            "Then cut it up, from the repository root:", "", "```", f"./story_prompt.py ingest {rel}", "```", "",
            f"That writes {what}. Running `./story_prompt.py ingest` with no path does every package in",
            "`story/packages/` that has a new image waiting. The returned file is never deleted, so a",
            "bad cut can always be redone after a fix.", "",
            "If the image needs another go, reply in the same chat and save the new one over "
            f"`{RETURNED}.png`."]


def package(title, attach, prompt, after, status=()):
    rel = lambda p: p.relative_to(ROOT.parent)
    lines = [f"# ChatGPT package: {PKG_TITLE or title}", ""]
    if status:
        lines += ["## What exists already", ""] + list(status) + [""]
    lines += ["## 1. Start a new chat and attach these files, in this order", ""]
    lines += [f"{n}. `{rel(p)}`" for n, (p, _) in enumerate(attach, 1)] or \
             ["(no reference images found; see the warnings the tool printed)"]
    lines += ["", "## 2. Paste this prompt exactly", "", "````", prompt, "````", "", "## 3. Afterwards", ""]
    lines += after
    return "\n".join(lines) + "\n"
