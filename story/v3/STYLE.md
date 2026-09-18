# STYLE — how this game is written

Game form, not book form. If a page could be read aloud as a novel, it is wrong.

## Names are tokens

Every character, place and thing name is a token in double braces, including speaker labels:
`{{HERO}}: Come down.` Never write a name into the story text. Tokens are UPPER_SNAKE.
`NAMES.md` is the only file that holds a real name, so a rename is a one-line edit.
Role-only speakers stay plain English and get no token: `CLERK`, `SOLDIER`, `BAKER`.

## A chapter is three things

**`## Goal`** — the words on the player's screen. A verb and a place. "Clear the entrance. That's the
whole job." A new `## Goal` goes in whenever the player's job changes.

**`## Play: <kind> — <place> (<minutes>)`** — 3-6 short lines, the design, in front of the player's
hands. Kinds: explore / fight / boss / puzzle / travel / shop / talk-to-townsfolk / chase.
Say what the player is doing, be concrete about mechanics ("the howler calls another dog every two
turns until it dies"), then: **Find:** what's in there. **Unlocks:** a door, a road, a party member,
a weapon, a clue. Every clip has a Play block in front of it. No two clips touch.

**`## Clip: <title>`** — a cutscene, which means **dialogue**.
**Ten lines of dialogue, maximum.** Longer, split it and put gameplay between the halves.
At most one stage direction per beat, in brackets, and it must be an action someone performs.
A clip with more bracket lines than speaker lines is a scene written as a book.

## Rules for clips

- **Someone wants something from someone else and says so, in words, before the clip ends.**
- People interrupt, talk over the answer, and get told they are wrong.
- Big feelings get said, loudly, by the person having them. Declare. Sulk. Apologise badly.
- Jokes go *before* the vulnerable line, or two lines after it. Never immediately after.
- End on a line, never a gesture. Do not let the mentor or the rival close two clips running.
- Write what a real person would blurt. A line that sounds composed has been composed twice.

## Five rewrite rules

**R1 — When a joke arrives, write the second joke.** The person the cliché landed on answers it.
**R2 — When a scene ends on a callback, end it one line earlier.** If the last line reframes, echoes
or completes something from earlier in the clip, delete it. If the earlier line only existed to set
that up, delete that too.
**R3 — When a character would make a gesture instead of answering, make them answer and delete the
bracket.** Every `[he picks it up]` is a line the writer declined to write. This is a pass, not a
principle.
**R4 — Break the ritual.** If a clip is executing the shape the premise predicted, put someone
through it on unrelated business. The obligations still get discharged, sideways, and no one gets to finish
their speech.
**R5 — Replace the noble motive with the petty true one, and let the dead be criticised.** Grief,
guilt and fear all flatter the confessor. Vanity, loneliness, laziness and "I liked you needing it"
do not. If the dead man gets eulogised as the finest of his generation, give him instead a flaw that
killed him and a habit that annoyed someone.

## Plain words

Nine named things exist in the whole game and they all live in `NAMES.md`. Everything else gets
described: *the ancients. The old halls. The guild. The dry city in the west. The hum.*
A new capital letter is a cost. Spend none.

## Banned — grep before any file is called done

Each line is prefixed `BAN:` so the checker skips this block.

### Prose habits
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
BAN: a joke, image or phrase used twice for a neat callback — keep the better one
BAN: aphorisms and epigrams
BAN: any line audibly written to be quoted, except the last line of a chapter
BAN: summing-up lines that restate the feeling — "It's about me. It was always about me."
BAN: rule-of-three list sentences — "charming, funny, and kind"
BAN: "[beat]" as a stage direction — cut it or give somebody a line
BAN: mood adverbs in stage directions — "radiant", "delighted", "ruefully", "coldly", "quietly"
BAN: "for the first time"
BAN: age measured in dust — "a thousand years of dust"
BAN: narrator asides that explain the joke or the reward — "which is your first money"
BAN: similes about ruins
BAN: ALL-CAPS dialogue
BAN: a line beginning with an ellipsis — "...X"
BAN: leading "Then you..." / "Then he..." as a retort's first word
BAN: a confession delivered as one sentence chained with "and"
BAN: exact integers doing the work of observation — one number per clip
```

### The reversal-as-revelation
```
BAN: "it isn't guarding the door from us, it's guarding us from the door"
BAN: any reveal that flips the object or subject of the first guess
BAN: a guardian, rule, object or person whose true purpose is the inverse of what it seemed
BAN: apparent hostility revealed as care; apparent care revealed as hostility
BAN: "it was never X, it was always Y" in any register
```
A revealed purpose must be a **different thing**, not the mirror of the first guess. Test: write the
first guess and the reveal as two sentences. If the second is the first with the nouns swapped or the
arrow reversed, throw the reveal out and find a third answer — a date, a procedure, a debt, a name,
an unfinished piece of admin. The building in chapter one is not hostile and not protective; it is
doing a headcount from a day in the distant past and cannot close it.

### Retrieved lines and shapes (four writers found these independently)
```
BAN: "Louder" as a standalone imperative line
BAN: "Say it" / "Say the thing" / "say what he calls you"
BAN: "out loud" / "in front of everyone" / "everybody" / "all these people"
BAN: "so the back can see" / "loud enough for the back" / "the back row"
BAN: "and it's yours"
BAN: "There it is."
BAN: "That's mine." as a first claim
BAN: "pale square" / "pale mark" / "where something used to hang" / "has hung a long time"
BAN: "my whole life"
BAN: "Eleven years." as a sentence on its own
BAN: "heavier than I thought" / "than he thought" / "than it looks"
BAN: "it's heavy" as a reaction to lifting anything
BAN: "Gods," as an interjection
BAN: "I told him he was ready" / "he was ready" as a confession pivot
BAN: "it was always about me" / "it was never about you"
BAN: "coward" / "a coward's plan"
BAN: "Best I ever trained" / "the best there was"
BAN: "carried him home" / "carried his boots" / "what was left of him" / "weighed nothing"
BAN: "Go anyway." / "Go on." as a final line
BAN: "Why now?"
BAN: "boy" as the mentor's vocative
BAN: "Shut the door" paired with cold or the knee
BAN: "Sit." / "Sit down" as a first line
BAN: "Look at me." / "Not at them — at me."
BAN: "I say it to everyone" / "He tells everyone that" / "You say that to everyone"
BAN: "I'm just naming a price"
BAN: a crowd counted in a bracket — "Twenty hunters", "Thirty hunters"
BAN: "one good job left" / "one slip left worth taking"
BAN: a job read out as "<place>, <thing>, <number> coin"
BAN: "plucks" / "peels" / "unpins" a paper off a nail or board
BAN: "walks out" / "He's gone." / "already gone" as a bracket's last words
BAN: "That's my father's." / "That's Dad's axe."
BAN: "It's yours." / "Then you'll be wanting this."
BAN: "I've been standing here since"
BAN: "he swung this all day"
```

### Structural defaults — not greppable, checked by reading
1. The mentor's confession that inverts his catchphrase (he told the father he was ready; the father
   died). Have the catchphrase already explained, badly, years ago, and make the scene about
   something else.
2. The rival's price being public self-humiliation. If he wants something handed over in ten seconds,
   it is a transaction, not a rivalry. Make him right instead of make him cruel.
3. The renege — the rival taking payment and not delivering. Cheaper than making him correct.
4. Mumble → "Louder." → shout. Any escalation built from repeating a line at higher volume.
5. A scene ending on the physical property of its symbolic object.
6. Prop placement explained as self-punishment. Objects do not need motives.
7. The healer as conscience: a moral imperative, then overruled. Give her the want the premise gave
   her or cut her from the scene.
8. The joke that caps the confession, in the same speech.
9. Absence-as-image in the opening bracket: the mark where the thing was, the empty hooks, the wall
   that looks naked.
10. The one-word send-off as the last line. "Go." "Dawn." "Go to bed."
11. The uninterrupted ritual: a scene that executes the premise's shape with no third party, no
    accident, and no one leaving early.
12. The older man closing every scene. The protagonist has to end scenes he is in.
13. The mentor's death proving the hero was ready. Let it prove nothing.
14. The villain's offer being ideological. Make it an offer the hero would take on the terms alone.
15. Ending the game on a callback to its first image.

## Ten lines in the voice

1. **{{THIEF}}:** I didn't steal it. It was on the ground. After I put it there, granted, but still.
2. **{{SCHOLAR}}:** Your survey crew. I am the survey crew. I paid the guild to be the survey crew.
3. **{{MENTOR}}:** I charged his father. He paid part of it and owed me the rest, so that's inherited as well.
4. **{{RIVAL}}:** Leave his line open. He's good for it.
5. **{{HERO}}:** Don't be kind to me in front of the book. Be kind to me outside where I can hit you.
6. **{{HEALER}}:** He's been talking about this job since the spring and he has never once asked me.
7. **{{VILLAIN}}:** I have a wet city and you have a dry one. Which of us would you like to be angry with?
8. **{{THIEF}}:** I want to go home and I want you all to know I said so.
9. **{{SCHOLAR}}:** I can't hear anything. Should I be able to hear something?
10. **{{HERO}}:** Write it small.
