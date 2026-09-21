# Editor's log — chapter one, the merge pass (2026-09-21)

One pass, merging four actors' notes, the AI-smell audit and the consistency audit against the
orchestrator's fourteen rulings. Six scenes, `story/field/text.md`, `characters.md`, `v3/NAMES.md`,
`THREADS.md`, `chapter01.md`, `COMBAT.md`, `BESTIARY.md`, `LOOT.md`, `ch01_room/flow.md` and the
reading script. No commit. No `src/`, no maps, no packages, no ids added, removed or renamed.

## Box counts, before → after

| Scene | before | after | note |
|---|---|---|---|
| 0110 The Yard | 14 | **14** | reveal order fixed (1,3,2,4 — her laugh now lands on the impact) |
| 0130 The Counter | 15 | **12** | the clerk's four recitations become one rule plus his fluster tic |
| 0140 Supper | 15 | **10** | the two coaching lines cut; she lays the third place instead of joking |
| 0150 The Sword | 14 | **15** | +1: the textless shock on the sword panel (ruling 6) |
| 0160 The Herder | 15 | **16** | +1: Distel's two facts split into two boxes (Distel actor) |
| 0180 What Was On Him | 15 | **18** | +3: the kill line split round a textless sorrow box; the coat panel |
| **total** | **88** | **85** | under the 86 ceiling |

