# Cold read — chapter one, draft four

I have read two files and nothing else: `story/v3/chapter01_script.md` and `story/field/text.md`.
I know nothing about this game. Everything below is what the page gave me, in the order it gave it.

A note before I start: the field file is full of `{{TOKEN}}`s that were never substituted for me —
`{{MENTOR}}`, `{{HEALER}}`, `{{HERO}}`, `{{RIVAL}}`, `{{SHEPHERD}}`, `{{HERDER}}`, `{{HERDER_PEOPLE_PL}}`,
`{{BEAST_NAME}}`, `{{TOWN_HEALER}}`, `{{HOME_TOWN}}`, `{{HIGH_PASTURE}}`, `{{VILLAGER_2}}`,
`{{NIGHT_CREATURE}}`, `{{GRAIN_CREATURE_PL}}`, `{{LOOT_YARD}}`, `{{LOOT_TEACHING_FIND}}`. I treated each
as an unknown proper noun. Where a `- what:` note told me who a token is I have said so, but a player
does not read `- what:` notes, so I have also said what the line does with the name hidden.

---

## 1. Scene by scene

### 0110 The Yard (panels)

**What I think is happening.** A young man is failing, repeatedly and painfully, at one of three
machines in a hilltop yard, while a girl sits on the wall eating an apple and enjoying it. She patches
his wrist with a light she holds in her hands. The older man at the bench tells him to go again and,
asked straight out, says he isn't ready.

This is the best opening scene of the six. It is almost all behaviour: the apple, the bandage already
in her lap before he's hurt, "It hit you once." Nothing is announced.

**Expected me to know already.** Nothing hard, which is the win. Three small ones:
- "**the swing**" — Ottilie says "He's at the swing again" over a picture of a *yard*, and the arm
  that hits him is the next panel. First time through I attached "swing" to the wrong object for a
  beat. The field line `hart_yard.machine_swing` names it, but only if I examined it.
- "**Hart**" — Falke says the name at line 46 over the first picture of the man. That's fine; the
  picture does the introducing. But nothing tells me what Hart *is* to him. Teacher? Father? The
  script never says, and I liked that, but see §3 — it matters later.
- The warm light between Ottilie's palms is never named or reacted to by anyone. Falke's "That's it?"
  reads as surprise at the *speed*, not at the magic. I concluded magic is ordinary here. I think
  that's deliberate and it works.

**Author informing me.** None. Genuinely none. This scene is clean.

**Couldn't attribute with names hidden.** No problem — two voices, one dry and one sore, sharply
distinct from the second line.

**Pictures vs words.** One misfire: the picture at line 25 (Ottilie close up, apple and bandage) is a
*reaction* shot, but the line under it, "It hit you once," is a punchline that wants to land while I
can still see him on the ground. And "My wrist's fine" (36) is spoken over the top-down shot of him
flat in the grass — that's the joke working — but then the next picture is already the healing hands,
before Ottilie has said anything about patching him. The hands arrive one line early.

### 0130 The Counter (talk, no pictures)

**What I think is happening.** Falke delivers a repaired part to a guild hall and asks for work. The
clerk refuses on a rule: no sword, no signature, and the sword has to come from your teacher. A
smoother, older boy called Stolz appears, says you can't buy one, that he did it in two years, and
lends him three coin.

**Expected me to know already.** This is where the chapter starts assuming.
- "**guild**" — the clerk uses "the guild book" as though I've heard of the guild. I hadn't, in the
  script. The field line `halm.guild_hall_door` is marked MANDATORY and says "the word *guild* enters
  the game here" — so the word arrives in the **field**, in a box I read on the way in, and the clip
  then proceeds as if it were established. If that trigger is missed the clip is opaque.
- "**the catch**" — "The catch goes on the counter" is the clerk's first line. I have never seen the
  word. It's in `hart_yard.bench_part` and `hart_yard.errand`, both field. Two field-only nouns in the
  first two lines of a scene is a lot of load on the field.
