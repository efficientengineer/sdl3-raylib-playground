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
