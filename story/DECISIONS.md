# DECISIONS — log for the owner to review

The orchestrator makes story and scope decisions while the owner is away and records them here,
newest last. Each entry says what was decided, why, and how to reverse it. Disagree with any of
them and the work can be redirected; nothing here is expensive to change until art is generated.

## D1. Story base: Pitch C "The Erased Town", with imports from A and B
**Decided:** The story is built on `pitches/pitch_c_the_erased_town.md`. World: Ourn, the Sallow Basin,
failing skywells. Peoples: humans, Kell, Vess, Onn. Institution: the Clement Works, mining memory
("strands") out of the Seam under the Ancient Halls. Spine: the town of Tellwater was blanked in one
night and rebuilt with employees; Bron's warm childhood was issued to him and belongs to Maren Ostry,
the woman the company made and lost.
**Why:** All three writers independently chose memory-as-industry and Bron-as-the-lie, which says the
premise is sound. C has the strongest Phantasy Star IV feel (failing planetary systems, a grown
caretaker people, hunters among several peoples), ties the unreliable past directly to the pursued
figure (the Final Fantasy VII architecture, by an original route), and its evil is the most human:
paperwork, postings, and a chairman whose sin is that he can live with it.
**Imported from A:** the opening image of a company tent behind the ancient door; a company officer
who comes from the exploited people themselves (A's Cabre Sunn idea).
**Imported from B:** a patronless hunters' guild with a job board and optional side contracts, which a
twenty-hour game needs; the last image of a new job on the board.
**Not taken:** A's memory-burned-as-lamp-fuel (too close to a famous energy source); B's weather
machine and the "someone stays inside the machine forever" ending.
**To reverse:** pitches A and B are kept in `story/pitches/`. Say which you prefer and the bible is rewritten.

## D2. Lyra dies in Act IV (chapter 11), as Pitch C proposes
**Decided:** Keep it. She is one of two characters with approved art, and that art is used for eleven
chapters; it is not wasted. Her death is by a frightened young company officer, not a villain's
flourish, and Bron knowingly takes her last hour as the first memory he is sure is real.
**Why:** It is the emotional hinge of the memory theme and the most human kind of harm in the story.
**Risk:** a mid-game healer's death echoes both source games. The circumstances are original.
**To reverse:** swap the death to Pip (C flags this option) or cut it; Act IV needs a different hinge.

## D3. Scale and scene economy
**Decided:** Fifteen chapters in five acts, sized to a Phantasy Star IV length game. Three scene types:
panel cutscenes (one ChatGPT sheet each, about 5-8 per chapter, for moments that earn them), talk
scenes (dialogue box with speaker portraits, no new art), and narration. Target for the whole game:
roughly 90-110 panel scenes and 150+ talk scenes.
**Why:** At one generated sheet per panel scene, 100 sheets is a realistic amount of art for one
person; 300 would not be. Talk scenes carry banter, town business, and aftermath, as the source game does.

## D4. Tools added this run
**Decided:** A tools agent added `type: talk` scenes, a `portraits` command that crops each character's
head-and-shoulders portrait out of their reference sheet, portraits beside the dialogue box in the
game, and an example talk scene after "The Warning". Compiled and validated; not yet seen on the phone.

## D5. Model
The owner asked for Opus 4.6 subagents. The agent tool only offers "opus", which resolves to the
current Opus model. All subagents in this run use that.

## D6. Story bible accepted; working title "The Fair Copy"
**Decided:** `story/bible.md` (about 22,600 words) is accepted as the source of truth for all later
writing. Working title: **The Fair Copy** (the company's file name for Maren Ostry, and what Bron's
childhood is). The bible logs fifteen of its own decisions in its section 16; the ones most worth your
eye are: Junior Director Vane renamed **Ottoline Kerrow** (name collision with a Final Fantasy
villain); Bron's own past was **erased, not stored**, so he can never be restored; and the hard rule
that **nothing in this world is ever copied**, only subtracted from someone.
**Why accepted:** its sixteen hard rules for the Seam keep the evil human (the Seam never acts or
intends; inversion kills nobody and leaves calm survivors who cannot say their names; nothing is
undone at the end). Lyra's death is caused by one man's act of conscience and another's paperwork,
not by a villain. The guild master is an Onn older than the company, which gives the guild weight.
**Not yet done:** the game's title screen still says "Ancient Halls". Left until you confirm a title.
**To reverse:** edit the bible; every later file is derived from it.

## D7. Scene file naming for the full game
**Decided:** new scenes are named `CCSS_slug.md`: two digits of chapter, two of order within the
chapter, in steps of five so scenes can be inserted (for example `0310_the_welcome.md`). The existing
`001`, `002`, `003`, `003b`, and `p01` keep their names because scene 003's approved panel art is
keyed to its file name. `story/playlist.md` lists every chapter's scenes in play order; the game only
plays the `## intro` list, so art-less chapters never show up on the phone by accident.
