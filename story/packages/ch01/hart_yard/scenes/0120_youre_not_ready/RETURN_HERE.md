# 0120_youre_not_ready — shot sheet — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/ch01/hart_yard/scenes/0120_youre_not_ready/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/ch01/hart_yard/scenes/0120_youre_not_ready
```

That cuts the image into:

- `story/panels/0120_youre_not_ready_p1_establishing_wide.png`
- `story/panels/0120_youre_not_ready_p2_low_angle_menace.png`
- `story/panels/0120_youre_not_ready_p3_object_insert.png`
- `story/panels/0120_youre_not_ready_p4_profile_flat.png`
- `story/panels/0120_youre_not_ready_p5_full_body_reveal.png`
- `story/panels/0120_youre_not_ready_p6_high_angle_down.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
