# NAMES — the token table

Naming style: plain German words used as names, the way Frieren does it (Stark, Fern, Himmel), plus
real old German given names (Ottilie). Avoid words Frieren already uses. Meanings: Falke = falcon, Bleibe = a place to stay, Wagen = cart,
Ottilie = given name, Hart = hard, Stolz = pride, Frage = question, Elster = magpie, Durst = thirst,
Halm = grain stalk, Rabe = raven, Linde = lime tree.

Every proper noun in the v3 story text is a token in double braces, including speaker labels.
Renaming anything is a one-line edit here. Tokens are UPPER_SNAKE.

| Token | Current name | What it is |
|---|---|---|
| `{{HERO}}` | Falke | Hunter's apprentice, seventeen. Given his first sword by his teacher. Has an introduction he doesn't know about. |
| `{{HEALER}}` | Ottilie | Apprentice healer, twenty-three. Lives next door to {{MENTOR}}, alone since she was nine. Wants people she chooses to be family. |
| `{{FATHER}}` | Rabe | {{HERO}}'s father. Hunter, carried the water out with {{MENTOR}} for twenty years, died on that road eleven years ago. Never appears on screen. |
| `{{SCHOLAR}}` | Frage | One of eight people alive who read any of the ancients' language, and the worst of them. Joins chapter two. |
| `{{THIEF}}` | Elster | Thief, small, loud. Sends money home to a sister a caretaker keeps alive. Joins chapter two. |
| `{{MENTOR}}` | Hart | The old hunter, now the town's engineer. Bad knee, a workshop, a yard full of training dummies he built. |
| `{{RIVAL}}` | Stolz | Nineteen, better, right about it. Takes the lord's coin in chapter one. |
| `{{VILLAIN}}` | Durst | The young lord in the west. Bought a caretaker, had the {{SECOND_MOON}} moved, and is walking to {{THE_DOOR}}. |
| `{{VILLAGER_1}}` | Bleibe | Woman by the square in {{HOME_TOWN}}. Her sister went west; she stayed. Minor. |
| `{{VILLAGER_2}}` | Wagen | Kept the carters' horses in {{HOME_TOWN}} and has no work now the convoy is gone. Minor. |
| `{{TOWN_HEALER}}` | Linde | The healer of {{HOME_TOWN}}, sixty. Took {{HEALER}} in at nine and has taught her since; decides when she gets full standing. |
| `{{HOME_TOWN}}` | Halm | Nine streets, a grain yard, a well. Where the hero starts. |
| `{{CARETAKER}}` | *(unnamed yet)* | One of the people the ancients made. Few left, all old. Took the water at the {{STAIR}} for generations; bought and taken west before chapter one. |
| `{{STAIR}}` | *(descriptive)* | A staircase the width of a town, down out of the cloud, stopping thirty feet above an empty field. The ancients' work nearest {{HOME_TOWN}}. |
| `{{SECOND_MOON}}` | *(descriptive)* | A moon that isn't a moon. Held one quarter of the sky for a thousand years until last spring. |
| `{{THE_DOOR}}` | *(descriptive)* | The mountain that is a door. The one place the ancients can be woken or overruled; the end of the pilgrimage. |
| `{{SEA_WALL}}` | *(descriptive)* | The edge where the sea stops, with dry seabed beyond it. |
| `{{COAST_ROAD}}` | coast road | The good job on the board the morning of the bell: escort work east to the coast. {{RIVAL}} signs it and sells it on. |

## Creatures and items

Common words, not spent names (see Rules below): a creature or an item may be renamed without
costing the story one of its named things. Every creature has a **singular** row and a `_PL` row,
because the table has no plural mechanism — the text writes `a {{ROAD_CREATURE}}` and
`two {{ROAD_CREATURE_PL}}`. A value never carries an article. `BESTIARY.md` and `LOOT.md` are the
long descriptions.

| Token | Current name | What it is |
|---|---|---|
| `{{GRAIN_CREATURE}}` | lid | Grey disc on short legs that eats grain and rolls at you. The tutorial fight. |
| `{{GRAIN_CREATURE_PL}}` | lids | Plural of `{{GRAIN_CREATURE}}`. |
| `{{THIEF_CREATURE}}` | thumb | Yellow hopping lump that takes one item out of your pack and runs for the ditch. |
| `{{THIEF_CREATURE_PL}}` | thumbs | Plural of `{{THIEF_CREATURE}}`. |
| `{{AMBUSH_CREATURE}}` | milestone | Grey post standing among the real markers that falls on you once, hard. |
| `{{AMBUSH_CREATURE_PL}}` | milestones | Plural of `{{AMBUSH_CREATURE}}`. |
| `{{STALKER_CREATURE}}` | follower | Bone-white three-legged thing; the big one at the back mends the others until you kill it. |
| `{{STALKER_CREATURE_PL}}` | followers | Plural of `{{STALKER_CREATURE}}`. |
| `{{ROAD_CREATURE}}` | drape | Pale blue skin lying flat in the grass that comes up and wraps whoever walks over it. Was "sheet" until the guild's job sheets took the word. |
| `{{ROAD_CREATURE_PL}}` | drapes | Plural of `{{ROAD_CREATURE}}`. |
| `{{RUIN_CREATURE}}` | gong | Bronze dome that strikes itself; the note hits the whole party. |
| `{{RUIN_CREATURE_PL}}` | gongs | Plural of `{{RUIN_CREATURE}}`. |
| `{{CAVE_CREATURE}}` | knuckle | Boulder-sized fist in the hill caves. The optional fight of chapter one. |
| `{{CAVE_CREATURE_PL}}` | knuckles | Plural of `{{CAVE_CREATURE}}`. |
| `{{SHRINE_CREATURE}}` | sitter | Knee-high grey thing in a ring round the shrine ground. Cannot be fought and nothing crosses it. |
| `{{SHRINE_CREATURE_PL}}` | sitters | Plural of `{{SHRINE_CREATURE}}`. |
| `{{LOOT_TEACHING_FIND}}` | weight | Chapter one's teaching find, off the grain scales. Doubles one character's attack. |
| `{{LOOT_YARD}}` | brace | Tin under {{MENTOR}}'s eaves. The wearer cannot be held, wrapped or knocked down. |
| `{{LOOT_ROAD}}` | tooth | In the {{THIEF_CREATURE_PL}}' hoard under the broken culvert. Critical chance. |
| `{{LOOT_SECOND_SWORD}}` | second sword | Off the {{CAVE_CREATURE}} in the cave. A plain sword that cannot break. |
| `{{LOOT_SHRINE}}` | green pebble | Under one of the {{SHRINE_CREATURE_PL}}. Fleeing always succeeds. |

