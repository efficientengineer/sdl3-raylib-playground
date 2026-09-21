# COMBAT — the rules, as far as chapter one needs them

Traditional Phantasy Star IV turn-based combat: the party on the right, a group of enemies on the
left, commands chosen for everybody, then the round resolves in speed order.

**The twist is EFFORT** (owner, 2026-09-21): *"a slider for effort, and doing attacks at higher
effort takes more stamina, but do more damage or magnitude. You can do any skill at any time as long
as you have the minimum effort needed to use a skill."* There is no MP, no cooldowns, and no unlock
gating. **Stamina regenerates by itself**, in and out of battle, so nothing is hoarded across a
dungeon. The only question the player ever answers is: **how hard do I go this turn, against what I
will have next turn?**

The second idea, which chapter one teaches alongside it, is the **parry**: enemies telegraph, a cheap
guard at the right moment opens them, and the opening is what effort is *for*.

**None of this exists in the engine yet.** §10 is how the chapter plays today.

---

## 1. Commands

Five entries, no submenus in chapter one.

| Command | What it does | Effort? |
|---|---|---|
| **Attack** | Weapon damage on one enemy. | yes, 1-5 |
| **Skill** | The character's short list (§5). Each skill has a **minimum effort**. | yes, min..5 |
| **Guard** | Halves damage this round; against a **tell** it is a **parry** (§4). | **no — always effort 1** |
| **Item** | One use, resolves first in the round. | no |
| **Run** | Leaves the fight. Not a turn. Only the boss refuses it. | no |

## 2. Effort — the slider

Every action that has a magnitude is performed at an **effort** of **1 to 5**. The slider sits under
the command the moment it is chosen, it **remembers its last setting per command per character**, and
on touch it takes a drag or a tap on a notch — most turns are therefore one tap, because the setting
you used last time is already there.

| Effort | Stamina cost | Magnitude |
|---|---|---|
| 1 | 2 | ×0.5 |
| 2 | 5 | ×0.8 |
| 3 | 9 | ×1.0 |
| 4 | 15 | ×1.25 |
| 5 | 24 | ×1.5 |

**Efficiency is deliberately worse than linear at the top**: effort 5 costs 2.7× effort 3 and gives
1.5× the result. Max effort is a decision about *this* round, never the default. Effort 1 is cheap
and weak but is never useless, because a hit into an **opening** doubles whatever it was.

"Magnitude" means whatever the action has: damage, healing restored, status duration, status chance.

## 3. Stamina, and the regeneration rule

One pool per combatant. **Chapter one: {{HERO}} 40, {{HEALER}} 36, {{HERDER}} 30.** A bar under the
portrait, and the effort slider shows the cost against it before you commit, so a player can never
accidentally spend what they do not have.

- **It regenerates by itself, every turn, for everybody, enemies included: +8 at the top of the
  round.** Out of battle it regenerates **continuously while walking**, about a full pool a minute,
  so a fight never begins depleted unless the last one just ended.
- **Guard adds +4 on top** (so a guarding turn nets +12), and a successful **parry adds +8** (nets
  +16). Waiting is not a dead turn; it is how you pay for the next one.
- **WINDED — the one thing that cancels the regeneration.** A character who **attacks into a tell**
  and is countered by something that winds (today: the swing) is **winded**: the counter drains
  `5 + 4 × the counterweight's notches` stamina on the spot, and for `1 + notches ÷ 2` rounds the +8
  at the top of the round **does not come**. Nothing else in the game suppresses the regeneration.
  **Guarding clears it immediately** — a parry or an ordinary guard, either one — which is §3's
  "waiting is how you pay for the next one" written as a rule. This is the mechanism §4a.3 calls
  *closing the door*: with the regen off, a spent player really cannot afford Attack, and Guard,
  which pays +4 by itself, is the only entry left lit. A parry also sets the counterweight back to
  zero, so the drain and the winding both unwind on the same input the chapter is teaching.
- **Items are extras, not the source.** Food restores a chunk out of battle. There is no MP potion.
- **At zero you are never stuck**: every character can always Guard, and can always Attack at effort
  1 (cost 2) — which is affordable the moment the round's +8 lands. What you lose at low stamina is
  **access to your big skills**, because each has a minimum effort (§5). **That is the whole gating
  system**: a tired character is not silenced, they are reduced to small work.

