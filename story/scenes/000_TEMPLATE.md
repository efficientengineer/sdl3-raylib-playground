# Scene 000: Title

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

## Dialogue
- Name [1] {tense}: line. [n] = panel revealed with this line (optional; default: next panel).
  {mood} = music mood from here on: wonder, dread, tense, confront, sorrow, hope (optional; default: keep)
- Name: drawn by the game, never sent to the image model