## Reserved, currently described rather than named

These have no proper name and should stay that way. If one ever gets named, add the token here and
tokenize it in the text in the same pass.

| Token | Current text | Note |
|---|---|---|
| `{{ANCIENTS}}` | ancients | The people who built the four great works and then stopped. |
| `{{GUILD}}` | guild | Hunters' guild. Job board, ledger, guarantors. |
| `{{DRY_CITY}}` | dry city in the west | The villain's city. |

## Voices

Voice is energy before it is vocabulary, and **punctuation carries the energy**. Three habits, one
plain sample, one loud sample. Every sample states a fact. Voice changes how a thing is said, never
whether it is said. People interrupt each other with a dash.

**{{HERO}}** — *Energy: blurts and asks.* Short bursts, contractions always, and he says what he
feels the second he feels it. Questions when he is behind, exclamations when he is surprised, and he
repeats the fact he just heard back as a question before he reacts to it. Never asks for pity.
> plain: "He walked out here every year for thirty years and sat under this step."
> loud: "Never once? Not one job, ever?"

**{{HEALER}}** — *Energy: easy, teasing, precise questions, rare exclamations that land.* The older
kid next door: relaxed, warm, never condescending, and she teases {{HERO}} the way you tease someone
you have known since he was small. Complete sentences, few contractions when she is correcting
somebody, and she corrects the number or the date rather than the point. Talks about what she will
have to patch up afterwards. Never raises her voice twice in a scene, and never says what she wants.
> plain: "He left it the year he died, then. Eleven years under a rock."
> loud: "Do you know he has never once asked me to come anywhere?"

**{{MENTOR}}** — *Energy: orders and grumbles.* Never asks a question. Fragments with the subject
dropped off the front. Four or five words where most people use twelve. Says "again" and means do it
again. Shows care by making things and by handing them over, never by saying so. Never says goodbye
and never says he is worried about you.
> plain: "Built that one to swing back. It swung back."
> loud: "Again. From the gate this time."

**{{RIVAL}}** — *Energy: gloats, and asks mocking questions.* Easy long sentences, contractions, and
a number in every speech: what he was paid, what it cost, what he has that you do not. Compliments
you while beating you and never insults you to your face.
> plain: "Eighty coin a week. I signed with him the same morning."
> loud: "Did you think he'd sign for you?"

**CLERK** — *Energy: recites, and gets flustered when interrupted.* States the rule, then the fact,
then what follows. No contractions and no opinions. Says "the rule is" the way other people say
"well". When cut off he starts the sentence again from the beginning.
> plain: "The rule is that every job needs a hunter who will vouch for you."
> loud: "Never once. Now — there is one job left."

**{{SCHOLAR}}** *(chapter two)* — *Energy: exclaims at everything, three questions in a row.* Says it
once for the room and once for the record. Given any subject he tells you who first wrote it down.
Never says "I don't know" without immediately saying what he does know.
> plain: "Eight people alive read any of their language. I am the eighth best of the eight."
> loud: "Who measured it? When? Did they write the number down anywhere?"

**{{THIEF}}** *(chapter two)* — *Energy: interrupts, teases, exclaims.* Fast and short, contractions
always, jokes by understatement. Given anything she tells you what it is worth and who is watching it.
Never says thank you.
> plain: "Someone's been here. Cart tracks, a day old."
> loud: "That rope's worth more than the water and you're leaving it!"

## Rename note

The year's offering was called "the bread" and "the loaf" in earlier drafts; it is now a sealed jar of
water from {{HOME_TOWN}}'s own well, carried unopened. The father's cloth under the shelf was cut.
Grep the v3 files for "bread", "loaf" and "cloth" and expect zero hits outside this note.

## Rules

- Named **people and places** are the scarce thing: eighteen of them exist, and the two minor
  villagers are the only ones that may be cut without a rewrite. Adding a nineteenth is a decision,
  not a convenience. {{FATHER}} and {{TOWN_HEALER}} were added in the chapter-one first draft
  (2026-09-20) because a player cannot hold a person with no name and both are load-bearing.
- **Creature and item rows are not named things.** They are common words the valley uses, tokenised
  so that a rename is one line — which is how "sheet" became {{ROAD_CREATURE}} when the guild's job
  sheets took the word.
- Speaker labels are tokens too: `{{HERO}}: line`, never `BRON: line`.
- Role-only speakers stay plain English and get no token: `CLERK`, `SOLDIER`, `BAKER`.
- After any rename, grep the v3 files for the retired name and expect zero hits.
