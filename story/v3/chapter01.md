# CHAPTER ONE — the hunt on the high pasture

**THE SPINE.** Written by the designer, 2026-09-21, replacing *The Jar Run* completely. This file
says what the player **does**; `story/scenes/` says what anyone says. Clip slots here are one
paragraph of purpose and no dialogue — see `ch01_room/flow.md` for the slot contract and
`ch01_room/designer_to_writer.md` for the tokens.

Party: {{HERO}} alone for days one and two; **{{HEALER}}** joins on the bell run; **{{HERDER}}**
joins at C5. Combat rules: `COMBAT.md` (and its no-combat fallback, which is how this plays today).
Creatures: `BESTIARY.md`. Hidden items: `LOOT.md`.

**Six clips, seventy-three minutes of play.** The chapter is one lesson — *you cannot win by
attacking; you wait, you turn the swing aside, and then you strike* — taught by a machine in a yard,
paid for with a sword, and cashed in on a hillside in the dark.

---

## Goals, in order

The on-screen goal line at every stage, in plain words. Rule 2: the player can always say what they
are doing and why.

| Stage | Goal line |
|---|---|
| P1 | Beat the last machine in {{MENTOR}}'s yard. |
| P2 | Take the repaired part to the guild hall. Buy bread for three. |
| after C2 | Get home before supper. |
| P3 | Eat, and go to bed. |
| night | Be at the machines before light. |
| P4 | Beat the last machine. |
| C4 → P5 | The board closes on the last ring of the bell. **Get to the guild hall.** |
| after signing | Take the job: find out what is taking the animals off the high pasture. |
| P6 | Get up to the high pasture before full dark. |
| P7 | Follow the sleeping animals. |
| after the tracks stop | Find the guard beast. |
| P8 phase 1 | Hold it open so {{HERDER}} can calm it. |
| P8 phase 2 | Stop it. It is going for {{HERDER}}. |

---

# DAY ONE

## PLAY P1 — {{MENTOR}}'s yard, midday (8 min)
*Map: `hart_yard`. Party: {{HERO}}. Light: day.*

**The chapter opens on the stick in your hand**, not on a picture. Falke is already in the yard with
a practice blade and Hart is already watching and Ottilie is already on the wall.

**Map.** One small enclosure, no exits open yet. Four machines stand in a line across the yard from
the gate: **the post**, **the barrel**, **the arm that holds**, **the swing**. Hart's workshop is at
the near end with a ladder against it. The wall between the two houses runs down the left and Ottilie
is sitting on it with her legs over.

**Minute to minute.**
1. *Machine 1, the post* (1 min). The game's first input: move, face, hit. Three clean hits and it
   falls over. An examine line on the post says what it is for, so the word for what these things do
   enters the game from an object rather than from a person.
2. *Machine 2, the barrel* (1.5 min). A barrel comes down a rope across the yard. Standing there is
   a knock-down. Step aside and hit it as it passes. **This is where the game says the word "tell"
   for the first time** — the rope creaks before the barrel comes — and the examine line spells out
   the whole rule in plain words: the thing warns you, then it comes.
3. *Machine 3, the arm that holds* (1.5 min). A jaw closes on the wrist. Getting out costs you two
   goes; parrying it on the tell costs you nothing. The first time the player chooses to wait
   instead of swing, and the first time waiting is obviously better.
4. *Machine 4, the swing* (4 min, and you do not win). A counterweighted arm. **Every attack that is
   not into an opening winds it and it comes back harder, and its counter drains your stamina**
   (`COMBAT.md` §7). The player feels both inside three attempts without being told. The tell is a
   click and the arm always comes from the left. The window is 0.45 s. The player will lose four to
   six times — flat on your back, up in two seconds, no menu, no penalty, Ottilie laughing.
   **The session ends with {{HERO}} out of stamina**, which is the picture C1 is written over.

**What the player learns.** That there is a tell; that the machines each teach one thing; that
effort has a price; and that this last machine is not hard, it is *different*, and they do not yet
know how.

**Encounters.** None. The machines are the encounters.
**The one hazard.** Machine 4's winding, which is the player's own impatience made visible twice
over: the counterweight climbs a notch every time they attack, and their own stamina bar falls.
**The optional find.** The ladder against the workshop is climbable from minute one and the roof is
walkable. **{{LOOT_YARD}}** is under the eaves across a three-cell gap (`LOOT.md`) — takeable now by
an adventurous player, but most will take it on the night of day one.
**{{HEALER}} as the hint system.** Her calls from the wall get more explicit with each failure:
a direction, then a direction and where to look, then what the sound means, and on day two the whole
answer. She has been saying it since he was twelve and he has never listened, which is why the
realisation on day two is earned and not given.
**What failing costs.** Nothing at all, ever, in this yard. That is a design rule, not a setting.
**Feel.** Funny. You are losing to furniture and a woman on a wall is enjoying it.

