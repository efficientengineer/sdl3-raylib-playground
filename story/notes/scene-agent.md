# scene-agent — chapter one as scene files, and the field art lists

Brief: turn the seven clips of `story/v3/chapter01.md` into scene files the art pipeline can build
from, add the four new faces to `characters.md`, put the new order in `playlist.md`, and give every
prop and tile a map so an art run can be ordered by map. Files written: `story/scenes/0110`-`0170`,
`story/characters.md`, `story/playlist.md`, `story/field/props.md`, `story/field/tiles.md`, this note.
Nothing committed, nothing under `src/`, `story/v3/` or `story/packages/` touched.

## 1. The seven clips: panels or talk, and why

| # | Scene | Kind | Map | Panels / lines |
|---|---|---|---|---|
| 1 | `0110_the_board` | panels | halm | 6 panels, 2 pages (3+3) |
| 2 | `0120_youre_not_ready` | panels | hart_yard | 6 panels, 2 pages (3+3) |
| 3 | `0130_he_signs` | talk | hart_yard | 6 lines, backdrop `0120_youre_not_ready:1` |
| 4 | `0140_the_sword` | talk | hart_yard | 6 lines, no backdrop |
| 5 | `0150_the_road_west` | panels | bridge | 6 panels, 2 pages (3+3) |
| 6 | `0160_the_fire` | talk | ridge_camp | 6 lines, no backdrop |
| 7 | `0170_the_jar` | panels | stair_shrine | 5 panels, 2 pages (2+3) |

Panel scenes are the four that introduce something the player has not seen: the hall and the empty
board, {{MENTOR}} on his roof, {{RIVAL}} on the bridge wall in the coat, and the shelf under
{{STAIR}}. Each is one shot sheet. The three talk scenes are the ones that happen in a room or a
camp the player has just walked through — the ladder foot, the kitchen, the fire — where a manga page
would repeat what the field already showed. `0130` takes the yard establishing shot from `0120` as its
dimmed backdrop, which is free: it is the same place ten minutes later.

`0140_the_sword` gets no backdrop because no panel scene is set in {{MENTOR}}'s kitchen and a talk
scene may only borrow a panel that exists; black is the right answer for one room at night.
`0160_the_fire` gets none for the same reason — **and this is the one place where a fifth panel scene
might be worth it**. The ridge camp is where {{STAIR}} first comes into view and `chapter01.md` says
"the camera holds on it". I have left that as a field camera moment rather than inventing a panel
scene the brief did not ask for; if the owner wants it as art, it is one `establishing_tall` of the
stair against the night sky and `0160` can then carry it as a backdrop.

**Panel counts are capped by dialogue, not by taste.** The game reveals at most one panel per
dialogue line (`reveal_panel` in `src/star_logic.cpp`, one `reveal` field per `CsLine`), so a panel
past the last line can never appear on screen. Every clip in `chapter01.md` is five or six lines, so
six panels is the ceiling and `0170` (five lines) gets five. That is why none of these hit the 6-8
shots `STYLE.md` asks for in "a scene that matters" — the six-line clip rule and the 6-8 panel
guideline are in tension, and the dialogue won.

Composition: every page has an anchor and a punch, no two neighbouring panels share a scale, and each
page uses three different panel shapes. Each scene has a `- staging:` line fixing screen direction
(`0120`: the roof is up and right, always; `0150`: {{RIVAL}} is up on the wall at the right, always)
and an acting line for every cast member in every panel.

Dialogue is verbatim from `chapter01.md`. The only additions are `[n]` reveal tags and `{mood}` tags.
Two deletions, both stage directions rather than speech, because the game prints the whole string
after the colon into the dialogue box and would have drawn the brackets on screen:

- `0130`, {{MENTOR}}'s line: `[signing the job over to him]` removed.
- `0170`, {{HEALER}}'s line: `[reading the receipts nailed inside the door]` removed.

Both survive as prose in the scene's `## Beat`, which is what the artist and the field designer read.
If the owner wants them back, they belong in a bracket line in `chapter01.md`, not in the spoken text.

## 2. Cast

- `## Bron` gains `- alias: Falke`; `## Lyra` gains `- alias: Ottilie`. No other line of either entry
  was touched: their looks and reference sheets are approved art.
