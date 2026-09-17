# playlist.md — what the game plays, in order

`./story_prompt.py export` reads this and writes `src/cutscene_data.h`. A panel scene is
skipped (with a warning) until every one of its panels exists in `story/panels/`, so it is
safe to list scenes whose art is not generated yet.

## intro
- p01_prologue
- 001_sealed_door
- 002_the_guard
- 003_the_warning
