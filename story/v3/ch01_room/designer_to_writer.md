# DESIGNER → WRITER — chapter one

One-way channel. The designer writes here; the writer answers in `writer_to_designer.md`.
Nothing here is a decision about what anyone SAYS — only about what the player does and what names
the design needs to exist.

---

## 0. Answers, after the writer's message of 2026-09-21

**All your names are adopted everywhere in my files.** Section 1's placeholder table is superseded;
what I now use is: `{{HERDER}}` Distel, `{{HERDER_PEOPLE_PL}}` Mohnen, `{{GUARD_BEAST}}` drover,
`{{BEAST_NAME}}` Klee, `{{SHEPHERD}}` Garbe, `{{HIGH_PASTURE}}`, `{{HILL_CREATURE}}` burr,
`{{NIGHT_CREATURE}}` lantern, `{{PASTURE_CREATURE}}` fleece.

- **All three of your creatures are placed, and I wrote the design to your concepts, not mine**
  (`BESTIARY.md`): the **burr** is the hill path's only encounter and it clings and slows, which is
  the exact argument for parrying instead of swinging; the **lantern** is the high pasture's first
  encounter and its whole point is that it is indistinguishable from the party's own lantern, which
  is better than what I had; the **fleece** stands in the knots of sleeping animals, so it prices the
  chapter's best activity — walking up to the sleepers — without stopping it. **Klee** is the boss
  and the kind is a drover; the player meets the kind in the field and the name only in C5.
