# characters.md — the current cast

One `## Name` per character. The `look` line is pasted **verbatim** into every image
prompt that includes the character, so keep it concrete and visual: build, hair, outfit
colors, one distinguishing feature. Change it only when the character's design changes.
`ref` is the character's reference image for ChatGPT (make it with
`./story_prompt.py refsheet <Name>`, save the result at that path). `status` and `notes` are story
state for whoever writes the next scene; they are never sent to the image model. `people` and
`voice` are for writers too; the tool ignores them.

**This file is chapter one's cast and nothing else.** The fifty-odd entries of the retired *Fair
Copy* draft went with it on 2026-09-21 (`story/DECISIONS.md`, D24); the `legacy-final` git tag holds
them if a later chapter wants one back. Add a character when a chapter needs them, not before.

The `## Name` is the **handle**: the one word a scene writer types in a panel description and as a
dialogue speaker. The tool matches handles case-insensitively as whole words inside panel text, so
handles are never common English words (that is why the guild clerk is `## Guildclerk` and not
`## Clerk`: four scenes describe "a clerk" in a panel). A character known on screen by another name
takes an `- alias:` line, which may be used as a dialogue speaker and is what the game prints.

**Design direction (owner's call):** the cast should look like the heroes of a 1993 sci-fantasy
anime JRPG (Phantasy Star IV is the touchstone), never like Western tabletop fantasy. So:
- Keep race and class words (**dwarf, halfling, elf, fighter, rogue, cleric, wizard**) and beards out
  of `look`. They drag image models toward braided beards, gritty muscle, and Tolkien armor. `role`
  keeps the story job; `look` describes only what is visible. `story_prompt.py` rejects them.
- Young adults with big expressive eyes and big hair; build comes from silhouette (short and compact,
  tall and slim, petite), not from age, bulk, or facial hair. **Age reads through posture, hair, and
  dress** — a straight back, a combed-flat parting, a coat kept for best — never through wrinkles.
- Costume vocabulary: high collars, bodysuits, long coats, half-capes, a single oversized shoulder
  plate, headbands, circlets, sashes, gauntlets, boots. One strong accent color per character.
- Original designs only. Borrow the era's archetypes, never a specific existing character.

### The four peoples, by what is visible

None of them is a species and none of them gets a fantasy word. Write only what an artist can draw.
To make a person unmistakably of their people, the `look` line names the marked features in this
order: build and height, skin, hair, ears, eyes.

- **Humans.** The baseline. Brown, tan and pale skins, ordinary black, brown, blonde and grey hair,
  round-topped ears, whites to the eyes. What marks a human in a crowd is **cloth**: they layer for
  cold, so a human on a winter street is three garments deep where the person beside them is in one.
- **Kell.** Very short and very wide — head and shoulders below a human, half again as broad through
  the shoulders. Clay-warm skin with a faint reddish undertone. Thick hair in one flat metal colour
  (copper, brass, iron-grey, rarely gold) that stands straight up and never lies flat. Amber eyes
  with a bright ring at the pupil. Broad flat fingernails. They run hot, so they are **bare-armed in
  any weather**: sleeveless coats, cut-off sleeves, no gloves unless the job needs them.
- **Vess.** Very tall and very thin, narrow through the shoulder. Ash-lilac skin with a grey cast.
  **Long backswept fluted ears with visible ribbing** that lamplight shows through. **Eyes in one
  solid colour with no whites at all** — violet, slate, amber-brown. Hair like drawn wire in cold
  indigos, ash-violets and silvers, worn long and pinned with tools or bone pins. Long hands.
- **Onn.** Grown, not born, and identical in build at any age: five foot six to five foot ten,
  narrow, upright. **Matte skin in one flat colour per individual** — bone-white, slate, ochre, dull
  green — with no flush and no variation. **No hair anywhere. No nose: two small slits above the
  mouth.** Large lensed eyes with a visible bright iris ring. **Four small lights set in a vertical
  row down each temple, eight in all**, and the `look` line always says how many are still lit. Lights
  only ever go out, never come back.

---

## The valley — chapter one

The whole cast of *The Last Job Sheet*. Handles are the names in `story/v3/NAMES.md`, so a scene
written in tokens resolves straight onto them: `{{MENTOR}}` reads Hart, `{{HERDER}}` reads Distel.
`{{HERO}}` and `{{HEALER}}` keep their old handles `Bron` and `Lyra` with the current names on
`- alias:` lines — renaming a handle would break every panel that already names them.

## Bron
- role: hunter of the Even Hand, axe, party leader
- alias: Falke
- ref: story/refs/bron.png
- people: Kell
- voice: Short sentences, concrete nouns, the thing first and the reason after; no irony and no idea how funny he is.
- look: Bron, a short, compact, sturdy boy of seventeen with a youthful clean-shaven face and an eager open expression, big spiky forest-green hair swept back under a plain white headband with long trailing ends, bright amber eyes, a cream long-sleeved shirt with the sleeves rolled to the elbow under a sleeveless quilted ochre training vest fastened with two toggles, a strip of white bandage wrapped round his right wrist, plain brown leather bracers, grey-green trousers, a brown belt with a small pouch, worn brown boots with turned-down tops.
- look_v3_day_one: Falke, exactly as above, with a plain wooden practice stick in one hand. Use this line for every chapter-one panel before {{MENTOR}} hands him the sword; after that a panel may put a plain straight sword across his back, but the `look` line never carries a weapon.
- status: alive, party leader, ch.1-15
- notes: Bron Sallow. Born Tellwater 1084; carried out of the street at nine and given stock template 4 plus two live sleeves drawn from Maren. Believes only what he can put his hands on, which is the one thing that was done to him. Ch.10 he learns his own past was erased, not stored. Ch.11 he takes Lyra's last hour. **The `look` line was redrawn on the owner's note of 2026-09-21** — green hair, no armour, no weapon, seventeen rather than early twenties, because the old one read as barbarian and the portraits must carry no weapon. The old ref art is superseded; regenerate the reference sheet.

## Lyra
- role: apprentice healer of {{HOME_TOWN}}, mace; in v3, {{HEALER}}
- alias: Ottilie
- ref: story/refs/lyra.png
- people: human
- voice: Complete unhurried sentences; teases the way you tease somebody you have known since he was small, corrects the number rather than the point, says the frightening thing plainly and quietly, and never signals a joke.
- look: Lyra, a tall, slender young woman with very long straight pale-gold hair, long bangs parted in the center, a thin gold circlet with a small red gem on her forehead, calm grey eyes, a white high-collared long coat with gold trim and wide gold-edged shoulder pieces over a dark navy bodysuit, a short white half-cape, a gold sun medallion on her chest, long white gloves, white heeled boots.
- status: alive ch.1-11; shot in the road at Tellwater in ch.11 and dies there
- notes: Sister Lyra, of the Order of the Late Hour, posted to Sallowgate in 1106. Sits the last hour of the dying and asks the Tally, the Order's fifteen questions — which are a strand-integrity check written by the people who built the Seam, a fact she learns in ch.9 and decides makes the prayer better. The gold sun medallion is the Order's hour-token, marked with the fifteen points. **The v3 `look` line above carries no weapon** (owner, 2026-09-21: nothing held or slung in a portrait); the mace stays in the prose and in her hands in a panel when a panel needs it. **In v3 this entry is {{HEALER}}, Ottilie**, and nothing above the alias line carries over except the art. She is twenty-three, an *apprentice* healer under {{TOWN_HEALER}}, and lives in the house next door to {{MENTOR}}'s — alone in it since she was seven, when a fever took both her parents in one winter. **That is backstory and not a present-day limit** (owner, 2026-09-21): today she is capable, quick and entirely reliable, and her apprenticeship is a formality — {{TOWN_HEALER}} wants her to work a season outside the valley before she hands over full standing. Not {{MENTOR}}'s student: the neighbour who sits on the wall between the two yards, keeps score of {{HERO}}'s beatings out loud because they are the best show in {{HOME_TOWN}}, patches him afterwards and eats at {{MENTOR}}'s table most nights. **She does not know how the machines are beaten and has never cared to work it out** (owner, 2026-09-21: she is not {{HERO}}'s coach and it is not her domain) — she rates the falls, she has an apple, and the parry is {{HERO}}'s own. She is better with the mace than she admits. **Her speech is loose** — contractions always, warm, teasing, never formal; she is the least formal person in the chapter. Her light magic closes a wrist quickly and easily while she is teasing the patient, which is the whole of what the player is shown about magic in chapter one: magic is ordinary and it does light work. She wants people she chooses to be her family and never says so: it is shown by her being at {{MENTOR}}'s table every night, by her packing before she is asked, and by her taking Distel along at the end without asking anyone. In v3 she is alive through chapter one and goes up to the {{HIGH_PASTURE}} with {{HERO}}.

## Hart
- role: the old hunter who taught {{HERO}}; the town's engineer, builds the training machines, bad knee
- ref: story/refs/hart.png
- people: human
- voice: Orders and grumbles, never a question. Fragments with the subject dropped. Four words where most people use twelve. Says "again" and means do it again, and shows care by making a thing and handing it over rather than by saying so.
- look: Hart, a stocky broad-shouldered man of sixty with a straight back and a stiff left leg, iron-grey hair cropped short and pushed about, deep-set brown eyes and a clean-shaven square jaw, wire spectacles pushed up on his forehead, a hinged brace of dark leather and iron strapped over his left knee, a scorched leather apron with a folding rule and two pencils in the chest pocket over a faded slate-blue work coat with the sleeves rolled to the elbow, big worked hands with one finger bandaged, patched canvas trousers, heavy laced boots, a plain hunter's knife on the belt.
- status: alive, ch.1; stays in {{HOME_TOWN}} when the party walks up to the {{HIGH_PASTURE}}
- notes: {{MENTOR}} in `story/v3/NAMES.md`. Hunted until the road broke his knee, and **the knee made him an engineer.** His part in chapter one ends at the sword: he trains {{HERO}}, he will not explain the machine he built, he gives him the first sword when he finally parries it, and he does not believe a word of the shepherd's job sheet — tracks do not stop, so somebody miscounted sheep. He does not leave {{HOME_TOWN}}. He builds with wood, iron, rope and counterweights and nothing else: the town's well winch, the bell works and its axles, and the four training machines in his own yard, one for each thing a hunter has to learn. He is at a workbench in half the scenes he is in and will not stop working to argue. The knee brace is his own work and is the centrepiece of the design; the knife on the belt is the one thing on him he did not make — it was his partner's. Age is in the straight back and the kept-for-work coat, never in the face.

## Distel
- role: night herder, fourteen; tracking her own guard animal; third member of the party
- ref: story/refs/distel.png
- people: Mohn (the night herders)
- voice: Blunt and literal. Answers the question that was actually asked and stops. States her own feelings as plain facts and keeps walking. No greetings, no thanks, no hedging, no idiom — she has never talked to a human before today.
- look: Distel, a Mohn, one of a small furred nocturnal people and plainly not human: a slight girl of fourteen who stands a head shorter than a grown woman, her whole face and body covered in short dense dusk-grey fur with a pale cream mask around the eyes and down the throat, a short blunt muzzle with a small dark nose and no human lips, huge round black eyes with a thin pale gold ring and no whites at all, large upright leaf-shaped ears tipped with charcoal tufts that stand well above her head, a shaggy crown of charcoal head-fur with two long thin braids hanging in front of the ears bound with bone beads, three-fingered furred hands with dark pads, a short tufted tail, a hooded sleeveless overdress of dusty violet felt with a wide folded collar, a broad woven belt of red and cream bands carrying a short curved horn and a coiled lead, loose grey leggings, bare furred feet with dark pads.
- status: alive, joins the party at the end of ch.1
- notes: {{HERDER}} in `story/v3/NAMES.md`; one of the {{HERDER_PEOPLE_PL}}. Her people herd at night, up above the tree line, and are known to the world by travellers' talk and sayings — {{HOME_TOWN}} has simply never had one walk in. She raised {{BEAST_NAME}}, a {{GUARD_BEAST}}, from a pup, and came down the mountain alone after he ran. She is **nocturnal**: half asleep and slow in daylight, sharpest after dark, and she sees in it. Her light magic puts an animal to sleep and calms it, which is all it does. She does not want {{BEAST_NAME}} killed, and at the end of chapter one {{HERO}} kills him to save her, so she is saved and bereaved in the same minute and is not ready to be grateful. She goes on with them because a {{GUARD_BEAST}} does not forget its handler, so something was done to this one, and if this one then others. **Gender is plain:** she is a girl and is called "she" on screen from her second line, because the player must be able to hold her.

## Garbe
- role: shepherd of the {{HIGH_PASTURE}} above {{HOME_TOWN}}; put up the last job sheet
- ref: story/refs/garbe.png
- people: human
- voice: Practical and unimpressed, gives you the number before the story, and has told this to four people already today.
- look: Garbe, a wide-shouldered woman in her forties with weather-reddened brown skin and pale creases at the eyes, dark hair scraped back hard into a short tail, heavy straight brows, a long sleeveless coat of oiled brown canvas over a high-necked cream shirt with the sleeves pushed past the elbow, a wide leather belt with a tally stick and a folded knife, thick green trousers, mud to the knee, laced boots, a coil of rope over one shoulder.
- status: alive, ch.1
- notes: {{SHEPHERD}} in `story/v3/NAMES.md`. Her animals go from the {{HIGH_PASTURE}} at night and turn up days later miles away, asleep and not wakeable, and the tracks stop in open grass. The older hunters laughed at the job sheet and she knows exactly why: it sounds like a tall tale and it pays four coin. She is not frightened and not grateful — she wants her sheep back and her job sheet taken down. She pays {{HERO}} his first coin as a hunter at the end of the chapter and does not make a speech about it.

## Stolz
- role: hunter, nineteen, signed with the lord of the western city
- ref: story/refs/stolz.png
- people: human
- voice: Gloats and asks mocking questions. Easy long sentences, a number in every speech, and he compliments you while he beats you.
- look: Stolz, a tall lean young man of nineteen with a loose easy stance, tan skin, big swept-back honey-blond hair with a hard shine band and one strand hanging over the brow, pale green eyes and a half-smile, a new ankle-length coat of deep wine red with a high black collar and a gold cord looping the right shoulder, a black high-necked bodysuit, black gloves, a fat purse on a wide belt, tall polished boots, a punched gold coin hung at his throat on a black cord.
- status: alive, ch.1; rides west in the lord's cart
- notes: {{RIVAL}} in `story/v3/NAMES.md`. Better than {{HERO}} and right about it. Signed the coast escort an hour before the bell, sold it on to another hunter and took the western city's eighty coin a week the same morning. The coat, the purse and the coin at his throat are all new and all paid for by the lord: the design says *this man got paid this week* before he opens his mouth. Never cruel to your face.

## Guildclerk
- role: the guild hall clerk of {{HOME_TOWN}}; keeps the board and the ledger
- alias: Clerk
- ref: story/refs/guildclerk.png
- people: human
- voice: Recites the rule, then the fact, then what follows. No contractions and no opinions; starts the sentence again from the beginning when he is cut off.
- look: Guildclerk, a slight upright young man with mousy brown hair combed flat in a hard side parting, small round grey eyes, a narrow clean-shaven face, a high-collared charcoal tunic buttoned to the throat over a white shirt with cuffed sleeves, a plain grey sash, ink on the first two fingers of the right hand, a pen behind one ear, flat black shoes.
- status: alive, ch.1
- notes: A role-only speaker with no token: written `CLERK` in `story/v3/chapter01.md`, and the speaker label in a scene file is the alias **Clerk**, which is what the game prints. The handle is `Guildclerk` because a handle of `Clerk` is matched as a whole word inside panel text and four older scenes describe "a clerk" in a panel — see `story/notes/scene-agent.md`. He is not an obstacle and not a joke; he is the only person in the hall who knows what the rules are, and he says them in order.
