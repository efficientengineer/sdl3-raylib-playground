# Scene 000: Title

Three scene types, set by the `- type:` line. This template shows a **panel** scene (the default,
omit the line): a manga page, `## Panels` plus `## Dialogue`, all composition rules apply.
`- type: narration` is text over black: `## Dialogue` only, no panels (see `p01_prologue.md`).
`- type: talk` is a Phantasy Star IV field conversation: `## Dialogue` only, no panels and no `[n]`
reveal tags, speaker portraits beside the dialogue box, and an optional
`- backdrop: <scene_stem>:<panel_number>` naming a panel from another scene to show dimmed behind it
(see `003b_after_the_warning.md`). Talk scenes need no art of their own.

- location: one visual sentence describing the place
- characters: Name, Name
- mood: one or two words
- staging: who stands on which side of the screen, facing which way, and where the thing they react to is. Fixed for the whole scene.
- dialogue_box: no

## Beat
One short paragraph: what is happening, what just changed, and what each character feels about
it. This IS sent to the image model as the situation, so write what an artist needs to know.

## Panels
1. shot_id | who, doing what, where (max 40 words, no style words, no text)
   - Name: where they look (a named target or a screen direction), expression, body language (max 25 words)
   - Name: ... one line for every cast member named in the panel
2. shot_id | ...
---
3. shot_id | a line of three dashes starts a new PAGE: the screen clears and these panels build a fresh page
4. shot_id | 2-4 panels per page, at most 8 panels per scene, numbered straight through

## Dialogue
- Name [1] {tense}: line. [n] = panel revealed with this line (optional; default: next panel).
  {mood} = music mood from here on: wonder, dread, tense, confront, sorrow, hope (optional; default: keep)
- Name: drawn by the game, never sent to the image model