**Tuning targets, stated so a programmer can hit them:**
1. **Effort 3 every turn is roughly break-even** with regeneration (cost 9 against +8, so a very
   slow bleed).
2. **Effort 5 every turn empties a full pool in 2-3 turns.**
3. **One or two turns of effort 1, or of guarding, brings you back** to where effort 4-5 is
   affordable again.
4. A fight lasts 4-8 rounds, so a player who spends correctly gets **two or three big turns** in it.

**Enemies use the same pool and the same rules,** and a spent enemy is readable: it attacks slower,
hits softer, and its tell is smaller and later. The player can see when an enemy is tired and choose
to press or to recover, and nothing about that needs a tutorial box.

## 4. The parry, the tell, and the opening

Every enemy has exactly one **tell** — a visible, named, one-thing-only animation with a sound — and
it plays **at the end of the round before the attack it belongs to.** The player always sees the
tell, then chooses, then the attack lands.

- **Attack into a tell** → **counter-hit**: your damage lands, then theirs lands at **double**.
- **Guard into a tell** → **parry**: no damage taken, **+8 stamina**, and the enemy is **OPEN** for
  one full round.
- **Guard with no tell** → an ordinary guard: half damage, +4 stamina. Never a waste.

**OPEN** means: every hit on it this round does **double** damage, it cannot act, and any status put
on it lands **without a check**.

**So the loop the whole game is built on is: parry cheap → open them → spend effort into the
opening.** Effort 5 into an opening is ×1.5 × ×2 = three times an ordinary effort-3 swing, and the
parry that bought it refunded a third of what it cost. Every fight in the game is that sentence.

The tell belongs to a target, and whoever guards is who parries it, so three party members can parry
three tells in a round.

## 4a. How the parry is taught — with no words at all

**Binding (owner, 2026-09-21):** *"I don't like that Ottilie tells Falke what to do, it's not her
domain. She just thinks it's funny he's getting whomped."* **No character coaches the player, ever,
in this chapter.** The old design ran an escalating hint script through her from the wall; it is
struck out of every file. The realisation is the player's own or it is worth nothing.

Five things carry it, in this order, and none of them is a sentence:

1. **The tell is a performance, not a cue.** The swing's counterweight **visibly drops** a hand's
   width, the arm **winds back** with a sound like a ratchet, and only then does it come. It is large,
   it is slow, it happens every single time, and it always comes from the same side. A player who
   never works out what to do with it has still seen it fifty times.
2. **Attacking makes it worse, on screen.** Every attack that is not into an opening winds the
   counterweight another notch up the post and it comes back harder. The post is a graph of the
   player's own impatience and it is standing in the middle of the yard.
3. **The stamina drain closes the door, and WINDED is how it closes** (§3). The swing's counter
   takes `5 + 4 × notches` off the attacker and then **stops their regeneration** for `1 + notches ÷ 2`
   rounds, so the +8 does not arrive to bail them out. Within a session the player cannot afford
   Attack, cannot afford Hard swing, and **Guard is the only command left that costs nothing.** The
   game does not suggest Guard. It removes everything else — and the one input that gives the regen
   back is guarding, which is also the answer.
4. **The menu says it in light.** Anything unaffordable is **greyed with its cost shown**; Guard is
   never greyed and is **visibly lit**. At the bottom of a bad session the command list is one bright
   entry in a row of grey ones. That is the loudest the game is ever allowed to get.
5. **Non-verbal escalation, only after repeated failure.** After three failures in a session the tell
   **slows by about a fifth** — the same tell, more of it. After five, the **Guard entry pulses once**
   as the tell plays. Nothing is said, nothing is unlocked, and the difficulty of the input does not
   change; the player is given more time to notice, not a different problem. Both escalations reset
   the moment a parry lands, and neither ever fires outside {{MENTOR}}'s yard.

**{{MENTOR}} says one word.** *"Again."* He is not withholding the answer to be wise; he cannot hand
over a thing that is learned in the hands. **{{HEALER}} is not a hint system.** She is on the wall
enjoying it: she winces at a big hit, she counts falls, she goes quiet for exactly one beat the first
time he turns it aside — feedback the player reads as applause, never as instruction. Her going quiet
happens **after** the parry, so it can never be a tip.

