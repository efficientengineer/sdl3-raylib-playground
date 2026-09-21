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

**Rules, the same ones dialogue answers to.** Two sentences at the most, and **most lines are one**:
this is a box on a phone screen that somebody dismisses with a thumb. No word on the banned lists in
`story/v3/STYLE.md` and `story/v3/SMELLS.md` — "nobody" is the one that keeps coming back; write
"no one", or write who. The validator enforces both, and an unknown token is an error.

**The one rule of draft three (owner, 2026-09-21: "too much text and exposition").** A line that tells
the player what they are looking at, or what they just did, is deleted rather than shortened. What
survives is what is **warm**, what is **funny**, and what the player could not have got any other way.
The interpretation sentence at the end of an examine — "Something stood them together before it put
them down" — is the exact thing that went.

A line still written as `[<id>: TODO]` is a placeholder: the tool exports it as it stands, so an
unwritten line shows up in the game as an obvious bracket rather than as silence. Replace the whole
line, brackets and all.

**Chapter one, draft three (2026-09-21).** Eighty ids became fifty-odd. Ten of them are **mandatory** —
the designer leads the player through them (`story/v3/chapter01.md`, *The mandatory ten*) and they
carry the chapter's setup, which used to sit in the optional layer. They are marked **MANDATORY**
below. Everything else is the reward for looking around.

**Numbers.** Nine is the ring of holes and nothing else in this chapter. The flock is eleven and
nothing else is. The job pays five coin, {{RIVAL}} lends three, the bread is three loaves.

---

# hart_yard — day one

## hart_yard.bench_part
The catch for the well winch, mended, on the bench among the half-built things with a chalk line still on it.
- what: **MANDATORY.** The errand's object, picked up off {{MENTOR}}'s bench; the player walks through the workshop to get it and works out what he is without a word about it

## hart_yard.errand
Take the catch down to the guild hall, and bread on the way back, enough for three. There is no coin in the house until the winch is paid for.
- name: {{MENTOR}}
- what: {{MENTOR}} sets the errand in one box, right after C1; the player still has the stick. He sends the boy out with nothing in his pocket, which is why {{RIVAL}} pays for the bread at the counter in C2 and why the player owes him three coin

## hart_yard.practice_posts
{{MENTOR}} built all three of them, and each one teaches you a different way to get hit.
- what: the row of machines as a whole; no trigger points at it yet (see story/notes/editor-ch01.md)

## hart_yard.machine_post
A split post on a spring. Hit it anywhere and it comes back straight at your face.
- what: the first machine

## hart_yard.machine_arm
A sprung arm that shuts on your wrist and holds it. {{MENTOR}} built it so you can practise getting somebody else out.
- what: the second machine

## hart_yard.machine_swing
Every hit you land winds the weight up.
- what: **MANDATORY**, and one sentence forever. Nothing about the click, the side or the answer

## hart_yard.book
{{MENTOR}}'s book, one line a day: which machine, the date, and how it went. Your name is on nearly every line of four years.
- what: the workshop book on the bench, examine

## hart_yard.ladder
A ladder up to the eaves.
- what: the ladder to the hidden {{LOOT_YARD}}

## hart_yard.workshop_door
Wood, iron, rope and counterweights, and something long in oilcloth on a hook at the back.
- what: the workshop door. The sword is on that hook on day one, wrapped, and no one says a word about it; it pays off in C4

## hart_yard.hart
Hand me the short nails and don't start. The answer is the same as it was at breakfast.
- name: {{MENTOR}}
- what: {{MENTOR}} at his workbench, spoken to on day one

## hart_yard.ottilie_idle
Go and do your errand. I'll still be up here when you get back.
- name: {{HEALER}}
- what: {{HEALER}} on the wall, spoken to on day one

# hart_yard — supper, day one evening

## hart_yard.supper_1
Get to the table before it's cold. He's been stirring that pot and saying nothing for an hour.
- name: {{HEALER}}
- what: supper box one; the player walks in and sits

## hart_yard.supper_2
Four years of my suppers and you still hold the spoon like a hammer.
- name: {{HEALER}}
- what: supper box two; C3 follows as the last beat of the meal

# hart_yard — the night of day one

## hart_yard.room_sword_gap
Two pegs on your wall, at the height you could reach when you were thirteen.
- what: the empty sword pegs in {{HERO}}'s room, night of day one

## hart_yard.bed
Sleep.
- what: the bed; interacting ends day one

# hart_yard — day two

## hart_yard.day2_b
I've brought my dinner up. Don't let me down.
- name: {{HEALER}}
- what: between day two's sessions; she has come out to the wall for the show, and says nothing about the machine