- "**Sheets go up at dawn**" — job sheets, unexplained, thrown away in a throwaway line. I worked it
  out only when `halm.job_sheet` turned up later.
- "**Stolz**" — appears from nowhere with no picture (talk scene) and Falke names him. I have no idea
  what he looks like, how old he is, or what he is to Falke. I guessed: rival, slightly older, friendly
  enough. That guess came entirely from "Mine took two" and the three coin.
- "**Four years**" is dropped here and repeated in 0140 and in three field lines. I got it.

**Author informing me.** "The rule is that your teacher gives you your first sword — / ... Then I write
you in the guild book." The clerk saying it twice is a nice character joke (he is a man who repeats
rules verbatim) and I'll allow the repeat. But the *second* half, "Then I write you in the guild book,"
is the author closing the loop for me. I already had it.

**Couldn't attribute.** "Sheets go up at dawn. Don't be up a hill when the bell rings." — with names
hidden I'd have guessed the clerk, not Stolz. It's procedural, not personal. It also happens to be a
piece of foreshadowing worn as a coat: he's telling me tomorrow's minigame.

**Pictures.** None — and this is the scene that most wanted one. The one image I want in the whole
chapter is Stolz, once.

### 0140 Supper (talk, no pictures)

**What I think is happening.** Falke announces he'll be at the board at dawn; Hart says no, twice, and
won't say why; Ottilie says Hart has never told her either, which tells me Hart's silence is a habit,
not a snub. Hart then says "Eat here tomorrow as well," which I read as the warmest thing he can manage.

Ten lines, no waste. I liked it.

**Expected me to know already.** "I beat two of your machines" — fine, I saw three. "You have never
beaten the swing" — fine.

**Author informing me.** No. "He's never told me either" is Ottilie doing plot work, but she's doing it
as a joke at Hart's expense, so it plays as character.

**Couldn't attribute.** No. Hart's one-word lines are unmistakable by now, which is a good sign.

**Pictures.** None. The silent box (line 82, Ottilie smiling with no words) before "I'll be here" is the
best piece of punctuation in the chapter and it works *without* a picture — I read it as her being
caught out by the invitation. That's a talk scene earning its format.

**The scene description I was shown is cut off mid-word**: "a workshop book open on the bench by the doo".
Same in 0130: "an empty job board on the wall be". If those strings reach the player, that's a bug.

### 0150 The Sword (panels)

**What I think is happening.** A second day at the same machine, at the end of it. He is wrecked, he
goes again, and this time he turns the arm aside with the stick and gets in at the post. Ottilie, who
has sat there all day, stands up. Hart walks over with a sword he made two winters ago and gives it to
him.

**Expected me to know already.** Nothing new. This scene is self-sufficient, which is the right call
for the payoff.

**Author informing me.** "Take it to the clerk. He'll write your name in the guild book." — this is the
third time the guild book has been explained to me, and here it's Hart, of all people, doing a
signpost. Hart has spoken eleven words in two scenes; spending four of his lines on a to-do list is the
worst use of the chapter's quietest man. "Take it to the clerk" alone does everything.

**Couldn't attribute.** "It suits you." could be anyone. Fine — it's the right size.

**Pictures vs words.** The problem scene. The winning blow (103) is a *beautiful* panel and it is
correct. But:
- Ottilie's "I've eaten two meals up here" is over the wide establishing shot, and then her close-up
  arrives on *Falke's* line "Don't start counting." Her joke and her face are on different pictures.
- "I had a very good thing ready to say and now I'm not saying it" — over a picture of her sitting up
  straight with the town below. That's lovely and it's the funniest line in the chapter.
- **The picture does not show me what he worked out.** Panel 103 shows the stick meeting the arm side
  on and turning it away. I can see *that* he parried. I cannot see *why it worked now and not the
  fifty times before*, and nobody says. More on this in §3 — this is my single biggest note.

