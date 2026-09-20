# writer-ch01-newplayer — the new player pass, round one (sample)

Owner's complaint: *"even in the first scene with the guild, the dialogue is rough. There is still
much assumed by the narrator that a new player wouldn't know what's going on."* And: *"I still don't
think we've explored the world enough."*

This round is a **sample**: one new style rule, one playable opening, one talk scene, one rewritten
clip, nine new field lines. Scenes 0120-0170 are untouched and are next round.

---

## 1. The cold reader's top 15 blockers — where each one is now

| # | Blocker | Where it is solved now |
|---|---------|------------------------|
| 1 | **"The bell."** | **Opening beat.** The player hears the bell start while he is across town at the grain yard, and the goal line changes to *Sign before the bell stops*. He has already read `halm.board` ("put your name on one and the guild pays you") before it rings. The clerk no longer has to explain it: 0110 line 1 is "You made the bell. The board closed all the same." |
| 2 | **The Stair — what it is, why water goes there** | **Opening beat + field line + 0110.** New examine `halm.stair_view` at the hill gate shows it on the horizon in segment 2, before any clip says the word. `halm.well` and new `halm.jarmen` show the jar being filled and roped. 0110 then has the hero **ask** — "Out to where?" / "What for?" / "No one has ever told me." That converts the confusion into the intended mystery, in three taps. |
| 3 | **"Sheets"** | **Opening beat, evidenced, not named.** New `halm.gate_post`: a strip of pale blue skin nailed to the west gatepost, and a carter's pack in the ditch opened from the outside. The word itself still arrives in 0140 — deferred, and **flagged below** as one of the three worst lines to fix next round. The tutorial fight is a **lid**, not a sheet: the player fights one battle before The Board, and the sheet stays a thing the roads have. |
| 4 | **"The ancients"** | **Deferred to 0130/0150 next round.** The agreed fix is the same five words in both mouths — "the ancients, who built the {{STAIR}}". Not touched this round: 0130 is not mine yet. |
| 5 | **"Guarantor"** | **Cut** from 0110 entirely. Not replaced. It is jargon a player cannot see. |
| 6 | **Who {{HEALER}} is** | **New talk scene 0105 *Bandages*** — she is the first person in the game the player talks to, she is named, she says what she does ("I patch up whoever needs it") and what she is to him. Plus new field line `hart_yard.ottilie`, so she can be talked to again. By the time she holds the ladder in 0120 the player knows her. |
| 7 | **"That knee"** | **Deferred to 0120 next round** (swap lines 3 and 4). The existing `halm.gate_watch` field line already sets the knee up and stays optional; the swap is the real fix. |
| 8 | **The woman under the step** | **Deferred to 0130 next round.** Fix stands: {{MENTOR}} names her, one word. |
| 9 | **"Coast escort" / the good job** | **Opening beat.** The player **sees** the board with sheets on it and **sees {{RIVAL}} take the good one down in front of him and walk out past him.** 0110 renames it the plainer "the coast road" and says nothing else about it. |
| 10 | **The lord in the west** | **Field lines, promoted by position.** `halm.marta` and `halm.ostler` are on the main route through the square in the opening block, so the drained valley is felt before 0110 says "every carter has walked west". The eighty-coin figure is **cut from 0110** — see §2. |
| 11 | **"He'd sign for you" (0150/2)** | **Deferred to 0150 next round.** Fix: "the clerk". |
| 12 | **{{HERO}}'s age and standing** | **Opening beat.** He lives in {{MENTOR}}'s house, he has never signed for a job, and {{HEALER}} was taught three years before him. Standing is now shown. The number seventeen is still not said and does not need to be. |
| 13 | **"Don't touch the stone"** | **Deferred to 0130 next round.** Recommend **cut** — an unfired rule reads as an authoring slip, and the chapter never fires it. |
| 14 | **Money scale** | **New field line `halm.baker`** (bread is a coin; a cart out of town is thirty) plus new `halm.paid_hunter` — the player watches a clerk count out a hunter's pay before he ever hears "thirty coin". |
| 15 | **The jar** | **Opening beat.** Two men fill it and rope the lid down at the well in segment 3, on the main route. It is the first physical object in the game. |

