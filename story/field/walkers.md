# Field walkers — the one-off NPCs

`./story_prompt.py walker <Name>` draws the nine-frame walk sheet (rows S, side, N; columns stand,
step-A, step-B; the engine mirrors the side row). **A cast member's look comes from
`story/characters.md`, never from this file**, and their reference sheet is attached to the package
so the walker matches the portrait — that is the whole point of the handle and alias machinery. Run
`walker Bron`, or `walker <alias>` once the name in `story/v3/NAMES.md` has an `- alias:` line on its
entry in `characters.md`; the sprite is written under the name you typed, so `walker Falke` writes
`walkers/falke.png` with Bron's look.

This file is only for walkers with **no cast entry at all**: the townspeople of {{HOME_TOWN}} and the
town healer. An id here that a cast entry has taken over is ignored, and `packages` says so.

## villager_a
- look: a middle-aged woman in a faded blue kerchief, a long brown skirt and a grey shawl, a rolled bed roped across her back

## villager_b
- look: a broad older man in a rust-red coat with the sleeves rolled past the elbow, cropped grey hair, a grain sack over one shoulder

## linde
- look: a small round-shouldered woman of sixty in a moss-green overdress over a cream underlayer, white hair pinned up under a dark headscarf, a wide canvas apron with deep pockets, a satchel of stoppered bottles on a long strap
