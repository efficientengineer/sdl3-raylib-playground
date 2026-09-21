# DESIGNER — chapter one: what the maps and the engine have to do

Written 2026-09-21 by the gameplay designer, alongside `story/v3/chapter01.md` (the spine),
`COMBAT.md`, `BESTIARY.md`, `LOOT.md` and `story/v3/ch01_room/`. This file is **for the map and
engine side**. Nothing here has been built and this agent has not touched `src/` or any script.

The engine today is the voxel field (`src/VOXFIELD_NOTES.md`): a voxel world generated from the
`.tmap` text maps, free analog movement on a generated navmesh, a 1.25-cell jump that crosses about
3.6 cells running, drops onto any walkable ground, ramps, water impassable, day/dusk/night lighting
with lamps, interact `!` prompts, cell triggers, exits and doors, wandering NPCs, and a follower
trail. **There is no combat and no inventory.**

---

## 1. Maps

### New maps

| Map | Size guess | Light | Why |
|---|---|---|---|
| **`hill_path`** | long and narrow, ~24 x 48 | starts `dusk`, ends `night` | P6. Switchbacks climbing a shoulder. Needs **ramps** (the engine has them and west_road already uses thirteen), a **two-cell drop onto a ledge** off the outside of one bend with a ramp back up (the hidden find), and a gate at the top. |
| **`high_pasture`** | wide and open, ~48 x 48 | `night`, very low | P7 + P8. Open dark grass, a stone fold, knots of sleeping animals, a band of turned ground, the place the tracks stop, and **one jump-only region on a shelf above the fold** for `{{LOOT_PASTURE}}`. The engine already detects and logs jump-only regions; this is the first time one is used on purpose. |

Both are **new tilesets' worth of nothing new**: grass, dry grass, dirt, gravel, stone wall, tree.
Nothing on either map needs art that `valley` does not already have, except the fold wall and the
sleeping-animal prop.

### Changes to maps that exist

**`hart_yard`** — the chapter opens here and spends 20 minutes in it.
- **Three machines** standing in a line, each an interactable object with its own trigger: the post,
  the arm that holds, the swing. *(The barrel on a rope is cut — see §2a.)* The swing needs a
  **counterweight that visibly drops as its tell and climbs the post as a progress read-out**, with a
  mark that stays at the session's best.
