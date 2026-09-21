# NAMES — the token table

Naming style: plain German words used as names, the way Frieren does it (Stark, Fern, Himmel), plus
real old German given names (Ottilie). Avoid words Frieren already uses. Meanings: Falke = falcon, Bleibe = a place to stay, Wagen = cart,
Ottilie = given name, Hart = hard, Stolz = pride, Frage = question, Elster = magpie, Durst = thirst,
Halm = grain stalk, Rabe = raven, Linde = lime tree.

Every proper noun in the v3 story text is a token in double braces, including speaker labels.
Renaming anything is a one-line edit here. Tokens are UPPER_SNAKE.

| Token | Current name | What it is |
|---|---|---|
| `{{HERO}}` | Falke | Hunter's apprentice, seventeen. Loses to his teacher's last training machine until he stops swinging at it. Given his first sword by his teacher, which is what lets him sign for a job. |
| `{{HEALER}}` | Ottilie | Apprentice healer, twenty-three. Lives next door to {{MENTOR}}, alone since she was nine. Wants people she chooses to be family. |
| `{{FATHER}}` | Rabe | {{HERO}}'s father. Hunter, carried the water out with {{MENTOR}} for twenty years, died on that road eleven years ago. Never appears on screen. |
| `{{SCHOLAR}}` | Frage | One of eight people alive who read any of the ancients' language, and the worst of them. Joins chapter two. |
| `{{THIEF}}` | Elster | Thief, small, loud. Sends money home to a sister a caretaker keeps alive. Joins chapter two. |
| `{{MENTOR}}` | Hart | The old hunter, now the town's engineer. Bad knee, a workshop, a yard full of training machines he built. His part in chapter one ends at the sword. |
| `{{RIVAL}}` | Stolz | Nineteen, better, right about it. Takes the lord's coin in chapter one. |
| `{{VILLAIN}}` | Durst | The young lord in the west. Bought a caretaker, had the {{SECOND_MOON}} moved, and is walking to {{THE_DOOR}}. |
| `{{VILLAGER_1}}` | Bleibe | Woman by the square in {{HOME_TOWN}}. Her sister went west; she stayed. Minor. |
| `{{VILLAGER_2}}` | Wagen | Kept the carters' horses in {{HOME_TOWN}} and has no work now the convoy is gone. Minor. |
| `{{TOWN_HEALER}}` | Linde | The healer of {{HOME_TOWN}}, sixty. Took {{HEALER}} in at nine and has taught her since; decides when she gets full standing. |
| `{{HERDER}}` | Distel | The young night herder, fourteen. Raised {{BEAST_NAME}} from a pup and came down the mountain after him alone. Joins the party at the end of chapter one. |
| `{{HERDER_PEOPLE}}` | Mohn | One of the night herding people of the high country: small, large-eyed, awake at night. Known to the world; never seen in {{HOME_TOWN}} until now. |
| `{{HERDER_PEOPLE_PL}}` | Mohnen | Plural of `{{HERDER_PEOPLE}}`. |
| `{{SHEPHERD}}` | Garbe | Keeps sheep on the high pasture above {{HOME_TOWN}}. Put up the job sheet no one would take, and pays {{HERO}} his first coin. |
| `{{HIGH_PASTURE}}` | *(descriptive)* | high pasture — the grazing ground on the hill above {{HOME_TOWN}}, an hour's walk up past the last wall. |
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
| `{{GUARD_BEAST}}` | drover | The {{HERDER_PEOPLE_PL}}' guard animal: shaggy, long-backed, walks a flock together and puts it to sleep. Chapter one's boss is one gone wrong. |
| `{{GUARD_BEAST_PL}}` | drovers | Plural of `{{GUARD_BEAST}}`. |
| `{{BEAST_NAME}}` | Klee | {{HERDER}}'s own {{GUARD_BEAST}}, raised from a pup. The one she is tracking. |
| `{{HILL_CREATURE}}` | burr | Bristled ball the size of a fist that rides your leg up the hill path and slows you down. |
| `{{HILL_CREATURE_PL}}` | burrs | Plural of `{{HILL_CREATURE}}`. |
| `{{NIGHT_CREATURE}}` | lantern | Slow pale light out on the night pasture that pulls whoever looks at it a step closer. |
| `{{NIGHT_CREATURE_PL}}` | lanterns | Plural of `{{NIGHT_CREATURE}}`. |
| `{{PASTURE_CREATURE}}` | fleece | White woolly thing standing among the sleeping sheep that is not a sheep until it moves. |
| `{{PASTURE_CREATURE_PL}}` | fleeces | Plural of `{{PASTURE_CREATURE}}`. |
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

**{{HEALER}}** — *Energy: loose, teasing, warm; precise about numbers and nothing else.* The older
cool neighbour, twenty-three to his seventeen. **Contractions always** — she is the least formal
person in the chapter and a line like "I cannot look at a wrist you are holding" is wrong for her;
she says "Put the wrist down, I can't look at it." She teases {{HERO}} the way you tease somebody you
have known since he was small, counts his losses out loud for fun, and says the frightening thing in
the same easy tone as the joke. Her worry arrives disguised as a tease. She never says what she
wants, and she gets out of any moment that is about her by making it about him.
> plain: "That's four times this week. I've got a running total, you know."
> loud: "Falke — put it down! You're bleeding on my bandage!"

**{{HERDER}}** — *Energy: blunt, literal, unsoftened.* She has never talked to a human before today,
so she has no politeness and no idiom at all. She answers the question that was actually asked and
stops. She states her own feelings as plain facts and keeps going — "I'm going to cry soon. Walk." —
and she asks about human things as flatly as she answers. Short declaratives, contractions, no
greetings, no thanks, no hedging. She says animals' names the way other people say people's names.
> plain: "I raised him. He knew me in the dark. He doesn't now."
> loud: "Don't kill him! He's mine!"

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

The jar of well water, the water run and the road west were the first draft's chapter one and were cut
whole (BRAINSTORM, 2026-09-21: "no sunk cost"). Chapter one is now the shepherd's job sheet and the
{{GUARD_BEAST}}. {{FATHER}}, {{STAIR}}, {{SECOND_MOON}}, {{CARETAKER}}, {{THE_DOOR}}, {{SEA_WALL}},
{{VILLAIN}} and {{DRY_CITY}} are kept because `PREMISE.md` still uses them; none of them appears in
chapter one, and none of them is canon until it is on screen (D23).

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