**The no-combat fallback (§10) teaches it identically**: the same drop-and-wind animation, the same
sound, the same stamina bar on the HUD emptying until the only press that does anything is the cheap
one, the same lit-versus-grey prompt.

## 5. The three of them

Each skill has a **minimum effort**; below it the skill is greyed on the list with its minimum shown.

**{{HERO}} — the sword.** No magic, this chapter or any.
| Skill | Min | What it does |
|---|---|---|
| Attack | 1 | damage |
| **Hard swing** | 4 | damage ×1.6 of the effort's magnitude, but you **cannot guard next round** |
| Guard / parry | — | §4 |

**His flaw is the system's tutorial.** Falke swings at effort 5 every single turn. The swing
counters full-effort attacks into its guard and drains him doing it, so by the end of a session he
is **too tired to swing at all** — and the only thing he can still afford is a **cheap guard**.
Which is the answer. The player learns both halves of the game in one beat: the parry is cheap, the
opening is what you spend into. **Nobody tells them.** See §4a.

**{{HEALER}} — the party's healer, and good at it.** She is an apprentice in title only: the town
healer has not given her full standing, which is a matter of experience away from home and not of
power. **She is reliable from the first fight and the player should feel that immediately.**

| Skill | Min | What it does |
|---|---|---|
| **Mend** | 1 | restores health, scaled by effort, full range 1-5 |
| **Steady** | 2 | removes one status (dazed, blinded, slowed, held) |
| **Shield** | 2 | one character takes reduced damage; the reduction and the duration scale with effort |
| **Attack** | 1 | a mace, and she is better with it than she lets on — real damage, not a joke |

Ordinary pool (**36**) and the ordinary effort rules, no special ceiling. The only limit on her is
the one that applies to everybody: **magic does light work in this world** — nobody, her included,
can push a spell to fireball magnitude. That is a rule of the world, not a flaw of hers, and why it
is true is an open thread nobody on screen knows.

**{{HERDER}} (Distel) — pace, not damage.** Sleep-work; never does a point of damage.
| Skill | Min | What it does |
|---|---|---|
| **Drowsy** | 1 | one enemy's speed drops. **Duration scales with effort**: 1 round at effort 1, 5 at effort 5 |
| **Calm** | 2 | one enemy's attack drops. Same duration scaling |
| **Settle** | 4 | the big one. Puts a weakened enemy out. High minimum, so a tired Distel cannot reach it — which is the boss fight's whole problem |
| **Night sight** | — | passive, free, always on: **every tell is shown one beat earlier**, enemies that hide their tell in the dark show it at all, and **enemy stamina bars are visible** |

**The world rule.** Every magic skill in the game has a **low effort ceiling** — light work only.
Nobody can push a spell to fireball magnitude. *Why* is an open thread and no one on screen knows.

**How the three combine.** {{HERO}} parries cheap → OPEN → {{HERDER}} lands a status without a check,
or {{HEALER}} spends her turn safely, or {{HERO}} himself spends effort 5 into it. The boss
demonstrates it with no dialogue at all.

## 6. Turn order

One **speed** per combatant, fixed this chapter; highest first, ties to the party. Within a round:
items → skills → attacks and guards → enemy attacks. **Guard is in effect for the whole round however
slow the character is**, so parrying never depends on out-speeding anything. That is a rule, not an
accident: **the parry is a reading test, never a speed check.** A Drowsy'd enemy can be knocked below
the party in the order, which is what makes effort-scaled duration worth buying.

## 7. The machines as enemies

Hart's **three** machines fight by these rules with everything else switched off (`BESTIARY.md`).
They have no HP worth the name; they are beaten by parrying and striking the opening. They are
**the post**, **the arm that holds**, and **the swing**, and they are named, never numbered.
(A fourth, a barrel on a rope, was cut: its only job was to introduce the word *tell* in a sentence,
and no sentence introduces anything in this chapter any more.)

**The swing cannot be beaten any other way**: every attack that is not into an opening **winds its
counterweight**, and it comes back faster and harder, and the counter drains the attacker's stamina.
The harder the player tries, the worse it gets. The machine is a graph of the player's own
impatience, and it is drawn on the post as a mark that climbs.

## 7a. Teaching order — at most one short prompt per new idea

