# Field walkers — the one-off NPCs

`./story_prompt.py walker <Name>` draws a 4x4 walk sheet (rows S, W, E, N; columns stand,
step-left, stand, step-right; 32x48 a frame). **A cast member's look comes from
`story/characters.md`, never from this file**, and their reference sheet is attached to the package
so the walker matches the portrait — that is the whole point of the handle and alias machinery. Run
`walker Bron`, or `walker <alias>` once the name in `story/v3/NAMES.md` has an `- alias:` line on its
entry in `characters.md`; the sprite is written under the name you typed, so `walker Falke` writes
`walkers/falke.png` with Bron's look.

This file is only for walkers with **no cast entry at all**: the villagers walking out of Halm. One
`## <id>` per walker, with a `- look:` line in the same voice as `characters.md` — what is visible,
no race, class or beard words, no style words. The guild clerk used to live here and does not any
more: he is `## Guildclerk` in `characters.md` with `- alias: Clerk`, so his walker comes from his
reference sheet like any other cast member's. An id here that a cast entry has taken over is ignored,
and `packages` says so.

## villager_a
- look: a middle-aged woman in a faded blue kerchief, a long brown skirt and a grey shawl, a rolled bed roped across her back

## villager_b
- look: a broad older man in a rust-red coat with the sleeves rolled past the elbow, cropped grey hair, a grain sack over one shoulder