- Four new entries under a new `## The valley (v3, chapter one)` section, each with a `look` that
  obeys the design direction (no race, class or beard words; age in posture, hair and dress) and a
  `- ref:` path for a sheet that does not exist yet:
  - **Hart** ({{MENTOR}}) — sixty, stocky, straight-backed, stiff left leg with the knee wrapped in
    grey cloth, iron-grey hair cropped and brushed flat, slate-blue work coat with the sleeves rolled,
    tool belt with a claw hammer. The hammer and the rolled sleeves are the character: he is on a roof
    in two of his three scenes.
  - **Stolz** ({{RIVAL}}) — nineteen, tall and loose, honey-blond, a new wine-red coat with a gold
    shoulder cord, a fat purse, and a punched gold coin of the western city hung at his throat. Every
    expensive thing on him is new, which says *this man got paid this week* before he speaks.
  - **Guildclerk** (speaker label **Clerk**) — slight, upright, hard side parting, charcoal tunic
    buttoned to the throat, ink on two fingers, pen behind the ear. See §4 for why the handle is not
    `Clerk`.
  - **Shrinewoman** (speaker label **Shrine Woman**) — small, stooped, straight neck, white hair in a
    flat coil, undyed wool robe over a faded green underdress, tin dipper on a rope belt. A villager
    beside a gift stall, not a seer.
- Handles are single words that cannot false-match inside panel text, following the `Nona`/`Nine`
  precedent already in the file; the display names ride on `- alias:` lines, so the game prints
  "Clerk" and "Shrine Woman" and the portrait still resolves.

## 3. What `chapter01.md` leaves open for staging

None of these blocked the work; they are choices I made that the owner may want to rule on.

1. **Where the board clip happens.** The bracket says "the guild hall" and the Play block says the
   board is on the wall beside the doors. I put the conversation at a counter inside with the board
   behind the Clerk, so the last sheet is in frame for the whole scene. If the board is meant to be
   outside on the porch, panels 2 and 5 move outdoors and the `guild_hall` prop needs its board face
   drawn to be readable at field scale.
2. **Who is in the hall.** `SMELLS.md` bans counting a crowd in a stage direction, so I wrote the hall
   as good as empty at the end of the day. That also makes the Clerk's unhurriedness read.
3. **Where {{HEALER}} is during *The Board*.** She is not in the clip and I did not add her; she first
   appears holding the ladder in `0120`. If the owner wants her with {{HERO}} from the first frame,
   `0110` needs a line for her and the clip is no longer verbatim.
4. **The kitchen.** *The Sword* says the sword is already on the table with a stone beside it, and the
   Play block gives the room a propped door and a goat outside. As a talk scene none of that is drawn;
   it is all in the field map. Listed here so the field agent does not lose the goat.
5. **The receipts.** `0170` has {{HEALER}} at the shrine door reading the papers while {{HERO}} is at
   the shelf — two places at once for one staging line. I fixed screen direction as "step and shelf
   right, houses and door left, everyone faces right" and gave the papers their own `object_insert`
   with only a gloved hand in it, the way `003_the_warning` handles Lyra's hand on the shoulder plate.
6. **The second jar.** Last year's jar is still sealed, so it must read as *older*, not *opened*: the
   panel says dusty beside new. Worth a line in the art review.
7. **Night under {{STAIR}} and the moon moving west** (the last Play block) has no clip and I wrote
   none, because the brief listed seven clips. It is the chapter's closing image and currently has no
   scene file of any kind.

## 4. `check` failures and tool collisions

Run: `./story_prompt.py check story/scenes/01[1-7]0_*.md`.

**All seven scenes pass.** Current output is seven `ok` lines and three warnings (see (d)).

**(a) Was failing mid-session, now fixed in the tool — token names that are aliases.** While these
scenes were being written, `{{HERO}}` substituted to **Falke**, which is an *alias* of the handle
`Bron`, and `names_in()` matched handles only. Every panel that named {{HERO}} or {{HEALER}} was
therefore read as naming nobody: four scenes, thirteen `R11 acting: ... who is not named in the panel`
errors, and — worse than the errors — Bron's and Lyra's `look` lines would have been left out of the
prompt. I wrote the scenes in tokens anyway, as briefed, and verified them by substituting handles
into scratch copies. The tool agent has since landed `name_matches()`, which matches a handle plus
any alias that a `NAMES.md` token currently resolves to, and all four scenes went green with no edit
to them. `./story_prompt.py sheet story/scenes/0110_the_board.md` now produces a package that calls
him Falke and attaches `story/refs/bron.png`, which is the behaviour we want. Worth knowing the
failure mode exists: **a character whose token name is not a handle and not an alias silently loses
their `look`**, and the only symptom is a warning about a listed character never being named.