**Rule: the game explains nothing it can instead withhold until the player does it.** Goal lines are
**six words or fewer**. There are no system pop-ups, no multi-box explanations, and no line anywhere
that describes something the player has just watched happen.

| # | New idea | How it arrives | The one prompt allowed |
|---|---|---|---|
| 1 | move, face, hit | **Only Attack exists.** The command list is one entry. | the button glyph, no words |
| 2 | things have tells | the arm that holds telegraphs, closes, and costs you two goes | none |
| 3 | Guard | **Guard appears in the list** the first time something telegraphs at you | *"Guard"* — the word, appearing |
| 4 | Guard on a tell is a parry | the player does it and the enemy is visibly OPEN | none; the stagger is the prompt |
| 5 | effort | the **slider appears under Attack** the first time the player has an opening to spend into | the notches, and the cost shown against the bar |
| 6 | minimum effort | a skill greyed **with its minimum shown** | the grey itself |
| 7 | stamina regenerates | the bar refills while walking, where the player can see it | none |

Nothing on this list is ever re-explained, and an idea that has not arrived yet is **absent from the
UI**, not greyed. That is the difference between the list growing and the list being a menu the
player must read.

## 8. Losing, and running

- **In {{MENTOR}}'s yard, losing is free and instant.** No menu, no reload; flat on your back, up in
  two seconds, Ottilie laughing from the wall — at him, about nothing useful. The player is meant to
  lose a lot and find it funny.
- **Everywhere else in chapter one, losing is gentle.** The party wakes at the last safe place — the
  yard, the last house on the hill path, the shepherd's fold — with everything they had. Nothing is
  lost but the walk back.
- **Running always works**, except the boss; it costs the party's turn.
- **The boss's phase one cannot be lost.** Phase two can, and losing it restarts phase two only.

## 9. The boss, in rules — restraint, then release

Two phases, no break. Full staging in `chapter01.md` P8.

**Phase one — hold it open** *(cannot be lost).* Goal line: *Hold it open for {{HERDER}}.* (Six words is the rule; the longer form once written here and in `BESTIARY.md` is struck.)
The mane lifts (a huge, slow tell), {{HERO}} parries **at effort 1, because Guard has no effort**, it
is OPEN, and {{HERDER}} spends **Settle at effort 5** into the opening. Settle's minimum is 4 and its
cost at 5 is 24 against a pool of 30, so **she can afford it twice, and the second one hurts**: the
first empties her, and three rounds of guarding are what buy the second. The player watches her bar
drain toward the floor while she does it. Attacking the beast does nothing at all in this phase — the
message says it is not hurt — so the phase is **entirely about restraint**: cheap parries, and
everything the party has goes into her. It works. Then it does not: it looks at her and does not know
her.
**This is the clearest statement the game will ever make of its own system**, and it is made with a
bar emptying, not a line of dialogue.

**Phase two — spend it** *(no run).* Same tell, **faster, and the window to choose is tighter.**
Damage counts now. After round three it stops attacking the party and **goes for {{HERDER}}**, who
cannot parry. {{HEALER}}'s remaining pool and {{HERDER}}'s drained one are what the player has left, which is
why a careless phase one costs something without ever having been a failure. Open it, and spend
everything into the opening. It dies to a parry and the strike after it — the exact input learned on
a machine in a yard the previous afternoon.

## 10. THE NO-COMBAT FALLBACK — how chapter one plays today

There is no battle system, and the chapter must be playable before there is one. So **every fight is
a field interaction with the same shape**, built from what the voxel field already has: the interact
press, the `!` prompt, cell triggers, and dialogue boxes that freeze movement.

An encounter is a creature sprite standing on the field. Walk into it, or press interact in its
radius, and:

1. It plays its **tell** — the same animation and sound the battle version will use.
2. A **window** opens, about 0.6 s on the pasture, 0.45 s for the swing. **Tap inside the window**
   = parry: the creature staggers and is OPEN.
3. **Effort is a hold.** The follow-up press into the opening is **hold-to-charge**: a short bar
   fills in four visible notches while the button is down and empties your stamina meter as it goes.
   Release early for a cheap hit, hold for a big one, hold past what you have and it fizzles and you
   are winded. **That one input teaches the whole slider before the slider exists.**
4. Tapping early, tapping late, or walking into the creature knocks you back two cells, costs a
   little stamina, and resets it. Nothing else is lost.

