# Writer — chapter one, draft three (2026-09-21)

One revision pass on draft two. Brief: the owner's notes on draft two (binding), then the ten fixes
in `cold-read-ch01-draft2.md` where they do not conflict with the owner.

## The numbers

| Scene | Draft 2 | Draft 3 |
|---|---|---|
| 0110 The Yard | 22 | **14** |
| 0130 The Counter | 28 | **14** |
| 0140 Supper | 31 | **14** |
| 0150 The Sword | 25 | **14** |
| 0160 The Herder | 28 | **15** |
| 0180 What Was On Him | 28 | **15** |
| **Total boxes** | **162** | **86** |
| Field text ids | 80 | **66** (52 chapter-one + 14 kept for map validation and the boss exchanges) |

## The four cuts that matter most

1. **The clerk's manual.** Five boxes of rules recited and repeated back, at minute twenty, replaced
   by the rule itself as the second line of the scene: "No sword, no signature." The other two rules
   now come out of {{RIVAL}} gloating.
2. **Every coaching line.** "Watch the arm — it comes back on the left", "Stand there and watch the
   arm come", the whole escalating `day2_b/c/d` hint ladder, and the two lines that described the
   parry as it happened. The chapter's one discovery is now made in silence, by the player.
3. **Hart delivering Ottilie's biography.** "Fever took her mother and her father in one winter. She
   was nine." — gone, with Falke's "I didn't know that" after it. He has always known.
4. **The interpreting sentence at the end of an examine.** "Something stood them together before it
   put them down", "that is the whole machine", "Whatever put these animals down did not care which
   kind they were". Fourteen field lines lost their second sentence; `hart_yard.machine_swing` is one
   sentence and is frozen there.

## How the player learns Ottilie's family now

Three things, none of them a speech, none of them addressed to Falke, in this order:

- **`halm.ottilie_house`** (mandatory, the lane home at dusk goes past her open door): *"One room,
  swept, everything put away, one chair at the table. Two coats on the hook by the door."* No one
  comments on it, then or ever.
- **`halm.town_healer`** (optional, day one): Linde, talking about somebody she is fond of, to a boy
  who is not the point of the sentence — *"I took her in the winter the fever went through…"*
- **The table, in C3:** Hart, seven words: *"Eat here tomorrow as well. You've eaten alone enough."*
  Ottilie gets out of it in one line about a spoon, which is what she always does.

It is never sad on purpose and it is never explained. The player assembles it; Falke never has to.

## What Ottilie does on the wall now

She is there because watching Falke get whomped is the best show in {{HOME_TOWN}}. She keeps score
out loud, rates the falls, winces happily, eats an apple, and patches him afterwards quickly and
routinely because it is a wrist and she is good. She does not know how the swing is beaten and has
never tried to find out — that is now written into `characters.md` so no later draft can hand her the
hint system again.

> **Ottilie:** Feet over his head. That's the best one all week.
> **Ottilie:** Don't ask me. I sit on a wall and eat apples.

Her one straight line in the chapter is after the win, and it is four words: *"It suits you."*

## Kept, deliberately

Distel's voice and "She has a name"; the ring of nine healed holes; Hart's economy and his one
oblique line ("You hit everything as hard as you can"); "No sword, no signature"; the empty hook in
`hart_yard.workshop_door` paid off by "Made it the winter before last. It's been on its hook since";
"I had a very good thing ready to say and now I'm not saying it"; the three lines after the kill.

## Refused

- **0160 and 0180 sit at fifteen boxes.** Every further cut removed something the player needs or
  something both the owner and the cold reader liked. Logged in `ch01_room/writer_to_designer.md`.
- **Ottilie's "What is a {{GUARD_BEAST}}?"** stays: it is the new-player rule working, not exposition.
- **`hart_yard.practice_posts` and the two `west_road` lines** stay because the maps trigger them and
  `check --all` fails without them. They are the designer's to delete at the trigger end.

## Also done this pass

- Falke's `look` line replaced on the owner's note (green hair, no armour, no weapon, seventeen);
  weapons taken out of Ottilie's and Stolz's `look` lines, since a `look` is what a portrait is drawn
  from and the owner wants nothing held or slung in one.
- Numbers reconciled with the designer: nine is the ring of holes alone; flock eleven; job five coin;
  Stolz lends three; Ottilie orphaned at seven; no "forty-one"; three machines, named, never numbered.
- `NAMES.md`, `PREMISE.md`, `THREADS.md`, `characters.md` updated; `chapter01_script.md` regenerated.
- `check --all` clean, `names` clean, no dialogue line over 100 characters.
