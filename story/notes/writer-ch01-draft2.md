# Writer's note — chapter one, second draft (*The Last Job Sheet*), 2026-09-21

Written as one half of a pair; the designer's half is `story/v3/chapter01.md`, `flow.md`,
`COMBAT.md`, `BESTIARY.md`, `LOOT.md`. Shared room: `story/v3/ch01_room/`.

## What this draft is

The first draft (the jar run, the road west, the woman under the step) was cut whole on the owner's
"no sunk cost" note. Kept, because the new chapter wants them: the training machines and the parry,
{{HEALER}} on the wall and being asked, and the sword as the sign that lets {{HERO}} sign the book.
Everything else is new.

Twelve scenes became **six clips** — the owner's "more play, fewer boxes". Nine talk scenes before
leaving town became **two**, and the errand, the night, the board and asking {{HEALER}} along are now
seventy-eight lines of field text.

## The decisions I made that the owner may want to reverse

1. **The night herders are the Mohn** (plural Mohnen), plant-word naming to sit beside Halm, Linde
   and Distel and to stay out of the bird family the humans use (Falke, Rabe, Elster). Alternates:
   *Nacht/Nachten*, *Schlaf/Schlafen*.
2. **The herder is a girl of fourteen called Distel** — thistle, which is what she is like. The
   owner left boy/girl/hard-to-tell open; I chose a girl and made it plain in her second line,
   because rule 14 makes an ambiguous pronoun expensive for a first-time player, and because
   {{HEALER}}'s chosen-family want reads cleanest when the person she takes in is a child. Alternates:
   *Nessel*, *Kiesel*.
3. **The guard animal's kind is a `drover`**, a plain English common word like `lid` and `drape`, and
   this one's name is **Klee** — clover, the name a small girl gives a pup. Alternates for the kind:
   *warden*, *fold*.
4. **What is wrong with the beast:** behind his left ear, under the matted hair, a **ring of nine
   small holes in the skin, all the same size, healed shut years ago.** Distel has known him since he
   was a pup and has never seen them. I have deliberately not decided what they are — it is a
   procedure somebody performed, not a mirror of the first guess (`STYLE.md` bans the reversal), and
   it is in `THREADS.md` as a question.
5. **{{MENTOR}}'s part ends at the sword.** He does not post the job, does not object, and scoffs at
   it: tracks do not stop, so somebody miscounted sheep. He is wrong and has not been told.
6. **{{FATHER}} does not appear and is not mentioned once.** His row stays in `NAMES.md` because
   `PREMISE.md` still uses it.

## The Ottilie correction

The owner's note arrived mid-draft: "not being strong enough has been taken far too literally". It is
now a fact about a nine-year-old, said once, by {{MENTOR}}, while she is holding a spoon — and she
gets out of it by teasing {{HERO}} about his cooking question. **She has no ceiling today.** The yard
patch-up is quick and she is rude to him through it; she does not sit down; the "limit" field line is
gone; the carter who died under her hands is gone. Her apprenticeship is a formality: {{TOWN_HEALER}}
wants a season's work outside the valley first, which is a reason to walk up a hill, not a deficiency.
Stripped from `characters.md`, `PREMISE.md`, `THREADS.md` and both scenes she is in.

Her speech also **loosened**, on the orchestrator's note the owner accepted. She had been writing as
the most formal person in the chapter, which is wrong for the cool older neighbour: contractions
always now, and "I cannot look at a wrist you are holding" became "Put it down, then. I can't patch a
wrist you're holding on to."

## What still has no answer

- Why the {{HERDER_PEOPLE_PL}} have never come to {{HOME_TOWN}}. Two people say so on day one; the
  chapter does not explain it, on purpose.
- Why magic only does light work. The chapter **shows the ceiling and never mentions it** — nobody
  in it thinks it is strange. It should stay that way until a player asks.
- Whether {{MENTOR}} dies mid-game (`PREMISE.md` beat 10). Untouched.

## Validation

`./story_prompt.py check --all` — six scenes ok, `story/field/text.md` ok (78 lines, 0 placeholders),
three tmaps ok, palette 139 images 0 problems, 0 errors. `./story_prompt.py names` — every token used
has a row. No dialogue line exceeds 100 characters after substitution; the longest is 85.