One to three cycles a creature. Stamina is a bar on the HUD that **refills as you walk**, so the
regeneration rule is learned by walking between encounters. Fallbacks for the rest:

- **The machines** are the same interaction at three window lengths. The swing's is shortest, and
  pressing outside it winds the weight *and* drains you — so the player is literally too tired to
  swing by the end of a session, which is the story beat, played. In the fallback the two prompts
  are a **charge glyph** (the swing press) and a **guard glyph** (the cheap press), and the charge
  glyph **greys out when the bar cannot pay for it** while the guard glyph stays lit. §4a's
  escalation applies here unchanged: after three failures the wind-up plays slower, after five the
  guard glyph pulses once as it plays, both reset on a parry.
- **{{HEALER}}'s Mend** is an automatic field event after a losing exchange: she patches you up
  quickly and it costs her a little of a bar that fills straight back as you walk. It is routine for
  her, and the field should make it look routine.
- **{{HERDER}}'s night sight** is a field effect: with her following, the tell and the `!` appear a
  beat earlier and the dark creatures become visible.
- **Drowsy / Calm / Settle** are one scripted press in the boss sequence, with her bar visibly
  draining per use.
- **The boss** is a scripted three-cycle phase one and five-cycle phase two, the tell speeding up.

**What the fallback needs from the engine** (priorities in `story/notes/designer-ch01.md`): a timed
interact window with an early/late/hit verdict; a hold-to-charge press with a meter; a knockback; a
per-character stamina value that regenerates while walking; and a party passive that shifts the
prompt's timing. All five are small beside a battle system, and all five are reused by it.

## 11. UI, minimum

- **One slider** under the chosen command, 5 notches, remembering its last setting per command per
  character, showing the stamina cost and the resulting magnitude before you commit.
- **A stamina bar** under every portrait, and — once {{HERDER}} is in the party — under every enemy.
- **A tell indicator** on the enemy that is telegraphing, unmistakable at a glance and audible.
- A skill below its minimum effort is **greyed with its minimum shown**, never hidden. So is a
  command the character cannot currently pay for. **Guard is never greyed** — it is the one entry
  that is always lit, and on a tired character it is the only one, which is §4a's fourth carrier and
  the closest thing to a hint the chapter contains.
- An idea that has not been introduced yet is **absent** from the list, not greyed (§7a). The list
  grows: one entry, then two, then the slider under them.
- **A goal line**, six words or fewer, settable per trigger, and never two lines at once.

## 12. Later, not chapter one

Effort ceilings rising with growth; a fourth and fifth party member; enemies that punish guarding;
equipment that changes the effort curve rather than adding numbers; a skill whose minimum effort is
5; the reason magic only does light work.

---

## What this changes for the writer

1. **{{MENTOR}}'s line in C3 is about effort, not about hitting.** The owner's own example:
   *"You hit everything as hard as you can."* That is the chapter's thesis said once, in four words
   more than Hart normally uses, and the player has felt it for eight minutes by then.
2. **{{HEALER}} is not limited and never was** (owner, 2026-09-21: "not being strong enough has been
   taken far too literally — that was when she was young… it doesn't have any bearing now"). She is
   a capable healer today, an apprentice in standing only. The patch-up in the yard is quick, easy
   and routine, done while she is teasing him. **No line about not being strong enough, no sitting
   down, no ceiling.** The world's "light work only" rule applies to everybody equally.
3. **{{HERDER}}'s failure in phase one is hers to be hurt by.** The calming does not fail because she
   is careless; it fails after she has spent nearly everything she has, on screen.
4. **Nobody says "stamina", "effort" or "magnitude" in dialogue** — and now nobody says the
   *content* of them either. **Struck (owner, 2026-09-21):** Ottilie's calls from the wall,
   including *you're swinging too hard* and *you've got nothing left*. She does not coach, hint,
   call a direction or name the sound. She is enjoying the show: she winces, she counts falls, she
   goes quiet for one beat **after** he turns it aside. {{MENTOR}} says *"Again."* The parry is
   taught by §4a and by nothing else, and no line of dialogue anywhere in chapter one may describe
   what the player has just seen or done.
5. **The machines are named, never numbered** (§7): the post, the arm that holds, the swing. There
   are three.