- **Two triggers to remove from `hart_yard.tmap`** when the map is next edited (map side, not the
  writer's): `hart_yard.practice_posts` — the old "the row as a whole" examine, made redundant by
  the three machines each having their own — and the old `machine_1..4` ids. Until the trigger goes,
  the line has to exist or `check --all` fails, so the writer has kept one short line for it. **This
  is the only known stale trigger in the chapter.**
- **Hart's workshop as a place you walk into**, not a façade: the errand's part is on his bench at
  the back, past the vice and the half-built things and the hook with nothing on it. This is the only
  way the chapter says he is an engineer.
- **The wall between the houses**, walkable-on by an NPC, with Ottilie sitting on it (she is not a
  follower in P1/P4; she is a stationary NPC who talks, and **she is not a hint dispenser** — her
  lines never change with the player's failure count).
- **The ladder to the workshop roof**, both roofs walkable, and a **three-cell gap** between workshop
  roof and house roof so the running jump clears it and a standing jump does not. `{{LOOT_YARD}}`
  under the eaves. This is the only place in the chapter where a jump is *taught*.
- **Hart's workshop bench** with the book on it (an examine that changes four times on day two).
- A **bed / room interior** for the night of day one — or a night variant of the yard with an
  interior trigger. Cheapest version: one small `hart_room` map.

**`halm`** — 18 minutes across two days.
- **The job board**, an examine object with two states (day one empty, day two four sheets).
- **The guild hall** with a counter the player can stand at (C2 plays there).
- **The bell tower**, examinable, and the bell audible as a timed sound in P5.
- **The grain yard**: a propped door, an interior with the scale bench (`{{LOOT_TEACHING_FIND}}`),
  three creature spawns, and **a gap in the far wall** as a through-route.
- **The bell-run shortcuts**: a water trough beside the grain-yard wall at jump height, the wall
  itself walkable along its top, two lane roofs with a two-cell drop into the alley behind the guild
  hall, and the well kerb to a low shed. All three must be **provably faster** than the street.
- **Ottilie's door**, on the fast route — and **Ottilie's house seen into** from the lane home at
  dusk: one chair at the table, two coats on the hook. A dressed interior visible through an open
  door, not an enterable map.
- **A stranger in the road on the errand route**, renewing a carter's lamp charm — the chapter's one
  sight of ordinary magic, and a trigger the player cannot walk around.
- **Three** townspeople with two line states each (before and after the sword), down from five.

---

## 2. Engine features, in priority order

"Playable without it?" means: can chapter one be walked end to end and be worth playing, if this is
not built?

| # | Feature | Needed by | Playable without it? |
|---|---|---|---|
| 1 | **Timed interact window** — a tell animation, a window, and an early / late / hit verdict, with a knockback on a miss | every fight in the chapter, via the no-combat fallback (`COMBAT.md` §10) | **No.** This is the chapter. Nothing else on this list matters before it. |
| 2 | **Hold-to-charge press with a meter**, spending a stamina value | the effort slider's fallback form (`COMBAT.md` §10) | Partly — fights work as pure timing, but the owner's effort idea is then untaught before combat exists. |
| 3 | **A stamina value per party member that regenerates while walking**, shown on the HUD | effort, and Ottilie's and Distel's kits | Yes, but the chapter loses the one resource the player watches. |
| 4 | **Goal line on screen**, settable per trigger | every block; rule 2 of `STYLE.md` | Yes, but "maximize for understanding" fails without it. This is cheap and should be done early. |
| 5 | **Time of day changing between blocks**, scripted rather than by clock | day one → night → day two → the climb into night | **No** for the chapter's shape. The lighting already exists per map; what is missing is *changing it on a trigger*. Cheapest version: separate map variants (`halm_dusk`, `hart_yard_night`). |
| 6 | **A party of three** on the follower trail | P7 onward | No — the trail supports followers; the count and a third walker sheet are the work. |
| 7 | **Lantern / darkness**: a light that follows the party, and a very low ambient on one map | P7's whole idea | **No.** The engine has lamps at fixed positions; this needs one attached to the party. |
| 8 | **A timed goal** — a counter that ticks (the bell), with a soft outcome, not a fail | P5 | Yes, but the best two minutes of the chapter go with it. |
| 9 | **Item pickup and a minimal inventory** (five slots, no UI to speak of) | the four hidden items | Yes for one run; but then the teaching find teaches nothing. Small. |
| 10 | **NPC line states** — an examine or NPC whose text changes after a flag | the townspeople before/after the sword; the board; the book | Yes, but it is the cheapest storytelling in the chapter. |
| 11 | **A scripted boss sequence**: phased, with in-field dialogue triggers mid-fight | P8, and the writer's `boss_break` / `boss_turn` | **No.** The chapter's ending needs it. |
| 12 | **A party-member passive that shifts prompt timing** | Distel's night sight, and `{{LOOT_PASTURE}}` | Yes, but C5's payoff is then only a line. |
| 13 | **Turn-based combat proper** (`COMBAT.md` §1-§9) | the real version of all of the above | **Yes** — deliberately. The fallback exists so combat is not on the critical path. |

### 2a. Added by the draft-three revision (2026-09-21)

The owner's note — *too much text; Ottilie must not coach* — moves work off the writer and onto the
engine. **The teaching that used to be dialogue is now animation, sound and menu state.** These are
new rows, in the same format, and three of them are now on the critical path.

| # | Feature | Needed by | Playable without it? |
|---|---|---|---|
| 14 | **The swing's tell as a real animation with a sound** — counterweight visibly drops a hand's width, arm winds back, ratchet audio, always the same side, ~1 s before the strike | `COMBAT.md` §4a carrier 1. **This is the whole hint system now.** | **No.** With Ottilie's hints struck, a swing with no readable tell leaves the chapter's one lesson untaught. Highest new priority. |
| 15 | **A counterweight that climbs the post, and a mark that stays at the session's best** | §4a carrier 2, and day two's progress read-out | **No** for day two, which is fourteen minutes of "am I getting better?" with no numbers and no dialogue to answer it. |
| 16 | **Greyed-out commands with their cost shown, and one always-lit Guard** — and in the fallback, a charge glyph that greys when the bar cannot pay and a guard glyph that never does | §4a carriers 3 and 4; `COMBAT.md` §11 | **No.** This is the game's loudest permitted hint. Cheap: it is menu state over the stamina value (row 3). |
| 17 | **Non-verbal difficulty escalation** — after 3 failures in a session the tell plays ~20% slower; after 5 the Guard entry pulses once as it plays; both reset on a parry; yard only | §4a carrier 5 | Yes, but then a player who cannot read the tell has nothing at all, and the chapter has no way to help them without someone speaking. Small: two counters and a timescale. |
| 18 | **An idea-arrival system for the command list** — a command is *absent* until its idea arrives, not greyed (Attack only → Guard appears → the effort slider appears), one short prompt each | `COMBAT.md` §7a "Teaching order" | Yes, but the alternative is tutorial boxes, which is exactly what the owner cut. |
| 19 | **Mandatory-interaction gating** — a trigger that *must* fire before the block advances: an object that must be picked up (`bench_part`), a trigger fired on entering rather than on interacting (`guild_hall_door`, `lamp_charm`), a scene held until an examine has happened (`body` before C6), and text fired by taking an object (`job_sheet` on taking the sheet down) | the mandatory ten (`chapter01.md`) | **No.** The cold read's main finding — the setup was all in the optional layer — is repaired by this row and nothing else. It is four small variants of a trigger the engine already has. |
| 20 | **A goal line of ≤6 words, one at a time, settable per trigger, changeable mid-block** | every block; and specifically the bell-run clock starting at the yard gate, not at the end of C4 | Already row 4; restated because the six-word cap and the mid-block change are new constraints. |
| 21 | **A timer that is armed by a scene and started by a player action** (the bell audible but not counting until the player leaves the yard) | C4 → P5, "the sword gets a breath" | Yes, but the sword — the object the whole first act is about — then gets one exclamation and a clock. |
| 22 | **A pair of dark set pieces**: a single distant light source on the pasture that looks exactly like the party's lamp and fires its text when the player has *stepped toward it*, and a trail of prints that ends | P7's two mandatory set pieces | **No** for the chapter's wonder, which the cold read says currently arrives at minute fifty. |

**Suggested order to build in, revised:** 4/20, 19, 1, 14, 16, 5, 9, 10, 7, 15, 22, 6, 11, 17, 18, 3,
2, 8/21, 12, then 13. Rows 19 and 16 are the cheapest big wins in the list.

**What got cheaper.** One machine fewer (three, not four: the barrel is cut), two townspeople fewer,
one room interact fewer, and two of day two's four between-session lines gone.

---

## 3. Art the chapter needs that does not exist

Listed for the art side, not scheduled here.

- **Distel** — walker sheet and portrait (third party member, small, large-eyed).
- **{{BEAST_NAME}} / the drover** — the boss. Big, and it has a mane that moves, which is the tell.
- **Three creatures**: burr (hill), lantern (a light, so mostly a shader), fleece (a sheep from
  behind — cheap, it reuses the sleeping-animal prop).
- **The sleeping animal prop**, used maybe twenty times on the pasture. High value per unit of work.
- **Three machines** in Hart's yard, and a counterweight that moves — **its drop is the chapter's
  hint system**, so it is the highest-value animation in chapter one, not set dressing.
- **The lamp-charm pair** in Halm's road (a carter, a woman warming a wick) and a **dressed interior
  behind Ottilie's open door** (one chair, two coats).
- **Hart** — the redesign already flagged in `BRAINSTORM.md` (engineer, knee brace, apron).
- **Four hidden items** as pickup sprites.

---

## 4. The five questions — ANSWERED (orchestrator accepted all five recommendations, 2026-09-21)

All five stand as written below and are now design, not proposals: **no fail state on the bell run**;
**day two stays at fourteen minutes in four sessions**; **the pasture is genuinely dark with the fold
faintly visible from anywhere**; **effort is taught in the fallback as the hold-to-charge press**;
**no inventory UI in chapter one**. One amendment from the draft-three pass: question 2's fallback —
"if it has to shrink, cut session 1" — is unchanged, but nothing was in fact cut from day two, which
is the only block the revision left at its full length.

1. **Does the bell run have a real fail state?**
   *Recommendation: no.* If the rings run out, the clerk is closing the shutter and signs him anyway
   with a line about it. Keep the pressure, never send the player backwards; nothing in chapter one
   should be repeatable-by-force.
2. **How long is day two allowed to be?** I have it at 14 minutes, built as four sessions with the
   light moving and visible progress, rather than as literal repetition.
   *Recommendation: keep 14.* It is the chapter's second-longest block and it is where the player
   learns the game. If it has to shrink, cut session 1 (the before-light one), not session 2 (where
   Hart slows the machine down, which is the teaching).
3. **Is the darkness of the high pasture allowed to be genuinely dark** — a small lantern circle in a
   large black field, where the player will sometimes not know which way they came?
   *Recommendation: yes, with one safety*: the fold is always faintly visible from anywhere on the
   map, so the player is never lost, only in the dark. Without real darkness the area has no idea.
4. **Should the effort slider be taught before combat exists**, as the hold-to-charge press?
   *Recommendation: yes.* It is a day's work in the fallback and it means the real battle system
   arrives to a player who already understands its twist.
5. **Do the hidden items need an inventory UI in chapter one?**
   *Recommendation: no UI beyond a pickup message and a line in a flat list.* Three of the four are
   passive rule changes; only the teaching find is used, and it can be a single button.

---

## 5. Reconciliation with the writer

The writer owns `story/scenes/`, `field/text.md`, `characters.md`, `NAMES.md`, `THREADS.md`,
`playlist.md`, `ch01_room/beats.md` and `writer_to_designer.md`, and has confirmed the six clip slots
and their file names (`0110_the_yard`, `0130_the_counter`, `0140_supper`, `0150_the_sword`,
`0160_the_herder`, `0180_what_was_on_it`). Their three creature names and concepts (burr, lantern,
fleece) are the ones designed in `BESTIARY.md`; their two in-field boss exchanges are in the spine.
Field-line ids the design needs are listed in `ch01_room/designer_to_writer.md` §2, and the
draft-three **mandatory / optional / deleted** split is §5 of the same file. In the draft-three pass
the writer and I agreed, in this order: the job sheet fires on taking the sheet; Ottilie carries no
hints at all; `high_pasture.lantern` and `tracks_stop` become unmissable set pieces; a new
`halm.lamp_charm` carries both the ordinary-magic sight and the first mention of the
{{HERDER_PEOPLE_PL}}; three machines named rather than four numbered; roughly eighty field ids down
to fifty-odd, of which ten are mandatory.
