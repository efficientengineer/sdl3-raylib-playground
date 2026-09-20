# playlist.md — what the game plays, in order

`./story_prompt.py export` reads this and writes `src/cutscene_data.h`. A panel scene is
skipped (with a warning) until every one of its panels exists in `story/panels/`, so it is
safe to list scenes whose art is not generated yet.

## intro

The v3 chapter one, in clip order: `story/v3/chapter01.md`. Seven clips, four of them panel scenes
and three field conversations. There is no narration prologue — the chapter opens cold at the board,
the way Phantasy Star IV opens on a job. Until the sheets are generated the four panel scenes are
skipped by `export` (warning each) and the intro plays as the three talk scenes; art priority is
0110, then 0120, then 0150, then 0170.

- 0110_the_board
- 0120_youre_not_ready
- 0130_he_signs
- 0140_the_sword
- 0150_the_road_west
- 0160_the_fire
- 0170_the_jar

## chapter01

*The Jar Run* — 8 scenes, one per clip of `story/v3/chapter01.md`. Maps in order: halm,
halm, hart_yard, hart_yard, hart_yard, bridge, ridge_camp, stair_shrine.

- 0105_the_wrist
- 0110_the_board
- 0120_youre_not_ready
- 0130_he_signs
- 0140_the_sword
- 0150_the_road_west
- 0160_the_fire
- 0170_the_jar

