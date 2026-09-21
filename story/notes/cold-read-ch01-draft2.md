# Cold read — chapter one, draft 2

I read exactly two files: `story/v3/chapter01_script.md` and `story/field/text.md`. Nothing else.
I know nothing about this game. Everything below is what the page actually put in my head.

**Unresolved tokens.** The script reads as finished names. `text.md` does not — I met these as unknown
proper nouns and had to guess: `{{MENTOR}}`, `{{HEALER}}`, `{{HERO}}`, `{{TOWN_HEALER}}`, `{{RIVAL}}`,
`{{SHEPHERD}}`, `{{HOME_TOWN}}`, `{{HIGH_PASTURE}}`, `{{BEAST_NAME}}`, `{{HERDER_PEOPLE}}`/`_PL`,
`{{VILLAGER_2}}`, `{{LOOT_YARD}}`, `{{LOOT_TEACHING_FIND}}`, `{{GRAIN_CREATURE_PL}}`,
`{{NIGHT_CREATURE_PL}}`. Two I could not resolve at all from the text: `{{GRAIN_CREATURE_PL}}` —
something lives in the grain shed and **I never find out what it is**, and `{{NIGHT_CREATURE_PL}}` on
`high_pasture.lantern` — "a pale light out on the grass… you have taken a step towards it" is the most
wondrous line in the chapter and it names a plural creature that no scene ever mentions.
`{{TOWN_HEALER}}` and the script's "Linde" are presumably the same woman; I had to infer it.

---

## 1. The six scenes

### 0110: The Yard

**What I think is happening.** A young man is losing, repeatedly, to the fourth of four training
machines in his teacher's yard, and a young woman who is evidently the local medic patches his wrist
while mocking him. He asks his teacher if he's ready and gets a flat "not quite."

**Expected knowledge, and whether I had it.**
- *Falke, Ottilie, Hart* — all three named before I know which body is which. **Fine** for Falke and
  Ottilie (they address each other). Hart arrives with two lines and no introduction: "Again. From the
  gate." I know he's the boss only from Falke's next line.
- *"the last one" / "that thing"* — "**He's at the last one again**" assumes I've seen four machines
  and know the fourth is the hard one. Visually probably obvious; verbally it isn't. The field line
  `hart_yard.practice_posts` does the work, and it's optional.
- *"It comes back on the left"* — this is the chapter's core mechanic and it is stated in line two, to
  a player who has not yet been hit. **Not yet earned**; it lands as noise the first time.
- *"You said that in the spring"* — asks me to accept a history I don't have. Cheap and fine.

**Author informing the player.** "**I've watched you find that out four times this week.**" — that's a
progress bar with a voice. Also "**It is nothing. Bring me something interesting one day**" is Ottilie
telling me her theme in her second scene.

**Speaker I couldn't have identified with names hidden.** "Not quite ready yet." could be anyone
older. Everything else is voiced clearly — this scene is the best-differentiated in the chapter.

---

### 0130: The Counter

**What I think is happening.** Falke delivers a machine part to a guild hall, asks how to get a job,
and learns he can't sign for one without a sword, which only his teacher can give him. A slightly
older friend who already has one lends him money for bread.

**Expected knowledge.**
- *"the guild"* — I'm in a hall with a clerk, a counter, a board and a book, and the word **guild**
  never appears in the scene. I inferred an institution. The field line `halm.guild_hall_door` names
  it; optional.
- *"the hall keeper"* — named once, never again, never seen. I filed it and dropped it.
- *"job sheets"* — introduced cleanly here. **Good.**
- *"Stolz"* — walks in mid-scene with no introduction at all. I worked out he's a peer with a sword
  from context, which mostly works, but "**Mine took me two years to get**" expects me to already care.
- *"the well winch"* — fine, but only the optional `halm.well` tells me Hart is the town's engineer,
  which is the thing that makes Hart interesting.

**Author informing the player.** Almost the whole first half. "**The rule is that job sheets go up on
that board at dawn.**" / "**The board stays open all day. It closes on the last ring of the evening
bell.**" / "**The board is bare now because it is not dawn.**" This is a manual read aloud by a man
with no personality, and Falke obediently repeats each rule back ("At dawn. And then what happens?",
"One bell, at the end of the day."). It is the most boring two minutes in the chapter and it's at
minute 20, where a new player is deciding whether to keep going.

