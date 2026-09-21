# 0150_the_sword — shot sheet — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/ch01/scenes/0150_the_sword/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/ch01/scenes/0150_the_sword
```

That cuts the image into:

- `story/panels/0150_the_sword_p1_establishing_wide.png`
- `story/panels/0150_the_sword_p2_over_shoulder.png`
- `story/panels/0150_the_sword_p3_eyes_slit.png`
- `story/panels/0150_the_sword_p4_object_insert.png`
- `story/panels/0150_the_sword_p5_impact.png`
- `story/panels/0150_the_sword_p6_profile_flat.png`
- `story/panels/0150_the_sword_p7_full_body_reveal.png`
- `story/panels/0150_the_sword_p8_two_shot.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
