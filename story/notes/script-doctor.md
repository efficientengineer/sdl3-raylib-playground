# script-doctor.md — the script editor's pass

The ten places `continuity.md` named, then chapter 1 start to finish. No plot fact, cast list, panel,
staging or acting line was changed anywhere. Every PANEL scene I touched has exactly the dialogue
line count it had before. `git diff --stat story/scenes/003_the_warning.md` prints nothing.
`./story_prompt.py check` passes on all twelve files.

Files touched: `p01_prologue`, `003b_after_the_warning`, `0115`, `0845b`, `0940`, `1275`, `1350`,
`1385`, `1405`, `1435`, `1520`. Left alone on purpose: `1175`, `1085`.

---

## 1. `1385_the_inner_doors` + `1405_the_ninth_door` — the two doors

**Weak.** The join rests on a geography ruling no scene states, carried by one abstract line
("Not the gallery"). A first-time player does not hold the word *gallery*, and neither line told
them what is different about the two places. Both are PANEL scenes, so the fix had to be two
swapped lines, not two new ones.

**Changed.** In `1385`, the doorward now refuses to count these doors as hers, which plants the
count that `1405` pays:

- before — Hesk: *Every door in that hill was shut from the inside. Somebody had a reason and you've not asked them.*
- after — Hesk: *That's no door of mine. Nine in my cliff, and every one shut from the inside. You've not asked them why.*

Maren's *"No. I haven't."* still answers it, so the oath beat is intact and the scene now also says
"this is not one of the nine".

In `1405`, Bron's line is made physical — flat versus down, hers versus this one:

- before — Bron: *Not the gallery. Her doors are still shut and still held. This one goes down.*
- after — Bron: *Different door. Hers is back along the flat, still shut, still held. This one goes down.*

Hesk's opening *"Nine doors in my cliff … That's the Ninth, from behind"* is now a direct answer to
what she said at the wrong door a day earlier. Two lines, no lecture, no invented distances.

**Left alone.** Ket's *"A mile of stair, and somebody has swept it"* does the vertical contrast
already; I kept "flat" out of her mouth so the two lines do not say the same thing.

## 2. `0845b_after_the_round_refused` — a cost in five boxes

**Weak.** The cost was reported, not felt, and Bron said the meaning out loud: *"That's her stove
and that's her winter, and I did that."* Hesk's answer ("Then she has three bills and a stove") did
not parse.

**Changed.** Same five boxes. The cost is now an image and a figure the player can check against
`0820` (the company bill "pays four times the other three put together"), and Bron says the
hall-keeper's own sentence back instead of confessing:

- before — Bron: *Three bills on that board now and no company work on it ever again. That's her stove and that's her winter, and I did that.*
- after — Bron: *A clerk had it in writing by four. He took the bill off her board himself and left the pin in it.*
- after — Zeph: *Three left on it. Rats, rats and a wall. A mark and six for the lot, and nothing sealed goes up there again.*
- after — Bron: *She's got a stove up there and no wood in it.* (her line, in `0820`, returned to her)
- after — Hesk: *Then go up and say that to her in her own room. And don't say sorry after it.*

Bron's *"..."* still closes it. Beat updated to match.

**Left alone.** The girl. Nothing here says whether refusing helped her; guessing at that would be
inventing an outcome the branch does not own.

## 3. `1350_ollo_asks` — the scene chapter 13 stands on

**Weak.** The question arrived cold in box one, and the ferry-rail argument drifted off Lyra within
four lines and became a bit about bread, a sick stomach and who is cleverer. By the end nobody was
talking about her.

**Changed.** Ollo now asks it in the middle of handing out plates, so it ambushes; two people say
nothing instead of one, so the silence is long enough to be uncomfortable; and every move in the
argument is a correction *of her*:

- before — Ollo: *What was Lyra like?*
- after — Ollo: *Hold that, it's hot. Mind the handle. — What was Lyra like?*
- before — Zeph: *Leeward crate, and it was not six hours, it was four, and it was the Wick run and not the Braid one.* / *We took on water at the second siding and you were sick over the stern and blamed the bread.*
- after — Zeph: *Leeward crate, four hours, and it was the Wick run. If we are doing this, get her right.*
- after — Ket: *She sat where she could see the rest of you and not the rail. Every crossing. I have four of them.*
- after — Pip: *She was cold, though. Hands up her sleeves the whole way and she'd not say it, and I never asked her.*
- after — Hesk: *She'd not say. She'd ask you if you were.*

Ket keeps a joke (*"the water stop is the sixth siding … You are both wrong"*) so the scene is still
funny; the bread bit is gone because it was the one exchange Lyra was not in. Net one box longer
(19 → 20): the added box is Hesk's silence at the top.

**Left alone.** Bron's *"She barred her sevens"* and *"Pass me the pot. I'll do those."* Those two
boxes are the scene; nothing goes near them.