**(b) Resolved by naming — `Clerk` as a handle breaks four older scenes.** A handle is matched as a
whole word inside panel text, and `0481_the_standing_notices`, `1275_the_confession`, `0692_the_payroll`
and `0445_the_meddra_well_head` all describe "a clerk" in a panel. With `## Clerk` in the cast file
all four fail `check` with R6 and R11 errors. I used `## Guildclerk` with `- alias: Clerk` instead, so
the game still prints "Clerk", the portrait still resolves, and `check --all` is green: 333 scenes ok,
none rejected. Reversing this is a two-line edit if the owner would rather have the short handle and
let the retired scenes fail. One cost of the longer handle: **`Clerk` is not a token name**, so
`name_matches()` does not match it in panel text and the panels have to write `Guildclerk` for his
look to be inserted. The generated prompt therefore says "Guildclerk" where an artist would say "the
clerk"; the dialogue box still says Clerk.

**(c) `the {{STAIR}}` renders as "the the stair".** `{{STAIR}}` has no name in `NAMES.md`, so `detok`
expands it to the phrase *"the stair"* — article included. `chapter01.md` writes "out to the
{{STAIR}}", and I kept that line verbatim, so `0110`'s third line currently reads *"The water run out
to the the stair."* on screen. It is a one-character-class fix in one of three places: drop the leading
article from `token_phrase`, give {{STAIR}} a real name in `NAMES.md`, or grep the v3 files for
`the {{` and delete the article there. **This affects the shipped text, so it should be fixed before
the intro is exported.** My own prose writes `{{STAIR}}` bare, never `the {{STAIR}}`.

**(d) Warnings that are expected and need no action.** `backdrop panel
0120_youre_not_ready_p1_establishing_wide.png has not been generated yet` (that is what the packages
are for), and `{{STAIR}} has no name in NAMES.md, so the text reads 'the stair'` on every scene that
mentions it.

## 5. Playlist

`## intro` is now the seven scenes in clip order, and the same list is repeated as `## chapter01_v3`.
The old `## chapter01`-`## chapter16` lists are untouched.

**No narration prologue.** Two reasons: the brief numbers the seven clips 0110-0170 and a prologue
would need a stem outside the files I own; and the chapter is stronger opening cold on the bell and
the empty board, which is how Phantasy Star IV opens on a job. If the owner wants one, it is four
lines over black before `0110` and it should state only what the player cannot be shown: the year's
jar, who carries it, and that no convoy came.

Until the sheets exist, `export` skips the four panel scenes with a warning and the intro plays as the
three talk scenes. Art priority is the order the player meets them: **0110, 0120, 0150, 0170**.

## 6. Field art by map

Every entry in `props.md` and `tiles.md` now carries `- map: <id>[, <id>]`, so "everything to generate
for chapter one" can be listed per map. Fifteen new props and six new tiles, all from `chapter01.md`:

- **halm** — added `scale_bench` (the teaching find sits on it) and `yard_wall` (the fork: shove
  through the queue or go over the wall).
- **west_road** — added `culvert` (the washed-out crossing, the rope hazard, and the drain the tooth
  is in). `milestone` already existed and stays: the bestiary's milestones stand in a row of real ones,
  so the real marker is load-bearing. Tiles `road`, `river_bank`, `old_road`.
- **bridge** — added `bridge_rail`; {{RIVAL}} stands on it in `0150`. Tiles `road`, `river_bank`.
- **north_grass** — added `cave_mouth` (the roped descent to knuckles) and `slide_rubble` (the
  rockslide that forces the camp). Tiles `grass_tall`, `old_road`, `scree`.
- **ridge_camp** — added `campfire`, `bedroll`, `ridge_rock`. Tile `scree`.
- **stair_shrine** — added `shrine_house`, `stone_shelf`, `water_jar`, `step_rope`, `sitter`,
  `rubbing_stall`. Tile `stair_stone`.

**Ford stones are gone** and no prop was written for them: the crossing in `chapter01.md` is the
bridge, and the broken culvert is a hole in the road, not a ford.

**The bottom step of {{STAIR}} is not a prop.** It is a staircase wider than a town stopping thirty
feet above the field: hundreds of cells across and taller than the camera at 64 pixels to the cell. It
has to be map geometry — a run of the new `stair_stone` wall tile for the face of the step with the
mass above carried by the map backdrop — and the note is written into the header of `props.md` so
nobody puts it on a prop sheet. The only sprites under it are `stone_shelf`, `water_jar`, `step_rope`
and `sitter`.

Two things the field needs that are **not** mine and are not written anywhere yet: walkers for
`hart`, `stolz` and the clerk (`walkers.md`), and the `halm` grain-shed interior, which the Play block
gives an inside and a gap in the far wall.
