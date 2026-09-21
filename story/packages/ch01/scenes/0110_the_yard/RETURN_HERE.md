# 0110_the_yard — shot sheet — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/ch01/scenes/0110_the_yard/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/ch01/scenes/0110_the_yard
```

That cuts the image into:

- `story/panels/0110_the_yard_p1_establishing_wide.png`
- `story/panels/0110_the_yard_p2_portrait_inset.png`
- `story/panels/0110_the_yard_p3_impact.png`
- `story/panels/0110_the_yard_p4_low_angle_menace.png`
- `story/panels/0110_the_yard_p5_high_angle_down.png`
- `story/panels/0110_the_yard_p6_object_insert.png`
- `story/panels/0110_the_yard_p7_full_body_reveal.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
