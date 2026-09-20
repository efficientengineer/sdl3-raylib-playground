# CHAPTER ONE — The Jar Run

All names are tokens. See `NAMES.md`. Party this chapter: {{HERO}} and {{HEALER}}.
{{SCHOLAR}} and {{THIEF}} arrive in chapter two.

Two days in {{HOME_TOWN}} and two days west. The chapter is built on one lesson — **you cannot win by
attacking** — taught by a machine in a yard, paid for with a sword, and cashed in on the road.

**Clip count.** Twelve, against `STYLE.md`'s six. Nine of them are talk scenes of a minute or less and
three are the panel set pieces. The owner's shape (two days, the loss, the win, the giving, the
board, the talk after) does not fit in six and the register asks for more short exchanges rather than
denser ones. Flagged in `story/notes/writer-ch01-draft1.md` for the owner to rule on.

---

## The training machines — the combat tutorial

{{MENTOR}} built four machines out of wood, iron, rope and counterweights after his knee ended his
hunting. Each one teaches one thing, and each one imitates something that is out on the road. They
are the whole of the game's combat teaching and they are the chapter's spine.

| # | The machine | It teaches | It imitates |
|---|---|---|---|
| 1 | a split post on a spring | hit a thing that is moving | anything |
| 2 | a barrel that runs down a rope | step aside; a straight line cannot turn | {{GRAIN_CREATURE}} |
| 3 | a sprung arm that closes on the wrist and holds | cut somebody loose, two rounds of sawing | {{ROAD_CREATURE}} |
| 4 | a counterweighted arm on a pivot | **wait, turn the swing aside, then strike the opening** | everything out there |

{{HERO}} has beaten the first three for a year. The fourth cannot be beaten by attacking: every hit
he lands winds the weight up, and the weight brings the arm back harder. The tell is a **click** — the
counterweight dropping — about a second before the arm comes, and it always comes on the left.
Parry on the click and the machine stands wide open for one round. That is the guard-break window,
and every creature in `BESTIARY.md` has the same shape of tell.

**Failure is expected and costs nothing.** The machine knocks you flat, the screen does not stop, and
you are up and swinging again in two seconds. The player is meant to lose thirty or forty times.

**{{HEALER}} is the hint system.** She sits on the wall between the two houses and calls the swing
before it lands, and her calls get more explicit the more the player fails:
first "Left." then "Left — watch the arm." then "It clicks first, {{HERO}}." then, on day two,
"Then don't swing at it. Stand there and watch the arm come." She has been saying this since he was
twelve and he has never once listened, which is why the realisation is earned rather than given.

**The no-combat fallback** (combat does not exist in the engine yet). The machines work as a timing
interact on the field: stand in front of one, the arm winds, the **click** plays, and a press in the
window turns the swing — a press before the click is a miss and the arm knocks you back a tile.
Three machines, three windows of different lengths, and the fourth is the one that cannot be
advanced by mashing. Everything below marked **Encounters** becomes the same timing interact on a
field creature until there is a battle system; nothing in the story changes.

**The payoff is on the road**, in the first {{ROAD_CREATURE}} encounter on day one west: it lifts a
corner before it takes anybody, which is the click, and the player parries it exactly as he parried
the machine. {{HEALER}} calls it — "You waited." — and that is the only line about it.

---

## Goal
Beat the last machine in {{MENTOR}}'s yard.

## Play: fight — {{MENTOR}}'s yard, midday of day one (8 min)

The first minutes of the game and the player has the controller before anyone says a word.

**Map.** One segment: the hill yard. The four machines in a row across the grass, the workbench under
the eaves with {{MENTOR}} at it, the low stone wall at the left with {{HEALER}} sitting on it, and the
gate down to {{HOME_TOWN}} standing open behind you.
**Encounters.** The machines, in order. One, two and three fall in a couple of tries each and teach
hit, step aside, cut loose. The fourth does not fall at all today: hit it and the weight winds, and
the arm comes back on the left. **The session ends when the arm catches your wrist**, which it will,
and the clip plays.
**Hazards / events.** None. Nothing here can hurt you but the fourth machine.
**Optional.** None yet; the yard's hidden item is the {{LOOT_YARD}} and it is not reachable until
supper on day one (below).
**Find:** nothing. This block is teaching.
**What failing costs.** Nothing at all. You cannot pass the fourth machine today however well you play.
**Feel.** A hot ordinary afternoon and a boy losing the same way he has lost all week, with somebody
on a wall finding it funny.

## Clip: The Dummies
*Scene file: `0101_the_dummies.md` (panels).*

[The yard. {{HERO}} is flat in the grass. {{HEALER}} is coming down off the wall with a bandage roll.]