## 4. `0940_eleven_years_of_answers` — Ilsa's delight

**Weak.** With the winter cut to a glance, Ilsa said *"Lovely"* twice and had nothing to be
delighted by. Her eleven years of books were established and then never used.

**Changed.** She now has a collector's theory, which lets her wave the glance away without the
winter being retold, and gives her delight an object — the stool:

- before — Ilsa: *Lovely. Now the other kind. The thing you did every year. Everyone's got one of those.*
- after — Ilsa: *Mm. Earliest ones are always thin. I've forty of those. Now the thing you did every year.*
- before — Ilsa: *Lovely. I'll put that one with the good ones.*
- after — Ilsa: *There. The stool. Everybody gives me the fair — I've thirty fairs. Nobody ever gives me the stool.*
- after — Ilsa: *Nobody would bother to make one up. I'll put that with the good ones.*

She is exactly wrong, she says why in her own terms, and nobody in the room reacts. Net +1 box
(15 → 16) for a beat that was genuinely missing.

## 5. `1520_everything_we_are_good_at` — cut to what the player watches

**Weak.** Eight people, fifteen boxes, four failures of the same shape in a row; it read as a list.

**Changed.** Three failures on the page — Pip's joke, Zeph's edges test, Sefa's rank — then Ollo.
Ket's attempt (correct figures belonging to other people) is now in the Beat only; she keeps the one
line that matters, *"Four seconds. She was one person for four seconds."* The tag is also tightened:

- before — Ollo: *Do what again?* / Ollo: *I only asked her if she'd eaten.*
- after — Ollo: *Do what again? I only asked her if she'd eaten.*

15 boxes → 12. Zeph's probe stays in `0465`'s wording, per continuity ruling 20.

**Left alone.** Sefa's docket. It is the cruellest two boxes in the chapter and the only failure
that turns on the failer's own paperwork.

## 6. `1275_the_confession` — eleven boxes, each doing something

**Weak.** Crewe's eleven boxes were fine; the *reactions between them* were not. Two directors asked
the same procedural question twice (one idea, used twice), Bron asked "What day" and then "What day
was it filed" (one question, asked twice), and Sefa Quill stood in the room while her own instrument
was read into the minute and the scene did not place her silence.

**Changed.** Still sixteen lines; Crewe still has exactly eleven boxes, as canon fixes. The second
director's question becomes Sefa's silence, on the panel that is her portrait inset:

- before — Bron [5]: *What day was it filed.* → after — Bron [5]: *Who filed it.*
- before — Crewe [6]: *The ninth day. A flood and a subsidence. Sefa Quill drafted the instrument in two days, at nine marks and two a head.*
- after — Crewe [6]: *I did. On the ninth day. A flood and a subsidence. Sefa Quill drafted the instrument in two days.*
- after — Sefa [6]: *...*
- after — Crewe [6]: *Nine marks and two a head. One more item: a study, Extraction ninety-nine twenty-six. Completed, never circulated.*
- cut — Director [6]: *Is that study before this board?*

The box now ends on her name and the next thing the player hears is nothing from her; the money
lands cold at the head of the following box instead of trailing off the end of the last one.

**Left alone.** *"Nobody decided anything at twenty-two oh two"*, the Hungry Winter arithmetic, and
the last box with *Brae, A. — Windrow — laundry* in it. Bron does not react and must not.

## 7. `1175_the_hour` — unchanged

I read it four times looking for a word to remove and did not find one. Every box does a different
thing; the questions thin from three a box to one as the time goes; her answer to 4 is *"She used
the whole of it"* and never the name; "Bread. Too much of it" pays the fair two scenes back; "It was
warm and there was no wind at all" pays `1180`'s narration; the medallion changes hands with nothing
said about it; Bron's last box is *"..."*. "The fair" is correct for question 7 — `1155` and `1160`
are the evening and midnight before `1165`'s morning. **No change.** If anyone ever trims this
scene, the only candidate is Pip's second clause, and it should survive.

## 8. `0115_the_last_hour` — making fifteen questions worth hearing