The redeeming line is "**No sword, no signature.**" That's a rule with a shape. Four lines later it's
explained twice more.

**Speaker I couldn't identify.** "He's right, Falke. Mine took me two years to get." — I had no idea
who had just walked in. With the name hidden I'd have guessed Hart.

---

### 0140: Supper

**What I think is happening.** Falke argues at the table that he's ready; Hart refuses and won't say
why; the conversation turns and I learn Ottilie is an apprentice too, waiting on her own
qualification, and that she's orphaned and eats here every night because her house is empty.

**Expected knowledge.**
- *"the fourth one is the one that is out there"* — the chapter's thesis. I understood it. **Good.**
- *"Linde. The town healer."* — new name, dropped in one line, never appears. I had to hold it on
  trust. (The field text calls her `{{TOWN_HEALER}}` and gives her the best line about Ottilie —
  optional.)
- *"apprentice"* applied to Ottilie — I had assumed she was simply *the* healer. This is a good
  reversal but it arrives as a fact, not a scene.

**Author informing.** "**Fever took her mother and her father in one winter. She was nine.**" —
Hart, a man who speaks in three words, delivers a biography. It's the wrong mouth: it exists so the
player is told. Ottilie's "Hart. He asked about my cooking." is a lovely save and half-fixes it.
Also "**Who decides when you stop being an apprentice?**" is a question no one asks at supper; it's
the player's question with Falke's name on it.

**Speaker I couldn't identify.** "He's right about the fourth one. You never wait, Falke." — reads
like Hart until the last clause. Several Ottilie lines and Hart lines are interchangeable when both
are being short.

**This is the best scene in the chapter.** The turn from arguing about swords to "There's no one next
door to cook for" is the only moment I felt something.

---

### 0150: The Sword

**What I think is happening.** After a day and a half of losing, Falke finally stops attacking, hears
the click, turns the arm aside, and hits. Hart immediately produces a sword he made two winters ago.
The evening bell starts ringing as he gets it.

**Expected knowledge.**
- *"That's forty-one"* — forty-one what? Losses, presumably; I counted my own, not this number.
- *"There's a click first. The weight drops, and then the arm comes."* — Falke **says the solution out
  loud** at the moment I'm meant to have discovered it. If I'd just worked it out with my thumbs this
  would deflate it. The optional `hart_yard.machine_4` already tells me the same thing.
- *"A first sword comes from your teacher. That is the sign."* — quoted verbatim from the clerk in
  0130. Deliberate, and it works; the only repeated line I liked.

**Author informing.** "**A hunter who only attacks dies out there. Today you waited.**" — the lesson,
stated as a lesson. The scene already proved it. One of these two sentences should go.

**Best beat in the chapter.** "I had a very good thing ready to say and now I'm not saying it." /
"Say it!" / "No. You'd have it framed." I laughed.

**Trouble.** The sword and the bell land in the same eight lines. Falke gets the thing he has wanted
for four years and has approximately one line to feel it before a timer starts. I felt rushed past
the payoff, not excited.

---

### 0160: The Herder

**What I think is happening.** Up on the night pasture Falke finds sheep asleep in the grass and
something crouched over one of them — a girl, not a monster. She's Mohn, a people who herd at night,
and the thing taking the sheep is her own animal, which has stopped obeying her.

**Expected knowledge.**
- *"Mohn"* — a whole people, named once, with no gloss except her own "You'd say night herder." I
  accepted it. **This is the wonder arriving and it's handled well.**
- *"Like in the carters' song?"* — a **callback to an optional field line** (`halm.carter_saying`). If
  I talked to nobody, Falke references a song I've never heard, and the moment reads as a mistake.
- *"drover"* — the game knows I don't know it, has Ottilie ask, and answers. **Correct and welcome.**
- *"He's the monster on my job sheet. He's been taking her sheep for weeks."* — "**her**" is the
  shepherd, whom I only met in optional text, and the job sheet's content is **only** in optional text
  (`halm.job_sheet`). A player who read nothing has signed for a job he was never told the terms of.
- *"Nine sheep"* — Falke counts nine; the job sheet says nine were lost over a month; `sleeper_4` says
  nine lie in the circle. Three different nines, and I couldn't tell whether these were the *same*
  missing sheep or new ones. **This reads as a contradiction.**

