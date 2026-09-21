# Art Tray

A small Mac app for the art loop: it hands you a package's **prompt and its reference images**, you
paste them into ChatGPT, and you bring the generated image back to the same window — it saves it in
the right folder and runs `./story_prompt.py ingest` for you.

It is a front end for `story/packages/`; it invents nothing. The queue, the prompts and the cutting
all come from `story_prompt.py`, so anything the tray does can still be done by hand.

## Build and run

```bash
./tools/arttray/run.sh            # builds if the source changed, then opens the app
./tools/arttray/build.sh          # just build → tools/arttray/ArtTray.app
./tools/arttray/run.sh --repo /path/to/repo      # use a particular checkout (remembered)
./tools/arttray/ArtTray.app/Contents/MacOS/ArtTray --selftest   # non-GUI checks, exits 0/1
```

Swift + AppKit, one file, no Xcode project and no dependencies — `build.sh` calls `swiftc` and writes
the bundle and its `Info.plist` itself. It needs the Xcode command line tools (`xcode-select
--install`). `ArtTray.app` is a built product and is gitignored.

**The first launch shows a macOS privacy prompt** ("ArtTray would like to access files in your
Desktop folder") because the repository lives under `~/Desktop`. Click **Allow** once; until you do,
the window is there but ignores clicks.

The repository is found by walking up from the app to the folder holding `story_prompt.py`, so the
app inside the checkout just works. `--repo` or **Choose repo…** (⌘O) overrides that and is
remembered in UserDefaults.

## The window

**Left — the queue.** Every package under `story/packages/`, in `story/packages/README.md`'s order,
then anything that page missed. Columns are step/kind (`×3` marks a package that is three
generations), the package path, and its status.

**The status is not worked out here.** `./story_prompt.py packages` decides it and stamps it into
each `package.json` (`status`, `status_why`); this app reads those fields, the README prints the
same ones, and the five words they may hold are defined once, in `storytool/packages.py`
(`STATUS_WORDS`) — see *"Package status: one source, two readers"* in `storytool/README.md` for the
words and what each means. `--selftest` asserts this app's `StatusWord` enum against
`tools/arttray/statuses.json`, which that command generates, so the two can never drift apart again.

`stale` is drawn in **red** with its reason beside it: it is art that exists but no longer matches
the description it was drawn from, and it is never counted as done.

One badge is the app's own, and it is not a status: **image waiting** (orange) — a `returned.png`
newer than the files it makes, i.e. `ingest` has not run yet. A multi-step package shows
`1/3 returned` (teal) until every image is back.

**Hide done** filters the queue. **Refresh** (⌘R) rescans, and so does bringing the window forward.

**Right — the selected package.**

1. *Attach these files, in this order* — the list from `prompt.md`, each with a thumbnail,
   **Copy** (puts the PNG on the clipboard as both `.png` and `.tiff`, so pasting into a browser
   works) and **Reveal in Finder**. A missing file is named in red.
2. *The prompt* — only the fenced prompt text from `prompt.md`, never the surrounding notes.
   **Copy prompt**, **Open ChatGPT**, **Open prompt.md**.
3. *Bring the image back* — a drop zone for an image file or image data, **Paste image from
   clipboard** (⌘V), and **Use newest image in ~/Downloads** (png/jpg/webp from the last hour, with
   the file name shown for confirmation). Below that, one line per step with a ✓ when its file is in
   the folder.

Then the package's checklist, so the result can be checked before it is cut.

**Steps.** Most packages are one generation. A `screen` package is three — PAINT → `returned.png`,
WALKABLE MASK → `returned_walk.png`, FOREGROUND MASK → `returned_fg.png` — declared in
`package.json` as `steps: [{title, prompt_index, return_file}]`. Those appear as a segmented control:
each step has its own prompt, its own return slot and its own ✓. Saving an image moves you to the
next step that is still empty. A package with no `steps` is treated as one step returning
`returned.png`, which is every package the tool writes today.

**Bottom — the log.** Everything the tray runs and everything those commands print. Failures also
show as a red status line under the toolbar.

## Where files go

- A returned image is written into the package folder under the current step's name
  (`returned.png`, `returned_walk.png`, …). Any previous one is kept as `<stem>_prev.png`.
  Whatever you hand it (TIFF from the clipboard, a JPEG file, a WebP download) is converted to PNG.
- Once **every** step of the package has its image, the tray runs
  `./story_prompt.py ingest story/packages/<path>` with the repository as the working directory and
  streams its output into the log. That command decides what the image is cut into.
- A non-zero exit leaves everything where it is: fix the image and drop it again, or fix the package
  and press **Cut it now (ingest)**.
- The tray never edits anything under `story/packages/` itself, and never deletes a returned image.

## The other buttons

- **Regenerate packages** — `./story_prompt.py packages`.
- **Hot reload** — `./fast_reload.sh`, to see the new art on the phone.
- Both run on a background queue with a spinner; the window stays live and only one runs at a time.

## Menus

⌘R refresh · ⇧⌘R regenerate packages · ⌘L hot reload · ⌘G open ChatGPT · ⌘O choose repo ·
⌘C copy (the text selection if there is one, otherwise the prompt) · ⌘V paste image · ⌘Q quit.

## `--selftest`

Opens no window. It parses a README, a `package.json` (one-step and three-step), a `prompt.md` with
several fenced prompts, derives every status from dates and from real folders in a temp directory,
round-trips a PNG through the pasteboard conversion, and then checks the real repository: every
README link points at a real package, every `package.json` has a kind, the first package has a
prompt, attachments and a checklist. Prints one line per check and exits 0 or 1.
