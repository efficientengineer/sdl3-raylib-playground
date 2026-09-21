# playlist.md — what the game plays, in order

`./story_prompt.py export` reads this and writes `src/cutscene_data.h`. **The game plays
chapters**: New Game runs the first `## chapterNN` list from its first scene. There is no `## intro`
list any more — the chapter is the intro.

A panel scene with no art yet is **exported anyway**, every panel a placeholder box carrying that
panel's one-line description, so a chapter plays end to end while its shot sheets are being drawn.
`export` warns about each one; it never drops a scene.

This file is also the tool's definition of what counts: a scene file in `story/scenes/` that no list
here names is a draft — no package, no count, no export — and a `scene` trigger in a `.tmap` may
only name a scene listed here.

## chapter01

*The Last Job Sheet* — six clips, matching the six slots in `story/v3/ch01_room/flow.md`. Four panel
scenes (`0110`, `0150`, `0160`, `0180`) and two talk scenes (`0130`, `0140`). Everything else the
chapter used to say in a box is now a field line in `story/field/text.md`. Maps in order: hart_yard,
halm, hart_yard, hart_yard, high_pasture, high_pasture.

- 0110_the_yard
- 0130_the_counter
- 0140_supper
- 0150_the_sword
- 0160_the_herder
- 0180_what_was_on_it
