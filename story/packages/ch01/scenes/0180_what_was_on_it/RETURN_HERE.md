# 0180_what_was_on_it — shot sheet — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/ch01/scenes/0180_what_was_on_it/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/ch01/scenes/0180_what_was_on_it
```

That cuts the image into:

- `story/panels/0180_what_was_on_it_p1_high_angle_down.png`
- `story/panels/0180_what_was_on_it_p2_object_insert.png`
- `story/panels/0180_what_was_on_it_p3_full_body_reveal.png`
- `story/panels/0180_what_was_on_it_p4_portrait_inset.png`
- `story/panels/0180_what_was_on_it_p5_establishing_tall.png`
- `story/panels/0180_what_was_on_it_p6_two_shot.png`
- `story/panels/0180_what_was_on_it_p7_object_insert.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