Three boxes carry a face and no words: `0140` Ottilie at the third place, `0150` Falke on the sword,
`0180` Distel after the apology. Every line in every scene now carries an explicit `[n]` panel tag
and an expression tag. `check --all` and `names` are clean; no dialogue line exceeds 100 characters
after substitution (longest: 92, the clerk's restarted rule).

## Chapter title

`The Last Job Sheet` is on the BAN list (C14, scarcity-as-stake) and is inaccurate — the job is the
one nobody wanted, not the last one going. Three plain options, none built on a number:

1. **The High Pasture** ← chosen. The place the chapter is for, the goal line's noun, and it is what
   the spine already calls itself ("the hunt on the high pasture").
2. **The Sheep on the Hill.** Warmer, funnier, sells the joke of the job rather than the dread.
3. **The Hunt on the High Pasture.** The spine's own subtitle; accurate, one word too long.

Changed in `THREADS.md`, `characters.md`, `chapter01.md`, `flow.md` and the reading script. **The
title string in `src/star_logic.cpp` is the engine agent's** — it wants
`"Chapter 1 — The High Pasture"`, with a spaced em dash in place of the `"  -  "` ASCII hyphen.

## Actors' and auditors' items — adopted / adapted / declined

### Falke (31 lines)
| item | verdict | why |
|---|---|---|
| Cut the second clause of the seven two-clause doubles | **adopted** for six | "Get away from her! Get back!" kept — panic earns the repeat |
| "Hart — say it straight…" → "Hart. Am I ready or not?" | adopted | |
| "Say that plainly." → "I don't have a sword." | adopted | stops him asking adults to restate; it is also the fact |
| "Stolz. Of course you're standing there." → a question | adopted (worded "How long have you been standing there?") | he has no irony |
| "Say it!" → "Tell me!" | adopted | BAN C1 |
| textless `(shock)` on the sword, then "I can sign tonight." | adopted | ruling 6 |
| "I'm sorry. I didn't want to." → "I'm sorry." | adopted | |
| Cut "Then we walk." | adopted | third "Then"-opener |
| textless `(sorrow)` at the five coins | **declined** | box budget; the coat panel already gives 0180 a silent beat, and two silent Falke boxes in one chapter is the limit |
| "That's how you hit things!" kept | **declined, with regret** | it only exists as an answer to Hart's "You hit everything as hard as you can", which ruling 2 cuts. The best line lost in this pass |
| "He forgot you." kept | adopted | ruling 3 |

### Ottilie (31 lines)
| item | verdict | why |
|---|---|---|
| "Best wall in Halm, this." kept | **declined** | rule 0 bans country cadence; smells' "I've got the good seat." used instead — same joke, her syntax |
| Trim 0140-8, 0140-9, 0150-11, 0160-16/17/18, 0180-20 | adopted, all | |
| 0140-10 cut to a textless box | **adapted** | the textless box went to the third place instead (ruling 5); the line is cut outright |
| The SECOND COAT as a silent panel in 0180 | adopted | new panel 8, three acting lines, no words. Pays off `halm.ottilie_house` |
| The third place at supper | adopted | replaces the duplicated spoon joke, which survives once in `hart_yard.supper_2` |
| One real exclamation for her | adopted | "Falke — she's a girl. Put the sword down!" — the chapter's only non-Falke `!` |
| "I've heard of your people all my life." | adopted-as-cut | one word off `BAN: "my whole life"`; the gloss moved to her and shortened |
| Her `voice` line in `characters.md` | adapted | rewritten: it said "complete unhurried sentences… never signals a joke", which contradicts NAMES and the owner |

### Hart, the clerk, Stolz, Linde, Garbe, the town
| item | verdict | why |
|---|---|---|
| "You hit everything as hard as you can." cut, box to "Eat." | adopted | ruling 2 |
| "The swing is the one that is out there." → "You have never beaten the swing." | adopted | ruling 2; a score, not a proverb |
| "You are not ready." → "Not ready." | adopted | he drops subjects |
| "You've eaten alone enough." cut | adopted | canon: he has never said why. "Eat here tomorrow as well." stands and she does not answer it |
| "Both hands" cut; "It's been on its hook since" cut | adopted | ruling 6 and the hook contradiction |
| "the book" → "the guild book" | adopted | r14, and it is the sword's meaning |
| One optional day-two examine for Hart's disbelief (`hart_yard.hart_day2`) | **declined** | a new id, and ids are frozen this pass. Listed below for the orchestrator |
| Clerk's tic "the rule is" + restart when interrupted | adopted | both now fire, in 0130 |
| Clerk's fourth recitation cut; Stolz answers "I'll buy one, then." | adopted | |
| Stolz's mocking question | adopted | "Four years now, is it? Mine took two." |
| Stolz's dawn/bell exposition folded | adopted | two boxes became one, and the scene ends on his coin |
| Linde split into two boxes | **adapted** | no new id: one box, two sentences, both facts kept |
| Garbe "forty years" | adopted | now twenty, against a woman in her forties |
| Garbe announcing her own name | adopted | the box prints it |
| The ostler's "eleven carts" | adopted | eleven is the flock's number and nothing else's |
| The ostler's day-two line | adopted | "A hunter, and I knew you at ten." — same man, not a different one |
| The carter's grandfather's saying | **rewritten** per ruling 10 | now a thing he saw on a road, one sentence, no rule of three, no proverb |
| Townspeople all wry | adopted | one dry turn left (the watch); the grain-yard woman wants something, the woman by the square is warm, the ostler is bitter then glad, the carter reports |

### Distel
All of the Distel actor's replacements adopted as ruling 4 directs: the lamp off the ewe's face,
"It's me — smell me.", "You stank of fear the whole fight.", the kill line split round a textless
`(sorrow)` box, the neutral tags on the facts in 0180, "pup" spent once (0160 only), the gloss handed
to Ottilie, "Not ever." and "Both of those are true." cut, "Someone did that to him." (ruling 3).
Her `look` line gains the crook the panels have always drawn (consistency #29), and four panels and
five acting lines now say furred, muzzled and leaf-eared where they used to read as a hooded human
girl (ruling 14). Ottilie's "She's a girl" stays — she means young and female, and the picture
answers her.

### Smells audit
All 23 line-level repairs adopted or superseded by an actor's version of the same line, except:
`halm.lamp_charm_2` (rewritten harder, per ruling 10, rather than kept), `0110`'s "I hit it a dozen
times! / It hit you once." (kept — see below), and the name changes: **`lantern` → `wick`** adopted
(ruling 11), **`Mohnen` → `Mohn`** adopted as an invariant plural, and **`Frage`, `Stolz`, `Garbe`,
`drover`** all **declined** — three of them are live handles in `characters.md` and a rename is the
orchestrator's call, not an editor's, and `drover` is on screen in four files and in the engine's
goal line.

### Consistency audit — the story and design-doc items
| # | item | done |
|---|---|---|
| 12 | Healer 24 → 36 | `chapter01.md`, `BESTIARY.md` |
| 13 | Distel's Settle affordability vs a pool of 30 | now "twice, and the second one hurts", with the arithmetic, in `chapter01.md`, `COMBAT.md`, `BESTIARY.md` |
| 14 | WINDED undocumented | written into `COMBAT.md` §3 as its own bullet and into §4a.3, with the engine's actual numbers read out of `src/battle.cpp`: drain `5 + 4×notches`, no regen for `1 + notches÷2` rounds, cleared by any guard, counterweight zeroed by a parry |
| 15 | two boss goal lines | the seven-word form struck from `COMBAT.md` and `BESTIARY.md` |
| 19 | C6 describes the holes twice | Ottilie's box is now the single word "Nine." |
| 20 | four coin vs five | `characters.md` fixed |
| 21 | "four training machines" | `characters.md` fixed and the three named; `flow.md`'s two "machine 4" fixed |
| 22 | P4 three sessions, ending in daylight | `chapter01.md` P4 now states the intent in a bold paragraph: four sessions, dawn → sun up → high sun → low gold, and if only three rows are affordable the one to drop is session 3, never session 4 |
| 26 | bread bought before the money exists | `halm.bread` puts the loaves on Hart's slate |
| 28 | `flow.md` stale in four places | all four fixed (goal line, C2's rules, 66 ids, the forty-one-times defence) |
| 29 | Distel's crook | in the `look` line |
| 31/32 | `Distel`/`Garbe` as bare speakers | tokenised in `text.md` and in both scene files |
| 33 | 0180's title | the spine and flow now say *what was on him* |
| 34 | script claims 86 boxes | regenerated; 85, stated |
| 38 | 0110's reveal order | 1, 3, 2, 4 |
| 42 | `{{FATHER}}`'s live bio | reduced to "unused in the live story" |
| 46 | `hart_yard.practice_posts`'s wrong `- what:` note | corrected |

## Symmetrical pairs left in the chapter — **four**

1. `0110` "I hit it a dozen times!" / "It hit you once." — the good one; the audit says it earns it.
2. `0110` "You fought a training dummy, not a bear." — **LOCKED**.
3. `0130` "No sword, no signature." — **LOCKED**.
4. `hill_path.ottilie_dark` "I've never been up here after dark. I've never been anywhere after dark."

Cut to get there: the clerk's "You do not buy one. You are given one.", Ottilie's 0140-8 and her
0160 "It isn't the same hill.", both halves of Distel's 0180 balance sheet, Falke's "Don't tell me
how many times. Please don't tell me." (now "Don't start counting."), and the mirror in
`hart_yard.ottilie_idle`.

**Ruling 8 is broken once, in 0110, and I could not honour it.** Two of the four sit in that scene,
and one of them is LOCKED, so the only pair available to cut is the chapter's best exchange. I kept
it and am flagging it rather than damaging the scene.

## Buttons left — three of six, and they are the three that are not snaps

| Scene | last line | kind |
|---|---|---|
| 0110 | "Not quite ready yet." **LOCKED** | a withheld verdict — the one ending that leaves a scene open |
| 0130 | "Here — three coin for your bread." | **flattened.** He walks out holding the rival's money |
| 0140 | "I'll be here." | **flattened.** Ends mid-argument on the flattest thing she says |
| 0150 | "Falke. That's the evening bell." | a clock starting, not a snap |
| 0160 | "She has a name." **LOCKED** | kept as a button, per ruling 7 — and it is now the *third* drop in the chapter rather than the fifth, so it lands |
| 0180 | "Which way is up your mountain?" | the chapter's last line, a question; STYLE allows it |

Both cut boasts were Falke's, which is where flattening cost nothing.

## Ids I want added or removed — for the orchestrator and the engine agent

**Add (none of them written this pass):**
- `hart_yard.hart_day2` — Hart's disbelief, which is in `characters.md` and in no line of the game:
  *"Tracks do not stop. Somebody miscounted sheep."* It sets up `high_pasture.tracks_stop`, the line
  the designer says the player will quote, and he is the only person who can be wrong out loud.
- `halm.town_healer_2` and `halm.shepherd_market_3` — both boxes are currently two facts crammed in
  because I could not split them. Cheap, and both are already written in substance.

**Remove (or place a trigger for):**
- `hart_yard.practice_posts` — nothing triggers it and the machines have their own three boxes. Its
  `- what:` note claimed a trigger that does not exist; I corrected the note rather than the id.
- `west_road.west_end` / `west_road.culvert` — kept only so a map chapter one should not reach
  validates. Closing the west exit retires both.

**Not mine, flagged:** the goal line `"Find the guard beast."` in `src/chapter01.h` is the one place
a player is told "guard beast"; every other word in the game says **drover**. It should read
`"Find the drover."` Goal lines do not go through substitution, so this is a source edit.

## For the OWNER to rule on — five, no more

1. **The chapter title.** *The High Pasture* is my pick; *The Sheep on the Hill* is the funnier one.
   The old title is banned and inaccurate, so one of them has to land.
2. **"That's how you hit things!"** Falke's best line in the chapter is gone, because the line it
   answers hands the player the solution. If the owner wants the laugh back, the price is one Hart
   line at supper that names the boy's habit without naming the answer — and I could not find one
   that did not coach.
3. **Distel's people, plural.** `Mohnen` is not a German plural and reads as an error; it is now
   invariant — *one Mohn, two Mohn, the Mohn*. If that reads wrong out loud, the alternative on the
   table is *Rauten* (rue), which pluralises cleanly and carries no breakfast association.
4. **`drover` for the guard animal.** In English a drover is a person who drives livestock, and
   Distel *is* the drover; "my drover" reads as a hired hand. The audit proposes **minder**. I have
   not touched it because it is in four files and in the engine, but it is the weakest word in the
   creature table and it is said aloud five times in one scene.
5. **The sword handover.** De-ceremonialised as instructed — one hand, hilt first, a rag still in the
   other fist, no two-palmed held beat — and Falke's shock is now a face rather than a line. If the
   owner wanted the ceremony, this is the change to reverse, and it is two panels.

---

# 4.1 — the cold read polish (2026-09-21)

Ten items from `story/notes/cold-read-ch01-draft4.md`, as the orchestrator ruled on them. Six scene
files, `story/field/text.md` (words only — no id added, removed or renamed), `chapter01.md`,
`THREADS.md`, the reading script. No commit, no `src/`, no maps, no packages.

## Box counts

| Scene | 4 | 4.1 | why |
|---|---|---|---|
| 0110 The Yard | 14 | **14** | untouched |
| 0130 The Counter | 12 | **12** | the dawn/bell box out (the spine says C2 does not need it), the bread beat in: two boxes |
| 0140 Supper | 10 | **10** | untouched |
| 0150 The Sword | 15 | **16** | +1 textless `Falke (resolve)` on the new grip panel |
| 0160 The Herder | 16 | **16** | two lines reordered, one retagged |
| 0180 What Was On Him | 18 | **17** | −1: "He's dead." cut |
| **total** | **85** | **85** | ceiling 86 |

Silent boxes go from three to four: 0140 (Ottilie at the third place), 0150 (Falke on the sword),
**0150 (Falke on the grip panel, new)**, 0180 (Distel after the apology). Longest line after
substitution: 92 characters, still the clerk's restarted rule. `check --all`, `names` and
`script 01` all clean.

## Item by item

1. **0150 — the parry has a cause now.** A new panel 4, `object_insert`: his top hand turning over on
   the practice stick and his boots planting with the weight going back, revealed by a **textless**
   `Falke (resolve)` box between "All right. Come on, then." and the winning blow. Nobody says
   anything about it, then or ever. **The scene was already at the eight-panel ceiling**, so the
   panel that paid for it is the old panel 2, the `portrait_inset` of Ottilie with the apple core —
   the cold read's own note that her joke and her face sat on different pictures — and "Don't start
   counting." now lands on the establishing shot with her line. She still gets her close-up at
   panel 6, `profile_flat`, where the scene wants it. `eyes_slit` now says his eyes are on the
   **counterweight**, not the arm, and the `## Beat` says the grip, the feet and the counterweight
   for the artist.
2. **0130 is a three-panel scene.** `- type: talk` gone, `- staging:` and acting lines in: (1) the
   counter, Guildclerk behind it with the ledger, the catch between them and the **bare board of
   planks and old nail holes on the wall behind**; (2) `full_body_reveal` of Stolz in the street
   doorway, nineteen, a head taller, the new wine-red coat, the purse and the **sword on his hip**;
   (3) `object_insert` of three coin going down on the counter under a wine-red cuff. The third panel
   names no cast, so the hand does the acting. Twelve boxes still. The art tray gains one shot sheet.
3. **0180 — "He's dead." cut.** Panel 1 is a picture of a dead animal; it does not need a caption.
   The scene now opens on Falke's "I'm sorry." and **her first box is the silent one**, which is the
   box the cold read said carried the whole death.
4. **0160 — the briefing halved.** "You're a night herder. No one here has ever seen one." →
   **"A night herder. No one here has ever seen one of you."** The two words stay, because the
   mandatory field line that plants them (`halm.lamp_charm_2`) says *"moving sheep in the dark"* and
   never says *night herder*; "of you" turns a briefing into something said to her face.
5. **Declined, per the ruling.** Hart keeps "Take it to the clerk. He'll write your name in the
   guild book." The sword's guild meaning stays plain.
6. **0160 reordered.** "Klee put them down. My drover." → **"My drover put them down. Klee."**
7. **0160 retagged, no new box.** "He ran in the spring. He stopped knowing me." moves off panel 6
   (the pretty lamp-lit close-up, where it was the fifth line stacked) onto **panel 7**, the
   over-shoulder from behind her, and Falke's job-sheet line answers it on the same picture.
8. **The bread is one story everywhere.** `hart_yard.errand`: *"…and bread on the way back, enough
   for three. There is no coin in the house until the winch is paid for."* `halm.bread` no longer
   puts the loaves on Hart's slate: *"A coin a loaf, and you want three. Come back when you have
   three coin in your hand."* 0130 ends on Falke's "I've got no coin for the bread." and Stolz's
   "Three coin for his bread. Pay me back when you sign for something." — said to the clerk, over the
   picture of the coin. `chapter01.md` P2 and C2 and `THREADS.md` all say the same thing now.
9. **The three nouns are on screen first**, in words under existing ids only.
   - *catch* — `hart_yard.bench_part` (MANDATORY) now names it: **"The catch for the well winch,
     mended…"**, which also ties it to `halm.well`, where the winch is Hart's work.
   - *guild* — `halm.guild_hall_door` (MANDATORY): **"The hunters' guild hall is open…"**. The bare
     board came out of this line, which also kills the duplicate the cold read flagged.
   - *job sheet* — `halm.board_closed` now carries it on the thing itself: **"The job sheets go up
     here at dawn. Today it is bare planks and a lot of old nail holes."** In the clip, the board is
     drawn behind the clerk in panel 1.
10. **Declined after reading it aloud.** "I had a very good thing ready to say and now I'm not
    saying it." stays whole. Trimmed to "I'm not saying it." the exchange dies: Falke's "Tell me!"
    has nothing to point at and "You'd have it framed." loses the *praise she had prepared*, which is
    the whole joke. The chapter's funniest line is not worth six words.

## For the orchestrator

- **The art tray gains one shot sheet** — `ch01/scenes/0130_the_counter` (3 panels) — and 0150's
  sheet changes shape: its panel 2 is dropped and a new panel 4 added, so **0150 must be regenerated,
  not patched**. `./story_prompt.py packages` and then the two sheets.
- `story/refs/stolz.png` and `story/refs/guildclerk.png` are both needed before 0130 can be drawn;
  they are the only two faces in the clip.
- Still open from the merge pass and untouched here: the `hart_yard.hart_day2`, `halm.town_healer_2`
  and `halm.shepherd_market_3` ids (ids are still frozen), and `"Find the guard beast."` in
  `src/chapter01.h`.