## hart_yard.day2_d
You went over the post and everything.
- name: {{HEALER}}
- what: the last between-session line before the parry lands; C4 follows. She never calls a swing

---

# halm — day one

## halm.lamp_charm
A carter holds his lamp out and a woman warms the wick between two fingers until it catches.
- what: **MANDATORY.** Ordinary magic done as a chore, in the road, on the way out of the yard. No one explains it

## halm.lamp_charm_2
Up the valley I passed the {{HERDER_PEOPLE_PL}} once, moving sheep in the dark. No one has seen one here.
- name: Carter with no cart
- what: **MANDATORY**, box two of the same stop; the first the player hears of the {{HERDER_PEOPLE_PL}}

## halm.bread
A coin a loaf, and you want three. Come back when you have three coin in your hand.
- name: Baker on the square
- what: the bread stall, the only price the player sees in town

## halm.grain_yard_scales
An iron weight off the grain scales, kicked under the bench and forgotten. It sits in your hand like it means it.
- what: the teaching find, {{LOOT_TEACHING_FIND}}, in the grain yard

## halm.grainwife
Something's in that shed again and it eats the sacks as well as the grain. Go in loud.
- name: Woman at the grain gate
- what: the grain yard gate; points the player at the {{GRAIN_CREATURE_PL}} and the find

## halm.guild_hall_door
The hunters' guild hall is open, and the clerk is behind the counter with the ledger.
- what: **MANDATORY**, fires on entering. The word *guild* enters the game here, and it is the hunters' guild before it is anything else. The bare board is `halm.board_closed`'s, not this one's

## halm.board_closed
The job sheets go up here at dawn. Today it is bare planks and a lot of old nail holes.
- what: the job board on day one, before C2. The words *job sheet* enter the game here, on the thing itself

## halm.bell
The bell hangs over the square on an axle of {{MENTOR}}'s iron.
- what: the bell tower; the player stands under the bell that rings on day two

## halm.well
Capped and roped, and the winch over it is {{MENTOR}}'s work.
- what: the well; what an engineer is, without the word

## halm.shepherd_market
They trade with the {{HERDER_PEOPLE_PL}} up the valley. Twenty years here, I have never seen one.
- name: {{SHEPHERD}}
- what: {{SHEPHERD}} at the market on day one; she is the woman who posts the job sheet

## halm.shepherd_market_2
Eleven of mine have gone off that pasture this month. The old hunters laughed at my sheet already.
- name: {{SHEPHERD}}
- what: her second box; the job exists before the player can take it

## halm.healer_bench
Is {{HEALER}} in today? I'd rather wait — she tells you what she is doing while she does it.
- name: Woman on the bench
- what: outside {{TOWN_HEALER}}'s room; the town asks for {{HEALER}} by name

## halm.town_healer
I took her in the fever winter. One season's work outside this valley and her standing is hers.
- name: {{TOWN_HEALER}}
- what: {{TOWN_HEALER}} in her room. The one place a person says the plain fact of {{HEALER}}'s family out loud, in passing, to somebody who is not {{HERO}} — he has always known

## halm.ottilie_house
One room, swept, everything put away, one chair at the table. Two coats on the hook by the door.
- what: **MANDATORY.** Her open door on the lane home at dusk. No one comments on it, then or ever

## halm.gate_watch
You're {{MENTOR}}'s apprentice. Four years and he still has you on those machines of his.
- name: Watch at the hill gate
- what: townsperson, day one, before the sword

## halm.gate_watch_after
A sword on your back. He gave you that himself, then — he doesn't sell anything he makes.
- name: Watch at the hill gate
- what: the same watch, during the bell run

## halm.marta
My brother signed for his first job at nineteen. You'll get one.
- name: Woman by the square
- what: townsperson, day one

## halm.marta_after
Go on, {{HERO}}! Straight through the grain yard, it's shorter!
- name: Woman by the square
- what: the same woman, during the bell run; she gives the player the shortcut

## halm.ostler
That yard was full of carts and I had work on every one of them.
- name: {{VILLAGER_2}}
- what: townsperson, day one

## halm.ostler_after
A hunter, and I knew you at ten. Mind the gate post!
- name: {{VILLAGER_2}}
- what: the same man, during the bell run

# halm — day two, the bell run

## halm.job_sheet
{{SHEPHERD}} of the {{HIGH_PASTURE}} has lost eleven animals off the hill in a month, taken at night. They turn up days later miles away, asleep and not to be woken.
- what: **MANDATORY.** Taking the sheet is reading it. The only place in the chapter the job is written down

## halm.job_sheet_2
It pays five coin. Somebody has written "monster" across the top in a different hand.
- what: **MANDATORY**, box two; the pay and the reason {{HERO}} wants it