**Author informing.** "**We keep flocks above the trees. We work in the dark.**" and "**He walks a
flock together and puts it to sleep, so it's safe until morning.**" — both are encyclopaedia entries,
though Ottilie's asking makes them tolerable.

**Speaker I couldn't identify.** "She's a girl. Falke, she's a girl — put the sword down." is
unmistakably Ottilie. Distel is distinct from her first line. This scene has the clearest voices after
0110.

---

### 0180: What Was On Him

**What I think is happening.** Falke has killed the drover to stop it killing Distel. Examining the
body they find a healed ring of nine small holes behind its ear — something was done to this animal by
someone. The shepherd arrives, pays four coin, and the three of them decide to go up the mountain.

**Expected knowledge.**
- *"Garbe"* — arrives, names herself in the third person, has three lines, leaves. I had met her only
  in optional field text.
- *"Behind his ear"* — and **the optional examine `high_pasture.body_2` already showed me the ring of
  holes** before the scene. Good if I examined; if I didn't, this is the discovery and it's fine.
- *"A drover does not forget his handler. Not ever. It doesn't happen."* — the rule I need for the
  hook, stated at the exact moment it's broken. Efficient but bald.

**Author informing.** "**So something was done to him. If it was done to mine, it was done to
others.**" — that's the chapter's plot hook read off a card. Distel does the deduction, the
generalisation and the call to action in one breath.

**Speaker I couldn't identify.** "There's a ring of little holes in the skin here." — could be any of
the three. The examine text gives the same information better.

**Best line in the chapter:** "**I'm going to cry soon. Keep walking.**"

---

## 2. The people, from the page only

**Falke** (hero). A 17-or-18-ish apprentice who has trained four years and wants to be signed off as a
hunter. Wants: a sword, a job, to be taken seriously. Talks in exclamations — I counted, **roughly
half of everything he says ends in "!"**. He asks a question, gets an answer, and shouts a number back
("Four?", "Two years?", "Nine?"). Charming for ten minutes; by supper I wanted him to say one calm
thing. He has **one** good quiet line — "I didn't know that. I've known you my whole — I didn't know
that." — and it's the only time I liked him.

**Ottilie.** Unambiguously **a person**, not a helper: she is an apprentice healer waiting on her own
qualification from Linde, orphaned at nine, eats at Hart's because her house is dark, and has her own
motive for leaving the valley (the field line "She'll be delighted, and I'm not telling her" is the
best thing anyone says about themselves). **But her scene function is the problem.** Count her lines:
she is the one who says "Watch the arm", "It comes back on the left", "That's forty-one", "He turned
it aside", "Something's kneeling over that ewe", "The sheep are standing up", "It's going for her".
She is the **camera and the tutorial voice**. A first-time player meets a narrator with jokes. The
material to fix this is already written — it's the supper scene and her two hill-path lines — there's
just not enough of it against the volume of commentary.

**Hart.** The teacher. Three-word man, withholds the reason, has secretly had the sword made for two
years. Wants: the boy not to die. This is a well-worn type played straight and it works, mostly
because he never explains himself — until "Fever took her mother and her father in one winter," which
is out of character, and "A hunter who only attacks dies out there," which is the moral with his name
on it. **He is the only adult in the chapter with authority and he has maybe nine lines.** I'd like one
more of him.

