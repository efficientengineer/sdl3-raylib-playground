# FLOW — chapter one, play blocks and clip slots

**Owner of this file: the DESIGNER.** Written first, before the writer's beats, so there is something
to build on. The designer has the final say on the play blocks and on where a clip slot sits; the
WRITER has the final say on what is said in a clip and on who the characters are.

The rule this file is built on (owner): *"more play, fewer boxes."* The last draft had twelve clips,
nine of them talk scenes, and seven of those happened before the player left town. **This flow has
six clips.** Everything else that was a talk scene is now a thing the player does: an examine point,
an NPC line in the field, or a short two-line exchange that plays while you keep walking (an
**in-field exchange**, not a scene file — see "Field lines" below).

---

## The whole chapter in one column

```
  PLAY  P1  Hart's yard, midday      the post, the arm, then the swing beats you  6 min
  CLIP  C1  the yard, after the loss  "not ready" + the patch-up  0110_the_yard   0.8 min
  PLAY  P2  Halm by day               the errand: the bench, the lamp-charm, the guild hall  10 min
              └ teaching find in the grain yard
  CLIP  C2  the guild hall counter    ONE rule: no sword, no signature            0.7 min
  PLAY  P3  Halm at dusk → supper     past her house; supper is two boxes          3 min
  CLIP  C3  supper at Hart's          he isn't ready; the day ends                0.8 min
  PLAY  P4  the yard, day two         four sessions, visible progress, then the parry  14 min
  CLIP  C4  the yard, the sword       the sword = you may sign; the bell starts   1.2 min
              └ then a silent beat in the yard; the clock starts at the gate
  PLAY  P5  the bell run              timed dash; the sheet; asking her            6 min
  PLAY  P6  the hill path, dusk       first real encounters; leaving town         10 min
  CLIP  C5  the pasture edge, night   meeting the herder                          1.2 min
  PLAY  P7  the high pasture, night   the false lantern; the tracks that stop     12 min
  BOSS  P8  the guard beast           two phases                                  6 min
  CLIP  C6  after                     the wrong thing on the beast; the three go on  1.6 min
```

**67 minutes of play, 6.3 minutes of clip.** Ratio 10.6:1, re-timed 2026-09-21 after the cuts.
Full table in `chapter01.md`.