### 0160 The Herder (panels)

**What I think is happening.** On a black hillside full of sleeping sheep, Falke finds a very small
hooded figure crouched over one, draws on her, and Ottilie physically stops him. The figure stands: she
is not human — grey fur, huge black eyes, leaf ears. She says "Mohn." She's a night herder; nobody here
has ever seen one. Her drover — some large animal she works with — ran off in the spring and stopped
recognising her, and he is the monster on Falke's job sheet.

**Expected me to know already.**
- "**Mohn**" — the answer to "What are you?" is one word I have never heard, and the script never
  unpacks it. Ottilie's next line, "You're a night herder," does the work instead, so I understood
  *what she does* and not *what her people are*. I ended the chapter thinking "Mohn" might be her
  species, her people, or her job. Field line `halm.lamp_charm_2` and `halm.shepherd_market` both
  gesture at "{{HERDER_PEOPLE_PL}} up the valley, no one has seen one" — with the token unresolved I
  literally cannot confirm that's the same word.
- "**drover**" — the script does the right thing and has Ottilie ask, "What is a drover?" That is the
  cleanest piece of exposition in the chapter because the question is in character (she's out of her
  depth all night) and the answer is one sentence. Keep it exactly.
- "**Klee**" — named in line 149 before I've seen him: "Klee put them down. My drover." Two unknown
  nouns and a possessive in six words. I got there, but only because "my drover" followed.
- "**my job sheet**" — Falke says it as if I've read it. I have, if I hit the MANDATORY field box.
- "**Get away from her!**" — Falke's first line about the ewe. He's shouting *her* about a sheep, which
  pays off four lines later as Distel's joke ("That's a sheep!" / "She has a name."). Nice.

**Author informing me.** "You're a night herder. No one here has ever seen one." Ottilie is telling
Distel what Distel is. It's the one line in the chapter where somebody clearly speaks to the audience.
It's also information I'd already had twice in the field. Cut or halve it.

**Couldn't attribute.** "Don't shout. Take that lamp off her face." — I loved this and could not place
it on first read; it arrives over a close-up of *Falke's eyes*, so the speaker is off-picture. That's
a deliberate reveal and it works, but it is the one place the picture and the voice are meant to
disagree, so I want to flag that it read as intentional.

**Pictures.** Strong. The two-panel reveal of Distel (standing wide, then close) is the best pair in
the chapter. One quibble: "He ran in the spring. He stopped knowing me." — the most devastating line
of the chapter — plays over *no new picture*; it's the fifth line stacked on the close-up of her face.
It deserves its own beat, and the face we're on is "lamp light on one cheek", a pretty shot, not a
grieving one.

### 0180 What Was On Him (panels)

**What I think is happening.** The animal is dead by Falke's hand. Distel is not sentimental about it
in front of them and then nearly is. Behind his ear there's a healed ring of nine holes — somebody put
them there, and that is the hook. The sheep wake up and walk home; the shepherd pays five coin; Distel
says she's going up the mountain alone and the other two go with her.

**Expected me to know already.**
- "**Garbe**" — a new named speaker in the last twelve lines, with a picture but no introduction. I
  assumed: the shepherd who posted the job. The field lines confirm a `{{SHEPHERD}}` exists and posted
  a sheet, but the *script* never connects Garbe to her, so if the player missed `halm.shepherd_market`
  Garbe is a stranger collecting a flock.
- "**Nine**" — Ottilie says the number and it is treated as significant by the staging (it's her only
  line on that panel). I did not know why nine mattered and I still don't. I took it as a hook. The
  field file's own note says "Nine is the ring of holes and nothing else in this chapter" — good, but
  from the page nine is just a count, and Ottilie saying it flat is the chapter telling me to remember.
- "**A drover does not forget his handler**" — first time the word "handler" appears; I inferred it
  means Distel. Fine.

