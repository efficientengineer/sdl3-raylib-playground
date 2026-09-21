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

**Chapter one (2026-09-21).** This file carries most of the ordinary day: the errand, the board, the
night, and asking {{HEALER}} along are all field lines now, not scenes. Ids follow the designer's
list in `story/v3/ch01_room/designer_to_writer.md`. Where an id has several boxes in a row, they are
numbered (`supper_1`, `supper_2`) and the engine plays them in order.

---

# hart_yard — day one

## hart_yard.errand_1
Take the part down to the guild hall and give it to the clerk. Bread on the way back, enough for three.
- name: {{MENTOR}}
- what: {{MENTOR}} sets the errand, box one of two, right after C1; the player still has the stick

## hart_yard.errand_2
{{HEALER}} eats here tonight, same as every night. Be back before the light goes.
- name: {{MENTOR}}
- what: {{MENTOR}} sets the errand, box two of two, on hart_yard

## hart_yard.practice_posts
Four machines standing in a row, wood and iron and rope, and {{MENTOR}} built every one of them. Each one teaches you a different way to get hit.
- what: the row of training machines as a whole

## hart_yard.machine_1
A split post on a spring. Hit it anywhere and it comes back straight at your face, so you learn to step.
- what: the first training machine, the message trigger in {{MENTOR}}'s yard

## hart_yard.machine_2
A barrel on a rope down the slope. It runs at you in a straight line and it cannot turn.
- what: the second training machine

## hart_yard.machine_3
A sprung arm that shuts on your wrist and holds it. {{MENTOR}} built it so you can practise getting somebody else out.
- what: the third training machine

## hart_yard.machine_4
Every hit you land winds the weight up, and the weight brings the arm back on the left. There is a click before the arm comes.
- what: the last training machine, the one the chapter turns on; the click is the tell the player must learn

## hart_yard.book
{{MENTOR}}'s book, one line a day: which machine, the date, and how it went. Your name is on nearly every line of four years.
- what: the workshop book on the bench, examine

## hart_yard.wall
The low wall between the two houses, worn smooth in one place at the top. {{HEALER}} sits there.
- what: the wall {{HEALER}} sits on, examined when she is not on it

## hart_yard.ladder
A ladder up to the eaves, and something tucked under them out of the rain.
- what: the ladder to the hidden {{LOOT_YARD}}

## hart_yard.workshop_door
Wood, iron, rope and counterweights, and a hook on the back wall with nothing hanging on it.
- what: the workshop door, shut until the sword comes out of it

## hart_yard.healer_house
One room, swept, everything put away, and one chair at the table. She eats next door most nights.
- what: {{HEALER}}'s house through the open door

## hart_yard.hart
Hand me the short nails and don't start. The answer is the same as it was at breakfast.
- name: {{MENTOR}}
- what: {{MENTOR}} at his workbench, spoken to on day one

## hart_yard.ottilie_idle
Go and do your errand. I'll be up here laughing at you again tomorrow.
- name: {{HEALER}}
- what: {{HEALER}} on the wall, spoken to on day one

# hart_yard — supper, day one evening

## hart_yard.supper_1
Get to the table before it's cold. He's been stirring that pot and saying nothing for an hour.
- name: {{HEALER}}
- what: supper box one; the player walks in and sits

## hart_yard.supper_2
Four years of my suppers, and you still hold the spoon like a hammer.
- name: {{HEALER}}
- what: supper box two

## hart_yard.supper_3
Eat what's in front of you. The machines will still be out there in the morning.
- name: {{MENTOR}}
- what: supper box three; C3 follows as the last beat of the meal

# hart_yard — the night of day one

## hart_yard.room_sword_gap
Two pegs on your wall with nothing across them. You put them up when you were thirteen.
- what: the empty sword pegs in {{HERO}}'s room, night of day one

## hart_yard.room_window
The yard in the dark, and the last machine standing in it with its arm up.
- what: the window in {{HERO}}'s room, night of day one

## hart_yard.bed
Sleep, and be out at the machines before it is light.
- what: the bed; interacting ends day one

# hart_yard — day two

## hart_yard.day2_a
Still dark, and your hands are already sore. {{HEALER}}'s shutters are shut.
- what: between training sessions one and two on day two

## hart_yard.day2_b
Watch its arm — it comes back on the left! I've been saying so since you were twelve.
- name: {{HEALER}}
- what: between sessions two and three; she has come out to the wall

## hart_yard.day2_c
You're swinging harder because you're tired. Swing harder, get hit harder — that is the whole machine.
- name: {{HEALER}}
- what: between sessions three and four; her call becomes the answer

## hart_yard.day2_d
Your arms have gone. Stop trying to reach it and listen for the click.
- name: {{HEALER}}
- what: the last call before the parry lands; C4 follows

---

# halm — day one