{{HEALER}}: Here he goes. He is at the last one again.
{{HEALER}}: Watch its arm, {{HERO}}. Left. It always comes back on the left —
{{HERO}}: I had it! I hit that thing eleven times!
{{HEALER}}: You hit it eleven times. It hit you once. That is the whole afternoon.
{{HERO}}: My wrist is fine.
{{HEALER}}: Let go of it, then. I cannot look at a wrist you are holding.
{{HERO}}: How is anyone meant to beat that thing? It hits back harder every time I hit it!
{{HEALER}}: I know. I have watched you find that out four times this week.
{{HERO}}: Four?
{{HEALER}}: Four. Hold still. That is me finished until the evening.
{{MENTOR}}: Again. From the gate.
{{HERO}}: Again? I can't hold a stick!
{{MENTOR}}: Tomorrow, then.
{{HERO}}: {{MENTOR}} — say it straight. Am I ready or am I not?
{{MENTOR}}: Not quite ready yet.
{{HERO}}: That is what you said in the spring!

## Play: explore — the yard and the workbench (2 min)

**Map.** The same yard with the fight over. Walk it and the examine lines do the introducing:
`hart_yard.practice_posts` and `machine_two/three/four` (what each one is for, and that the fourth's
rope is set below {{HERO}}'s shoulder because {{HEALER}} uses it when the yard is empty),
`hart_yard.workshop_book`, `hart_yard.workshop_door` (a hook on the back wall with nothing on it),
`hart_yard.coat_hook` inside the door, `hart_yard.healer_house` next door — one room, one chair.
**Encounters.** None.
**Find:** the bundle on the bench, which starts the next block.
**Feel.** A workshop, not a barracks. Everything in this yard was made by the man sitting in it.

## Clip: The Errand
*Scene file: `0102_the_errand.md` (talk).*

[The workbench. A new iron catch in the vice and a tied bundle beside it.]

{{MENTOR}}: {{HEALER}}. Eat here tonight. Sundown.
{{HEALER}}: You have asked me that every week for six years.
{{MENTOR}}: And you have come every week for six years. Sundown.
{{HEALER}}: Sundown, then. I am at {{TOWN_HEALER}}'s room until it gets dark.
{{MENTOR}}: {{HERO}}. Town. Bread for three, and this bundle goes to the guild hall.
{{HERO}}: What's in the bundle?
{{MENTOR}}: The catch off the well winch. It sheared. I cut a new one.
{{HERO}}: And the rest of it? That's a letter.
{{MENTOR}}: Post. Give the whole bundle to the clerk and come home.
{{HERO}}: It's sealed, and that's your mark on the seal. What's in it?
{{MENTOR}}: Mine. Bread for three. Go.
{{HEALER}}: He will not tell you. He has never told me either, and I eat at his table.
{{HERO}}: Then I'll ask the clerk when he opens it!

## Goal
Take {{MENTOR}}'s bundle to the guild hall and buy bread for three.

## Play: explore — {{HOME_TOWN}}, the afternoon of day one (12 min)

The town says everything the chapter needs and says none of it in a cutscene. Nothing here is
compulsory except the bundle, the bread and one fight.

**Map.** (1) **The hill gate**, where the watch is leaning; from this one step the country west is
open and the {{STAIR}} stands on the horizon (`halm.stair_view`). (2) **The square and the well**:
two men filling the year's jar at the capped well and roping the lid down (`halm.well`,
`halm.jarmen`), {{MENTOR}}'s winch over it (`halm.well_winch`), the bread stall (`halm.baker`), an old
man looking up and saying plainly that the small moon has never moved (`halm.moonwatcher`), and
{{TOWN_HEALER}}'s door with a bench of people outside it (`halm.healer_door`, `halm.healer_bench`,
`halm.town_healer`). (3) **The street where {{HERO}} was born**: one doorstep, somebody else's boots
on it (`halm.fathers_step`). (4) **The guild hall**: the bare board and the empty hall
(`halm.board`, `halm.guild_hall_door`), a hunter being paid at the counter a coin at a time
(`halm.paid_hunter`), an old hunter on the porch who calls {{HERO}} "{{FATHER}}'s son"
(`halm.old_hunter`). (5) **The west gate and the grain yard**: the strip of pale blue skin nailed to
the gatepost and the opened pack in the ditch (`halm.gate_post`), and the grain gate
(`halm.grainwife`).
**Encounters.** One, and it is the first real fight: a {{GRAIN_CREATURE}} alone in the open grain
yard, tipping up on its rim and rolling at you in a straight line, unable to turn once it rolls, with
the woman at the gate shouting over the wall the whole time. It is the second machine with legs, and
the player has already beaten the second machine. It drops two coin, which the player has just watched
a clerk count out.
**Hazards / events.** None. The bell does not ring today; the board is bare and the clerk says why.
**Optional — the teaching find.** The {{LOOT_TEACHING_FIND}}, a lead lump off the grain scales, on
the scale bench two steps inside the shed door with a glint on it. The way out is the gap in the far
wall; the bench is one step the other way. Doubles one character's attack for three rounds, once per
fight, for the rest of the game. This is where the player learns that looking around pays.
**Find:** four coin under the well bucket, and bread costs one.
**What failing costs.** Nothing. You cannot lose this afternoon.
**Feel.** A small hot town with one loaded cart going the wrong way out of it every few minutes.

