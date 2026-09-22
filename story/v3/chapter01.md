# CHAPTER ONE — *The Sheep on the Hill*

**THE SPINE.** Written by the designer, 2026-09-21, replacing *The Jar Run* completely. This file
says what the player **does**; `story/scenes/` says what anyone says. Clip slots here are one
paragraph of purpose and no dialogue — see `ch01_room/flow.md` for the slot contract and
`ch01_room/designer_to_writer.md` for the tokens.

Party: {{HERO}} alone for days one and two; **{{HEALER}}** joins on the bell run; **{{HERDER}}**
joins at C5. Combat rules: `COMBAT.md` (and its no-combat fallback, which is how this plays today).
Creatures: `BESTIARY.md`. Hidden items: `LOOT.md`.

**Checked against the writer's draft three, 2026-09-21.** Every id in *The mandatory ten* exists in
`story/field/text.md`; the three machines are named not numbered in every scene; nobody coaches
anywhere (0150's beat says it outright — *"Nobody tells him to do it and nobody says afterwards what
he did"*); nine appears only as the ring of holes; the flock is not counted on the sheet and the job
pays five coin; the sword gets its held beat before the bell; and the animals are warm, unhurt and
gathered in every line that touches them. **No scene contradicts play.**

**Revision pass, 2026-09-21** (owner's notes on draft 2, and `story/notes/cold-read-ch01-draft2.md`).
Three things changed and they run through every block below. **(a) No character coaches the player.**
Ottilie is no longer the hint system — she is on the wall enjoying it. The swing's own tell, the
stamina drain and a lit Guard entry teach the parry (`COMBAT.md` §4a). **(b) Ten mandatory
interactions** carry everything the chapter's setup depends on; the path goes through them
(§ *The mandatory ten*, below). Everything else is optional and has been cut hard. **(c) Less text
everywhere**: goal lines of six words or fewer, no system pop-ups, no line that describes what the
player just did.

**Six clips, sixty-seven minutes of play.** The chapter is one lesson — *you cannot win by
attacking; you wait, you turn the swing aside, and then you strike* — taught by a machine in a yard,
paid for with a sword, and cashed in on a hillside in the dark.

---

## Goals, in order

The on-screen goal line at every stage. **Six words or fewer, always** — the player can say what they
are doing without the game using a sentence to tell them. One line on screen at a time, never two.

| Stage | Goal line |
|---|---|
| P1 | Beat the swing. |
| P2 | Deliver the part. Buy bread. |
| after C2 | Get home before supper. |
| P3 | Eat, and go to bed. |
| night | Be at the machines before light. |
| P4 | Beat the swing. |
| after C4, in the yard | *(unchanged — the bell is not counting yet)* |
| on leaving the yard | Get to the guild hall. |
| after signing | Find what is taking the animals. |
| P6 | Get up to the pasture. |
| P7 | Follow the sleeping animals. |
| after the tracks stop | Find the {{GUARD_BEAST}}. |
| P8 phase 1 | Hold it open for {{HERDER}}. |
| P8 phase 2 | Stop it. It wants {{HERDER}}. |

---

## The mandatory ten

The cold read's main finding was that **the chapter's setup lived in the optional layer and its
exposition in the mandatory one** — backwards. The repair is in play, not in more text. These ten
interactions are **led through**: the route passes them, the goal line points at them, or they are
the block's objective. Each is short (one or two boxes). **Nothing else in the chapter is mandatory**,
and everything else has been trimmed or deleted (the list is in `ch01_room/designer_to_writer.md` §5).

| # | Id | Where it is unmissable | What it carries, without saying it |
|---|---|---|---|
| 1 | `hart_yard.machine_swing` | the block's objective | one sentence: hitting it winds the weight. Nothing about the click. |
| 2 | `hart_yard.bench_part` | the errand's object is on {{MENTOR}}'s bench | you walk through his workshop, among his machines. He is an engineer because you were in it. |
| 3 | `halm.lamp_charm` | on the lane out of the yard, in the road | ordinary magic, done by a stranger as a chore — and the carter's saying, which is the first the player hears of the {{HERDER_PEOPLE_PL}}. |
| 4 | `halm.guild_hall_door` | fires on entering, and you must enter to deliver | the word *guild*. |
| 5 | `halm.ottilie_house` | the lane home at dusk passes her open door | one chair at the table, two coats on the hook. Nobody comments, then or ever. |
| 6 | `halm.job_sheet` + `_2` | **signing is reading**: taking the sheet fires it | the whole job. The one place it is ever written down. |
| 7 | `halm.ottilie_door` + `_2` | her door is *on* the fast route in P5 | she is **asked**. She does not simply appear on the pasture. |
| 8 | `high_pasture.lantern` | fires on the first one sighted | the chapter's wonder, at minute forty rather than fifty. |
| 9 | `high_pasture.tracks_stop` | the set piece the trail leads to; C5 fires here | the best sentence in the chapter, and the shape of the boss. |
| 10 | `high_pasture.body` + `_2` | C6 does not run until the player has looked | the ring of nine holes, found by the player, not reported to them. |

**Two of these are set pieces, not boxes** — see P7: the false lantern and the tracks that stop are
built as places, and the text is the small part of them.

---

# DAY ONE

## PLAY P1 — {{MENTOR}}'s yard, midday (6 min)
*Map: `hart_yard`. Party: {{HERO}}. Light: day.*

**The chapter opens on the stick in your hand**, not on a picture. Falke is already in the yard with
a practice blade and Hart is already watching and Ottilie is already on the wall.

**Map.** One small enclosure, no exits open yet. **Three** machines stand in a line across the yard
from the gate: **the post**, **the arm that holds**, **the swing**. They are named, never numbered
(`COMBAT.md` §7; the barrel-on-a-rope is cut — its whole job was to say the word *tell* out loud).
Hart's workshop is at the near end with a ladder against it. The wall between the two houses runs
down the left and Ottilie is sitting on it with her legs over.

**Minute to minute.**
1. *The post* (1 min). The game's first input: move, face, hit. **Attack is the only command that
   exists** (`COMBAT.md` §7a). Three clean hits and it falls over.
2. *The arm that holds* (1.5 min). It telegraphs, then a jaw closes on the wrist. **Guard appears in
   the list the first time it telegraphs at you** — the word arriving is the entire tutorial. Getting
   out costs two goes; guarding on the telegraph costs nothing. The first time waiting is obviously
   better, and the player found that out by being caught.
3. *The swing* (3.5 min, and you do not win). A counterweighted arm. The weight **drops a hand's
   width**, the arm **winds back** with a ratchet sound, and then it comes — always from the same
   side, every time, unmissably. The window is 0.45 s. **Every attack that is not into an opening
   winds the weight another notch and its counter drains you** (`COMBAT.md` §7), so by the end of the
   session Attack and Hard swing are **greyed** and Guard is the one lit entry in the list. Nobody
   says a word about any of it. The player loses five or six times — flat on your back, up in two
   seconds, no menu, no penalty. **The session ends with {{HERO}} out of stamina**, which is the
   picture C1 is written over, and it ends *without the win*.

**What the player learns.** That things telegraph; that waiting sometimes beats swinging; that effort
has a price; and that the swing is not hard, it is *different*, and they do not yet know how. They go
to bed not knowing. That is the design.

**Encounters.** None. The machines are the encounters.
**The one hazard.** The swing's winding, which is the player's own impatience made visible twice
over: the counterweight climbs a notch every time they attack, and their own stamina bar falls.
**The optional find.** The ladder against the workshop is climbable from minute one and the roof is
walkable. **{{LOOT_YARD}}** is under the eaves across a three-cell gap (`LOOT.md`) — takeable now by
an adventurous player, but most will take it on the night of day one.
**{{HEALER}} is NOT the hint system.** *(Struck 2026-09-21, owner: "I don't like that Ottilie tells
Falke what to do, it's not her domain. She just thinks it's funny he's getting whomped.")* She never
calls a direction, never names the sound, never gets more explicit. She is on the wall enjoying the
show: she winces at a big hit, she counts the falls, she is eating something. **Her reactions are a
feedback layer, not instruction** — the one that matters is that she goes quiet for exactly one beat
*after* he turns it aside on day two, which is applause, and cannot be a tip because it comes after.
**The hint system is the machine** (`COMBAT.md` §4a): the tell, the winding weight, the drain that
leaves Guard the only lit command, and — only after repeated failure — the tell playing a fifth
slower and the Guard entry pulsing once. No words. {{MENTOR}} says *"Again."*
**What failing costs.** Nothing at all, ever, in this yard. That is a design rule, not a setting.
**Feel.** Funny. You are losing to furniture and a woman on a wall is enjoying it, and she is no help
whatsoever.

### CLIP C1 — "You're not ready" *(1 min, panels)* — `0110_the_yard`
**Purpose.** After the player has personally failed the swing several times, the game states the
problem: he cannot see why it beats him, and Hart can. Hart says he is not ready — kindly, and about
the machine, not about the boy. Ottilie comes off the wall and Mends the wrist **quickly, easily and
without stopping teasing him** — the player's first sight of magic, and what it shows is that it is
ordinary, small, reliable, and that she is good at it. No cost worth showing; she is not limited.
Nothing about the job, the board, the sword or the herders. One beat.
*Back into:* the yard, goal line *Deliver the part. Buy bread.*

---

## PLAY P2 — Halm by day, the errand (10 min)
*Map: `halm`. Party: {{HERO}}. Light: day.*

Hart's errand is **one short box**, and then **the player fetches the part themselves**: it is on his
bench, at the back of his workshop, past the vice and the half-built things and the hook with nothing
on it. `hart_yard.bench_part`, **mandatory**, because the errand cannot start until you have the
part. *This is how the chapter says Hart is the town's engineer* — the player walks through the
evidence. No line says it, then or later. **This is the first walk, and its real job is to teach that
the town is a place with things in it.**

**Map, in segments.**
1. *Hart's lane to the square.* Houses, the well, people. **Three** townspeople, each with a line now
   and a different one after he has the sword (cut from five; the pair of states matters more than
   the count). **In the road, unavoidable:** a carter on the kerb having the charm on his lamp
   renewed by somebody doing it the way you'd re-sole a boot — `halm.lamp_charm`, **mandatory**.
   Magic is ordinary and small and it is a **chore**, and the carter's saying about the
   {{HERDER_PEOPLE_PL}} comes out of him while he waits. That is the only time they are mentioned
   before Distel is standing in front of you, and it costs one trigger.
2. *The square.* The baker (bread for three — an actual transaction, the player's first, and **it
   cannot be made yet**: a coin a loaf and {{MENTOR}} sent him out with an empty pocket, so the
   player walks away from the stall and buys the loaves on the way home with {{RIVAL}}'s three coin
   from C2). The bell
   tower, examinable, so the bell that rings tomorrow is a thing the player has stood under. The job
   board is visible across the square and **empty**: it is not dawn.
3. *The grain yard.* The short way through to the guild hall is the gap in its far wall. The door is
   propped, three {{GRAIN_CREATURE_PL}} are inside, and the long way round the outside is open to
   anybody who does not want to. **The first real fights of the game**, and the first parry landed on
   something alive — and **the first fight in which the effort press exists at all**, because it
   appears the first time the player has an opening to spend into (`COMBAT.md` §7a).
4. *The guild hall.* `halm.guild_hall_door` fires **on entering**, and you must enter to deliver the
   part: it is where the word *guild* comes into the game. Then the counter, the clerk, the part.

**What the player learns.** That the town is walkable and full of examines; that fighting is normal
and happens on the way somewhere; that magic exists and is nothing much; that hidden things exist
(the find, below).

**Encounters.** {{GRAIN_CREATURE_PL}}, three, in the grain yard only, one behind another. One
behaviour: rolls in a straight line and cannot turn. Tell: stands up on its rim. Drops grain and the
player's first coin. That is the whole encounter design of Halm, on purpose — **one idea per area.**
**The one hazard.** None in town. Day one is a day off.
**The teaching find — the one that is not hidden.** **{{LOOT_TEACHING_FIND}}** on the scale bench,
two steps inside the grain-yard door, in plain sight, glinting, one step off the route out
(`LOOT.md`). The game does not point at it. This is the chapter's contract with the player: *look
around and you will be paid.* Everything else in the game is hidden properly.
**Length.** 10 min with the examines, 5 if the player runs it.
**Feel.** An ordinary afternoon in a town that is about to matter.

### CLIP C2 — the counter *(1 min, panels)* — `0130_the_counter`
**Purpose.** **One rule, said once: no sword, no signature.** *(Shortened after the cold read: the
clip used to recite three rules and it was two minutes of dead time at minute twenty.)* Open on Falke
asking when he can take a job and let the rest come out of him arguing with it. The **dawn** rule and
the **closing bell** are not needed here — the player meets the empty board in play with
`halm.board_closed` on it, and meets the closing bell as the timer in P5, which is a better teacher
than a clerk. Not the job; there is no job yet.
**Three panels (draft 4.1).** It used to be the one clip with no pictures and {{RIVAL}} was the one
character with no face. Now: the counter with the bare board behind the clerk, {{RIVAL}} full length
in the street doorway, and his three coin going down on the counter. **The bread is paid for here, in
front of the clerk** — {{MENTOR}} sent the boy out with no coin on him (`hart_yard.errand`), the
baker wants three (`halm.bread`), and {{RIVAL}} settles it out loud: *"Pay me back when you sign for
something."* That is the debt in `THREADS.md`, and it is the last thing in the clip.
*Back into:* free walk in Halm, goal line *Get home before supper.* The board can be walked up to and
read: empty, with an examine line saying why.

---

## PLAY P3 — home, supper, night (3 min)
*Map: `halm` → `hart_yard`. Party: {{HERO}}. Light: day → dusk → night.*

The walk home at dusk, with the town's lamps coming on — the first time the player sees the light
change, which is preparation for a chapter that ends in the dark.

**The lane home goes past Ottilie's house, and this is the sixth mandatory interaction**
(`halm.ottilie_house`). Her door is open on a warm evening and she is not in it, because she is
already at Hart's table. What the player sees is **one chair at the table and two coats on the hook**
— coats nobody has taken down in years. Nothing is said about it, by anyone, in this chapter.
*Falke has always known her family died; he grew up next door.* **Nobody explains it to him and
nobody explains it to the player** — the player is the one who did not know, and the house tells
them, and then the supper he walks into immediately afterwards is the answer to it without ever
naming it. *(This replaces the draft-2 version where the fact was spoken at supper — owner: "why
wouldn't Falke know her family died? That's a big oversight.")*

Then Hart's kitchen: the player walks in, sits, and **supper is two boxes** with one examine beside
it (Hart's workshop book on the bench, every attempt at every machine written down and dated,
including today's). Then his own room: **one** interact — the gap on the wall where a sword would
hang — and bed.

**The optional find, properly.** While Hart is inside at supper, the ladder and the roof and the
three-cell jump to the eaves are unobserved and the game is quiet. **{{LOOT_YARD}}** wants to be
taken here.
**Encounters.** None. Rule 9: this is the rest.
**Length.** 3 min.

### CLIP C3 — supper *(0.8 min, talk)* — `0140_supper`
**Purpose.** The last beat of the meal only. Hart says he is not ready for the guild, and means it
as care. **Nobody states anything about Ottilie's family** — the player walked past the house ten
minutes ago and is sitting at the table she chose instead; that is the whole statement and it is made
by the level, not by a line. If a line survives at all it is hers, flat and fast, and it does not
explain. One beat, no speeches.
**Nobody says anything about the system here either** *(struck 2026-09-21 by the editor's pass:
{{MENTOR}}'s "You hit everything as hard as you can. Every single time." hands the player the
answer, and "The swing is the one that is out there" is a proverb).* What he says at this table is a
fact with no instruction in it — *"You have never beaten the swing."* — and then *"Eat."* The last
thing anyone says about the machine before the player beats it is a flat statement of the score.
*Back into:* his room, then sleep. Goal line *Be at the machines before light.*

---

# DAY TWO

## PLAY P4 — the yard, all day (14 min)
*Map: `hart_yard`. Party: {{HERO}}. Light: dawn → day → afternoon → evening.*

The long grind, and **it must be fun, not 41 literal retries.** It is built as **four sessions with
visible progress and the light moving between them**, so the player experiences a whole day passing
in fourteen minutes and can see themselves getting better.

**P4 IS FOUR SESSIONS AND IT ENDS AT LAST LIGHT.** Stated plainly because the engine currently runs
three steps and finishes at full day, which puts C4 — *"the last light of the second day"*, the low
gold the sword is written into — in the middle of the afternoon. The intent, which the engine should
be built to: **four** play rows in P4, and the light walks **grey dawn → sun up → high sun → low
gold**, one step a session, ending on the low gold that C4 opens in. The number of sessions is what
makes the day feel long; the last one's light is what makes the sword land. If only three rows are
affordable, the one to drop is session 3, never session 4.

| Session | Light | What is actually different | Fails |
|---|---|---|---|
| **1. Before light** | grey dawn, lamp on the workshop | The swing as it was. The player does what they did yesterday, harder. The counterweight is at the top of the post inside a minute, and the command list is one lit entry in a row of grey ones. | 2-3 |
| **2. Morning** | sun up | **Hart changes the machine, and says nothing.** He comes out, pulls a pin, and the arm now winds back slower and longer — *the same tell, more of it*. An easier version of the same window, never an easier problem. **This is where most players get it**, and they get it by watching a machine, alone, with a man behind them who has said one word. Beat this one. | 2 |
| **3. Afternoon** | high sun, Ottilie arrives on the wall | The pin goes back. Full speed. The player has parried it once slow and knows the shape. **Ottilie gives nothing** — she arrives, gets comfortable, and enjoys it. | 2-3 |
| **4. Evening** | low gold | **The win.** One guard as the weight drops, one strike into the opening, and the arm hangs dead. The counterweight never leaves the bottom of the post. | 0-1 |

**How progress is shown.** Three ways, all visual, no numbers: the **counterweight's height on the
post** at the end of each session (a mark on the post stays where your best attempt left it); the
**light moving**; and the **workshop book**, examinable between sessions, where Hart has written the
day's attempts down in his own hand, session by session. A player who looks at the book after each
session watches the day accumulate.

**Between sessions there are two field lines, not four** (`hart_yard.day2_b`, `hart_yard.day2_d`),
and **neither of them coaches**. The day is marked by the light and the mark on the post.

**Ottilie arrives for session 3** for the usual show and is no use at all: she counts, she rates the
falls, she eats. She goes quiet for exactly one beat when he beats it — **after**, so it can never be
a hint — and then ruins it. *(The draft-2 escalating-hint script through her is struck: owner,
2026-09-21. `COMBAT.md` §4a is the hint system now.)*

**Encounters.** The machine. **The hazard.** The winding, again, and by now the player knows it is
them. **Optional.** {{LOOT_YARD}}, if it is still up there. **Failing costs nothing**, all day.
**Length.** 14 min. **Feel.** A montage the player is inside of.

### CLIP C4 — the sword *(1.2 min, panels)* — `0150_the_sword`
**Purpose.** The player has just beaten the machine themselves. Hart gives him the sword he made, and
because of one rule stated once yesterday the player already knows what that means: **he may sign.**
Nobody re-explains it and **nobody narrates the parry he just did** (struck after the cold read: "He
turned it aside and then he hit it" and "there's a click first" are both the game describing what the
player watched). **One wordless panel (draft 4.1) does show the cause**: his top hand turning over on
the grip and his feet planting with the weight going back, on a textless box, between *"All right.
Come on, then."* and the parry. It is a picture, not an explanation, and nobody remarks on it.
The bell starts underneath the last box.
**THE SWORD GETS A BREATH.** The bell is **audible but not counting**. Control returns in the yard,
no fade, the sword on his back, the old goal line still up — **and nothing is pressing.** The player
can stand in that yard as long as they like and look at it. **The ring counter appears, and the timer
starts, only when the player walks out of the gate**, and the goal line changes on that step to
*Get to the guild hall.* The game's whole first act is about this object; it gets thirty seconds
before it gets a clock.
*Back into:* the yard, quiet, sword in hand. Then the run, on the player's own step.

---

## PLAY P5 — the bell run (6 min)
*Map: `hart_yard` → `halm`. Party: {{HERO}}, then +{{HEALER}}. Light: evening.*

**A timed dash across town, and the first time the player is asked to move well.**

**The timer is the bell itself** — a set number of rings, shown as a small counter, each ring a
sound the player has been hearing all chapter. **It starts when the player leaves the yard, not when
C4 ends** (see C4): the pressure is real, and it is never sprung on somebody mid-sentence.
**The direct route down the lane and round the square does not make it.** The shortcuts do, and all of them are jumps the player has already been taught
in Hart's yard by the ladder and the roof:

- **over the grain-yard wall** — a running jump off the water trough onto the wall, along it, and
  down into the yard, cutting the whole square;
- **the roofs off the lane** — two roofs and a two-cell drop into the alley behind the guild hall;
- **the well kerb to the low shed** — the small one, for players who missed the other two.

**Asking her along — mandatory (7).** Ottilie's door is *on* the fast route, not off it, and the run
goes through it: `halm.ottilie_door` / `_2`, two boxes while the bell is still ringing, and she joins
as a follower on the spot — the engine's follower trail — and the run finishes with two people
jumping the grain-yard wall. **She is asked, she does not follow.** She must never simply appear on
the pasture.

**The board — mandatory (6).** Three job sheets, two struck through. The one left is the shepherd's:
**eleven** animals off the high pasture at night, turning up miles away asleep and unwakeable, the
tracks stopping in open ground. It pays **five coin** and it sounds like a tall tale, which is why it
is the one still there. **Reading it is not optional and it is not a cutscene**: `halm.job_sheet` /
`_2` fires **on taking the sheet down**, because taking it to the counter is how you sign it — *the
act of signing IS the act of reading.* Two short boxes, the only place in the chapter the job is ever
written down. Signing is `halm.clerk_signing`.
*(Numbers: nine belongs to the ring of holes and to nothing else in this chapter.)*
**If the rings run out**, `halm.clerk_signing_late` runs instead: the clerk pretends the shutter is
not already down and signs him anyway. **The run is pressure, never a restart** — nothing in chapter
one sends the player backwards.
**Encounters.** None; the town is the obstacle. **Hazard.** The clock. **Optional.** None — the
player has one thing to do and six minutes of adrenaline to do it in.
**Length.** 6 min including the board and the door. **Feel.** The best two minutes of the chapter so
far, and it is entirely the player's hands.

---

## PLAY P6 — the hill path, dusk (10 min)
*New map: `hill_path`. Party: {{HERO}}, {{HEALER}}. Light: dusk → night, changing as you climb.*

**The first real walk out of town, and the first real fights.**

**Map, in segments.**
1. *The last house.* Halm's edge, a lamp, a gate. The last examine point before the dark.
2. *The long climb.* Switchbacks up a grass shoulder. Halm gets small behind you and the light drops
   a step at each switchback — **the time-of-day change is the level design**, and by the top the
   party's lantern is the only light.
3. *The shoulder of rock.* The switchback that hides the drop. Above the fold wall.
4. *The top gate.* The pasture's stone wall and a gap in it. Beyond it is black.

**What the player learns.** What a fight is when there is no Hart and no wall: the tell, the parry,
the opening, against something that is actually coming at them. Also what Ottilie is worth: her Mend
and her Shield are what keep the climb survivable, and the player should finish the hill path glad
she was asked.

**Encounters.** {{HILL_CREATURE_PL}} only — one or two on the lower path, three near the top,
about one encounter every twenty paces. One behaviour: a short straight charge that ends **clinging
to your leg and slowing you** until you shake it off, which costs a turn or an effort. Tell: the
mouth opens and the body sinks. Generous window, because this is the exam for the machines, and the
lesson is exact: parry it and it never touches you; swing at it and you spend two rounds pulling it
off. **They are coming down past the party**, which the player notices without being told, and which
is the first clue that something is up there.
**The one hazard.** The light going. By the third segment the player is fighting at the edge of a
lantern's reach and the tells are getting harder to see — which is the whole setup for the pasture
and for what {{HERDER}} is worth.
**The one optional find.** **{{LOOT_HILL}}**, off the outside of the switchback: a two-cell drop onto
a ledge visible only from above, with a ramp back up (`LOOT.md`). Costs nothing but nerve.
**What failing costs.** The party wakes at the last house with everything they had. The walk back up
is the only price.
**Length.** 10 min. **Feel.** Going up out of the world you know, in the dark, with a sword you got
this afternoon.

---

## PLAY P7 — the high pasture, night (12 min)
*New map: `high_pasture`. Party: {{HERO}}, {{HEALER}}, then +{{HERDER}}. Light: night.*

**The chapter's one strong idea: you cannot see.** No fog, no puzzle, no gimmick — the map is dark,
the party has a lantern, and the lantern is a small circle in a large field. Everything on this map
is a consequence of that one idea and nothing else has been added.

**Map, in segments.**
1. *Inside the wall.* Wide open dark grass. The lantern shows about four cells. Somewhere in it,
   **the first sleeping animal** — the player finds it by walking into it.
2. *The trail of sleepers.* Three more, spaced far enough apart that finding the next one is the
   activity. **Warm, unhurt, bedded down** — never harmed, never scattered, laid in the grass like
   something put them there on purpose, which is exactly what happened and is what Distel will say
   out loud much later. Each examine is **one sentence**; the game never interprets a sleeper for the
   player (cut after the cold read: three sleepers in a row all ended with the game explaining
   itself). **The trail IS the navigation**: no marker, no arrow.
3. *Where the false lantern is — SET PIECE, mandatory (8).* Not an examine the player may walk past:
   the trail's third sleeper lies in the open, and **the only other light on the map is out there**,
   at the edge of sight, the same warm colour as the party's own lamp. The player heads for it —
   everyone does; it looks like a person — and `high_pasture.lantern` fires **when they have taken
   the step**, which is what makes the line land. Then it flares. **This is the chapter's wonder and
   it arrives at minute forty, not fifty.**
4. *The knots of sleepers.* Where the sleeping animals lie thickest, and therefore where the
   {{PASTURE_CREATURE_PL}} are standing among them. The player learns to look at a group before
   walking into it, and can read the direction the beast went from where the knots are.
5. *Where the tracks stop — SET PIECE, mandatory (9).* The trail of sleepers runs out and the trail
   of **prints** takes over, pressed deep in mud, and the lantern circle follows them across open
   ground **until there are no more.** The grass past them is not flattened. The walk itself is the
   discovery; the player is already stopped and looking down when `high_pasture.tracks_stop` fires,
   and it is the one line in the chapter meant to be quoted. **C5 fires here**, because {{HERDER}} is
   standing at the far edge of the lamp looking at the same thing.

**What the player learns.** Everything the job sheet said, by finding it: the animals are taken, they
are **unhurt and put down carefully**, they cannot be woken, and whatever does it does not walk away
afterwards. The player can assemble the shape of the boss before anyone says a word about it — and
the "taken" on the sheet and the "kept safe" of Distel's explanation are **both** supported by what
the player has been walking past for ten minutes. Nothing in play shows a harmed animal, ever.

**Encounters.** Two kinds, both about seeing:
- {{NIGHT_CREATURE_PL}}, two to four, each a pale light in the dark that **looks exactly like
  somebody else's lantern** and pulls the party a step closer before it flares and blinds someone for
  two rounds. Their tell is the light **dimming** for a beat first — without {{HERDER}} that is a
  beat too late to react to reliably.
- {{PASTURE_CREATURE_PL}}, one or two, standing among the sleeping animals with their backs to you
  and no faces. Their tell is the head turning the wrong way — read by motion, not by light, so it is
  the one thing the lantern does not help with. **Puts a price on walking up to the sleepers.**

**The one hazard.** The dark itself, and the party's lantern as the thing that both saves you and
is impossible to tell from a {{NIGHT_CREATURE}} at fifteen paces. No chained penalties, nothing else.
**The one optional find.** **{{LOOT_PASTURE}}** — the strongest item in the chapter, on a jump-only
shelf above the fold, glinting only with night sight on, reached by a running jump (`LOOT.md`).
**Only findable after C5.** A player without the herder walks past a black hillside; a player with
them sees it and has to make the jump in the dark.
**What failing costs.** Wake at the shepherd's fold, keep everything.
**Length.** 12 min. **Feel.** Two people out of their depth, and then three.

### CLIP C5 — the herder *(1.2 min, panels)* — `0160_the_herder`
**Purpose.** The third party member exists. At the place where the tracks stop, in the dark, someone
small is already standing. Falke nearly swings; Ottilie stops him. They are tracking the same thing,
it is theirs, they raised it, and they do not want it killed. That is the one beat, and it changes
the goal from *find it* to *stop it without killing it*.
*Back into:* a party of three. **Night sight comes on and the map changes**: tells show a beat
earlier, the {{NIGHT_CREATURE_PL}} are readable, and the shelf above the fold glints. No line explains
this; the player sees the field change. Goal line *Find the {{GUARD_BEAST}}.* (the token reads **drover**; goal lines are hardcoded in the engine and must be written out as *Find the drover.* — the player is never told "guard beast", which is a designer's word)

---

## BOSS P8 — {{BEAST_NAME}} (6 min)
*Map: `high_pasture`, the fold. Party: all three. Light: night. No run.*

Rules in full in `COMBAT.md` §9 and `BESTIARY.md`.

**Phase one — hold it open** *(cannot be lost, 2.5 min).* Goal line: *Hold it open for {{HERDER}}.* The mane lifts — an enormous slow tell — {{HERO}} parries (**Guard costs no effort**), it
is OPEN, and {{HERDER}} spends **Settle at effort 5** into the opening, 24 stamina a go against a
pool of 30 with +8 a round. **She can afford it twice, and the second one hurts** — the first
empties her, three rounds of guarding pay for the second, and the phase is over before a third. **Attacking does nothing and the game says so each time.** So the phase
is restraint: cheap parries, everything poured into her, and her bar visibly not coming back. The
chapter's thesis as a mechanic — *neither of them can do this alone* — and the player reads it off a
bar rather than out of a box. Three cycles and it goes quiet.

> **In-field exchange `high_pasture.boss_break`** *(the writer's ask, accepted; 2 boxes)* — at the
> phase break, {{HERDER}} calls {{BEAST_NAME}} by name and for a moment it works. The turn is
> **played, not reported**: the boss UI stays up, control is frozen for two boxes, and the goal line
> does not change yet. No scene file.

Then the mane lifts again, and it looks at her, and it does not know her.

> **In-field exchange `high_pasture.boss_turn`** *(2 boxes)* — it goes for her. Fires on the round it
> changes target, not before. The goal line changes on the last box: *Stop it. It wants {{HERDER}}.*

**Phase two — a real fight** *(3.5 min).* Same tell, **faster, tighter window.** Damage counts; this
is the release, and everything held back in phase one gets spent. After round three it stops
attacking the party and **goes for {{HERDER}}**, who cannot parry. {{HEALER}}'s pool of 36 and
{{HERDER}}'s drained one are what the player has left, which makes a careless phase one cost
something without ever having been a failure. Open it and finish it. Losing restarts phase two only.

**It is killed with a parry and the strike after it — the exact input the player learned on a machine
in a yard yesterday afternoon.** That is the chapter.

### CLIP C6 — what was on him *(1.6 min, panels)* — `0180_what_was_on_it`
**The player finds it first, in play — mandatory (10).** `high_pasture.body` / `_2` is an examine on
the body after the fight: **a ring of nine small healed holes behind the left ear.** The body is the
only thing on a dark map and the party will not leave until it has been looked at; **the clip does
not run until it has been.** The chapter's last discovery is made with the stick in hand.
**Purpose of the clip.** The three of them react to what the player already found. It is not
explained, and nobody describes the holes a second time. The flock wakes — **unhurt, which the player
has known since the first sleeper.** {{SHEPHERD}} pays: the first coin {{HERO}} earns as a hunter.
Three people who have no answer decide to go and find one. One beat: *paid on paper, not solved.*

---

## Pacing — play against clips

**Re-timed 2026-09-21** after the cuts. Every clip got shorter and P1, P2 and P3 lost play time to
trimming rather than to content: the barrel machine is gone, the errand lost two townspeople and a
recitation, the night lost an interact. Nothing was cut from P4 onward — the second half of the
chapter is the part the whole thing is for.

| Block | Kind | Minutes play | Minutes clip | Change |
|---|---|---|---|---|
| P1 {{MENTOR}}'s yard, midday | play | 6 | — | −2 (three machines, not four) |
| C1 "You're not ready" | clip | — | 0.8 | −0.2 |
| P2 Halm, the errand | play | 10 | — | −2 (three townspeople; tighter examines) |
| C2 the counter | clip | — | 0.7 | −0.3 (one rule, not three) |
| P3 home, supper, night | play | 3 | — | −1 (supper two boxes; one room interact) |
| C3 supper | clip | — | 0.8 | −0.2 |
| P4 the yard, all day | play | 14 | — | — |
| C4 the sword | clip | — | 1.2 | −0.3, and it gains a silent beat in play |
| P5 the bell run | play | 6 | — | — |
| P6 the hill path | play | 10 | — | — |
| C5 the herder | clip | — | 1.2 | −0.3 |
| P7 the high pasture | play | 12 | — | — |
| P8 the boss | play | 6 | — | — |
| C6 what was on him | clip | — | 1.6 | −0.4 |
| **Total** | | **67** | **6.3** | **73 minutes** |

**Ratio 10.6:1**, up from 9:1 — the direction the owner asked for. **Total scene text is down about a
third** and the mandatory share of it is ten short interactions. The longest stretch without a clip is
**P5 → P6, 16 minutes** (the bell run and the climb), which is on purpose: it is the chapter's
momentum and nothing may be inserted into it. The longest clip is still the last one. No two clips
are adjacent anywhere in the chapter.

**The no-teaching rule, stated once for the whole chapter.** No goal line is longer than six words;
no system is explained in a box; no character coaches; and no line of dialogue or field text
describes something the player has just watched. Anything that breaks one of those is cut, not
rewritten.

**Tension and release** (rule 9): P1 is funny, P2-P3 is the rest and the laugh, P4 is effort, C4-P5
is the spike, P6 is the first real danger, P7 is dread, P8 is the payoff. The rest is early because
everything after C4 is uphill.