## halm.bread
Bread is a coin a loaf. Three loaves and you'll be carrying them, so mind the dog on the corner.
- name: Baker on the square
- what: the bread stall, the only price the player sees in town

## halm.grain_yard_scales
An iron weight off the grain scales, kicked under the bench and forgotten. It sits in your hand like it means it.
- what: the teaching find, {{LOOT_TEACHING_FIND}}, in the grain yard

## halm.grainwife
Something's in that shed again and it eats the sacks as well as the grain. Go in loud.
- name: Woman at the grain gate
- what: the grain yard gate; points the player at the {{GRAIN_CREATURE_PL}} and the find

## halm.board_closed
Bare planks and a lot of old nail holes. Job sheets go up at dawn and the board closes on the last ring of the evening bell.
- what: the job board on day one, before C2

## halm.guild_hall_door
The hall is open and the clerk is behind the counter. The board beside the door is bare.
- what: the guild hall door on day one

## halm.bell
The bell hangs over the square on an axle of {{MENTOR}}'s iron. It is rung once a day, at the end of it.
- what: the bell tower; the player stands under the bell that rings in C4

## halm.well
Capped and roped, and the winch over it is {{MENTOR}}'s work. He made the bell works too, and the catch you are carrying.
- what: the well; where the player sees what an engineer is before the word is used

## halm.carter_saying
My grandfather had a saying: sleep like a {{HERDER_PEOPLE}}, wake like a {{HERDER_PEOPLE}}, and get nothing done all day.
- name: Carter with no cart
- what: the first of two idle mentions of the {{HERDER_PEOPLE_PL}}; the player is not asked to hold it

## halm.shepherd_market
Up the valley they trade with the {{HERDER_PEOPLE_PL}} twice a year. Not here — I have kept sheep forty years and never seen one.
- name: {{SHEPHERD}}
- what: {{SHEPHERD}} at the market on day one, the second mention; she is the woman who posts the job sheet

## halm.shepherd_market_2
Nine of mine have gone off that pasture this month. I am putting a job sheet up at dawn and the old hunters have laughed at it already.
- name: {{SHEPHERD}}
- what: {{SHEPHERD}}'s second box on day one; the job exists before the player can take it

## halm.healer_door
{{TOWN_HEALER}}'s room, and a bench outside it with three people waiting on it.
- what: the town healer's door on the square

## halm.healer_bench
Is {{HEALER}} in today? I'd rather wait for her — she's quick, and she tells you what she's doing while she does it.
- name: Woman on the bench
- what: outside {{TOWN_HEALER}}'s room; the town asks for {{HEALER}} by name

## halm.town_healer
She has closed more in this room than I did at her age. She wants a season's work outside this valley before I give her full standing, and she has never been further than the bridge.
- name: {{TOWN_HEALER}}
- what: {{TOWN_HEALER}} in her room, day one afternoon

## halm.gate_watch
You're {{MENTOR}}'s apprentice. Four years and he still has you on those machines of his.
- name: Watch at the hill gate
- what: townsperson reaction, day one, before the sword

## halm.gate_watch_after
A sword on your back. He gave you that himself, then — he doesn't sell anything he makes.
- name: Watch at the hill gate
- what: the same watch, during the bell run, after the sword

## halm.npc_2
Tell {{MENTOR}} the hinge he cut for my door has outlasted the door.
- name: Man on the west street
- what: townsperson reaction, day one

## halm.npc_2_after
Run, then! The clerk shuts that book on the last ring and he has never waited for anyone.
- name: Man on the west street
- what: the same man, during the bell run

## halm.marta
My brother signed for a job at nineteen. You'll get one.
- name: Woman by the square
- what: townsperson reaction, day one

## halm.marta_after
Go on, {{HERO}}! Straight through the grain yard, it's shorter!
- name: Woman by the square
- what: the same woman, during the bell run; she gives the player the shortcut

## halm.ostler
Eleven carts used to stand in that yard and I had work on every one of them. Ask me what I do now.
- name: {{VILLAGER_2}}
- what: townsperson reaction, day one

## halm.ostler_after
That's a hunter walking, that is. Mind the gate post!
- name: {{VILLAGER_2}}
- what: the same man, during the bell run

## halm.npc_5
Four coin says he doesn't have a sword by the autumn. {{RIVAL}} took two years.
- name: Hunter on the porch
- what: townsperson reaction, day one, outside the guild hall

## halm.npc_5_after
Well. There's four coin I owe somebody.
- name: Hunter on the porch
- what: the same hunter, during the bell run

# halm — day two, the bell run

## halm.board_sheets
Four job sheets, and three have a line struck through them. The last one has no line on it.
- what: the board on day two; the examine that sets up the job sheet itself