## Clip: The Counter
*Scene file: `0103_the_counter.md` (talk).*

[The guild hall counter. The ledger is shut. The board behind it is bare.]

CLERK: {{MENTOR}}'s bundle. Put it on the counter.
{{HERO}}: The catch for the well winch is in there. And post.
CLERK: The post I open now, so that you can tell him it arrived.
{{HERO}}: He wouldn't say what was in it.
CLERK: A job, and the money to pay for it. That is all a letter to me ever is.
{{HERO}}: A job! Put it up on the board and I'll sign for it now.
CLERK: The rule is that job sheets go up at dawn. Not tonight. Tomorrow, at dawn.
{{HERO}}: Dawn. Then I'll stand here at dawn.
CLERK: The board stays open all day. It closes on the last ring of the evening bell.
{{HERO}}: One bell at the end of the day. I can hear that from the yard.
CLERK: You can. I still cannot write your name in the book.
{{HERO}}: Why not? I've trained four years!
CLERK: No sword, no signature. An apprentice signs when his teacher says he is ready.
{{HERO}}: My teacher is up a hill. How would you know what he says?
CLERK: A hunter's first sword comes from his teacher. It is the sign. I look for the sword.
{{RIVAL}}: He looks for the sword, {{HERO}}. Mine took two years.
{{HERO}}: Two years?
{{RIVAL}}: Two. I signed for the {{COAST_ROAD}} job the morning after I got it.
{{HERO}}: That's the best job on the board and it isn't even up yet!
{{RIVAL}}: Here. Four coin for your bread. Pay me back when you sign for something.
{{HERO}}: I don't want your four coin, {{RIVAL}}!
{{RIVAL}}: Take it. I would rather you owed me than went home short.
{{HERO}}: I'll have a sword by the bell tomorrow and you can watch me sign!

## Goal
Be at {{MENTOR}}'s table by sundown.

## Play: talk-to-townsfolk — the walk back up the hill (3 min)

**Map.** The town street to the hill gate at sundown, and every NPC has one different line now that
{{HERO}} has been to the hall. The watch at the gate sends you up.
**Encounters.** None. This is the rest.
**Find:** {{HEALER}} walking up the hill from {{TOWN_HEALER}}'s at the same time — the player can
walk with her, and her house is dark and {{MENTOR}}'s is lit.
**Feel.** The end of an ordinary day in the only place either of them has ever lived.

## Clip: The Table
*Scene file: `0104_the_table.md` (talk).*