**Weak.** The dying man had three answers and the last of them explained its own joke (*"that's a
funny thing to ask a man in a bed"*). After it, five unbroken boxes of liturgy with nothing under
them; a player skims that, and the whole ladder from here to `1585` is weaker.

**Changed.** Ten boxes still, all fifteen questions still verbatim and in order. He now answers the
seventh with the true and ridiculous thing, and asks her to write it down — she writes nothing down:

- before — Merrit: *Soup. No idea. And nothing at all — that's a funny thing to ask a man in a bed.*
- after — Merrit: *Soup. Couldn't tell you. And yesterday I got to that window and back, and it took me an hour. Write that down.*

And the box the player is waiting on — *"What did you do that you would do again?"* — gets this:

- new — Merrit [5]: *...*

That silence is bought by merging her last two boxes (*"Is there anything you want said out loud
now, while there are people? What do you want done?"*), so the count is unchanged. Reveal tags
re-hung so his silence lands on panel 5, where the gloved hand closes on his, and her last two
questions run over panel 6, the empty window he had just mentioned. Beat rewritten to describe it.

**Left alone.** *"The bunk room. Eleven empty beds and mine. And you, sister."* and *"Merrit Tack.
She used Merry. Nobody else ever got away with it."* Both are perfect and both are eight words of a
whole life.

## 9. `1435_the_boy_who_got_cold` — the joke and the four boxes

**Weak.** Two things. The joke had been paraphrased, so the callback to `0260` no longer matched the
line the player heard (*"You got cold once, didn't you?"* against `0260`'s *"You got \*cold\*? You lot
sleep in snowdrifts for fun."*). And Bron's four boxes said his decision twice.

**Changed.**

- before — Pip: *Bron doesn't mind it. Bron likes it. — You got cold once, didn't you? You lot sleep in snowdrifts for fun.*
- after — Pip: *Bron doesn't mind it. Bron likes it. — You got \*cold\*? You lot sleep in snowdrifts for fun.*
- before — Bron: *It's hers. Maren Ostry's. She was nine in it, and I'm going to keep saying so out loud.* / *I'm not giving it back and I'm not putting it down. I'll just say whose it is when I tell it.*
- after — Bron: *It's hers. Maren Ostry's. She was nine in it, and I've had the good of it for fourteen years.* / *I'm keeping it. I'll say whose it is when I tell it. That's all I'm doing about it.*

Four boxes, four different jobs: permission, the compressed winter, whose it is, what he will do.
The fourteen years are his own arithmetic (nine to twenty-three) and they rhyme with Maren's without
anybody noticing.

**Left alone.** Ollo and the cheese. Nothing needed doing to it.

## 10. `1085_the_tea` — unchanged

Checked every word against canon and against `1640`, which collects it (*"You're the one that
wouldn't take the tea."*). Anneke is Kell, complains about the heat in the shop, is friendly, brisk
and holds nothing back because she has nothing; Bron says six words; the last line is about the ruts
and not about him. There is no word in it I would change and one added line would ruin it.
**No change.**

---

## Chapter 1, start to finish

Read in play order. It is in better shape than the notes suggested; two lines were worth fixing.

- **`p01_prologue`.** *"toward something that sang to them in the dark"* → *"toward something that
  sang in the dark"*. Canon's fifth prohibition is that the Seam never notices, warns, spares or
  chooses anything; *sang to them* gives it an audience, in the first sentence of the game. This is
  also the wording the BRIEF's established facts use.
- **`003b_after_the_warning`.** Two lines. Lyra announced her own fear and then paraphrased the
  frozen line from `003` in flatter words; and the exit line was a slogan.
  - before — *That's what frightens me. It sealed itself in with the thing it was afraid of.*
  - after — *Every door in this hill was barred from the inside. That isn't keeping people out.*
  - before — *Then we go in with our eyes open. And we tell the others before we take another step.*
  - after — *Then we go in. And we go back up and tell the other two before anybody takes another step.*
  The second now states the fourth Line as an act rather than a motto and hands straight to `0175`.
  **Orchestrator: flag.** `DECISIONS` D8.9 and canon §2.9 say `003b` is "kept unchanged". My brief
  named it in the chapter 1 polish list and froze only `003`, so I took the brief; the art is
  untouched either way (`003b` is a talk scene on `003:6` as a backdrop) and both lines revert cleanly.

Left alone in chapter 1, deliberately: `0105` (the sack argument is the best introduction the party
gets); `0110`'s filthy joke, which works because it is never said; `0125`, which carries the Four
Lines plant and Tovin's *"I just said it three times"*; `0130`'s five-box winter, which is the
game's most load-bearing telling and is already the right length; `0150`'s *"And this?"*, which is
two words of stage direction and does not want content in it; `0155`, `0160`, `0165`, `0170`,
`0175`, `0180`, `0185`, `0190`, `0195`. `001` and `002` carry three bible sample lines verbatim
(*"Then something came out."*, *"Old. You said old…"*, *"It's mine because I carried it"* at `0155`)
and need nothing.

## Things the next person should know

1. `Clerk`, `Guard`, `Director` and `Woman` are still several people wearing one label. `1275` now
   has one `Director` instead of two, which helps slightly and does not solve it.
2. The rhyme between `0115`'s *"Write that down"* and `1175`'s *"Somebody write the time down"* is
   deliberate and is the two ends of the same rope. Do not let a third scene use it.
3. `0845b` now quotes `0820`'s hall-keeper (*"a stove up here and no wood in it"*) back at her. If
   `0820` is ever rewritten, that line has to survive.
