# WRITER BRIEF — for every scene writer

You turn one chapter of the outline into finished scene files. The owner is away; an orchestrator
merges your work. Another writer may be working on a different chapter at the same time.

## Read, in this order
1. `story/notes/BRIEF.md` — direction, scale, rules of the road.
2. `story/canon.md` — frozen facts, rulings, the fifteen questions, who knows what when. Canon beats
   everything else, including the bible and the outline.
3. `story/bible.md` — all of it once: the hard rules (section 5), your characters' voices and sample
   lines (sections 6, 9, 10, 11), the clue ladders (13), and the tone guide (14) matter most.
4. Your act's outline in `story/outline/` — your chapter's entries closely; skim the chapters either
   side so your first and last scenes join up.
5. `story/characters.md` (handles, voices, who exists) and `story/locations.md` (paste `location` lines
   from here; never invent a description for a place that has a key).
6. `story/STYLE.md` (rules R1-R11, shot menu, review checklist), `story/scenes/000_TEMPLATE.md`, and the
   worked examples `story/scenes/003_the_warning.md` (panel) and `story/scenes/003b_after_the_warning.md` (talk).
7. `story/DECISIONS.md` entry D9: three notes to all writers.
8. Any earlier chapters already written in `story/scenes/` that your chapter quotes (your assignment
   names them). Quote their wording exactly.

## What you write
- One file per outline entry, named exactly `<scene id>.md` in `story/scenes/` (for example
  `0405_the_ferry_rail.md`). Title line: `# Scene 0405: Title`.
- **PANEL scenes:** full format: `location`, `characters`, `mood`, `staging`, optional `dialogue_box`,
  `## Beat` (written for an artist: what is happening, what changed, what each person feels),
  `## Panels` with shot ids from the menu, `---` page breaks, an acting line for every cast member named
  in a panel (where the eyes point, the face, the body), and `## Dialogue` with `[n]` reveal tags and
  `{mood}` tags. 1-3 pages of 2-4 panels, at most 8 panels. Follow the outline's suggested images, but
  you are the director: fix anything that would not draw well or would break R3-R5.
- **TALK scenes:** `- type: talk`, `location`, `characters`, optional `backdrop: <scene_stem>:<panel_number>`
  pointing at a panel of a PANEL scene in your own chapter, `## Beat`, and `## Dialogue` with `{mood}`
  tags and no `[n]` tags.
- **NARRATION scenes:** `- type: narration`, `## Beat`, `## Dialogue` with the speaker `Narrator`.
- Only characters in `story/characters.md` may be NAMED in a panel description. Unnamed figures use the
  wording in its "Uniforms and recurring extras" section. If the outline needs someone who does not
  exist, write around it and list them in your notes file.

## Craft
- A text box holds about 120 characters. Most lines are far shorter. One thought per line.
- Every character sounds like themselves: use the `voice` lines and the bible's sample lines. Read each
  scene aloud in your head; if two characters could swap lines, rewrite.
- Subtext over statement. Nobody says what the scene means. Cut any line that explains the theme.
- Be funny where the outline allows. Let silences happen: a line can be `...` when that is the right line.
- Grief is shown by what people do. Villains are reasonable. The Seam never acts or intends.
- Plants must be light. If the outline says a clue is subtle, a first-time player should not notice it.
- Never reveal anything listed under "Things the player must NOT learn early" before its chapter.
- Do not pad. If the outline says 8 lines and the scene is done in 6, stop at 6.

## Validate
Run `./story_prompt.py check story/scenes/<your chapter prefix>*.md` until every file passes. For each
PANEL scene also run `./story_prompt.py sheet story/scenes/<file>.md` once and confirm it succeeds
(warnings about missing reference images are expected). Fix your scene, never the tool.

## Your lane
Create only your chapter's scene files and `story/notes/writer-chNN.md` (problems, characters or
location keys you needed and did not have, places you departed from the outline and why, lines you are
proud of, lines you are unsure of). Do not edit any other file unless your assignment says so. No git.
Do not run `fast_reload.sh`, `deploy.sh`, or `export`. End with a report of at most 200 words including
the count of files written and the check result.
