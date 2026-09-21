# 0160_the_herder — shot sheet — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/ch01/scenes/0160_the_herder/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/ch01/scenes/0160_the_herder
```

That cuts the image into:

- `story/panels/0160_the_herder_p1_establishing_tall.png`
- `story/panels/0160_the_herder_p2_high_angle_down.png`
- `story/panels/0160_the_herder_p3_eyes_slit.png`
- `story/panels/0160_the_herder_p4_two_shot.png`
- `story/panels/0160_the_herder_p5_full_body_reveal.png`
- `story/panels/0160_the_herder_p6_portrait_inset.png`
- `story/panels/0160_the_herder_p7_over_shoulder.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