**Also fixed in passing:** contradiction (c) — 0110 now says "two days out and two days back", so
{{HEALER}}'s "Four days, not two" in 0130 is arithmetic, not a correction of a line the clerk never
said. Contradictions (a) and (b) are 0170's and are next round.

**Cut and not relocated:** "the rule is that you sign before the bell" (the bell now *is* the rule,
shown); "it leaves at dawn" (0130 and the goal line carry it); "guarantor".

**The eighty coin a week** is cut from 0110 and **lives in two places already**: `halm.marta`'s field
line ("my sister sends eighty coin a week to a house with no one in it") in the opening block, and
{{RIVAL}}'s mouth in 0150, which is where it belongs — it is his boast, not the clerk's statistic.

---

## 2. Requests to the tool (I cannot edit `story_prompt.py`)

1. **Validate dialogue line length: 100 characters maximum after token substitution**, for scene
   files and `story/field/text.md` alike. This is now STYLE rule 14 and it is the single number that
   would have caught nine of the fourteen overlong lines the cold reader found. Warning is enough;
   error if the owner prefers.
2. Report a scene's **mean** line length too. Chapter one's forty-one lines averaged 98 characters.
   The new 0110 averages 44 over twenty lines, and 0105 averages 60 over six.
3. `check` on a bare scene stem (`check 0110_the_board`) fails; it needs the path. Small, but every
   brief in the repo writes it the short way.

## 3. Proposed new tokens

