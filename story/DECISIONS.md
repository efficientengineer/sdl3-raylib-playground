# DECISIONS — log for the owner to review

The orchestrator makes story and scope decisions while the owner is away and records them here,
newest last. Each entry says what was decided, why, and how to reverse it. Disagree with any of
them and the work can be redirected; nothing here is expensive to change until art is generated.

## D1. Story base: Pitch C "The Erased Town", with imports from A and B
**Decided:** The story is built on `pitches/pitch_c_the_erased_town.md`. World: Ourn, the Sallow Basin,
failing skywells. Peoples: humans, Kell, Vess, Onn. Institution: the Clement Works, mining memory
("strands") out of the Seam under the Ancient Halls. Spine: the town of Tellwater was blanked in one
night and rebuilt with employees; Bron's warm childhood was issued to him and belongs to Maren Ostry,
the woman the company made and lost.
**Why:** All three writers independently chose memory-as-industry and Bron-as-the-lie, which says the
premise is sound. C has the strongest Phantasy Star IV feel (failing planetary systems, a grown
caretaker people, hunters among several peoples), ties the unreliable past directly to the pursued
figure (the Final Fantasy VII architecture, by an original route), and its evil is the most human:
paperwork, postings, and a chairman whose sin is that he can live with it.
**Imported from A:** the opening image of a company tent behind the ancient door; a company officer
who comes from the exploited people themselves (A's Cabre Sunn idea).
**Imported from B:** a patronless hunters' guild with a job board and optional side contracts, which a
twenty-hour game needs; the last image of a new job on the board.
**Not taken:** A's memory-burned-as-lamp-fuel (too close to a famous energy source); B's weather
machine and the "someone stays inside the machine forever" ending.
**To reverse:** pitches A and B are kept in `story/pitches/`. Say which you prefer and the bible is rewritten.

## D2. Lyra dies in Act IV (chapter 11), as Pitch C proposes
**Decided:** Keep it. She is one of two characters with approved art, and that art is used for eleven
chapters; it is not wasted. Her death is by a frightened young company officer, not a villain's
flourish, and Bron knowingly takes her last hour as the first memory he is sure is real.
**Why:** It is the emotional hinge of the memory theme and the most human kind of harm in the story.
**Risk:** a mid-game healer's death echoes both source games. The circumstances are original.
**To reverse:** swap the death to Pip (C flags this option) or cut it; Act IV needs a different hinge.

## D3. Scale and scene economy
**Decided:** Fifteen chapters in five acts, sized to a Phantasy Star IV length game. Three scene types:
panel cutscenes (one ChatGPT sheet each, about 5-8 per chapter, for moments that earn them), talk
scenes (dialogue box with speaker portraits, no new art), and narration. Target for the whole game:
roughly 90-110 panel scenes and 150+ talk scenes.
**Why:** At one generated sheet per panel scene, 100 sheets is a realistic amount of art for one
person; 300 would not be. Talk scenes carry banter, town business, and aftermath, as the source game does.

## D4. Tools added this run
**Decided:** A tools agent added `type: talk` scenes, a `portraits` command that crops each character's
head-and-shoulders portrait out of their reference sheet, portraits beside the dialogue box in the
game, and an example talk scene after "The Warning". Compiled and validated; not yet seen on the phone.

## D5. Model
The owner asked for Opus 4.6 subagents. The agent tool only offers "opus", which resolves to the
current Opus model. All subagents in this run use that.

## D6. Story bible accepted; working title "The Fair Copy"
**Decided:** `story/bible.md` (about 22,600 words) is accepted as the source of truth for all later
writing. Working title: **The Fair Copy** (the company's file name for Maren Ostry, and what Bron's
childhood is). The bible logs fifteen of its own decisions in its section 16; the ones most worth your
eye are: Junior Director Vane renamed **Ottoline Kerrow** (name collision with a Final Fantasy
villain); Bron's own past was **erased, not stored**, so he can never be restored; and the hard rule
that **nothing in this world is ever copied**, only subtracted from someone.
**Why accepted:** its sixteen hard rules for the Seam keep the evil human (the Seam never acts or
intends; inversion kills nobody and leaves calm survivors who cannot say their names; nothing is
undone at the end). Lyra's death is caused by one man's act of conscience and another's paperwork,
not by a villain. The guild master is an Onn older than the company, which gives the guild weight.
**Not yet done:** the game's title screen still says "Ancient Halls". Left until you confirm a title.
**To reverse:** edit the bible; every later file is derived from it.

## D7. Scene file naming for the full game
**Decided:** new scenes are named `CCSS_slug.md`: two digits of chapter, two of order within the
chapter, in steps of five so scenes can be inserted (for example `0310_the_welcome.md`). The existing
`001`, `002`, `003`, `003b`, and `p01` keep their names because scene 003's approved panel art is
keyed to its file name. `story/playlist.md` lists every chapter's scenes in play order; the game only
plays the `## intro` list, so art-less chapters never show up on the phone by accident.

## D8. Outlines accepted; cross-act rulings
Five act outlines came back: **322 scenes** (111 panel, 188 talk, 23 narration) across fifteen chapters
and an epilogue, in `story/outline/act1.md` to `act5.md`. The cast file has 34 characters with looks
that pass the art tool; `story/locations.md` has 227 ready-to-paste location lines for 32 places.
The outliners raised conflicts. My rulings:
1. **The Tally's fifteen questions are frozen** using Act V's text (question 7 is "What happened the day
   before this one?", the same test that exposes a set memory; question 15 is "What do you want done?").
   They are heard in chapter 1, found cut into a wall in chapter 5, read from a maintenance manual in
   chapter 9, left unfinished at Lyra's death in chapter 11, and used by Bron to hold Maren together in 15.
2. **Maren holds nothing of Bron's own past.** The bible contradicted itself; its decision 4 governs. The
   winter she recites is her own childhood, drawn from her in 1093 and set into him.
3. **Anneke Brae, Bron's real mother, is Kell** (the bible listed her as human; Bron is Kell).
4. **Scene ids** may use any two-digit order number, not only steps of five, so a chapter can exceed twenty scenes.
5. **Ket's dimming temple lights** get one central ledger so three acts do not each spend them.
6. **Side contracts** invented by outliners for chapters 6, 11, and 13 are accepted. "Bad water at Kettle"
   is posted in chapter 2 and can only be finished in chapter 6.
7. **Optional panel scenes cost art**, so at most one per chapter; the rest become talk scenes.
8. Accepted as canon from Act V's notes: giving the record to the Vess chant-lines empties Maren because a
   stored pattern must be rehearsed to stay coherent, and once a line rehearses it she no longer has to
   (nothing is copied); "taking the hour" is an act between two people, not a device.
9. Act I's plan for the existing scenes: prologue revised (the party is never split: the guild's fourth
   Line is "bring them back"), scene 001 replaced, 002 revised ("old magic" is a banned idea in this
   world), 003 and 003b kept. Zeph and Pip are just up the corridor during 003.
**To reverse any of these:** tell me which; the story editor's `story/canon.md` is where they are applied.

## D9. Story editor pass accepted; writing order and three notes to all scene writers
A story editor reconciled the five outlines (written in parallel) into one story and wrote
`story/canon.md`, which every scene writer reads after the bible. It found and fixed six real defects:
thirteen duplicate scene ids, a side contract that could never be completed, two payoffs with no setup
anywhere (the Vess second tone; the guild's Four Lines on the board frame), an impossible calendar in
Act IV, and a miscount of Ket's lights. Final count: **323 scenes** (111 panel, 189 talk, 23 narration).
**Writing order** follows quotation, not chronology: chapter 1 alone first (it owns the fifteen
questions, the Four Lines, Bron's winter story), then 4, 5, 11; then 2, 3, 6, 10; then 7, 8, 9, 12;
then 13, 14, 15; then the epilogue.
**My notes to all writers, from the editor's risk list:**
1. Bron's winter story is over-planted. It is told once in chapter 1 and retold once more only (chapter 5), not in chapter 4.
2. Six courteous, partly-right company people blur together. Keep their temperatures distinct: Cadder
   warm and funny, Kerrow cold, Ovey brisk to the point of rude, Teach slow and heavy, Crewe tired and
   courteous, Sark mildly annoyed.
3. Chapters 7-10 each end on a document. Chapters 8 and 9 must lead with their physical set-pieces and
   keep document beats short.
**Known risks I am accepting** (see `story/notes/editor.md`): a healer's death two-thirds through is the
most recognisable beat in both source games, saved only by its paperwork cause; chapter 13 is the
thinnest chapter; 111 panel sheets is at the top of the art budget.

## D10. Run complete
**Decided:** The story is finished. **326 scenes — 111 panel, 192 talk, 23 narration — 689 panels and
3,628 dialogue lines across fifteen chapters and an epilogue**, with 54 cast entries, 311 location
keys, and a current ChatGPT package (165 files) for every panel scene and every reference sheet in
`story/packages/`. `check` passes on all 326 scene files (only `000_TEMPLATE.md` fails, by design);
`export` and `stats` succeed; the exported intro is unchanged and `003_the_warning`'s art is untouched.

**The phases.**
1. **Pitches and tools** — three independent story pitches; `type: talk` scenes and dialogue portraits added to the tool.
2. **Bible** — `story/bible.md` (~22,600 words) from pitch C, accepted in D6.
3. **Design and outlines** — five act outlines, the full cast, and the location book, written in parallel.
4. **Editorial merge** — one story editor reconciled the five outlines and wrote `story/canon.md`, which outranks the bible and the outlines from here on.
5. **Scene writing** — sixteen chapters written in five waves ordered by quotation, not chronology; chapter 1 first and frozen on delivery, the epilogue last.
6. **Passes** — a continuity editor over all 326 files, the playlist, the branch splits and the speaker list; then a script doctor on the ten weakest scenes and chapter 1.
7. **Close-out** — packages regenerated, `story/REVIEW.md` written, this entry, `CLAUDE.md` and `plot.md` updated.

**Agents: thirty-four Opus subagents**, one brief each, exclusive file ownership, no agent ever
committing. Three pitch writers · one bible writer · five act outliners · two cast designers · two
location designers · one story editor · sixteen chapter writers · two tools agents · one continuity
editor · one script doctor. The orchestrator assigned, merged, ruled on conflicts, and made every
commit.

**What the owner reads:** **`story/REVIEW.md`** — what was made, the story in one page, the cast, the
numbered list of decisions that need a yes, what to generate first in ChatGPT, what plays on the phone
today, the known risks, and the suggested next steps. Everything else hangs off it.

**To reverse anything:** every ruling in this log and in `story/canon.md` says how. Nothing is
expensive to change: no art exists except scene 003's seven approved panels.

## D11. Owner's verdict on the first draft: rejected on feel
The owner read REVIEW.md and could not tell what the story was: "an AdLib with a ton of proper nouns",
nothing like Phantasy Star IV or FF7 in feel. Diagnosis: the orchestrator's brief over-rewarded invented
vocabulary and literary restraint (subtext, reasonable villains, no theme lines), which produced quiet
paperwork drama. The source games are earnest, direct, loud, with a handful of words, monsters, vehicles,
villains with presence, and a set piece in the first ten minutes.
**Decided:** before any rewrite, produce a plain-language feel-first treatment (no invented names) and a
rewritten opening hour in that voice, for the owner to judge. The existing draft stays in the repo as raw
material (cast designs, art pipeline, structure) but is not the plan.

## D12. Story v3 direction accepted with corrections
Owner: chapter one in game form is "leaps and bounds better". Corrections: still some AI smells; stop
re-using legacy beats (the warning guard, the door that opens for Bron) just because art exists; clips
are too long, cap at about ten lines; make the gameplay between clips explicit (what you do, how long,
what it unlocks). The old draft (bible, outlines, 326 scenes) is now reference only.

## D13. Names are tokens
Owner: scenes must be written with token replacement in mind, because names will change often. Rule:
every proper noun in story text is a token like `{{HERO}}` or `{{HOME_TOWN}}`, including speaker labels;
`story/v3/NAMES.md` maps tokens to current names; the tool substitutes when exporting to the game and
when building ChatGPT packages, and can render a readable copy for review.

## D14. The world is 2.5D, Octopath style; props are sprites, only surfaces are tiles
Owner (2026-09-18): after chapter one's play blocks were accepted, "we need to be able to walk around
the world." Chose 2.5D (3D ground/walls/heights, fixed-angle perspective camera, pixel-art billboards)
over PS4-style flat top-down, and ChatGPT-sliced sheets over PixelLab or placeholders for the art.
Owner's refinement: tile only the big surfaces (ground, floors, walls); everything else is a sliced
sprite, which allows more interesting shapes and compresses well. Contract in `FIELD.md`. To reverse:
the map format and art folders survive a switch to flat top-down; only the renderer changes.

## D15. Painted backgrounds over the 3D block-out (FF8 style)
Owner (2026-09-18): "a true 3D game might be limiting … use an ortho camera, render the scene as 3D,
then slice it out so we can layer it, only give it the feel of 3D, like FF8." Adopted as: the 3D field
is the block-out. Each fixed camera zone can be captured from the game, painted over by ChatGPT
(`story_prompt.py view <map> <zone>`), and the painting is drawn as the zone's backdrop while the
block-out renders to depth only, so sprites layer per pixel without hand-made masks. Zones without a
painting keep the live 3D; ortho or perspective per zone. Navmesh, zones, triggers and all authored
maps are unchanged. To reverse: delete the paintings; the 3D is still there.

## D16. ChatGPT paints the screen; the game is fitted to the painting
Owner (2026-09-18), hours after D15: block-outs still mean we build the place first. "I want ChatGPT to
basically generate the map." Adopted: a field screen is one painting plus two binary masks requested in
the same chat (walkable = green/black; foreground = white/black), because the owner's hand test showed
a three-colour mask fails (an invented yellow class, line art left in) while the painting itself was
excellent. The tool derives the navmesh (screen-space convex polygons), occluder cut-outs with base
lines for y-sorting, exits at frame edges, and a walker scale by screen y. The 3D field (D14) and
painted block-outs (D15) stay available where exact layout matters (the Stair). To reverse: maps
without a `.screen` file fall back to 3D automatically.

## D17. The field is 2D top-down, Phantasy Star IV style
Owner (2026-09-19): "I'm over trying to do this like FF8. Why don't we just go with the original 2D
like Phantasy Star 4." After a day of 2.5D (D14), painted block-outs (D15) and angled painted screens
(D16), the angled view's cost was all in occlusion: ChatGPT cannot paint ground hidden behind objects,
and foreground masks merge into blobs. Top-down removes the problem: nothing is walked behind. Kept from
D16: ChatGPT paints the whole map; one walkable mask gives the navmesh; exits at frame edges; the
screen-map engine, the tray app's multi-step packages, `ingest`. Dropped: foreground mask, base map,
depth scaling. Added: a stated walker height per map, 4-direction facing, a trailing party member, an
optional "overhead" overlay for arch tops and bridge decks, door notches. The 3D field, sweeps,
captures and painted block-outs are parked in the code, not deleted. To reverse: they still load.

## D18. Sega-style tile maps and grid walking. Final.
Owner (2026-09-19), on seeing the painted top-down Halm: "I don't like it. I want Sega style, top down,
using tile sheets, with walking based on a grid for the most part. No more screen navmesh." Contract in
`TILES.md`: 32-px tiles, 640x360 view, one-tile steps in four directions, snake followers, tile-flag
collision, multi-tile stamps for buildings and trees with `over` rows to pass behind, three rotated
fringe tiles per terrain instead of an autotile set, text maps (`.tmap`) authored by hand, tile sheets
generated by ChatGPT through the numbered-slot template pipeline at 4x and packed into an atlas, with
procedural placeholders so maps are walkable before art. New engine module `src/tilefield.*`; the 3D
field (D14), painted block-outs (D15) and painted screens (D16, D17) are parked in the code. What the
detours left behind that is still used: text ids, interact-press with "!", name tokens, the packages
tree, `ingest`, the tray app, magenta keying, the Mac capture path, `map.flag`, the map design rules.

## D19. Everything is 256 colours, one byte per pixel
Owner (2026-09-19): "What if we did just a 256 color palette and kept each image to one byte per pixel?
We could use this palette across all ChatGPT generations … the cutter could do that." After the
side-by-sides (`tools/palette/`): "They all look good. Let's go ahead." Contract in `PALETTE.md`: one
master palette for field + cast (index 0 transparent, 1 black, 2 white; material ramps), a palette per
cutscene scene, nearest colour in Oklab, no dithering (every mode tested made this art worse), hard
alpha, indexed PNGs, conversion as the cutter's last step. The engine draws R8 index textures through a
colormap texture with day/dusk/night and 32 light levels, blending colours (not indices) so full-res
art still scales smoothly. Lighting, time of day, lanterns, palette cycling and status tints all come
from tables. To reverse or revise: every shipped image is re-derived from `story/sheets/` by
`ingest --force`.

## D20. TILES2: terrains through dual-grid masks, swatches, decals; measured art sizes; 9-frame walkers
Owner (2026-09-19): the first tile art was "too noisy"; chose a flat, luminous cel style from a reference
painting, asked for "a more robust tile system … look up state of the art practices", overlay props on a
toned-down base, buildings at least 2x2 that fill their footprint, mirrored walk frames, and smaller
assets where detail allows. Adopted from `TILES2_PROPOSAL.md` (research with sources) and
`tools/sizes/REPORT.md` (measurements): terrain ids stay on the world cells; ground is drawn on a dual
grid through procedurally generated 1-bit masks (16 corner cases → 6 classes by rotation; the Age of
Empires II blendomatic model) over one seamless world-space swatch per terrain, by priority; hashed,
clustered decals; macro light drift and cloud shadows through the colormap; per-tile flags (`pass`,
`tag`); h-flip for nature stamps only. ChatGPT now draws one swatch per terrain and one decal sheet per
biome instead of matched transition tiles. Sizes: atlas cell 128, walker frame 128x192 in a 3x3 sheet
(side row mirrored), portraits 288 tall, panels at display size. Grid walking, collision, stamps and the
text maps are unchanged. To reverse: the v1 atlas and fringe code are in git at 3bd149c.

## D21. Building kits and square structural terrain (parked with the tile field)

Designed 2026-09-19 for the tile field; the half-built work is on branch `parked/tiles2-wip`. Superseded by D22.

## D22. The field is a voxel world with sprites

Owner (2026-09-19, after a day of TILES2 bugs): "Let's try a completely different system, a voxel and
sprite approach. World mostly rendered like Minecraft, but we have sprites for characters and detail."
Then: Octopath-style look; half-size voxels ("4 blocks should fit into the smallest block size"); shadow
map; generated navmesh with fully free analog movement and jumping. Engine: `src/voxfield.*`,
`src/VOXFIELD_NOTES.md`. The world is generated at load from the `.tmap` text maps (terrain → blocks,
rule-based houses, trees, water channel, reach proof). Flat palette colours + procedural shader detail;
D19's palette still rules. Nothing is pushed to the owner's phone unless they ask. To reverse: the tile
field is still in the build behind a Dev button.

## D23. The story is written as we go

Owner (2026-09-20): "I want to write as we go and explore what feels natural instead of trying to
follow a prescribed story." So `story/v3/PREMISE.md`'s fifteen beats stop being a plan. They are a pool
of ideas the owner may draw from or ignore. **Canon is only what is on screen in a finished chapter**
(plus `NAMES.md` and the character entries those chapters rely on). Process per chapter: talk it through
with the owner (ideas collected in `story/v3/BRAINSTORM.md`, no agents) → one writer pass when the owner
says write → cold read by an agent who knows nothing → owner reads `chapterNN_script.md` → revise.
No planting for a future that is not decided (e.g. Hart's death, the workshop book as inheritance): a
detail stays if it is good in its own scene. `story/v3/THREADS.md` keeps the short list of what finished
chapters have promised the player, so discovery writing does not drop its own setups. Art, maps and
level generators are only built for chapters that are written. To reverse: PREMISE.md is untouched.

## D24. The legacy is deleted, not archived

Owner (2026-09-21): *"Before anything else, clear my art tray tool. Make sure we have no legacy
packages, only things that are in the new game, and ensure we're clear so I don't redo any work. …
Remove all legacy content and systems. No sunk cost fallacy."*

**Everything below is recoverable from the git tag `legacy-final`, which is the pre-cleanup state of
the whole repository.** Nothing was copied into an archive folder; the tag is the archive.

**The art tray now generates five kinds and nothing else** — `cast/<name>/refsheet|expressions|walker`,
`chNN/sprites`, `chNN/scenes/<scene>` — because the voxel world's ground, walls and buildings need no
generated art at all. `story/packages/README.md` is one ordered queue with a computed status a row.
Scenes are filed flat under the chapter; the per-map folders went with the per-map art.

**New:** `story/field/sprites.md` and `./story_prompt.py sprites` — the billboards (creatures,
animals, the training machines), reusing the magenta template machinery of the old `props` command,
with per-frame animation and a shared trim box. Contract in `WORLD.md` §2.

**No redone work.** A package's `package.json` now records a fingerprint of the inputs it was built
from, and `ingest` stamps the fingerprint it cut at, so `stale` is computed rather than guessed. The
handful of packages cut before fingerprints existed are ruled on by hand in `LEGACY_STATUS`.

**Removed from `story_prompt.py`** (about 2,250 lines): the `tiles`, `props`, `building`, `view`,
`screen` and `tileset` package generators, the painted-screen navmesh derivation, the painted-view
fitter, and the single-page `build` mode. **The tileset CUTTER stays** so the valley atlas can still
be re-cut from `story/sheets/` if the palette is refitted — verified byte-identical.

**Removed from `story/`:** `scenes_rejected/`, `outline/`, `canon.md`, `bible.md`, `plot.md`,
`locations.md`, `REVIEW.md`, `TREATMENT.md`, `pitches/`, `v4/`, `v3/tableread/`, `v3/variants/`, 36 of
40 agent reports in `notes/`, and the parked field data and art (`screens*`, `views`, `maps`,
`props*`, `tiles.md` + `tiles/`, `buildings*`). `v3/sidestories/PITCHES.md` became
`v3/ideas_interludes.md`. `characters.md` was cut from 54 entries to chapter one's seven.
`story/panels/` and `story/sheets/` lost the retired scenes' art. **`003_the_warning`'s art went with
them** — it was flagged as canon in an earlier memory, and it belongs to a scene the v3 rewrite
retired; `git checkout legacy-final -- story/panels story/sheets` brings it back.

**Removed from the root:** `FIELD.md`, `TILES.md` and `TILES2_PROPOSAL.md`, folded into the new
short **`WORLD.md`** (what needs art, the `.tmap` format, the map design rules, the asset sizes from
the retired `tools/sizes` study). `PALETTE.md` lost the tile-shader specifics. `tools/palette`,
`tools/sizes`, `tools/tiles2` and `tools/script` are gone; `tools/arttray` stays. `CLAUDE.md` was
rewritten to describe the project as it is.

**`story/playlist.md`'s `## intro` list is gone** — the game plays chapters, New Game starts at the
first scene of `## chapter01`, and `export` emits `CS_CHAPTERS[]` over every `## chapterNN` list. A
panel scene with no art is exported as placeholder boxes carrying each panel's description instead of
being skipped, so a chapter always plays end to end.

**Kept on purpose:** `story/packages/tilesets/valley/**` and the `tilesets-valley-*` sheets in
`story/sheets/` (the voxel field reads that atlas every frame; they are frozen and off the queue),
the stale Falke and Ottilie walkers (so the demo has sprites until they are redrawn), and
`villager_a`/`villager_b`.

**To reverse:** `git checkout legacy-final -- <path>` for anything named above.
