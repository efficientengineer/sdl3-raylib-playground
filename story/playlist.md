# playlist.md — what the game plays, in order

`./story_prompt.py export` reads this and writes `src/cutscene_data.h`. A panel scene is
skipped (with a warning) until every one of its panels exists in `story/panels/`, so it is
safe to list scenes whose art is not generated yet.

## intro

What plays on the phone **today**, in chapter order: every talk scene, plus the two panel scenes whose
sheets are already cut (`0110`, `0150`). Those two sheets are stale — they were drawn before the Hart
redesign and before {{HERO}} had a sword on his back — and are queued for regeneration; they are here
because a stale picture plays better than a black screen while the new sheets are made. `0101`, `0107` and `0170` are left out on purpose: `0101` and `0107` have no art at all and `0170` has three new
panels, so `export` would skip them and warn. Add each one to this list the moment its sheet is cut.

- 0102_the_errand
- 0103_the_counter
- 0104_the_table
- 0105_the_night
- 0110_the_board
- 0120_what_the_road_is
- 0130_asking_her
- 0150_the_road_west
- 0160_the_fire

## chapter01

*The Jar Run* — 12 scenes, one per clip of `story/v3/chapter01.md`. Five panel scenes (`0101`, `0107`,
`0110`, `0150`, `0170`) and seven talk scenes. Maps in order: hart_yard, hart_yard, halm, hart_yard,
hart_yard, hart_yard, halm, hart_yard, hart_yard, bridge, ridge_camp, stair_shrine.

- 0101_the_dummies
- 0102_the_errand
- 0103_the_counter
- 0104_the_table
- 0105_the_night
- 0107_the_sword
- 0110_the_board
- 0120_what_the_road_is
- 0130_asking_her
- 0150_the_road_west
- 0160_the_fire
- 0170_the_jar
