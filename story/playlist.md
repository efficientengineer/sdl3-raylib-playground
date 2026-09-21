# playlist.md — what the game plays, in order

`./story_prompt.py export` reads this and writes `src/cutscene_data.h`. A panel scene is
skipped (with a warning) until every one of its panels exists in `story/panels/`, so it is
safe to list scenes whose art is not generated yet.

## intro

What plays on the phone **today**. Chapter one was rewritten on 2026-09-21 (*The Last Job Sheet*) and
every panel scene's art is new and not yet generated, so `export` would skip the panel scenes and
warn. Until the sheets are cut, the intro is the two **talk** scenes, which need no art. Add each
panel scene to this list the moment its sheet is cut: `0110`, `0150`, `0160`, `0180`.

- 0130_the_counter
- 0140_supper

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
