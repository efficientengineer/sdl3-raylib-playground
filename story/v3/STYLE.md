# STYLE — how this game is written

Game form, not book form. If a page could be read aloud as a novel, it is wrong.

## A chapter is three things

**`## Goal`** — the words on the player's screen. A verb and a place. "Clear the entrance. That's the
whole job." A new `## Goal` line goes in whenever the player's job changes.

**`## Play: <kind> — <place> (<minutes>)`** — 3-6 short lines, the design, in front of the player's
hands. Kinds: explore / fight / boss / puzzle / travel / shop / talk-to-townsfolk / chase.
Say what the player is doing, be concrete about mechanics ("the howler calls another dog every two
turns until it dies"), then use the labels:
**Find:** what's in there. **Unlocks:** a door, a road, a party member, a weapon, a clue.
Every clip has a Play block in front of it. No two clips touch.

**`## Clip: <title>`** — a cutscene, which means **dialogue**. `BRON: ...`
**Ten lines of dialogue, maximum.** Longer than that, split it in two and put gameplay between them.
At most one stage direction per beat, in brackets, and it must be an action someone performs:
`[Dorn puts the axe on the table.]` A clip with more bracket lines than speaker lines is a scene
written as a book. Delete the brackets and let them talk.

## Rules for clips

- **Someone wants something from someone else and says it out loud before the clip ends.**
- People interrupt, talk over the answer, and walk out mid-sentence.
- Big feelings get said, loudly, by the person having them. Declare. Sulk. Apologise badly.
- Jokes sit next to the melodrama. That is how the melodrama survives.
- End on a line, never a gesture.
- Write what a real person would blurt. A line that sounds composed has been composed twice.

## Plain words

Eight invented proper nouns exist in the whole game: **Bron, Lyra, Zeph, Pip, Dorn, Cray, Sevran,
Fallow**. Everything else gets described: *the ancients. The old halls. The old tower. The guild.
The dry city in the west. The hum.* A new capital letter is a cost. Spend none.

## Banned — grep for these before any file is called done

Each line is prefixed `BAN:` so the checker skips this block.

```
BAN: the word "nobody" — write "no one", or write who
BAN: "not X but Y" and "isn't X, it's Y" — say the thing you mean and stop
BAN: "doesn't take it", "doesn't answer", "doesn't look", "doesn't move" — silence beats; make them talk
BAN: "which is worse"
BAN: "and that is the first time it hurts"
BAN: "one degree wrong"
BAN: sentences that explain what a moment means
BAN: any narration longer than two lines
BAN: characters who refuse to say what they feel
BAN: a joke, image or phrase used twice for a neat callback — keep the better one, cut the other
BAN: aphorisms and epigrams — "Ready is a word other people use about you afterward"
BAN: any line audibly written to be quoted, except the last line of a chapter
BAN: summing-up lines that restate the feeling — "It's about me. It was always about me."
BAN: rule-of-three list sentences — "charming, funny, and kind"; "he sneers, he preens, and he hates"
BAN: "[beat]" as a stage direction — cut it or give somebody a line
BAN: mood adverbs in stage directions — "radiant", "delighted", "ruefully", "coldly"
BAN: "for the first time"
BAN: age measured in dust — "a thousand years of dust", "dust had made a shoulder of it"
BAN: narrator asides that explain the joke or the reward — "which is your first money"
BAN: similes about ruins — "like the hill grew around something that refused to move"
```

The banned habits all do the same thing: they let the writing admire itself instead of handing the
player a scene. A character who will not say it has no scene. Give them the line and let them regret
it next chapter.

## Ten lines in the voice

1. **PIP:** I didn't steal it. It was on the ground. After I put it there, granted, but still.
2. **ZEPH:** There are nine books about that building. There are nine. I've read them.
3. **DORN:** I was four feet behind you with a spear and you've not shut up about it since.
4. **CRAY:** Say it louder, the clerk missed it.
5. **BRON:** Fine. Then I'll do it badly.
6. **LYRA:** You went without me. Apologise properly or I'll make you do it twice.
7. **SEVRAN:** It rains on eleven thousand people now. Go on. Say the bad part.
8. **PIP:** Wait, is he coming with us? Is that a thing that's happening?
9. **ZEPH:** I can't hear anything. Should I be able to hear something?
10. **BRON:** He was right. I'm going anyway.