**Author informing me.** "He's dead." over a picture of a dead animal lying in flattened grass is the
clearest example in the chapter. Cut it — her silence is the line. (You already have a silent box for
her two lines later.) Also "Someone did that to him" is the thesis statement of the chapter said out
loud; it works because it's angry rather than explanatory, but it's on the edge.

**Couldn't attribute.** "Put it somewhere you won't sit on it." — over a picture of coins in a palm.
With names hidden this is obviously Ottilie and it's the last laugh of the chapter. Good.

**Pictures vs words.** "I'm going up to find out what was done to him. I don't need either of you." is
spoken over a picture of **Ottilie putting a coat round her shoulders**. That is the best
picture/word disagreement in the whole script — the image contradicts the line and settles the
argument before Ottilie answers it. Deliberate, and it's the best thing in the scene. Keep.

One misfire: Distel's "I've never seen those" (177) plays over a wide low-angle of her standing over
the body with dawn behind — a heroic shot for a line of bewilderment.

---

## 2. The people

**Falke.** A four-years apprentice who wants to be signed off and is not. Wants: permission. Talks in
short, hot, defensive sentences, almost all of them arguments ("I hit it a dozen times!", "I've trained
four years!", "Then tell me what I'm doing wrong!"). He is sympathetic because he is *wrong* about
himself and keeps getting up. I liked him. His one soft moment is "I'm sorry" over the body, and it
lands because he has never once apologised before. What I don't know: his age, whether Hart is his
father, where his parents are. The field line `hart_yard.room_sword_gap` ("two pegs on your wall, at
the height you could reach when you were thirteen") is the only hint that he's been here since he was
a child, and it's optional.

**Ottilie.** The best-written person here. Healer, roughly his age, funny, the only one who enjoys
anything. Wants: him to be all right, and — this is the quiet one — to get out of the valley. How she
talks: undercuts, counts things, never comforts. "It hit you once." "I've eaten two meals up here."
"You'd have it framed." "Walk in the middle, you're asleep on your feet."

*Is she a person or a helper?* **A person — but only just, and only if I walk around.** In the six
clips she is entirely reactive: she watches, she patches, she quips, she follows him up a hill. Her
own life exists in exactly three places, all field, and two of them optional:
- `halm.ottilie_house` (MANDATORY) — "One room, swept, everything put away, one chair at the table. Two
  coats on the hook by the door." I understood this *immediately and completely* and it is the single
  most effective piece of writing in either file. One chair, two coats. Somebody left, and she still
  keeps their coat. No one comments on it, which is why it works.
- `hill_path.ottilie_dark_2` (optional) — "She's wanted me out of this valley for a year. I'm not
  telling her I went." So there's a mother, or a teacher, alive, wanting her gone. That reframes the
  two coats — maybe nobody died, maybe somebody is *waiting for her to leave*. I found that genuinely
  good and it is **optional**.
- `halm.town_healer` (optional) — "I took her in the fever winter." So she was taken in. Orphan or
  near it, trained by the town healer.

So: I learned her home life **from a room and a joke**, not from a speech, and it was the best
experience the chapter gave me. But two of the three are missable, and the *script* — the part every
player sees — contains nothing about her at all. She has no want in the clips. Her one line that hints
at a want, "I've never been anywhere after dark," is a field line.

**Hart.** The teacher. Engineer — I worked that out from the machines, the scorched apron, the wire
spectacles, and the field lines about the well winch and the bell axle, and I enjoyed working it out.
Knee brace, so he was hurt once; nobody mentions it, good. Wants: him not to go out too early. Talks
in two-to-four word imperatives. Total lines in the chapter: eight. That restraint is why "Made it the
winter before last" hits — he has been quietly ready for two years. Best character economy here.
What I don't know and wanted to: what he is to Falke, and what he *did* before. `halm.gate_watch_after`
— "He gave you that himself, then — he doesn't sell anything he makes" — is a lovely optional line
about him.

**Distel.** Small, furred, not human, a night herder of the Mohn. Wants: to know who did this to Klee.
Talks in flat corrections and refusals: "Don't shout." "Move your foot." "I don't need either of you."
*Charming or grating?* **Charming**, and by a wide margin, because her rudeness is always *about
something real* — the lamp in the ewe's face, the foot on the lead, the sheep having a name. Grating
would be rudeness for texture; hers is all competence. Her three best lines are "She has a name," "You
stank of fear the whole fight," and "I'm going to cry soon. Keep walking." That last one is the
chapter's finest line: it's funny, it's a refusal to be comforted, and it's a child.

*Did I understand what she is?* I understood **not human, small, furred, big-eyed, leaf-eared,
peaceful, works at night, keeps animals.** I did **not** understand what "Mohn" means as a word, or
whether there are many of them, or where they live, beyond "up the valley" from an optional carter.
That's probably right for chapter one; I only want the word to have one more anchor.

**The clerk.** A rule. Two lines, one of them repeated verbatim, which characterises him perfectly:
the man will not improvise. Wants: correct paperwork. I didn't need more and didn't get more. Good.

**Stolz.** The thinnest. Appears once, in a scene with no pictures, to say he was faster than Falke and
then to lend him money. That contradiction is interesting — is he a friend or a needle? — but it is
never resolved, and I have no face for him. His only other appearance is the optional `halm.rival_door`
("You waited two days for a sword and you've spent it on sheep"), which is much sharper than anything
he says in the clip, and which most players won't see.

**The animal's death.** Honest answer: **I felt it at one remove.** I felt for *Distel*, not for Klee,
because I never met Klee as anything but a threat — I have no picture of him alive, no moment of him
being hers. What carried it was entirely her reaction, and specifically the silent box after Falke's
"I'm sorry". After: yes, the coat panel, and "I'm going to cry soon" — those two landed properly. But
the emotional load is all on a character I met eleven lines ago. The field line `high_pasture.sleeper_3`
("This one is a dog, and it is asleep too") did more to make me uneasy than the boss did.

---

## 3. The world and the game

**The guild.** A hall with a counter, a clerk, a book, and a board where job sheets go up at dawn. You
sign, you take a sheet, you get paid. I understand it as an employment office for hunters. I don't
know who runs it, whether it's one town's or many, or what a "hunter" is licensed to do. That's fine
for an hour.

**Job sheets.** Posted by a member of the public (Garbe/the shepherd posted this one and the old
hunters laughed at it), pinned at dawn, taken at the board, read as they're taken, paid on completion
by the person who posted. I understood this from `halm.job_sheet` + Garbe's "Five coin, the way the job
sheet said." Clean.

**The sword rule.** "No sword, no signature. Your teacher gives you your first sword." Understood
immediately, and it's an excellent rule because it makes a character, not an institution, the
gatekeeper. You cannot buy your way past your teacher's opinion of you. That's the whole chapter.

**Magic.** Ordinary and small. A healer closes a wrist with light between her palms in a few seconds; a
woman lights a lamp wick with two fingers as a favour in the street. Nobody explains, names, or is
impressed. I understood the *rule* — magic is a trade skill here — and I liked it a lot. I do not know
if Falke can do any, and nothing suggests he can.

**The Mohn.** See above: Distel's people/kind, night herders, up the valley, never seen in Halm.

**Drovers.** Distel's answer: "He beds a flock down where he thinks it's safe." So a drover is a large
animal that works *for* a Mohn handler, guarding and settling a flock. I got this from the one line and
it was enough.

**What's wrong with Klee.** He ran in the spring and stopped knowing her. Behind his ear: a healed ring
of nine holes, all the same size, healed shut a long time ago — so the thing done to him predates the
running by a long way. "Someone did that to him." So: a person, deliberately, with an instrument, years
ago, and the effect is that he forgot his handler and started putting animals to sleep and carrying
them away. I understood all of that and it is a very good hook.

One thing I could not reconcile: the sheep are found *asleep and not to be woken* miles away, days
later. Klee putting them down is a *gentle* act. Yet Klee also charges and has to be killed. Is he
stealing them, moving them, or mothering them? Distel says "He beds a flock down where he thinks it's
safe" — which suggests he is doing his *job*, wrongly, in the wrong place, for someone who isn't there.
That reading is beautiful and the chapter never quite says it. If that's what's intended, one line.

**How the training swing is beaten.** This is my biggest note.

What I was told: **`hart_yard.machine_swing` — "Every hit you land winds the weight up."** That is
the whole rule, in one field box, marked MANDATORY, and it is *excellent*. From it I can derive: hit it
a dozen times and you have built the blow that knocks you flat. So you must not keep hitting it. So you
must either not hit it, or take it off-line.

What the script shows: panel 103 — the stick meeting the arm **side on, turning it away**, and the other
shoulder driving in at the post.

So **did anyone tell me? No. Did I work it out? Yes — but only from the field box, and only afterwards.**
The clip itself does not let me solve it, because I never see him *choose*. Line 100 is "All right. Come
on, then." and the next thing is the win. The designer's note says the player finds the answer with
their thumbs over four sessions, and if that's true in play then the clip is right not to say it — but
as *reading*, the parry arrives as luck, not as a decision. The one thing that would fix it without a
word: a panel between 100 and 103 of his **hands changing on the stick** — the script's own note says
"you find the answer with your thumbs", and that picture is not in the script.

Everyone conspicuously not telling him is well handled: Ottilie "He's never told me either", Hart "Eat",
and the field notes are disciplined about it ("Nothing about the click, the side or the answer", "She
never calls a swing"). That discipline is the right call and it's holding.

**Contradictions I noticed.**
- Falke: "I beat two of your machines!" But `hart_yard.machine_arm` says the arm-machine's purpose is
  "so you can practise getting somebody else out" — that's a rescue drill, not something you *beat*. Minor.
- Ottilie in 0150: "I've eaten two meals up here" — in a scene the note places at the *end* of day two,
  after four sessions. Two meals in one day up a hill is fine, but I first read it as two days' worth
  and then had to re-read. And in 0140 Hart says "Eat here tomorrow as well," so I expected her at
  Hart's table on day two, not on the wall.
- Stolz lends three coin for bread; `halm.bread` says a coin a loaf and you're carrying three, and to
  put them on Hart's slate. So the bread is *both* on the slate and paid for in cash. Which is it?
- The clerk says the board is where sheets go up; `halm.board_closed` says the board is bare planks on
  day one; `halm.guild_hall_door` also says the board beside the door is bare. Two boxes telling me the
  same bare board.

---

## 4. Feel

**Where I was charmed.** The apple and the bandage already in her lap. "It hit you once." The silent
smile before "I'll be here." The lamp-wick woman in the road. One chair, two coats. "She has a name."
The coat round Distel's shoulders under the line about not needing anyone. "I'm going to cry soon. Keep
walking."

**Where I laughed.** "I had a very good thing ready to say and now I'm not saying it." / "No. You'd have
it framed." — out loud. Also "Four years of my suppers and you still hold the spoon like a hammer" and
"You stank of fear the whole fight."

**Where I was bored.** 0130, the counter. It's the only scene that is purely mechanical — a rule
delivered, refused, restated, and a stranger inserted — and it has no pictures to carry it. It is also
where three unexplained nouns arrive at once. Second place: the middle of 0160, lines 147–152, which is
six consecutive boxes of world-information in a row while three people stand still in the dark.

**Where it read like a book rather than a game.** Three places. (a) "You're a night herder. No one here
has ever seen one" — narration in a mouth. (b) "He's dead." — caption for a picture. (c) "Take it to the
clerk. He'll write your name in the guild book" — quest log in Hart's voice. Everything else reads as
people.

**Too wordy?** Barely anywhere, and that's a real achievement at 85 boxes. The two spots: the
drover/Mohn block in 0160, and the clerk's doubled rule. The field file is *tighter* than the script;
its one-sentence discipline is working, and `hart_yard.machine_swing` at seven words is the model.

**Too clipped to follow?** Two. "Mohn." as a complete answer — one word for a whole people, never
returned to. And "Klee put them down. My drover." — two unknowns and a possessive before I know what
either is; the sentences are in the wrong order. "My drover put them down. Klee." would have read.

**Does the sword land as "now he may sign"?** **Yes.** This is the chapter's spine and it holds. The
rule is planted in 0130 by a man who won't bend, refused in 0140 by a man who won't explain, and paid
in 0150 by that same man producing something he made *two winters ago* — which retroactively says he
was never withholding out of doubt in the boy, only in the moment. "I can sign tonight" is the right
three words. My only reservation is that the *earning* (the parry) is invisible to me, so the sword
feels given on a schedule rather than won — see §3.

**Does the ending make me want chapter two?** Yes, strongly, and specifically because of the holes. Not
because of the monster, which is resolved, but because the holes are *healed shut a long time ago* and
someone put them there on purpose. I think chapter two is: the three of them go up the mountain, find
where Klee was kept or made, and discover that whoever does this to drovers is still doing it — and
that it's people, not a creature. I also expect Ottilie to be leaving the valley for good without
telling whoever wanted her to, and for that to cost her something.

That's a strong end. The chapter's last line, "Which way is up your mountain?", is exactly right —
it's a question, it's his, and it hands her the lead.

---

## 5. Field lines

**Best five.**
1. `halm.ottilie_house` — "One room, swept, everything put away, one chair at the table. Two coats on
   the hook by the door." Best writing in either file. Says a whole life and asks nothing.
2. `hart_yard.machine_swing` — "Every hit you land winds the weight up." Seven words, and it is the
   game's entire central puzzle. Perfect.
3. `hart_yard.book` — "one line a day: which machine, the date, and how it went. Your name is on nearly
   every line of four years." Four years stops being a number and becomes a stack of paper.
4. `high_pasture.tracks_stop` — "then two, then flat grass for a hundred paces. Whatever left here
   stopped putting its feet down." Genuinely frightening. Best horror beat in the chapter.
5. `hart_yard.room_sword_gap` — "Two pegs on your wall, at the height you could reach when you were
   thirteen." He has been waiting for this since he was a child, and he built the hook for it himself.

Honourable mention: `high_pasture.lantern` — "You find you have taken a step towards it." The one line
that describes *me* and gets away with it.

**Five to cut or fix.**
1. `halm.board_closed` — "Bare planks and a lot of old nail holes." Duplicates `halm.guild_hall_door`,
   which already told me the board is bare. Cut one. (Also: "nail holes" three scenes before "a ring of
   small round holes" is an unfortunate rhyme on the chapter's key image.)
2. `hart_yard.practice_posts` — "each one teaches you a different way to get hit." A good joke, but the
   note admits nothing points at it, and it overlaps the three individual machine lines. Cut.
3. `hart_yard.bed` — "Sleep." Not a line. Make it a prompt, not an examine box.
4. `halm.shepherd_market` + `halm.lamp_charm_2` — two different strangers say almost the same sentence
   ("they're up the valley / I have never seen one"), and then Ottilie says it a *third* time in 0160.
   Three statements of the same fact. Keep the carter (it's MANDATORY), trim the other two.
5. `halm.town_healer` — "One season's work outside this valley and her standing is hers." I could not
   parse "her standing is hers" on one read. Whatever it means about qualification, say it plainly.
   Also `hart_yard.machine_arm` — "practise getting somebody else out" is the only field line that made
   me stop and re-read the mechanism.

**Important things living ONLY in optional lines.**
- **Ottilie's reason for being on the hill at all** — `hill_path.ottilie_dark_2`, "She's wanted me out
  of this valley for a year. I'm not telling her I went." This is her only *want* in the entire
  chapter and it is optional. Promote it.
- **That she was taken in** — `halm.town_healer`. The two coats are unreadable without it, or at least
  much dimmer.
- **That Hart doesn't sell what he makes** — `halm.gate_watch_after`. This is what makes the sword
  mean something, and it's optional and it only fires during a timed run, when nobody reads boxes.
- **Stolz's only interesting line** — `halm.rival_door`, "You waited two days for a sword and you've
  spent it on sheep." Sharper than everything he says on screen, and optional, during the same run.
- **The shortcut** — `halm.marta_after`. Gameplay-critical information in an optional NPC during a
  timed sequence.

Putting four of the chapter's best lines inside a timed run is my structural worry. During a countdown
I do not talk to people.

---

## 6. Top ten fixes, ranked, cheapest repair each

1. **The parry has no visible cause.** Add **one panel** between "All right. Come on, then." and the
   winning blow: his hands changing grip on the stick, close. No words. Your own designer note already
   says "you find the answer with your thumbs" — put the thumbs in the picture.
2. **Give 0130 one picture of Stolz.** It's the only scene with no images and the only character with
   no face. One panel: him leaning in the doorway with three coin held out. Cheaper than any rewrite.
3. **Cut "He's dead."** The picture says it. Let her silent box come first instead.
4. **Cut or halve "You're a night herder. No one here has ever seen one."** It's the one line that
   speaks to the audience, and the field has already said it twice. "No one here has ever seen one" on
   its own is a reaction, not a briefing.
5. **Cut "He'll write your name in the guild book" from Hart's line.** Leave "Take it to the clerk."
   Hart has eight lines; don't spend one on a quest marker.
6. **Reorder "Klee put them down. My drover."** → "My drover put them down. Klee." Unknown noun after
   the known one. Free.
7. **Move "He ran in the spring. He stopped knowing me." onto its own panel** — and make that panel her
   face doing something other than being pretty in lamp light. It's the line the chapter turns on.
8. **Promote `hill_path.ottilie_dark_2` to MANDATORY**, or move it into the clip on the way up. It is
   Ottilie's only want and it's currently optional. One box.
9. **Move the three field lines that carry weight out of the timed bell run** (`gate_watch_after`,
   `rival_door`, `marta_after`). Either put the shortcut on the route as a sign, or start the clock
   after the player leaves the hall. Design fix, no new words.
10. **Fix the two truncated talk-scene descriptions** ("on the wall be", "on the bench by the doo") —
    and decide whether the bread is on Hart's slate or paid with Stolz's three coin. Both are typos-
    level, both are visible to a player.

Near misses: give "Mohn" one more anchor somewhere (a single repetition by Ottilie later, using the
word); cut one of the two bare-board boxes; consider one panel of Klee *alive* and hers, anywhere,
so his death has somewhere to land.

## 7. Five things that must survive

1. **The sword rule, exactly as it is.** "No sword, no signature. Your teacher gives you your first
   sword." A person, not an institution, decides when you're ready. It is the chapter.
2. **Nobody explains the swing.** Not Hart, not Ottilie, not a box. The field line
   `hart_yard.machine_swing` is the only clue and it should stay at seven words forever.
3. **One chair, two coats** — and the fact that no one comments on it. The moment someone says a
   sentence about Ottilie's family, the chapter loses its best trick.
4. **Distel's rudeness, all of it.** "Move your foot." "She has a name." "You stank of fear the whole
   fight." "I'm going to cry soon. Keep walking." Do not soften one word.
5. **The coat panel over "I don't need either of you."** The picture contradicting the line is the most
   sophisticated thing in the chapter and it costs no dialogue at all. Do more of this.

Also, quietly: the three silent boxes. They are the chapter's best punctuation. There is room for a
fourth.
