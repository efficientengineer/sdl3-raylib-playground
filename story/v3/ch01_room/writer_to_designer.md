# WRITER → DESIGNER — chapter one

One-way channel. The writer writes here; the designer writes in `designer_to_writer.md`.
Nothing here is a decision about what the player DOES — only about what is said and who people are.

---

## 1. Your four questions, answered

1. **The clerk has a handle already.** `## Guildclerk` in `characters.md`, with `- alias: Clerk`.
   Per `STYLE.md` a role-only speaker gets no token, so write him as `CLERK` in your files and
   `Clerk:` as a speaker label in a scene. **Do not add `{{CLERK}}`.**
2. **{{SHEPHERD}} is on screen twice, and your design is better than mine was.** She is a market NPC
   on day one (`halm.shepherd_market`, `halm.shepherd_market_2`) saying the job exists and that the
   old hunters laughed at it — so the player hears about the job before they can take it, which makes
   the sheet on day two land — and then in person after the boss, to pay. She is **not** at the board.
3. **Shown, and not even shown as a limit any more.** The owner's correction landed while I was
   writing: {{HEALER}}'s "wasn't strong enough" is a fact about a nine-year-old and has no bearing on
   her now. She is quick and reliable, she closes {{HERO}}'s wrist in a moment while teasing him, and
   she does not sit down afterwards. `high_pasture.ottilie_limit` is gone; the line in that slot is
   now `high_pasture.ottilie_ready`, which is her being unbothered and still teasing.
4. **The herder is {{HERDER}} = Distel.** Thistle: small, prickly, a plant word, and one word a
   player can say. She is **a girl, fourteen**, and the game says "she" from her second line, because
   the new-player rule makes an ambiguous pronoun expensive and the player has to be able to hold her.

## 2. Your assumptions — all eight accepted

Including 2 (the {{GRAIN_CREATURE_PL}} stay; `halm.grainwife` sends the player in after them) and 5
(the run cannot be failed permanently). **The line you asked me to write for 5 is
`halm.clerk_signing_late`:** *"The bell has stopped and the shutter is halfway down. Put your hand on
it and I will call that the last ring."* The on-time version is `halm.clerk_signing`. The clerk is
covering for him on purpose and never says so.

## 3. Clip slots: conformed exactly

Six scenes, your names, all six passing `check --all`. Nothing I needed was missing a slot, so I have
not asked for a seventh clip. The two things I would otherwise have made clips are in your field:

- **The board** is `halm.board_sheets` then `halm.job_sheet` + `halm.job_sheet_2` — the only place in
  the chapter the job is written down.
- **The boss turn** is the two exchanges you staged: `high_pasture.boss_break` / `boss_break_2`
  (Distel calls Klee and it works for a moment) and `high_pasture.boss_turn` / `boss_turn_2`
  (he doesn't know her, and {{HEALER}} tells the player what to do). Thank you for freezing control
  and keeping the boss UI up — that is exactly why I wanted it played rather than reported.

## 4. Everything in §2 of your file is written

All ids in `story/field/text.md`, `check --all` clean. Differences from your list, all small:

- `halm.npc_1..5` → I kept the four ids the existing `halm.tmap` already triggers
  (`halm.gate_watch`, `halm.marta`, `halm.ostler`, `halm.grainwife`) so your maps keep validating, and
  added `halm.npc_2`, `halm.npc_5`. **Each has a `_after` twin** for the bell run, as you asked.
- `hart_yard.machine_1..4` written, plus `hart_yard.practice_posts` (the row as a whole), which the
  existing `hart_yard.tmap` triggers.
- `hart_yard.supper_1..3`, not 1..4 — three boxes get the player to the table and C3 is the rest.
- `high_pasture.sleeper_1..4` each say one thing more than the last: asleep → they all face uphill →
  it took a dog as well → nine of them lying inside one flattened circle, so they were **gathered**.
- `high_pasture.tracks_stop` is the line I want the player to quote: *"Four heavy tracks in the mud,
  then two, then flat grass for a hundred paces. Whatever left here stopped putting its feet down."*
- `west_road.culvert` and `west_road.west_end` are rewritten rather than deleted, so the existing
  `west_road.tmap` validates. Chapter one does not play that map.

## 5. Two notes on your side of the line

- **"You hit everything as hard as you can" is now Hart's line at supper**, straight after "It isn't
  about hitting it" — so the effort system is stated in words on day one and played on day two.
  Nothing else in 0150 changed; the forty-one losses and "my arms are gone" were already the system.
- **{{MACHINE}} does not need a token.** "Machine" is a common word and {{MENTOR}} calls them
  machines; `NAMES.md` rule is that role-only and common words stay plain English. The player's one
  plain word is **machine**, used everywhere in the scenes and the field lines.

## 6. Dropped rows

I have **not** deleted the old creature and loot rows you listed ({{ROAD_CREATURE}} and the rest).
They cost nothing, `names` reports them as unused, and D23 says nothing is canon until it is on
screen — so they are a menu for chapter two, not debt. Say the word and they go.
