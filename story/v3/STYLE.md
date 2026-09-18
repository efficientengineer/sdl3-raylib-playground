# STYLE — how this game is written

Game form, not book form. If a page could be read aloud as a novel, it is wrong.

## The checklist

Run every scene against all fifteen before it is written, and again before it is done.
Rule 0 and rule 11 are the owner's; 1-10 are after Evan Skolnick, *Video Game Storytelling*.

0. **Plain.** After every clip a twelve-year-old could say what just happened, why, and what happens
   next. Lines state facts and intentions directly. No line that works only by implication: not
   "{{MENTOR}} against {{RIVAL}}, four times this year. Against you, nothing" — write "{{MENTOR}} has
   vouched for {{RIVAL}} four times. He has never vouched for you." No folksy idiom, no country
   cadence, no proverb-shaped lines. Put the reason inside the line where the player needs it: "He
   died on that road eleven years ago" beats "he never once came straight home." Keep the one quip
   per clip and keep the quip plain too. The model is Phantasy Star IV: *"Zio destroyed Molcum. We
   must go to Tonoe."* This rule outranks every other rule in this file.
0b. **Voice rides on clarity, never instead of it.** Once the fact is stated outright, say it in the
   speaker's voice from `NAMES.md`. Every fact in the plain draft must survive the voicing.
   Plain: "{{MENTOR}} has vouched for {{RIVAL}} four times. He has never vouched for you."
   In the clerk's voice: "The rule is that every job needs a hunter who will vouch for you.
   {{MENTOR}} has vouched for {{RIVAL}} four times this year. He has never once vouched for you."
0c. **Energy: punctuation carries voice.** A page of calm declaratives has no characters in it.
   Every clip needs at least one question and one exclamation that arise from what someone wants, and
   people interrupt each other with a dash. Big feelings get said: anger, fear, wanting, hurt. Each
   speaker's energy is fixed in `NAMES.md` — {{HERO}} blurts and asks, {{HEALER}} asks precisely and
   rarely exclaims, {{MENTOR}} never asks a question at all, {{RIVAL}} gloats and mocks, the CLERK
   recites and gets flustered.
1. **The player holds the controller.** A clip exists only when play cannot carry the beat, and it
   never repeats what play just showed.
2. **One sentence, always.** At any moment the player can say what they are doing and why they care,
   in one sentence — and the `## Goal` line is that sentence.
3. **Two wants meeting.** Everyone wants something sayable in one line; every scene is two of those
   wants colliding; and the villain wants something the player could imagine wanting.
4. **The middle turns.** Three acts, and the long middle must turn rather than fill: every region
   changes what the party knows or what the party can do.
5. **Find, then see, then hear.** Let the player find it. Failing that, let them see it. Telling them
   is the last resort and the weakest.
6. **Shorter.** Every line does work. Cut greetings, cut recaps, cut any line that explains the line
   before it.
7. **Backstory is not story.** Deliver it in bites, on demand, where the player asked. Never a lecture.
8. **Emotion is earned in play hours,** not in text. If the scene wants feeling the game has not paid
   for, the scene is too early.
9. **Tension and release alternate.** Every chapter has a rest and a laugh in it.
10. **Story serves the map and the mechanics.** The writer's output, in order of importance:
    Goals, Areas, Encounters, Clips.
11. **Why now, why these people, why not before, who else.** Before writing any job, goal or reveal,
    answer all four in your notes, and the answers must live in the world rather than in the plot's
    convenience. A job no one has taken for months needs a reason it went untaken. A road only
    these four can walk needs everyone else to be somewhere, for a reason, today.
12. **One beat per clip.** If a clip contains two discoveries, one of them belongs in the Play block
    or in the next clip.

## Names are tokens

Every character, place and thing name is a token in double braces, including speaker labels:
`{{HERO}}: Come down.` Never write a name into the story text. Tokens are UPPER_SNAKE.
`NAMES.md` is the only file that holds a real name, so a rename is a one-line edit.
Role-only speakers stay plain English and get no token: `CLERK`, `SOLDIER`, `BAKER`.