- **Your two in-field exchanges inside the boss fight: accepted, and they are in the spine**
  (`chapter01.md` P8) as `high_pasture.boss_break` (she calls Klee by name, it works for a moment)
  and `high_pasture.boss_turn` (it doesn't know her, it goes for her). Boss UI stays up, control
  frozen for two boxes, no scene file, and `boss_turn`'s last box changes the goal line to
  *Stop it. It is going for {{HERDER}}.* No seventh clip.
- **The job sheet id you asked for: `halm.job_sheet`.** One place, one examine, day two only. I have
  dropped my `halm.board_sheets` in favour of it; `halm.board_closed` stays for day one's empty board.
- **Your file names are in the spine** against C1-C6 exactly as you listed them.

---

## 1. Tokens I need rows for in `NAMES.md` *(superseded by §0 — kept for the design intent)*

Every proper name in my files is a token, per the owner. I have used the placeholder values below so
`chapter01.md`, `BESTIARY.md` and `LOOT.md` read. **Please add rows (your values win) and tell me if
a token should be renamed or dropped.** Plural rows follow the existing `_PL` convention.

### People and peoples

| Token | My placeholder | What the design needs it to be |
|---|---|---|
| `{{HERDER_PEOPLE_PL}}` / `{{HERDERS_SG}}` | night herders / night herder | The people. Known to the world, never seen in Halm. Small, large-eyed, nocturnal; their light magic is sleep and calm. |
| `{{HERDER}}` | Wik | The third party member. Young, prickly, blunt, literal. Raised the guard beast. Battle role: drowsy, calm, night sight. |
| `{{SHEPHERD}}` | Onne | Posted the job sheet. Halm's high-pasture shepherd. Appears at the board (as a name on the sheet) and after the boss, to pay. |
| `{{CLERK}}` | — *(exists?)* | The guild hall clerk. Says the three rules in C2. If there is already a handle for them, tell me. |

### Places

| Token | My placeholder | Notes |
|---|---|---|
| `{{HILL_PATH}}` | the hill path | New map. Halm → high pasture. Dusk on the way up. |
| `{{HIGH_PASTURE}}` | the high pasture | New map, night only this chapter. The area with the one strong idea (darkness). |
| `{{GUILD_HALL}}` | guild hall | Building in halm.tmap. Exists in the text already, I think. |
| `{{GRAIN_YARD}}` | grain yard | Already in halm.tmap. Holds the teaching find, and the wall is the bell-run shortcut. |

### Creatures

| Token | My placeholder | One line |
|---|---|---|
| `{{HILL_CREATURE}}` / `_PL` | nibbler / nibblers | Hill-path grazer's pest. Barges. |
| `{{NIGHT_CREATURE}}` / `_PL` | duster / dusters | Pasture. Drifts to a lantern, bursts a blinding dust. |
| `{{PASTURE_CREATURE}}` / `_PL` | digger / diggers | Pasture. Comes up under you where the ground is turned. |
| `{{GUARD_BEAST}}` | warden | The boss. The herders' guard beast, gone rogue. |
| `{{MACHINE}}` / `_PL` | machine / machines | Hart's training machines. He may well call them something of his own — your call, but the player needs one plain word. |

### Items

| Token | My placeholder | Effect (mine to design, name yours) |
|---|---|---|
| `{{LOOT_TEACHING_FIND}}` | weight | **Kept from the old table**, moved to the grain yard on day one's errand. Doubles one character's attack for three rounds, once a fight, never consumed. |
| `{{LOOT_YARD}}` | brace | Under Hart's eaves. Wearer cannot be held, knocked down or put to sleep. (The sleep clause is new and matters this chapter.) |
| `{{LOOT_HILL}}` | whistle | On the hill path, off a drop. Once a fight, one enemy skips its turn. |
| `{{LOOT_PASTURE}}` | lens | On the high pasture, reached in the dark. Enemy tells are always shown, for the whole party, forever. |

**Dropped tokens** (the old road is gone): `{{ROAD_CREATURE}}`, `{{THIEF_CREATURE}}`,
`{{AMBUSH_CREATURE}}`, `{{STALKER_CREATURE}}`, `{{RUIN_CREATURE}}`, `{{CAVE_CREATURE}}`,
`{{SHRINE_CREATURE}}`, `{{LOOT_ROAD}}`, `{{LOOT_SECOND_SWORD}}`, `{{LOOT_SHRINE}}`,
`{{GRAIN_CREATURE}}` *(see question 3 below — I would like to keep this one)*. I have not edited
`NAMES.md`; leaving or removing the rows is yours.

---

## 2. Field lines I need written (`story/field/text.md`)

These carry what used to be talk scenes. Ids are my proposal; the map prefix must match the .tmap.
Two sentences at most each, per `check --all`.

**hart_yard**
- `hart_yard.errand` — Hart gives the errand (part to the guild hall, bread for three). 2-3 boxes,
  player keeps the stick.
- `hart_yard.machine_1/2/3/4` — one examine line each: what this machine is for. This is where the
  player learns what a tell is, in plain words.
- `hart_yard.book` — Hart's workshop book on the bench: every attempt, dated. Examine.
- `hart_yard.wall` — the wall Ottilie sits on, examined when she is not on it.
- `hart_yard.ladder` — the ladder to the eaves (the hidden `{{LOOT_YARD}}` is up it, day two).
- `hart_yard.supper_1..4` — the supper exchange; C3 is only its last beat.
- `hart_yard.room_sword_gap`, `hart_yard.room_window`, `hart_yard.bed` — night of day one.
- `hart_yard.day2_a/b/c/d` — one line between each of day two's four training sessions, marking
  time passing and progress (see `chapter01.md` P4). Ottilie's call gets more explicit each time.

**halm**
- `halm.bread` — the baker. NPC, with a name if you want one.
- `halm.grain_yard_scales` — the teaching find's examine, with a glint on it.
- `halm.board_closed` — the job board on day one: empty, and why.
- `halm.job_sheet` — day two: the last sheet on the board, three struck through beside it. The one place the job is written down, ever.
- `halm.bell` — the bell tower, examined.
- `halm.npc_1..5` — five townspeople who react to Falke: before the sword, and a second line after
  he has it. **Please write both states** — it is the cheapest way to show the sword means something.
- `halm.ottilie_door` — asking her along, during the bell run. 2 boxes, then she joins.

**hill_path**
- `hill_path.last_house`, `hill_path.dusk_view`, `hill_path.drop` (the hidden find), `hill_path.gate`.

**high_pasture**
- `high_pasture.sleeper_1..4` — the sleeping animals that are the trail. Each one says a little more.
- `high_pasture.tracks_stop` — the tracks that stop in open ground. The chapter's best examine;
  please make it the one line a player quotes.
- `high_pasture.lantern`, `high_pasture.fold`, `high_pasture.find` (the hidden find).

---

## 3. Assumptions I proceeded on (tell me if any is wrong)

1. **Six clips, not twelve.** C1-C6 in `flow.md`. The errand, the board, the night, and asking
   Ottilie along are play, not scenes.
2. **The grain creature survives the rewrite.** The player needs one harmless enemy inside Halm on
   day one so the teaching find sits behind a tiny bit of play rather than nothing at all. It is the
   only old creature I kept, and only if you are happy for it to be in the grain yard.
3. **Day two is four sessions, not forty retries.** Time passes in stages with visible progress
   (see P4). The player does not literally lose 41 times; they lose about 8 and see the day move.
4. **Hart does not post the job and the father does not appear.** Per the owner's no-sunk-cost note.
   Hart's part ends at the sword.
5. **The bell run is timed but cannot be failed permanently.** If the rings run out, the clerk is
   closing the shutter and lets him sign anyway with a line about it. I want the pressure, not a
   restart. Please write that line — it is the one place the design needs you to cover for it.
6. **Ottilie joins during the bell run,** not before, so day one and day two are Falke alone.
7. **The herder's night sight changes the field**, not just battle: with them in the party the
   pasture reads differently. That is C5's payoff and it needs no line.
8. **Falke kills the beast to save the herder** (owner's reading confirmed in BRAINSTORM).

---

## 4. Questions for the writer

1. Does the clerk have a handle already, or do I need `{{CLERK}}`?
2. Is `{{SHEPHERD}}` on screen at the board on day two, or only after the boss? I designed for
   "only a name on the sheet, then in person at the end" — cheaper and it makes the sheet matter.
3. ~~Ottilie's limit~~ **WITHDRAWN — owner ruled 2026-09-21.** "Not being strong enough has been
   taken far too literally. That was just when she was young and couldn't save her parents. It
   doesn't have any bearing now." She is a capable healer today, an apprentice in standing only.
   No small pool, no ceiling, no sitting down, no "wasn't strong enough" line. The yard patch-up is
   quick and routine and she teases him through it. Design updated in `COMBAT.md` §5.
4. The herder's name: I need one word the player can say. Wik is a placeholder.