## halm.job_sheet
{{SHEPHERD}} of the {{HIGH_PASTURE}} has lost nine animals off the hill in a month, taken at night. They turn up days later miles away, asleep and not to be woken, and the tracks stop in open grass.
- what: THE job. The only place in the chapter the job is written down; reading it is how the player takes it

## halm.job_sheet_2
It pays four coin. Somebody has written "monster" across the top in a different hand.
- what: second box of the job sheet; the pay and the reason {{HERO}} wants it

## halm.clerk_signing
You made it on the last ring, and you have a sword on your back. Give me the sheet and I will write your name in the book.
- name: Clerk
- what: the clerk when the player reaches the counter in time

## halm.clerk_signing_late
The bell has stopped and the shutter is halfway down. Put your hand on it and I will call that the last ring.
- name: Clerk
- what: the clerk when the rings run out; the run cannot be failed permanently

## halm.clerk_after
The rule was a sword, and you have one. Bring the job sheet back signed off by whoever posted it.
- name: Clerk
- what: the clerk's line as the player leaves the hall with the job

## halm.rival_door
Sheep. You waited two days for a sword and you've spent it on sheep.
- name: {{RIVAL}}
- what: {{RIVAL}} in the guild hall doorway during the bell run

## halm.rival_door_2
Take it, take it. And you still owe me four coin for bread.
- name: {{RIVAL}}
- what: {{RIVAL}}'s second box; he is kind about it, which is worse

## halm.ottilie_door
{{HEALER}} — I signed for a job up on the {{HIGH_PASTURE}} and I'm going tonight. Come with me.
- name: {{HERO}}
- what: asking her along, box one; the bell is still ringing

## halm.ottilie_door_2
Yes. My bag's by the door and I've been carrying bandages for you since you were nine — wait, where are we going?
- name: {{HEALER}}
- what: asking her along, box two; she joins as a follower on the spot

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
{{TOWN_HEALER}} has wanted me out of this valley for a year. She'll be delighted, and I'm not telling her.
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
Another one, forty paces on, lying the same way with her legs folded under her. They are all facing uphill.
- what: the second sleeper; the trail has a direction

## high_pasture.sleeper_3
This one is a dog, and it is asleep too. Whatever put these animals down did not care which kind they were.
- what: the third sleeper; the thing does not choose

## high_pasture.sleeper_4
The grass here is walked flat in a wide circle, and there are nine of them lying inside it. Something stood them together before it put them down.
- what: the fourth sleeper site; the animals were gathered, not scattered

## high_pasture.tracks_stop
Four heavy tracks in the mud, then two, then flat grass for a hundred paces. Whatever left here stopped putting its feet down.
- what: the tracks that stop in open ground; the chapter's best examine

## high_pasture.lantern
A pale light out on the grass, the same colour as your own lamp. You find you have taken a step towards it.
- what: the {{NIGHT_CREATURE_PL}} on the pasture

## high_pasture.body
{{BEAST_NAME}} lying in the flattened grass, still warm, bigger than he looked standing up. Under the matted hair behind his left ear the skin is not right.
- what: the examine on the body, BEFORE C6 — the player finds the holes themself (designer's ask)

## high_pasture.body_2
A ring of small round holes in the bare skin, all the same size, healed shut a long time ago. Someone made these on purpose.
- what: the second box of the body examine; C6 is the three of them reacting to it

## high_pasture.fold
A sheepfold of piled stone with its gate hanging off. There is nothing inside it.
- what: {{SHEPHERD}}'s empty fold

## high_pasture.find
A flat disc of pale glass on a cord, dropped in the grass where you would only find it in the dark.
- what: the hidden find on the pasture, reached in the dark

## high_pasture.ottilie_ready
Stay where I can reach you both. I can patch anything you two manage out here, but I'd rather not.
- name: {{HEALER}}
- what: {{HEALER}} before the fight; capable, unbothered, and still teasing

# high_pasture — the fight

## high_pasture.boss_break
{{BEAST_NAME}} — stand! It's me, you great fool.
- name: Distel
- what: in-field exchange at the phase break; she calls him and it works for a moment

## high_pasture.boss_break_2
He's standing — hold him just like that!
- name: Distel
- what: box two of the phase break; the plan appears to be working

## high_pasture.boss_turn
He doesn't know me. He's looking at me the way he looks at a sheep.
- name: Distel
- what: the turn; the beast goes for its own handler

## high_pasture.boss_turn_2
{{HERO}}, get to her! It's going for her!
- name: {{HEALER}}
- what: box two of the turn; the player is told what to do and phase two starts

---

# west_road — not played in chapter one

## west_road.culvert
The road drops into the gap where the spring took the culvert out. Everyone going west walks the long way round it.
- what: kept so the existing west_road.tmap validates; the road west is chapter two's

## west_road.west_end
The road keeps going west, down to the river bridge. That is a day's walk and your job is up the hill behind you.
- what: kept so the existing west_road.tmap validates
