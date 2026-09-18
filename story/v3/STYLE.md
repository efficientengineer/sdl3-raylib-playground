# STYLE — how this game is written

Game form, not book form. If a page could be read aloud as a novel, it is wrong.

## A chapter is four things

**`## Goal`** — the words on the player's screen. A verb and a place. "Get to the old halls and
clear the entrance." "Convince the old man." The goal changes whenever the player's job changes,
and a new `## Goal` line goes in when it does.

**`## Area: <name>`** — 3-6 lines. What you see, what you can do, what's hidden. Written for
someone holding a controller. Name the thing you can pick up.

**`## Encounter`** — 2-3 lines. What jumps you, how it fights, what it drops. Monsters are
described by what they look like doing, and they drop things the player will spend.

**`## Clip: <title>`** — a cutscene, which means **dialogue**. `BRON: ...`, 8-25 lines.
At most one stage direction per beat, in brackets, and it must be an action someone performs:
`[Dorn puts the axe on the table.]` A clip with more bracket lines than speaker lines is a scene
that has been written as a book. Delete the brackets and let them talk.

## Rules for clips

- **Someone wants something from someone else, and says it out loud before the clip ends.**
- People interrupt. People talk over the answer. People walk out mid-sentence.
- Big feelings get said, loudly, by the person having them. Declare. Sulk. Apologise badly.
- Jokes sit right next to the melodrama. That is how the melodrama survives.
- End every clip on a line, never on a gesture.
- No sentence anywhere explains what a moment meant. The moment happened; the player was there.

## Everyone wants something sayable in one sentence

Write the want first, then the scene. If two characters in a clip want the same thing, cut one.

## Plain words

Eight invented proper nouns exist in the entire game: **Bron, Lyra, Zeph, Pip, Dorn, Cray,
Sevran, Fallow**. Everything else is described: *the ancients. The old halls. The old tower.
The guild. The dry city in the west. The doorkeeper. The rain machine. The quiet.*
A new capital letter is a cost. Spend none.

## Banned — grep for these before any file is called done

Each line is a pattern, prefixed `BAN:` so the checker can skip this block.

```
BAN: the word "nobody" — write "no one", or write who
BAN: "not X but Y" and "isn't X, it's Y" — say the thing you mean and stop
BAN: "doesn't take it", "doesn't answer", "doesn't look", "doesn't move" — silence beats are forbidden; make them talk
BAN: "which is worse"
BAN: "and that is the first time it hurts"
BAN: "one degree wrong"
BAN: sentences that explain what a moment means
BAN: any narration longer than two lines
BAN: characters who refuse to say what they feel
```

The last two are the ones that killed the previous draft. A character who will not say it is a
character with no scene. Give them the line and let them regret it next chapter.

## Ten lines in the voice

1. **PIP:** I didn't steal it. It was on the ground. After I put it there, granted, but still.
2. **ZEPH:** This is a door from before the quiet. Touch it. Everyone touch it right now.
3. **DORN:** Ready is a word other people use about you afterward. Stop trying to say it first.
4. **CRAY:** You've got his axe, his face, and none of his job.
5. **BRON:** Then I'll do it badly. Badly is still done.
6. **LYRA:** You went without me. Apologise properly, or I'll make you do it twice.
7. **SEVRAN:** I made it rain on eleven thousand people. Tell me the part where I'm the monster.
8. **THE DOORKEEPER:** Others came. They turned it on. Please turn it off when you leave.
9. **PIP:** We are going to die down here and my last words are going to be his.
10. **BRON:** He was right. I'm going anyway.