### CLIP C1 — "You're not ready" *(1 min, panels)* — `0110_the_yard`
**Purpose.** After the player has personally failed machine 4 several times, the game states the
problem: he cannot see why it beats him, and Hart can. Hart says he is not ready — kindly, and about
the machine, not about the boy. Ottilie comes off the wall and Mends the wrist **quickly, easily and
without stopping teasing him** — the player's first sight of magic, and what it shows is that it is
ordinary, small, reliable, and that she is good at it. No cost worth showing; she is not limited.
Nothing about the job, the board, the sword or the herders. One beat.
*Back into:* the yard, goal line *Take the repaired part to the guild hall.*

---

## PLAY P2 — Halm by day, the errand (12 min)
*Map: `halm`. Party: {{HERO}}. Light: day.*

Hart's errand is given as two field lines while the player still has the stick: a repaired part to
the guild hall, and bread for three, because Ottilie is coming to supper. **This is the first walk,
and its real job is to teach that the town is a place with things in it.**

**Map, in segments.**
1. *Hart's lane to the square.* Houses, the well, people. Five townspeople with a line each.
2. *The square.* The baker (bread for three — an actual transaction, the player's first). The bell
   tower, examinable, so the bell that rings tomorrow is a thing the player has stood under. The job
   board is visible across the square and **empty**: it is not dawn.
3. *The grain yard.* The short way through to the guild hall is the gap in its far wall. The door is
   propped, three {{GRAIN_CREATURE_PL}} are inside, and the long way round the outside is open to
   anybody who does not want to. **The first real fights of the game**, and the first parry landed on
   something alive.
4. *The guild hall.* The counter, the clerk, the part delivered.

**What the player learns.** That the town is walkable and full of examines; that fighting is normal
and happens on the way somewhere; that hidden things exist (the find, below).

**Encounters.** {{GRAIN_CREATURE_PL}}, three, in the grain yard only, one behind another. One
behaviour: rolls in a straight line and cannot turn. Tell: stands up on its rim. Drops grain and the
player's first coin. That is the whole encounter design of Halm, on purpose — **one idea per area.**
**The one hazard.** None in town. Day one is a day off.
**The teaching find — the one that is not hidden.** **{{LOOT_TEACHING_FIND}}** on the scale bench,
two steps inside the grain-yard door, in plain sight, glinting, one step off the route out
(`LOOT.md`). The game does not point at it. This is the chapter's contract with the player: *look
around and you will be paid.* Everything else in the game is hidden properly.
**Length.** 12 min with the examines, 6 if the player runs it.
**Feel.** An ordinary afternoon in a town that is about to matter.

### CLIP C2 — the counter *(1 min, talk)* — `0130_the_counter`
**Purpose.** The clerk states the rules, plainly, because the player has walked in holding
somebody else's parcel and asked about the empty board. Job sheets go up at **dawn**. The board
closes on the **last ring of the evening bell**. And, when Falke asks whether he can take one:
**no sword, no signature** — an apprentice cannot sign until his teacher says he is ready, and the
sword is the sign. One beat: the rules. Not the job; there is no job yet.
*Back into:* free walk in Halm, goal line *Get home before supper.* The board can be walked up to and
read: empty, with an examine line saying why.

---

## PLAY P3 — home, supper, night (4 min)
*Map: `halm` → `hart_yard`. Party: {{HERO}}. Light: day → dusk → night.*

The walk home at dusk, with the town's lamps coming on — the first time the player sees the light
change, which is preparation for a chapter that ends in the dark. Then Hart's kitchen: the player
walks in, sits, and **supper is a short field scene** of three or four exchanges with one examine in
the middle of it (Hart's workshop book on the bench, every attempt at every machine written down and
dated, including today's). Then his own room: two interacts — the gap on the wall where a sword
would hang, the window — and bed.

**The optional find, properly.** While Hart is inside at supper, the ladder and the roof and the
three-cell jump to the eaves are unobserved and the game is quiet. **{{LOOT_YARD}}** wants to be
taken here.
**Encounters.** None. Rule 9: this is the rest.
**Length.** 4 min.

### CLIP C3 — supper *(1 min, talk)* — `0140_supper`
**Purpose.** The last beat of the meal only. Hart says he is not ready for the guild, and means it
as care. One plain fact about Ottilie's own life lands — she came from the dark house next door, she
chose this table. One beat. No speeches, and she does not explain herself.
*Back into:* his room, then sleep. Goal line *Be at the machines before light.*

---

# DAY TWO

## PLAY P4 — the yard, all day (14 min)
*Map: `hart_yard`. Party: {{HERO}}. Light: dawn → day → afternoon → evening.*

The long grind, and **it must be fun, not 41 literal retries.** It is built as **four sessions with
visible progress and the light moving between them**, so the player experiences a whole day passing
in fourteen minutes and can see themselves getting better.

| Session | Light | What is actually different | Fails |
|---|---|---|---|
| **1. Before light** | grey dawn, lamp on the workshop | Machine 4 as it was. The player does what they did yesterday, harder. The counterweight is at the top of the post inside a minute. | 2-3 |
| **2. Morning** | sun up | **Hart changes the machine**: he comes out, pulls a pin, and the arm now swings slower with a longer click — the game giving the player an easier version of the same window rather than an easier problem. Beat this one. | 2 |
| **3. Afternoon** | high sun, Ottilie arrives on the wall | The pin goes back. Full speed. But the player has now parried it once at slow speed and knows the shape. Ottilie's call is the whole answer this time: don't swing at it, stand there and watch the arm come. | 2-3 |
| **4. Evening** | low gold | **The win.** One parry on the click, one strike into the opening, and the arm hangs dead. The counterweight never leaves the bottom of the post. | 0-1 |

**How progress is shown.** Three ways, all visual, no numbers: the **counterweight's height on the
post** at the end of each session (a mark on the post stays where your best attempt left it); the
**light moving**; and the **workshop book**, examinable between sessions, where Hart has written the
day's attempts down in his own hand, session by session. A player who looks at the book after each
session watches the day accumulate.

**Ottilie arrives for session 3** for the usual show. She goes quiet for exactly one beat when he
beats it, and then ruins it.

**Encounters.** The machine. **The hazard.** The winding, again, and by now the player knows it is
them. **Optional.** {{LOOT_YARD}}, if it is still up there. **Failing costs nothing**, all day.
**Length.** 14 min. **Feel.** A montage the player is inside of.

### CLIP C4 — the sword *(1.5 min, panels)* — `0150_the_sword`
**Purpose.** The player has just beaten the machine themselves. Hart gives him the sword he made,
and the player must understand — because the clerk said it yesterday and the game says it again here
— that **this means he may sign.** And underneath the last box, the **evening bell starts**, and does
not stop: the board has been open all day while he was in this yard. One beat, and the second half
of it is the alarm.
*Back into:* control, instantly, no fade, bell ringing, a ring counter on screen.

---

## PLAY P5 — the bell run (6 min)
*Map: `hart_yard` → `halm`. Party: {{HERO}}, then +{{HEALER}}. Light: evening.*

**A timed dash across town, and the first time the player is asked to move well.**

**The timer is the bell itself** — a set number of rings, shown as a small counter, each ring a
sound the player has been hearing all chapter. **The direct route down the lane and round the square
does not make it.** The shortcuts do, and all of them are jumps the player has already been taught
in Hart's yard by the ladder and the roof:

- **over the grain-yard wall** — a running jump off the water trough onto the wall, along it, and
  down into the yard, cutting the whole square;
- **the roofs off the lane** — two roofs and a two-cell drop into the alley behind the guild hall;
- **the well kerb to the low shed** — the small one, for players who missed the other two.

**Asking her along.** Ottilie's door is *on* the fast route, not off it. Two field lines at the door
while the bell is still ringing and she joins as a follower on the spot — the engine's follower trail
— and the run finishes with two people jumping the grain-yard wall. **She is asked, she does not
follow.**

**The board.** Four job sheets, three struck through. The fourth is the shepherd's: animals go
missing off the high pasture at night and turn up miles away, asleep and unwakeable, and the tracks
stop in open ground. It pays badly and it sounds like a tall tale, which is why it is the one still
there. **Reading it is an examine, not a cutscene** — `halm.job_sheet` / `_2`, the only place in the
chapter the job is ever written down. Signing is `halm.clerk_signing`.
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
   activity. Each one's examine says a little more: warm, unhurt, will not wake, and each is further
   from the fold than the last. **The trail IS the navigation**: no marker, no arrow, just sleeping
   animals in the dark leading the same way.
3. *The knots of sleepers.* Where the sleeping animals lie thickest, and therefore where the
   {{PASTURE_CREATURE_PL}} are standing among them. The player learns to look at a group before
   walking into it, and can read the direction the beast went from where the knots are.
4. *Where the tracks stop.* Open ground. The tracks are there and then they are not, and the grass
   past them is not flattened. The chapter's best examine point. It is also where {{HERDER}} is
   standing, looking at the same thing.

**What the player learns.** Everything the job sheet said, by finding it: the animals are taken, they
sleep, they cannot be woken, and whatever does it does not walk away afterwards. The player can
assemble the shape of the boss before anyone says a word about it.

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

### CLIP C5 — the herder *(1.5 min, panels)* — `0160_the_herder`
**Purpose.** The third party member exists. At the place where the tracks stop, in the dark, someone
small is already standing. Falke nearly swings; Ottilie stops him. They are tracking the same thing,
it is theirs, they raised it, and they do not want it killed. That is the one beat, and it changes
the goal from *find it* to *stop it without killing it*.
*Back into:* a party of three. **Night sight comes on and the map changes**: tells show a beat
earlier, the {{NIGHT_CREATURE_PL}} are readable, and the shelf above the fold glints. No line explains
this; the player sees the field change. Goal line *Find the guard beast.*

---

## BOSS P8 — {{BEAST_NAME}} (6 min)
*Map: `high_pasture`, the fold. Party: all three. Light: night. No run.*

Rules in full in `COMBAT.md` §9 and `BESTIARY.md`.

**Phase one — hold it open** *(cannot be lost, 2.5 min).* Goal line: *Hold it open so {{HERDER}} can
calm it.* The mane lifts — an enormous slow tell — {{HERO}} parries (**Guard costs no effort**), it
is OPEN, and {{HERDER}} spends **Settle at effort 5** into the opening, 24 stamina a go, which she
can afford about three times. **Attacking does nothing and the game says so each time.** So the phase
is restraint: cheap parries, everything poured into her, and her bar visibly not coming back. The
chapter's thesis as a mechanic — *neither of them can do this alone* — and the player reads it off a
bar rather than out of a box. Three cycles and it goes quiet.

> **In-field exchange `high_pasture.boss_break`** *(the writer's ask, accepted; 2 boxes)* — at the
> phase break, {{HERDER}} calls {{BEAST_NAME}} by name and for a moment it works. The turn is
> **played, not reported**: the boss UI stays up, control is frozen for two boxes, and the goal line
> does not change yet. No scene file.

Then the mane lifts again, and it looks at her, and it does not know her.

> **In-field exchange `high_pasture.boss_turn`** *(2 boxes)* — it goes for her. Fires on the round it
> changes target, not before. The goal line changes on the last box: *Stop it. It is going for
> {{HERDER}}.*

**Phase two — a real fight** *(3.5 min).* Same tell, **faster, tighter window.** Damage counts; this
is the release, and everything held back in phase one gets spent. After round three it stops
attacking the party and **goes for {{HERDER}}**, who cannot parry. {{HEALER}}'s pool of 24 and
{{HERDER}}'s drained one are what the player has left, which makes a careless phase one cost
something without ever having been a failure. Open it and finish it. Losing restarts phase two only.

**It is killed with a parry and the strike after it — the exact input the player learned on a machine
in a yard yesterday afternoon.** That is the chapter.

### CLIP C6 — what was on it *(2 min, panels)* — `0180_what_was_on_it`
**The player finds it first, in play.** `high_pasture.body` / `_2` is an examine on the body after
the fight: **a ring of nine small healed holes behind the left ear.** The player walks up and looks,
and only then does the clip run — so the chapter's last discovery is made with the stick in hand.
**Purpose of the clip.** The three of them react to what the player already found. It is not
explained. The sheep wake. {{SHEPHERD}} pays: the first coin {{HERO}} earns as a hunter. Three
people who have no answer decide to go and find one. One beat: *paid on paper, not solved.*

---

## Pacing — play against clips

| Block | Kind | Minutes play | Minutes clip |
|---|---|---|---|
| P1 {{MENTOR}}'s yard, midday | play | 8 | — |
| C1 "You're not ready" | clip | — | 1.0 |
| P2 Halm, the errand | play | 12 | — |
| C2 the counter | clip | — | 1.0 |
| P3 home, supper, night | play | 4 | — |
| C3 supper | clip | — | 1.0 |
| P4 the yard, all day | play | 14 | — |
| C4 the sword | clip | — | 1.5 |
| P5 the bell run | play | 6 | — |
| P6 the hill path | play | 10 | — |
| C5 the herder | clip | — | 1.5 |
| P7 the high pasture | play | 12 | — |
| P8 the boss | play | 6 | — |
| C6 what was on it | clip | — | 2.0 |
| **Total** | | **72** | **8.0** |

**Ratio 9:1.** The longest stretch without a clip is **P5 → P6, 16 minutes** (the bell run and the
climb), which is on purpose: it is the chapter's momentum and nothing may be inserted into it. The
longest clip is the last one. No two clips are adjacent anywhere in the chapter.

**Tension and release** (rule 9): P1 is funny, P2-P3 is the rest and the laugh, P4 is effort, C4-P5
is the spike, P6 is the first real danger, P7 is dread, P8 is the payoff. The rest is early because
everything after C4 is uphill.