**Distel.** Mohn, a night herder. **Nothing on the page tells me she's fourteen** — not her age, not
her height, not a word about it. I read her as maybe twenty and slightly feral. If fourteen matters,
it is currently invisible. Wants: to know what was done to Klee. Is she charming or grating? **Charming,
narrowly** — because her bluntness is aimed at *situations*, not at Falke ("Don't shout. You'll wake
her.", "Move your foot. You're standing on my lead.", "She has a name."). The one that tips toward
grating is "I don't need either of you," which is the standard prickly-newcomer beat; the line after it
rescues her.

**Does anyone sound like anyone else?** Yes — **Hart and Distel** are the same instrument: short flat
declaratives that refuse to elaborate ("Eat." / "Keep your voice down."). They never share a scene, so
it isn't confusing, but it means the party is Falke shouting and two people being terse at him.
Ottilie is the only one with rhythm. The clerk and Garbe are furniture.

---

## 3. The world, as I understood it

- **Magic:** I do not know whether this world has any. A dog-thing puts a flock to sleep and I cannot
  tell if that's magic, an animal ability, or a trick. Nobody in the scene reacts as though something
  impossible happened — Ottilie's response to a sleeping flock is to ask what a drover is. **If the
  owner wanted wonder, this is where it leaks out.**
- **The guild:** a hall with a counter, a clerk, a board and a book. Jobs are posted by ordinary
  people, paid in coin, and signed off by whoever posted them. Hunters are members. The word "guild"
  appears **only in optional field text**.
- **Job sheets:** go up at dawn, board closes on the last ring of the evening bell, you sign your name
  in a book, you take the sheet, you bring it back signed off. Clear. **Except:** the content of the
  only job sheet in the chapter exists solely in optional examine text.
- **The sword rule:** "No sword, no signature." A first sword is given by your teacher and cannot be
  bought; it's the sign that you're ready. **The cleanest idea in the chapter** and the thing I'd tell
  a friend about.
- **The Mohn:** a people who keep flocks above the treeline and work at night. Traded with up the
  valley twice a year, never seen in this town, present in a carters' saying. That's a real-feeling
  minor people. All of that texture except one line is optional.
- **Drovers:** animals raised from pups by a handler, walk a flock together and put it to sleep, never
  forget their handler.
- **The pasture creatures:** sheep, a dog, all asleep and unwakeable, gathered into a circle facing
  uphill, tracks that stop in open grass.

**Contradictions and wobbles.**
1. **The nines.** Nine sheep lost over a month; nine asleep in the circle tonight; nine holes behind
   the ear; Ottilie orphaned at nine; "carrying bandages since you were nine". I noticed. Once the
   number is a clue, every other nine is noise — I spent 0180 wondering if the sheep count was a hint.
2. **The fours.** Four machines, four years, four coin from Stolz, four coin from Garbe, four job
   sheets, four sessions, four tracks, "four times this week", "four coin says he has no sword by
   autumn", "bread for three"/four coin. The world has two numbers in it.
3. **Sleeping vs taken.** Distel says Klee "takes any animal he finds"; the job sheet says animals
   "turn up days later miles away, asleep"; but tonight the flock is asleep *in place* and Distel says
   putting them to sleep is what keeps them safe. So is the drover stealing them or minding them? I
   could not reconcile it, and it's the fact the whole job rests on.
4. **"No one here has ever seen one"** (Ottilie) vs **Falke recognising the term from a song** — small,
   but the song is optional and she says the flat version.
5. **The tracks that stop / "stopped putting its feet down"** implies something that flies or
   vanishes. Klee is a four-legged animal on the ground. That examine promised me a bigger mystery
   than the scene delivered.

---

## 4. Feel, bluntly

**Charmed:** the supper scene's turn ("There's no one next door to cook for. My house is dark and this
one has a fire."), and Hart having had the sword finished on its hook for two winters. That second one
is genuinely good — it recontextualises every "not quite ready yet."

**Laughed:** twice. "No. You'd have it framed." And "wait, where are we going?" in the field text
(which, note, is *optional* and is the funniest line in the chapter). "It's a wrist, Falke, not a war"
nearly landed and is one word too neat.

**Bored:** the clerk, hard. Lines 43–57 are rules delivered by a man with no interest in me, and
Falke's job in them is to say each rule back. I also sagged in 0140's first half — three rounds of
"I'm ready"/"You're not" before anything moves.

**Felt like a book, not a game:** two places. (a) Falke narrating the machine-4 tell ("There's a click
first. The weight drops") at the exact moment the *game* is supposed to be teaching me that with my
hands — a cutscene taking credit for my discovery. (b) Ottilie's play-by-play at the fight
("He turned it aside. He turned it aside and then he hit it.") — I just did that. Being told what I
did is the specific thing that makes a game feel like someone reading to me.

**Did the wonder land?** **No — not until minute fifty, and then only a crack.** Seventy minutes of
play and the fantastic content is: one girl of another people, and one dog that sleeps sheep. Before
Distel arrives, this could be a story about a farm boy in any century. The genuinely wondrous writing
is all in optional examines — "a pale light out on the grass, the same colour as your own lamp; you
find you have taken a step towards it", "four heavy tracks, then two, then flat grass for a hundred
paces", "a flat disc of pale glass on a cord, dropped where you would only find it in the dark". A
player who doesn't press the button on scenery plays a chore simulator with a nice ending. **This is
the single biggest problem in the chapter.**

**Does the kill land?** **Half.** The three lines that carry it are excellent — "He's dead. You killed
him, and he'd have killed me. Both of those are true." / "I'm sorry. I didn't want to." / "I'm going
to cry soon. Keep walking." But I had known Klee for about ninety seconds and I never saw him do
anything except be described. Distel loves him; I have no reason to. The grief is hers and I'm
watching it.

**Does the ending make me want chapter two?** **Yes.** Healed holes made on purpose, in a ring, behind
the ear, on an animal that forgot its handler. That's a hook. **What I think chapter two is:** they go
up Distel's mountain, find other drovers — or other Mohn animals — with the same ring of holes, and
start tracing back to whoever put them there. I assume a person did it, and I assume the thing that
made the holes is nine of something, and I assume the drover's sleep power is what they were being
harvested or trained for. The west road exists and someone will eventually take it.

---

## 5. The field lines

**The best five.**
1. `high_pasture.tracks_stop` — "Four heavy tracks in the mud, then two, then flat grass for a hundred
   paces. Whatever left here stopped putting its feet down." The best sentence in either file.
2. `high_pasture.lantern` — "…the same colour as your own lamp. You find you have taken a step towards
   it." Real wonder, and it acts on *me*.
3. `hart_yard.workshop_door` — "…and a hook on the back wall with nothing hanging on it." A whole plot
   hidden in a subordinate clause. Devastating in hindsight.
4. `halm.ottilie_door_2` — "…I've been carrying bandages for you since you were nine — wait, where are
   we going?" Funniest line in the chapter and the best thing Ottilie says.
5. `halm.town_healer` — "…she has never been further than the bridge." Gives Ottilie a life and a cage
   in eleven words.

Honourable mention: `hart_yard.room_sword_gap`, "You put them up when you were thirteen."

**The five to cut or fix.**
1. `hart_yard.machine_4` — "Every hit you land winds the weight up… There is a click before the arm
   comes." **Solves the puzzle in a box.** This is the one discovery the chapter has. Cut everything
   after "winds the weight up" and let the player hear the click.
2. `hart_yard.day2_c` — "Swing harder, get hit harder — that is the whole machine." Same crime, plus
   "that is the whole machine" is the designer talking.
3. `high_pasture.sleeper_3` — "Whatever put these animals down did not care which kind they were." The
   first half ("This one is a dog, and it is asleep too") *is* the discovery; the second half explains
   it to me. Cut the second sentence.
4. `high_pasture.sleeper_4` — "Something stood them together before it put them down." Same pattern
   again. Three sleepers in a row all end with the game interpreting the sleeper for me.
5. `halm.bread` — "…so mind the dog on the corner." A joke about a dog I never meet, on the one box
   that has to teach me what money is worth. Either the dog exists or the line doesn't.

Also flag: `hart_yard.practice_posts` — "Each one teaches you a different way to get hit" is a good
line doing a tutorial's job, and `halm.carter_saying` is a charming joke whose punchline ("and get
nothing done all day") is a slur about a people I haven't met yet — it may read differently once I
like Distel.

**Important things that exist ONLY in optional lines.** A player who talks to no one and examines
nothing misses **all** of this:
- **What the job actually says** (`halm.job_sheet`, `job_sheet_2`): nine animals, taken at night, found
  miles away asleep, tracks stop in open grass, pays four coin, someone wrote "monster" on it. Scene
  0160 assumes I read this.
- **Who Garbe is** (`halm.shepherd_market`, `_2`) — she walks into 0180 as a stranger otherwise.
- **That the Mohn exist at all** before Distel (`halm.carter_saying`, `halm.shepherd_market`) — and
  Falke's "Like in the carters' song?" is a dangling reference without it.
- **That Hart is the town's engineer** (`halm.well`, `halm.bell`, `halm.npc_2`) — the thing that makes
  him more than Generic Mentor, and the reason his machines are clever.
- **That Ottilie is wanted and held back** (`halm.healer_bench`, `halm.town_healer`) — supper states
  it flatly, the field text makes me feel it.
- **The word "guild"** (`halm.guild_hall_door`).
- **That asking Ottilie along is the player's choice** (`halm.ottilie_door`) — she simply appears on
  the pasture in 0160 otherwise.

That last group is the report's main finding: **the chapter's warmth, its world-building and its
setup are in the optional layer, and its exposition is in the mandatory layer.** It is exactly
backwards.

---

## 6. Top 10 fixes, ranked, cheapest repair each

1. **Move the job sheet into a scene, or into a forced box.** The one job in the chapter is readable
   only by choice. *Repair:* make `halm.job_sheet` + `job_sheet_2` fire on taking the sheet (it's
   already "reading it is how the player takes it" — make that mandatory), or give the clerk one line:
   "Nine off the hill in a month, found asleep miles away. Somebody wrote 'monster' on it."
2. **Stop the game telling me what I just did.** *Repair:* cut `hart_yard.machine_4`'s click sentence,
   `day2_c`'s "that is the whole machine", Falke's "There's a click first. The weight drops, and then
   the arm comes." (0150 l.112–113), and Ottilie's "He turned it aside and then he hit it." (l.115).
   Her next line, "I had a very good thing ready to say," works better with nothing before it.
3. **Rewrite the clerk scene as conflict, not rules.** *Repair:* cut lines 43–47 entirely. Open on
   "When can I take a job?" → "No sword, no signature." Let the dawn/bell rules come out of Falke
   arguing, and put the closing-bell rule in the mouth of someone who cares (Stolz: "And it shuts on
   the last ring, so don't be up a hill at supper"). Saves two minutes of dead time at minute 20.
4. **Get the wonder in before minute fifty.** *Repair:* promote two existing optional lines to
   mandatory boxes on the way up the hill — `high_pasture.lantern` and `high_pasture.tracks_stop`.
   They cost nothing, they're already written, and they're the two best sentences in the chapter.
5. **Give the sword thirty seconds before the bell.** *Repair:* move "That's the evening bell" three
   lines later and add one beat of Falke holding it — the game's whole first act is about this object
   and it currently gets one exclamation. One line from Ottilie, not a joke.
6. **Fix the numbers.** Nine is a clue; four is a motif; both are used for everything. *Repair:*
   change the sheep counts so only the holes are nine. Make the flock eleven, or seven. Change
   Ottilie's orphaning age off nine. Change one of the two four-coin payments.
7. **Take the camera out of Ottilie's mouth.** *Repair:* cut or reassign her play-by-play
   ("Something's kneeling over that ewe", "The sheep are standing up, all of them, at once", "That's
   forty-one") and let the art and the field boxes do it. Keep every line where she's a person.
   Roughly six deletions; she gets sharper, not smaller.
8. **Resolve the taken-vs-minded contradiction.** *Repair:* one line for Distel — "He's been walking
   them off the hill and bedding them down where he thinks they're safe. He doesn't know whose they
   are any more." Answers the job sheet and deepens him in one sentence.
9. **Give me a reason to care about Klee before he dies.** *Repair:* one memory from Distel in 0160,
   concrete and small — she already has "I raised him from a pup"; give her the pup. Five seconds buys
   the kill.
10. **Stop Hart delivering Ottilie's biography.** *Repair:* give the two lines to Ottilie, flat and
    fast, and keep Hart's "He did." Or leave it to the field text, which already handles her house.

*Eleventh, if it's free:* Stolz walks in unannounced. One line of Falke seeing him first ("Oh, of
course you're here.") fixes it.

---

## 7. Five things that work and must survive

1. **"No sword, no signature."** The whole first act hangs off one rule that is concrete, unfair, and
   solvable. Don't soften it and don't explain it a third time.
2. **The finished sword on its hook the whole time.** Set up in an optional examine
   (`hart_yard.workshop_door`: "a hook on the back wall with nothing hanging on it"), paid off by
   "Finished and on its hook. Waiting for you to stop swinging." That's real craft.
3. **The supper turn** — from arguing about swords to "My house is dark and this one has a fire," and
   Ottilie shutting it down with "He asked about my cooking." The only place a character surprised me.
4. **The three lines after the kill.** "Both of those are true." / "I'm sorry. I didn't want to." /
   "I'm going to cry soon. Keep walking." Do not add a fourth.
5. **The ring of nine healed holes behind the ear.** A physical clue that is specific, old, and
   deliberate, and a rule to break it against ("A drover does not forget his handler"). That's why
   I'd start chapter two.