[{{MENTOR}}'s one room. Three plates, a lamp, and the workshop book open at the far end of the table.]

{{HERO}}: Job sheets go up at dawn. The clerk told me himself.
{{MENTOR}}: Heard you the first time.
{{HERO}}: And the board is open all day! I could sign and be gone by noon.
{{MENTOR}}: You could not. You are not ready.
{{HERO}}: I beat three of your machines! Three out of four!
{{MENTOR}}: The fourth one is the one that is out on the road.
{{HEALER}}: He is right about the fourth one. You never wait, {{HERO}}.
{{HERO}}: I hit it eleven times! What is there to wait for?
{{MENTOR}}: It isn't about hitting it.
{{HERO}}: Pass the bread.
{{HEALER}}: Both of you. He does not know what you meant, and you have not said it.
{{MENTOR}}: Said it.
{{HERO}}: What is that book you write in every night?
{{MENTOR}}: Every machine, every day, and how it went. Eat.
{{HERO}}: Four years of that. It's a book of me losing!
{{HEALER}}: {{TOWN_HEALER}} keeps one on me. Mine is longer than yours.
{{HERO}}: Why would she keep a book on you? You've healed this town since I was small.
{{HEALER}}: I am an apprentice. She writes down what I can close and what I cannot.
{{HEALER}}: I am twenty-three. You are seventeen. We are both somebody's apprentice.
{{HERO}}: What can't you close?
{{HEALER}}: Anything big. I run out of strength part way, and then I have to sit down.
{{MENTOR}}: She was nine when {{TOWN_HEALER}} took her on. A fever took her mother and her father that winter.
{{HERO}}: Nine?
{{MENTOR}}: She tried to close it herself. Was not strong enough. Next door on her own ever since.
{{HEALER}}: {{MENTOR}}. He asked me what I cannot close.
{{MENTOR}}: He did.
{{HERO}}: I'm going out to the machines before it's light.

## Play: explore — the house after supper (2 min)

**Map.** One room and the yard outside it, with {{MENTOR}} inside and the lamp on.
**Optional — the yard's hidden item.** The {{LOOT_YARD}}, a flat tin nailed under the eaves, reached
by going up the workshop ladder while he is inside at supper. The game never points at it and he has
never mentioned it. Whoever wears it cannot be held, wrapped or knocked down for the rest of the
game — which is the thing that would have saved {{FATHER}}, and it has been in his roof eleven years.
**Find:** {{FATHER}}'s hunter badge at the bottom of the water barrel, and {{MENTOR}} watching you
take it out. A story find, worth no money.
**What failing costs.** Nothing. Miss the tin and it is gone for the game.

## Clip: The Night
*Scene file: `0105_the_night.md` (talk).*

[The yard in the dark, the lamp out behind him, {{HERO}} in front of the last machine.]

{{HERO}}: Three out of four. He said it like it was nothing at all.
{{HERO}}: Eleven hits. Eleven, and that arm was still up.
{{HERO}}: Fine. I'll be out here before he is, and I'll stay out here all day.

## Goal
Beat it. Stay out there until you do.

## Play: fight — the long day, before dawn to last light (15 min)

The longest uninterrupted stretch of play in the chapter and the one the chapter is about. The player
fails with him.

**Map.** One segment, the yard, and the light moves across it: before dawn, morning, midday,
afternoon, last light. Nothing else on the map is open today. {{MENTOR}} comes out to the bench after
dawn and does not speak; {{HEALER}} arrives on the wall mid-morning for the usual show and stays.
**Encounters.** The fourth machine, again and again. Every hit winds the weight; the arm comes back on
the left; the click is a second before it. **Attacking cannot win**, and after about ten attempts the
game stops offering the player anything new to try — which is the point. A parry inside the window
turns the arm, the rope goes slack, and the machine stands open for one strike.
**Hazards / events.** {{HEALER}}'s calls, which get plainer the more you fail, ending on the line
that finally lands: *Then don't swing at it. Stand there and watch the arm come.*
**Optional.** None. Nothing is hidden today.
**What failing costs.** Time on the clock in the corner, which is the bell, which is the whole cost:
the board has been open since dawn and no one in the yard is thinking about it.
**Unlocks:** the parry, and the sword.
**Feel.** Grinding, and then one quiet second.

## Clip: The Sword
*Scene file: `0107_the_sword.md` (panels).*

[The yard at last light. {{HERO}} in front of the fourth machine with nothing left in his arm.]

{{HEALER}}: That is the forty-first time. You have been at it since before it was light.
{{HERO}}: Don't count today. Please don't count today.
{{HEALER}}: Left. Left! It is always the left, {{HERO}}. I have said so since you were twelve.
{{HERO}}: My arm's gone. I can't swing at it again.
{{HEALER}}: Then don't swing at it. Stand there and watch the arm come.
{{HERO}}: There's a click. The weight drops, and then the arm comes.
{{HERO}}: It's been clicking at me all day.
{{HERO}}: Come on, then. Come on!
{{HEALER}}: He turned it aside. He turned it aside and then he hit it.
{{HEALER}}: I had a very good thing ready to say to you and now I am not saying it.
{{HERO}}: Say it!
{{HEALER}}: No. You would have it framed.
{{MENTOR}}: Put the stick down. Both hands.
{{HERO}}: Did you see what it did? The arm went nowhere!
{{MENTOR}}: Saw it. Forty-one times I watched you walk into that arm.
{{MENTOR}}: Take this. I made it the winter before last.
{{HERO}}: The winter before last? You've had it the whole time?
{{MENTOR}}: Finished and on its hook. Waiting for you to stop swinging.
{{HEALER}}: The grip is dark in the middle. He has had that down off the hook a hundred times.
{{HERO}}: Why today? What changed?
{{MENTOR}}: A hunter who only attacks dies out there. I was not burying {{FATHER}}'s son as well.
{{MENTOR}}: You're ready. A first sword comes from your teacher — show the clerk and he will let you sign.
{{HERO}}: Sign? The sheets went up at dawn!
{{HEALER}}: {{HERO}}. That is the evening bell. It started while you were shouting at a machine.
{{HERO}}: The board closes on the last ring. Hold the gate open!

## Goal
Sign for work before the bell stops.

## Play: chase — down the hill and across {{HOME_TOWN}} (5 min)

**Map.** The hill road down into the town and nine streets across it, with the bell strokes as the
clock. The straight lane to the hall is corked by a loaded cart no one is coming back for, with a line
of families queued behind it carrying their beds. The fork: shove through the queue and lose strokes,
or go over the grain-yard wall and fight your way out of the shed.
**Encounters.** {{GRAIN_CREATURE_PL}} in the grain shed, three of them, one behind another. The only
fight on the run, and it is the one creature the player has already beaten.
**Hazards / events.** The cart. Climb it and it shifts and the family shouts at you; the wall is the
answer and the game wants that worked out in four seconds.
**Optional.** The {{LOOT_TEACHING_FIND}} again if it was missed yesterday — the scale bench is still
two steps inside the shed door.
**What failing costs.** Come in after the last stroke and the clerk finishes closing the ledger before
he will speak to you, and the chapter starts an in-game day late. The job is still there; it is the
only one left either way.
**Feel.** Everything the player has been doing for two days now has one minute left in it.

## Clip: The Board
*Scene file: `0110_the_board.md` (panels).*

[The guild hall. The bell has stopped. One sheet is left on the board. There is a sword on his back.]

CLERK: You made it on the last ring of the bell.
{{HERO}}: The last ring? I came the whole width of the town for that!
CLERK: Then look at the board. There is one job sheet left.
{{HERO}}: One? The board was full of job sheets at dawn!
CLERK: The others went all day. {{RIVAL}} signed for the {{COAST_ROAD}} job at noon.
{{HERO}}: At noon I was in the yard with a stick in my hand!
CLERK: You were. You have a sword on your back now.
{{HERO}}: {{MENTOR}} put it in my hands an hour ago. He said to show you.
CLERK: Then I will write your name. The last job sheet is the water run.
{{HERO}}: What does the water run pay?
CLERK: Thirty coin. Two days out and two days back.
{{HERO}}: Out to where?
CLERK: Out to the {{STAIR}}. A jar of our well water goes there sealed, every year.
{{HERO}}: What for?
CLERK: No one has ever told me. It goes all the same.
{{HERO}}: Who put it on the board?
CLERK: You did. It came in yesterday, with {{MENTOR}}'s post.
{{HERO}}: I carried it in? I stood right here while you opened it!
CLERK: You did.
{{HERO}}: {{MENTOR}}? But the carters take the jar out. They take it every year.
CLERK: There is no convoy this year. Every carter in the valley has walked west.
{{HERO}}: Then who is paying the thirty coin?
CLERK: {{MENTOR}} is. His own money.
{{HERO}}: A man who walks that road alone dies out there.
{{HERO}}: He was my father.
{{HERO}}: I'll take it! Write my name down now!

## Goal
Go back up the hill and ask {{MENTOR}} what is out there.

## Play: talk-to-townsfolk — back up the hill in the dark (3 min)

**Map.** The same nine streets at night with the job sheet in his hand. Every NPC has heard within
ten minutes: the ostler, the baker, the old hunter on the porch who knew {{FATHER}}, and the watch at
the hill gate who says the last man who walked that alone did not come back and does not say who.
**Encounters.** None.
**Find:** the guild's copy of the sheet in his pack, examinable: the water run, thirty coin, one
sealed jar, and {{MENTOR}}'s name in the payer's line in {{MENTOR}}'s own hand.
**Feel.** A town that already knows more about your job than you do.

## Clip: What The Road Is
*Scene file: `0120_what_the_road_is.md` (talk).*

[{{MENTOR}}'s yard in the dark, the lamp lit inside the door, the signed sheet in {{HERO}}'s hand.]

{{HERO}}: You posted it. You put the water run in my hands and sent me down the hill with it!
{{MENTOR}}: I did.
{{HERO}}: You let me carry my own job into that hall and you never said a word.
{{MENTOR}}: You were not ready. Yesterday you were not ready.
{{HERO}}: So you sent it somewhere I couldn't reach it!
{{MENTOR}}: Where anybody in the valley could take it but you. Then this afternoon you were ready.
{{HERO}}: And now you want it back off me.
{{MENTOR}}: I made that sword. I put it in your hand. I cannot take it off you again.
{{HERO}}: Then tell me what's out there. You've never once told me what's out there.
{{MENTOR}}: Twenty years your father and I carried that jar out together.
{{HERO}}: Twenty years, the two of you.
{{MENTOR}}: The twentieth year that road took my knee. I have not walked it since.
{{MENTOR}}: The year after, {{FATHER}} walked it on his own. He did not come back.
{{HERO}}: I was six, and no one has ever told me where he died.
{{MENTOR}}: Out in the grass, north of the river. There are {{ROAD_CREATURE_PL}} in that grass.
{{HERO}}: What is a {{ROAD_CREATURE}}?
{{MENTOR}}: Pale blue skin, lying flat. It comes up and wraps whoever walks over it.
{{HERO}}: And then what happens?
{{MENTOR}}: Then you cannot move your arms, and somebody else has to cut you out.
{{MENTOR}}: Two of us went out so that there was always a somebody. He went alone.
{{HERO}}: Is that what the last machine is? The arm that comes back at you?
{{MENTOR}}: Everything out there hits back. That is the whole of what I built it to teach.
{{HERO}}: What's at the far end of it? I've never been past the bridge.
{{MENTOR}}: A staircase the ancients built, coming down out of the cloud.
{{HERO}}: Who are the ancients?
{{MENTOR}}: The people who built it. They have been gone a thousand years.
{{HERO}}: And we carry a jar of well water to a staircase.
{{MENTOR}}: A woman lives under the bottom step. She takes the jar, once a year.
{{HERO}}: What is the water for?
{{MENTOR}}: No one in {{HOME_TOWN}} knows. It has gone every year since before my father.
{{HERO}}: Who is she?
{{MENTOR}}: I never learned. Your father sat and talked with her, every year, for twenty years.
{{HERO}}: About what?
{{MENTOR}}: He never said and I never asked. Put the jar on her shelf and come home.
{{HERO}}: I'm taking it.
{{MENTOR}}: I know. Take {{HEALER}} with you, and ask her yourself.
{{HERO}}: She'll say yes before I've finished the sentence!

## Play: shop — the workshop and the bench, last thing at night (4 min)

**Map.** The workshop, with {{MENTOR}} selling out of his own stock because the town's shops are shut.
**Shop.** Rope, a waterskin, a food bag, and bandage linen. You cannot afford the rope and the food
bag both, which is the point; the rope is what gets you down the washed-out culvert tomorrow and the
{{LOOT_ROAD}} is at the bottom of it.
**Unlocks:** the sword is already yours. It has long reach, it is fast, it is the only weapon in the
game that can break, and the parry is what it is for.
**Feel.** He charges you for the rope. He does not charge you for the sword and will not discuss it.

## Clip: Asking Her
*Scene file: `0130_asking_her.md` (talk).*

[The low wall between the houses. Her house dark behind her, his lit behind him.]

{{HERO}}: {{HEALER}}. I'm walking the water run west at dawn. Come with me.
{{HEALER}}: Yes.
{{HERO}}: That was fast.
{{HEALER}}: My bag has been packed since the bell. Someone has to carry the bandages you will need.
{{HERO}}: You packed it before I asked?
{{HEALER}}: I packed it because {{TOWN_HEALER}} told me to pack it.
{{HERO}}: {{TOWN_HEALER}} wants you to go west?
{{HEALER}}: She gives full standing after a season of work away from home. I have never had one.
{{HERO}}: A season away? You've healed this whole town since I was small!
{{HEALER}}: In this town. I have never been further than the bridge in my life.
{{HERO}}: Four days is not a season.
{{HEALER}}: It is four days more than I have. She said to start.
{{HEALER}}: Two winters ago they carried a carter in off that road. I could not close what he had.
{{HERO}}: What happened to him?
{{HEALER}}: He died on {{TOWN_HEALER}}'s bench with my hands on him. I want to be out where it happens.
{{HERO}}: You never told me any of that.
{{HEALER}}: You have never asked me anything until tonight.
{{HERO}}: I'm asking now. Come west with me.
{{HEALER}}: I said yes the first time. Be at the gate before it is light.
{{HERO}}: I'll be there before you are!
{{HEALER}}: You will not.

## Goal
Walk the jar two days west to the {{STAIR}}.

## Play: travel — the west road, morning of day one west (12 min)

**Map.** (1) The road out of the valley, wide, flat and packed: handcarts, families, a man leading
four goats, a woman carrying a board with the lord's name painted on it. (2) The verges and the old
roadbed, where the crowd thins and the walking is in long grass at the side. (3) The culvert, washed
out in the spring, the queue backed up at it, the river bridge visible beyond.
**Encounters.** {{THIEF_CREATURE_PL}} on the verges, four to six at a time, hopping in to take one
item out of your pack and running for the ditch; kill one and it drops what it took. {{AMBUSH_CREATURE_PL}}
standing in the row of real markers on segment 2, one or two, motionless until you walk past and then
falling on you for one enormous hit; the seam opens a finger's width first, so a player who is
watching for a tell — which is now every player — gets a free round.
**Hazards / events.** **The culvert.** The road is out and the queue is going the long way round.
Rope puts you straight down and up in one move; without it you walk round with everyone else.
**Optional.** The {{LOOT_ROAD}}. Down on the rope, in the drain under the broken culvert, is a
{{THIEF_CREATURE_PL}}' hoard — a year of everything stolen on this road. Ten per cent critical chance
for whoever wears it, for the rest of the game, and it looks like nothing at this level.
**What failing costs.** A {{THIEF_CREATURE}} that reaches the ditch keeps what it took.
**Feel.** A road with more people on it going one way than anyone alive has seen.

## Play: boss — the bridge (5 min)

**Map.** One segment: the river bridge, the only crossing for a day in either direction, backed up
with carts and kept moving by a man in a new coat.
**Encounters.** None. The bridge is the busiest place in the chapter and nothing comes near it.
**Boss.** {{RIVAL}}. One on one, no healing allowed, and he breaks your guard twice on purpose before
the fight is winnable, so the player learns the window under pressure against somebody who is better.
**What failing costs.** Lose and you cross at the back of the queue: an hour of daylight.

## Clip: The Road West
*Scene file: `0150_the_road_west.md` (panels).*

[The bridge. {{RIVAL}} is standing on the parapet wall in a new coat.]

{{HERO}}: Where did you get that coat?
{{RIVAL}}: The lord of the {{DRY_CITY}} bought it for me.
{{HERO}}: You signed for the {{COAST_ROAD}} job. I watched the clerk write your name down.
{{RIVAL}}: I did, and I sold it on to another hunter the same morning.
{{HERO}}: You sold a guild job the day you signed it?
{{RIVAL}}: For eighty coin a week. That is what the lord pays a hunter.
{{HEALER}}: Paying you to do what, exactly?
{{RIVAL}}: To walk west with him. He is going to the mountain.
{{HEALER}}: What is at the mountain?
{{RIVAL}}: The ancients are. He means to ask them for rain on his city.
{{HERO}}: People have asked the ancients for a thousand years! They have never answered anyone.
{{RIVAL}}: He has someone with him who can make them answer.
{{HEALER}}: Who has he got?
{{RIVAL}}: He did not say, and at eighty coin a week I did not ask.
{{RIVAL}}: You're carrying a sealed jar and I'm riding in a cart. Enjoy the walk!
{{HERO}}: Thirty coin, and I signed for mine myself!

## Goal
Leave the road and cross the empty country to the {{STAIR}}.

## Play: travel — north off the road, afternoon of day one west (15 min)

**Map.** (1) The turning: past the bridge the crowd keeps west along the river, the jar path goes north
into open grass, and in ten steps the noise of the road stops. (2) The grass, flat and shoulder-high,
the ridge on the horizon. (3) The low hills under the ridge, broken ground, a cave mouth off the path.
**Encounters.** The densest stretch in the chapter, and constant. **{{ROAD_CREATURE_PL}}** in the flat
grass, one or two, lying still until a corner lifts — that is the tell, and it is the machine's click.
Parry it and it drops flat and opens; miss and it wraps a party member, who cannot act until the other
cuts them out over two rounds. **This is the chapter's payoff and it happens the first time here.**
{{STALKER_CREATURE_PL}} in threes to fives, the big one at the back putting the others back on their
feet until you kill it. {{RUIN_CREATURE_PL}}, always alone, always near a piece of old road: it strikes
itself, the note hits everyone wherever they stand, and it cannot sound twice running.
**Why the road was safe and this is not.** Out here you fight everything that finds you and there is
no one within half a day. The convoy crossed this grass twenty strong with its own healer. Two people
cross it as two people. One person crosses it as one — which is the line {{MENTOR}} said in the yard
and the reason {{FATHER}} died.
**Hazards / events.** **The rockslide.** The slope lets go behind you in the low hills and buries the
short path over the shoulder, so you camp on the ridge tonight instead of pushing on.
**Optional.** The cave mouth in the low hills, a roped descent: a {{CAVE_CREATURE}}, the hardest
fight in the chapter, armoured on top, soft underneath, open only when it rocks back to swing. It
drops the {{LOOT_SECOND_SWORD}} — worse than the one {{MENTOR}} made, and it does not break. Meant to
be too hard on a first visit.
**Feel.** The noise of the world stops in ten steps and does not come back.

## Play: explore — camp on the ridge, night one (5 min)

**Map.** A dip under the ridge, out of the wind, one fire. From up here the {{STAIR}} comes into view
for the half day ahead: a staircase as wide as a town coming down out of the cloud and stopping thirty
feet above an empty field, with nothing holding it up. The camera holds on it.
**Encounters.** None while you set up. Build the fire, take the watches — {{HEALER}} takes the first
because you are asleep on your feet.
**Hazards / events.** **The ambush**, scripted, once: {{STALKER_CREATURE_PL}} and a {{ROAD_CREATURE}}
come into the firelight in the small hours and you fight them half awake at a penalty.
**What failing costs.** Lose and you start day two hurt. The jar cannot be lost.

## Clip: The Fire
*Scene file: `0160_the_fire.md` (talk).*

[Under the ridge. {{HEALER}} has the first watch. The {{STAIR}} is a shape against the sky.]

{{HERO}}: That was the worst day I have ever had. How many was that?
{{HEALER}}: Nineteen. And I patched you four times, twice for the same arm. Some things do not change.
{{HERO}}: You saw the first {{ROAD_CREATURE}}, though. I waited for it and I turned it!
{{HEALER}}: You did. Two days ago you would be lying in that grass wrapped up like a parcel.
{{HERO}}: Did you know my father? Properly, I mean.
{{HEALER}}: I was twelve, and he was the big one who came and went.
{{HEALER}}: I remember one thing about this run. He came home a day late off it. Every year.
{{HERO}}: A day late, every year? I was six when he died and no one ever told me that!
{{HEALER}}: {{MENTOR}} used to stand at the gate and count the days.
{{HERO}}: How do you even remember that? You were twelve.
{{HEALER}}: I had lost mine three years before. You notice whose father still comes home.
{{HERO}}: {{HEALER}} —
{{HEALER}}: Sleep. I wake you at the turn.
{{HERO}}: Wake me early. I want to see it in the light.

## Goal
Put the jar on the shelf under the {{STAIR}}.

## Play: travel — day two and the shrine (10 min)

**Map.** (1) Half a day of grass with the {{STAIR}} growing in front of you until it stops being a
shape and becomes a wall you cannot see the top of. (2) The {{SHRINE_CREATURE_PL}}' ring: knee-high,
four-armed, shrine-grey, facing outward, evenly spaced, there longer than the houses have been — walk
between two and a green line brightens down each spine until you are through. (3) The shrine: six
houses, a well, pilgrims, and a stall selling rubbings of the bottom step.
**Encounters.** Segment 1 only, and heavy: {{STALKER_CREATURE_PL}} and {{RUIN_CREATURE_PL}} again,
plus one last {{ROAD_CREATURE}} on the flat before the ring. Nothing crosses the ring, so from segment
2 the game stops spawning entirely — the first quiet since the bridge.
**Hazards / events.** Ask anyone at the shrine why nothing comes in and they say the
{{SHRINE_CREATURE_PL}} were here first and change the subject.
**Optional.** The {{LOOT_SHRINE}}, under one of them, found only by examining the gap in the ring
instead of walking through it. Fleeing succeeds from anything for the rest of the game.
**What you do.** Put the jar on the stone shelf, sealed, the way you were told. The job is done and
the screen says so, with a day and a half of chapter left.
**Find:** last year's jar still sealed; a rope hanging from the bottom step; cart tracks in the grass;
eleven carters' receipts nailed inside the shrine door; and the shrine's oldest woman, who talks to
anyone who will stand still.
**Feel.** Somewhere holy that turns out to be a village with a gift stall.

## Clip: The Jar
*Scene file: `0170_the_jar.md` (panels).*

[The stone shelf under the bottom step. The new jar is on it next to last year's.]

SHRINE WOMAN: Set it down there, beside last year's. She keeps the shelf for them.
{{HERO}}: Who does? Who takes the water?
SHRINE WOMAN: The woman who lives under the bottom step. She was here before these houses were.
{{HERO}}: What is her name?
SHRINE WOMAN: She has never given one. I have asked her for sixty years.
SHRINE WOMAN: The hunter who used to bring the jar sat with her a whole day. Every year, twenty years.
{{HERO}}: A whole day.
{{HERO}}: That's the day! He was never late coming home — he was here!
{{HEALER}}: {{HERO}}. There are papers nailed inside this door. Come and count them.
{{HERO}}: They're carters' receipts. One for carrying the jar.
{{HERO}}: Eleven of them. One for every year since he died.
{{HEALER}}: Read whose name is at the bottom of each one.
{{HERO}}: {{MENTOR}}. It's {{MENTOR}} on all eleven. He paid a carter every single year.
{{HEALER}}: He never told you, and he never told me.
{{HERO}}: Twenty years my father sat here and he never once said her name to me!
{{HEALER}}: He may not have had one to say. Look at last year's jar.
{{HERO}}: The seal is still on it.
{{HEALER}}: Then she did not come for it. She has been gone a year and no one noticed.
{{HERO}}: It goes up and up. Where does the top of it come out?
{{HEALER}}: In the cloud. I have been looking at it an hour and I cannot find an end.
{{HERO}}: That old man in the square told me the {{SECOND_MOON}} has never moved.
{{HEALER}}: It has not. It has sat over the same roof his whole life.
{{HERO}}: {{HEALER}} — it is moving. It is moving west, right now!
{{HEALER}}: I see it. Listen — every bell in the village has started.
{{HERO}}: Pack the fire. We're going west before it stops.

## Play: explore — night under the {{STAIR}} (2 min)

**Map.** A fire and two bedrolls at the foot of it, inside the {{SHRINE_CREATURE_PL}}' ring. The stone
goes up past where the firelight reaches and keeps going. This block sits **inside** the clip above:
the player walks the shrine ground at night, looks up, and the moon moves while the camera holds.
**Encounters.** None, and the game says so: no watch tonight.

## Goal
Go west and catch the lord before the moon stops moving.
