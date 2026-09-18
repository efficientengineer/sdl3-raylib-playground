# 0150_the_road_west — shot sheet — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/ch01/bridge/scenes/0150_the_road_west/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/ch01/bridge/scenes/0150_the_road_west
```

That cuts the image into:

- `story/panels/0150_the_road_west_p1_establishing_wide.png`
- `story/panels/0150_the_road_west_p2_full_body_reveal.png`
- `story/panels/0150_the_road_west_p3_portrait_inset.png`
- `story/panels/0150_the_road_west_p4_over_shoulder.png`
- `story/panels/0150_the_road_west_p5_eyes_slit.png`
- `story/panels/0150_the_road_west_p6_low_angle_menace.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