**The three rules of this revision** (owner's notes on draft 2, binding):
1. **Nobody coaches.** Ottilie is not the hint system and never was in her own right — she is on the
   wall enjoying it. The parry is taught by the machine (`COMBAT.md` §4a).
2. **Ten interactions are mandatory** (`chapter01.md`, *The mandatory ten*) and everything else is
   optional and short. The setup lives in the mandatory layer now; it used to live in the optional one.
3. **Fewer words everywhere.** Goal lines of six words or fewer; no system boxes; no line that
   narrates what the player just did.

---

## What is NOT a clip any more

These were scenes in the old draft. They are now play.

| Old scene | Now |
|---|---|
| `0102_the_errand` (Hart sends him to town) | **One box, then play.** Hart says it in one box; the part is then **fetched by the player off his bench** (`hart_yard.bench_part`, mandatory), so the errand starts by walking through his workshop. Goal line: *Deliver the part. Buy bread.* |
| `0104_the_table` (supper) | **A short field scene at the table** — the player walks in, sits, **two** in-field boxes plus one examine (Hart's workshop book). C3 is only the last beat of it. Nothing at this table states Ottilie's history; her house did that, on the walk home. |
| `0105_the_night` (alone at night) | **Play.** One screen: Falke's room, **one** interact on the sword-shaped gap on the wall, then bed. Thirty seconds, no boxes. |
| `0110_the_board` (the job sheet) | **A mandatory examine, not a scene, and not optional.** Three sheets, two struck through; **taking the last one down fires `halm.job_sheet` / `_2`** in two short boxes, because taking it to the counter is how you sign it. The clerk's line is an NPC line. |
| `0130_asking_her` (asking Ottilie along) | **An in-field exchange at her door,** two boxes, while the bell is still ringing. She joins as a follower on the spot. |
| `0120_what_the_road_is` | **Gone with the road.** (No sunk cost.) |

## Field lines

An **in-field exchange** is one to four dialogue boxes played with the camera where it is, no panels,
no scene file, no music change — the engine's `message`/`npc` trigger. The writer owns the text; it
lives in `story/field/text.md` under a `## <map>.<id>` heading, not in `story/scenes/`. This is the
main way the ordinary day gets carried. I list every id I need in `designer_to_writer.md`.

---

## The clip slots

Each slot: **where**, **the one job**, **what the player just did**, **what control they get back
into**. Scene file names go in the last column when the writer has them.

### C1 — "You're not ready"
- **Where.** Hart's yard, midday, day one. Panels (this is the game's first set piece).
- **The one job.** Establish the three people and the fact that the last machine cannot be beaten by
  attacking. Nothing else.
- **What the player just did.** Lost to machine 4 — three or four times, fast, funny losses.
- **What they get back into.** Standing in the yard with a goal line: *Take the repaired part to the
  guild hall.* Ottilie's patch-up happens inside the clip; she is NOT a follower yet.
- **Must not contain.** The job, the board, the sword, the herders, the father.
- Writer's file: `0110_the_yard` **(confirmed by the writer, 2026-09-21)**

### C2 — the counter
- **Where.** Guild hall, inside, afternoon. Talk scene (no art).
- **The one job.** The clerk states two rules plainly: job sheets go up at dawn; the board closes on
  the last ring of the evening bell. Then, because Falke asks, the third: **no sword, no signature.**
- **What the player just did.** Carried the part across town, done the bread, found the teaching find.
- **What they get back into.** Free walk in Halm with a goal line: *Get home before supper.* The
  board is visible in the hall and readable — empty, because it is not dawn.
- Writer's file: `0130_the_counter` **(confirmed by the writer, 2026-09-21)**

### C3 — supper
- **Where.** Hart's kitchen, sundown. Talk scene.
- **The one job.** Hart says he is not ready, and means it kindly. Ottilie has a life of her own that
  the player sees a fact of (she came from the dark house next door).
- **What the player just did.** Walked home, sat down, examined the workshop book.
- **What they get back into.** Night: his own room, one interact, sleep. The goal line becomes
  *Be at the machines before light.*
- Writer's file: `0140_supper` **(confirmed by the writer, 2026-09-21)**

### C4 — the sword
- **Where.** Hart's yard, evening of day two. Panels.
- **The one job.** Hart gives the sword, and the player must understand it means **he may sign.**
  The evening bell starts in this clip and does not stop.
- **What the player just did.** Beaten machine 4 with a parry, themself, after a day of losing.
- **What they get back into.** A timed run, immediately, with the bell audible and a counter of rings
  on screen. No pause between the last box and control.
- Writer's file: `0150_the_sword` **(confirmed by the writer, 2026-09-21)**

### C5 — the herder
- **Where.** The edge of the high pasture, full night. Panels.
- **The one job.** The third party member exists, is a person, and does not want the beast killed.
  Falke nearly swings at them; Ottilie stops him.
- **What the player just did.** Followed a trail of sleeping animals to the place the tracks stop.
- **What they get back into.** A party of three, with the herder's night sight on: enemy tells now
  show a beat earlier and the dark reads differently. The goal line: *Find the guard beast.*
- Writer's file: `0160_the_herder` **(confirmed by the writer, 2026-09-21)**

### C6 — what was on it
- **Where.** The pasture, after the boss. Panels.
- **The one job.** Something visibly wrong is found on the beast; the sheep wake; the shepherd pays.
  Three people decide to find the cause.
- **What the player just did.** Struck the beast down to save the herder.
- **What they get back into.** The chapter ends here.
- Writer's file: `0180_what_was_on_it` **(confirmed by the writer, 2026-09-21)**

---

## Final reconciliation, 2026-09-21 (designer's last pass)

Checked `beats.md`, `story/scenes/` and the writer's message against this flow.

- **Every slot names a scene and every scene has a slot.** C1 `0110_the_yard`, C2 `0130_the_counter`,
  C3 `0140_supper`, C4 `0150_the_sword`, C5 `0160_the_herder`, C6 `0180_what_was_on_it`. Six files
  exist in `story/scenes/` and there is no seventh. Still open: **`story/playlist.md` `## chapter01`
  is the old twelve-scene list** — the writer's file, the writer's edit.
  **CLOSED, same day:** the playlist is now the six clips (*The Last Job Sheet*), the old twelve are
  deleted, `story/field/text.md` holds 80 written ids with no placeholders, and `check --all` is
  clean. Every field id in `designer_to_writer.md` §2 exists, including `halm.job_sheet`,
  `halm.clerk_signing_late` (assumption 5, covered), `high_pasture.boss_break` and
  `high_pasture.boss_turn`. The writer added `high_pasture.body`, which is better than what I asked
  for: the ring of nine healed holes is **found by the player in play**, and C6 is the three of them
  reacting. Folded into the spine. `{{MACHINE}}` is withdrawn — "machine" stays plain English.
- **Nothing a scene says contradicts what the player did.** Two checked closely:
  `0150_the_sword`'s "lost forty-one times" is **Ottilie's running count out loud, not a retry
  counter** — the player loses about eight times across day two's four sessions (`chapter01.md` P4)
  and her tally covers the day, which is exactly the joke. And "his arms are gone, which is the only
  reason he finally stops swinging" **is** the design: he is out of stamina, so the only thing he can
  afford is a guard (`COMBAT.md` §5). The scene and the system say the same thing.
  `0160_the_herder` has one lamp burning on the ground in the dark, which is what the high pasture's
  {{NIGHT_CREATURE_PL}} are built to imitate — good, and no change needed.
- **Two in-field exchanges inside the boss fight** (`high_pasture.boss_break`, `high_pasture.boss_turn`)
  are the writer's ask, accepted, and staged in `chapter01.md` P8. They are not clips.
- **Ottilie is not limited** (owner, 2026-09-21). Any wording in this room that made her healing
  costly has been removed from `COMBAT.md` and `chapter01.md`.

## Reconciliation with the writer

`story/v3/ch01_room/beats.md` and `writer_to_designer.md` did not exist when this was written
(2026-09-21). The designer proceeded on the brief. Open assumptions are logged in
`designer_to_writer.md` and this file is re-read against the writer's files at each milestone and
once at the end. Where a writer beat needs a clip this flow does not have, the rule is: **fold it
into an adjacent clip or make it an in-field exchange** before adding a seventh clip.
