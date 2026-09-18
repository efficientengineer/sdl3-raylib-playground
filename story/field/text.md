# Field text — what the world says when you look at it

Every line of examine text in the field lives here, not in the `.map` files and not in the engine.
A map's `message` trigger names an id; this file is what that id says. `./story_prompt.py export`
writes `src/field_text.h` from it and `./story_prompt.py check --all` validates it.

Format, one `## <map>.<id>` per line of text:

- the heading is the map's file name, a dot, and a short id for the thing being looked at:
  `## halm.well`. Nothing else may carry a dot.
- the first plain line under it is **the text**. Tokens are allowed and expected — write
  `{{MENTOR}}`, never the name — and the tool substitutes before the game ever sees it.
- `- what:` is a note for the writer: which trigger this is and where it fires. Never exported.

**Rules, the same ones dialogue answers to.** Two sentences at the most: this is a box on a phone
screen that somebody dismisses with a thumb. No word on the banned lists in `story/v3/STYLE.md` and
`story/v3/SMELLS.md` — "nobody" is the one that keeps coming back; write "no one", or write who.
The validator enforces both, and an unknown token is an error.

A line still written as `[<id>: TODO]` is a placeholder: the tool exports it as it stands, so an
unwritten line shows up in the game as an obvious bracket rather than as silence. Replace the whole
line, brackets and all.

## halm.guild_hall_door
[halm.guild_hall_door: TODO]
- what: the shut guild hall door, the message trigger at 20 15 on halm

## halm.well
[halm.well: TODO]
- what: the capped well in the middle of the town, the message trigger at 17 16 on halm

## hart_yard.practice_posts
[hart_yard.practice_posts: TODO]
- what: the row of practice posts in {{MENTOR}}'s yard, the message trigger at 16 3 on hart_yard