## A chapter is three things

**`## Goal`** — the words on the player's screen. A verb and a place. "Clear the entrance. That's the
whole job." A new `## Goal` goes in whenever the player's job changes.

**`## Play: <kind> — <place> (<minutes>)`** — three lines, the design, in front of the player's
hands. Kinds: explore / fight / boss / puzzle / travel / shop / talk-to-townsfolk / chase.
Say what the player is doing, be concrete about mechanics ("the howler calls another dog every two
turns until it dies"), then: **Find:** what's in there. **Unlocks:** a door, a road, a party member,
a weapon, a clue. Every clip has a Play block in front of it. No two clips touch.

**`## Clip: <title>`** — a cutscene, which means **dialogue**.

## Rules for clips

- **Three to six lines. Most of them tell the player what is happening or where to go next, and
  exactly one is a quip, a tease, or a feeling stated plainly.** A clip that will not fit in six lines
  is two clips with play between them. This is the first rule and it outranks the rest.
- At most one stage direction per beat, in brackets, and it must be an action someone performs.
  A clip with more bracket lines than speaker lines is a scene written as a book.
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

## Chapter density

Match Phantasy Star IV's first hour: a town, a guild, one job, one companion. Per chapter, at most:
six clips, 1,200 words, one new party member, one kind of ordinary fight plus one boss, one new
mechanic, one sighting of the ancients' work, one appearance by the rival. No puzzles unless the
chapter spends its mechanic on one. If an element is there because it was fun to write, cut it.

## The ancients are unobtainable

Their works are the size of geography and people have prayed to them for a thousand years. No one
operates them. There are three ways in and no fourth: a caretaker, their language, or one of their
own vehicles. Nothing in their work has controls of any kind, and no door opens for the hero. The
villain cannot turn anything on; he is walking to the one place where they can be woken or overruled, and
every region is a station on that road for him and for the party. Ancient things join the party one
at a time, as set pieces, never as equipment.

## Plain words

Nine named things exist in the whole game and they all live in `NAMES.md`. Everything else gets
described: *the ancients. Their work. The guild. The shrine. The road west.*
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
BAN: age measured in dust — "a thousand years of dust", "dust had made a shoulder of it"
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

### The model-default story (forbidden by name, from `story/v4/DIFF.md` §4)
```
BAN: the words "engine", "tower", "switch", "hum", "hatch", "crawler", "terrace", "console"
BAN: the terraced valley home; the floor hatch; the blue stair; the crawler tutorial
BAN: a ruin under the home town with something inside it to turn on
BAN: the drought-motivated polite official as antagonist; a dated promise of rain
BAN: a crew of four one step ahead of the party
BAN: salt flats; the stilt town; the seafloor station; the drowned forest
BAN: the boat / diving bell / flying machine ladder of vehicles
BAN: the sky ring finale; throwing the last switch by hand
BAN: the elders who do not believe the hero
BAN: "they were dead for centuries"
```

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
2. The rival's price being a public climbdown. If he wants something handed over in ten seconds,
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

1. **CLERK:** {{MENTOR}} against {{RIVAL}}, four times this year. Against you, nothing. Not one line, ever.
2. **{{HERO}}:** I'll take it. Write it down before I think about it.
3. **{{MENTOR}}:** It was your father's run. He did it thirty years and never once came straight home.
4. **{{MENTOR}}:** I'm saving it.
5. **{{HEALER}}:** Ask me, then.
6. **{{MENTOR}}:** Badly. He swung at a thing he should have run from and liked that it worked.
7. **{{HEALER}}:** Pay him, {{HERO}}. He will stand there all night at six coin an hour.
8. **{{RIVAL}}:** You'll be up all night with a loaf and I'll be in a cart.
9. **{{HEALER}}:** Wrapped, untouched, a year in the weather.
10. **{{HERO}}:** Pack the fire. We're going west before it stops.
