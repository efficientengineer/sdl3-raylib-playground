# LOOT — hidden items, chapter one

Rewritten 2026-09-21 for the hunt on the high pasture. The west road's items are gone with the road.

**One hidden item per map, off the main path, missable on purpose — except the first**, which is the
teaching find and is meant to be got by anyone who pokes around at all. Every effect is a **rule
change or a multiplier**, never "+3 attack", and still worth having at level forty. Every name is a
token in `NAMES.md`.

| Ch | Map | Item | Exactly how it is reached | What it does | Why it lasts |
|---|---|---|---|---|---|
| 1 | Halm, the grain yard | **the {{LOOT_TEACHING_FIND}}** *(weight)* — **the teaching find** | Day one, on the errand. The yard's door is propped open and three {{GRAIN_CREATURE_PL}} are inside; the route through to the guild hall is the gap in the far wall. The lead weight off the grain scales is on the scale bench **one step the other way**, in plain sight, with a glint on it and an `!` prompt. Nobody points at it. Anyone who looks around at all gets it. | Use in a fight: one character's attack is **doubled for three rounds**. Once per fight, never consumed. | It multiplies whatever your attack is, so it is as good in the last dungeon as it is in the grain yard. |
| 1 | {{MENTOR}}'s yard | **the {{LOOT_YARD}}** *(brace)* | A flat tin nailed under the eaves. The ladder is against the workshop wall from the first minute of the game and can be climbed at any time — but the tin is only reachable by climbing to the workshop roof and **jumping the gap to the house roof**, which is about three cells: the engine's running jump clears it and a standing one does not. Best taken on the night of day one, while Hart is inside at supper. | The wearer **cannot be held, knocked down, or put to sleep.** | Sleep is the {{HERDER_PEOPLE_PL}}' whole magic and the {{GUARD_BEAST}}'s whole method. A player who found this has a different boss fight, and it answers every binding attack in the game after it. |
| 1 | {{HILL_PATH}} | **the {{LOOT_HILL}}** *(whistle)* | On the way up, the path switches back around a shoulder of rock. Off the outside of the bend there is a **drop of two cells onto a ledge** you can see from above and cannot see from the path. Drop onto it — the engine allows any drop onto walkable ground — and it is there, in an old fold wall. Getting back up is a ramp at the far end, so it costs nothing but nerve. | Once a fight, **one enemy skips its next turn.** No cost, no check, works on anything that takes turns. | A free round against anything, forever, including bosses. It is the "I need one more round" button and there is never a game where that is not worth a slot. |
| 1 | {{HIGH_PASTURE}} | **the {{LOOT_PASTURE}}** *(lens)* | Only findable **after {{HERDER}} joins**, and only in the dark. There is a jump-only shelf above the fold — the pasture's jump-only region, the same kind the engine already detects and logs — and with night sight on, the thing on it glints where nothing should glint. Reached by a running jump off the fold's wall. A player without the herder walks past a black hillside. | **Enemy tells are always shown, for the whole party, permanently** — and shown early, the way night sight shows them, even when {{HERDER}} is not in the party. | It makes the game's one distinctive mechanic reliable forever. It is the single strongest item in chapter one and it is behind the chapter's only real piece of platforming, in the dark, after the player has understood what a tell is. |

## Rules for later chapters

- One hidden item per map, one row in this table before it appears in a chapter file.
- **The teaching find happens once, in chapter one.** Everything after it is behind something: an
  encounter, a side turning, a drop or a jump, the dark, or an examine the game never asks for.
- The effect is a rule change or a multiplier. If it can be written "+3 attack", it is wrong.
- Plain names, common words, no invented vocabulary; every one a token in `NAMES.md`.
- Name the later payoff here and plan it. Do not foreshadow it in the chapter.
- **Use the engine's own shapes**: the jump-only regions `vx_build_nav` already finds and logs are
  reserved for exactly this. Chapter one uses a jump (the eaves), a drop (the hill path), and a
  jump-only region in the dark (the pasture) — one of each, so the player learns the whole vocabulary.