| Token | Proposed value | Why |
|---|---|---|
| `{{FATHER}}` | *(owner's call — a plain German word name, per the naming style)* | The father is "my father"/"your father" nine times in seven scenes and is the chapter's emotional payload. A player cannot hold a person with no name. **Not yet used anywhere**: an unknown token is a validation error, so `hart_yard.coat_hook` says "your father's" and waits. One row in `NAMES.md` and I will tokenise the nine hits next round. This does spend a name from the sixteen — it is a decision, not a convenience, which is why it is a proposal. |

No other new tokens. `{{VILLAGER_1}}` and `{{VILLAGER_2}}` **do** have rows in `NAMES.md` (Bleibe and
Wagen) — the cold reader was given an older table. No action.

## 4. New field text ids, and where they should stand in Halm

Coordinates are suggestions in the existing `halm.tmap` / `hart_yard.tmap` frame; the map side owns
the actual placement. All of these are on or one step off the main route, and all are optional
except that the player walks past them.

| id | Kind | Where | Teaches |
|---|---|---|---|
| `hart_yard.coat_hook` | message | inside {{MENTOR}}'s door, ~6 11 | the father exists and is dead — the one place that holds him |
| `hart_yard.ottilie` | npc | on the water barrel, ~13 8 | {{HEALER}} is a person you can talk to again |
| `halm.stair_view` | message | the hill gate, looking west, ~23 7 | the {{STAIR}} exists, before any clip says it |
| `halm.jarmen` | npc | at the well, ~18 16 | the jar, being filled and roped, and the town's shrug |
| `halm.moonwatcher` | npc | the square, ~16 18 | the {{SECOND_MOON}} exists and has moved |
| `halm.baker` | npc | the square stall, ~15 17 | what a coin buys |
| `halm.board` | message | the guild board outside the hall, ~20 14 | what a job is and how you get paid |
| `halm.paid_hunter` | npc | inside the hall at the counter, ~21 13 | what a hunter is — shown getting paid |
| `halm.gate_post` | message | the west gatepost, ~8 18 | the roads have something on them |

Also: `west_road.culvert` and `west_road.west_end` were the two `TODO` placeholders and are now
written. (They still warn because there is no `story/field/maps/west_road.map` — pre-existing, the
engine side's.)

`halm.guild_hall_door` currently reads as the *closed* door after the bell. In the opening block the
hall is **open**, so either that trigger needs a second state or the engine should swap it on the
bell. Flagged for the engine side; not changed.

## 5. Art and engine needs

- **The {{STAIR}} on the horizon** from the hill gate. Needs a backdrop/skybox element on `halm` —
  one wide, pale, very distant piece of geometry or a painted band above the tile field, visible from
  segment 2 only. **This is the single most valuable new art request in the chapter**: it converts
  the chapter's central noun from a word into a thing the player has seen.
- **A bell sound**, and strokes the player can count. The opening block's whole tension is the bell
  starting while you are across town. Without audio the goal-line change has to carry it alone,
  which is weaker but playable.
- **The tutorial battle.** *Combat does not exist in the engine yet.* **When combat exists:** one
  lid in the open grain yard, alone, slow, with the guard-break window taught on it.
  **No-combat fallback for now:** the lid is a field obstacle that rolls across the yard gate in a
  straight line on a timer, and the player walks past it when it has rolled by — same tell, same
  lesson about the window, no battle system. The grain-gate woman's existing line already shouts the
  warning. Do not cut the beat; downgrade it.
- **{{RIVAL}} taking the good sheet** needs a scripted walker on `halm`: he stands at the board, an
  NPC walks to the board and off screen past the player. No dialogue, no portrait.
- **Two men at the well** — one static prop pair or one npc walker; the line is `halm.jarmen`.
- **A hunter being paid at the counter** — one npc inside the hall, `halm.paid_hunter`.
- **The playlist.** I added `0105_bandages` to `## chapter01` only, as briefed. **`## intro` is what
  the game actually plays** — someone with that file's ownership needs to add the same line there,
  before `0110_the_board`, or the new opening talk never reaches the phone.

## 6. Next round — applying rule 14 to 0120-0170

- **0120** — give {{HEALER}} one line so she is not silent for a whole scene ("You're not climbing
  this, {{HERO}}"). Swap lines 3 and 4 so the knee exists before it is a taunt. Split line 4's four
  facts into four boxes. 0105 now sets up "I'm not ready", so line 5 lands.
- **0130** — cut "Don't touch the stone" or fire it in 0170. Name the woman under the step. Use
  "the ancients, who built the {{STAIR}}" — the exact words 0150 will reuse. Keep line 3 untouched;
  it is the best line in the chapter.
- **0140** — the "sheets" line, broken into seven boxes on the cold reader's own model, and the
  notch explicitly tied to cutting the father free. This is a talk scene with nothing to look at, so
  it needs the most boxes and the shortest ones.
- **0150** — "he" → "the clerk". Cut two of {{RIVAL}}'s three facts per line and let {{HERO}} and
  {{HEALER}} ask for them. Her "Paying you to do what, exactly?" is the model; write three more.
- **0160** — nearly clean. Put "the worst day either of us has had" into line 1 where the Beat has
  it. Leave line 6 exactly as it is.
- **0170** — give {{HERO}} the deduction and {{HEALER}} the evidence: she hands him the receipts and
  he counts them out loud. Fix the pronoun on the shrine woman's first line. Fix contradictions (a)
  and (b).

**The three worst lines the cold reader found, in order:**
1. **0140/5, 228 chars** — "Sheets, north of the river…" Five facts, the only description of the
   game's monsters, in a scene with no picture.
2. **0110/5, 219 chars** — the guarantor line. **Already fixed this round**: cut to
   "Then who is paying the thirty coin?" / "{{MENTOR}} is. His own money."
3. **0170/3, 168 chars** — the climax delivered as a briefing by the person not having the emotion.

## 7. Files touched this round

- `story/v3/STYLE.md` — rule 14 appended, and the header count 16 → 17.
- `story/v3/chapter01.md` — new `## Goal` and `## Play: explore — {{HOME_TOWN}}` block and
  `## Clip: Bandages` before The Board; The Board's dialogue synced to the scene file.
- `story/scenes/0105_bandages.md` — new, `type: talk`.
- `story/scenes/0110_the_board.md` — `## Dialogue` only. Panels, panel count, order, staging and
  acting lines are **byte for byte unchanged**; the art is already generated.
- `story/field/text.md` — nine new ids, two `TODO`s written.
- `story/playlist.md` — `0105_bandages` inserted in `## chapter01`.
