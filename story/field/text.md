# Field text — what the world says when you look at it

Every line of examine text in the field lives here, not in the `.map` files and not in the engine.
A map's `message` trigger names an id; this file is what that id says. `./story_prompt.py export`
writes `src/field_text.h` from it and `./story_prompt.py check --all` validates it.

Format, one `## <map>.<id>` per line of text:

- the heading is the map's file name, a dot, and a short id for the thing being looked at:
  `## halm.well`. Nothing else may carry a dot.
- the first plain line under it is **the text**. Tokens are allowed and expected — write
  `{{MENTOR}}`, never the name — and the tool substitutes before the game ever sees it.
- `- name: <text>` is optional: the display name the game draws over the box, for a line that
  somebody says rather than a line about a thing. Tokens are resolved here too, so an NPC whose name
  may change is `- name: {{VILLAGER_2}}`. A line with no `- name:` exports an empty one.
- `- what:` is a note for the writer: which trigger this is and where it fires. Never exported.

**Rules, the same ones dialogue answers to.** Two sentences at the most: this is a box on a phone
screen that somebody dismisses with a thumb. No word on the banned lists in `story/v3/STYLE.md` and
`story/v3/SMELLS.md` — "nobody" is the one that keeps coming back; write "no one", or write who.
The validator enforces both, and an unknown token is an error.

A line still written as `[<id>: TODO]` is a placeholder: the tool exports it as it stands, so an
unwritten line shows up in the game as an obvious bracket rather than as silence. Replace the whole
line, brackets and all.

## halm.guild_hall_door
One sheet left on the board inside, and the bell is still going. The water run out to the {{STAIR}}, thirty coin, leaves at dawn.
- what: the shut guild hall door, the message trigger at 20 15 on halm

## halm.well
Capped, roped, and the rope is new. One sealed jar of this water goes west to the {{STAIR}} every year, and the town has never been told what it is for.
- what: the capped well in the middle of the town, the message trigger at 17 16 on halm

## halm.marta
My sister went west in the spring and sends eighty coin a week to a house with no one in it. I'm not walking that far to watch it rain.
- name: {{VILLAGER_1}}
- what: a woman who stayed, standing by the square, npc at 17 19 on halm

## halm.ostler
Eleven carts used to stand in that yard and I had work on every one of them. Ask me what I do now — go on, ask.
- name: {{VILLAGER_2}}
- what: the man who kept the carters' horses, on the west street, npc at 12 19 on halm

## halm.grainwife
Something's in the shed again and it eats the sacks as well as the grain. If you're going in there, go in loud.
- name: Woman at the grain gate
- what: at the grain yard gate, points the player at the shortcut and the lids, npc at 9 16 on halm

## halm.gate_watch
{{MENTOR}} is up on his own roof with that knee and he will not be told. Go up if you like; he'll shout at you from there.
- name: Watch at the hill gate
- what: at the hill gate on the way up to {{MENTOR}}'s, npc at 22 6 on halm

## hart_yard.practice_posts
Six posts, all split at the same height, and that height is your shoulder. The sixth one has been moved since yesterday.
- what: the row of practice posts in {{MENTOR}}'s yard, the message trigger at 16 3 on hart_yard

## hart_yard.hart
Hand me the short nails and don't start. Whatever you came up here to ask, the answer is the same as it was at the bell.
- name: {{MENTOR}}
- what: {{MENTOR}} on his roof, before the You're Not Ready clip, npc at 10 9 on hart_yard