## halm.clerk_signing
The job sheet, please. I will write your name in the guild book.
- name: Clerk
- what: the clerk when the player reaches the counter in time

## halm.clerk_signing_late
The shutter is not down yet. Put your hand on it and that is the last ring.
- name: Clerk
- what: the clerk when the rings run out; the run cannot be failed permanently

## halm.rival_door
Sheep. You waited two days for a sword and you've spent it on sheep.
- name: {{RIVAL}}
- what: {{RIVAL}} in the guild hall doorway during the bell run

## halm.ottilie_door
{{HEALER}}! I signed for a job — come up the hill with me tonight.
- name: {{HERO}}
- what: **MANDATORY**, box one; her door is on the fast route and the bell is still ringing

## halm.ottilie_door_2
Yes. My bag's by the door — wait, where are we going?
- name: {{HEALER}}
- what: **MANDATORY**, box two; she joins as a follower on the spot

---

# hill_path — dusk, going up

## hill_path.last_house
The last house of {{HOME_TOWN}}, with its shutters already closed. The path above it is a sheep track.
- what: the bottom of the hill path

## hill_path.dusk_view
{{HOME_TOWN}} laid out small below with one lamp lit in it, and the grass above you going grey.
- what: the view back down, halfway up

## hill_path.ottilie_dark
I've never been up here after dark. I've never been anywhere after dark.
- name: {{HEALER}}
- what: an in-field exchange on the way up, box one

## hill_path.ottilie_dark_2
She's wanted me out of this valley for a year. I'm not telling her I went.
- name: {{HEALER}}
- what: box two; her own reason for being here, said as a joke

## hill_path.drop
A hunter's whistle on a cord, dropped off the path and caught in the thorns below.
- what: the hidden find on the hill path

## hill_path.gate
The gate onto the {{HIGH_PASTURE}}, propped open with a stone. Sheep tracks go through it and out the other side in a line.
- what: the top of the hill path, into the pasture

---

# high_pasture — night

## high_pasture.sleeper_1
A ewe lying on her side in the grass, warm and breathing slowly. Shouting at her does nothing at all.
- what: the first sleeping animal of the trail

## high_pasture.sleeper_2
Another one, forty paces on, with her legs folded under her. They are both facing uphill.
- what: the second sleeper

## high_pasture.sleeper_3
This one is a dog, and it is asleep too.
- what: the third sleeper

## high_pasture.sleeper_4
The grass here is walked flat in a wide circle, with a dozen of them lying inside it.
- what: the fourth sleeper site

## high_pasture.tracks_stop
Four heavy tracks in the mud, then two, then flat grass for a hundred paces. Whatever left here stopped putting its feet down.
- what: **MANDATORY**, and a set piece rather than a box. The line the player quotes

## high_pasture.lantern
A pale light out on the grass, the same colour as your own lamp. You find you have taken a step towards it.
- what: **MANDATORY**, fires on the first {{NIGHT_CREATURE}} sighted. The chapter's wonder, at minute forty

## high_pasture.fold
A sheepfold of piled stone with its gate hanging off. There is nothing inside it.
- what: {{SHEPHERD}}'s empty fold

## high_pasture.find
A flat disc of pale glass on a cord, dropped in the grass.
- what: the hidden find on the pasture, reached in the dark

## high_pasture.body
{{BEAST_NAME}} lying in the flattened grass, still warm. Behind his left ear the skin is not right.
- what: **MANDATORY**, before C6 — the player finds the holes themself

## high_pasture.body_2
A ring of small round holes in the bare skin, all the same size, healed shut a long time ago.
- what: **MANDATORY**, box two. C6 is the three of them with what the player found

# high_pasture — the fight

## high_pasture.boss_break
{{BEAST_NAME}}, stand — it's me. Smell me.
- name: {{HERDER}}
- what: in-field exchange at the phase break; she calls him and it works for a moment

## high_pasture.boss_break_2
He's standing — hold him just like that!
- name: {{HERDER}}
- what: box two of the phase break

## high_pasture.boss_turn
He's looking at me the way he looks at a sheep.
- name: {{HERDER}}
- what: the turn; the beast goes for its own handler

## high_pasture.boss_turn_2
{{HERO}}, get to her! It's going for her!
- name: {{HEALER}}
- what: box two of the turn; phase two starts

---

# west_road — not played in chapter one

## west_road.culvert
The road drops into the gap where the spring took the culvert out.
- what: kept so the existing west_road.tmap validates; the road west is chapter two's

## west_road.west_end
The road keeps going west, down to the river bridge.
- what: kept so the existing west_road.tmap validates
