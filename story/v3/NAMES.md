# NAMES — the token table

Naming style: plain German words used as names, the way Frieren does it (Stark, Fern, Himmel), plus
real old German given names (Ottilie). Avoid words Frieren already uses. Meanings: Falke = falcon,
Ottilie = given name, Hart = hard, Stolz = pride, Frage = question, Elster = magpie, Durst = thirst,
Halm = grain stalk.

Every proper noun in the v3 story text is a token in double braces, including speaker labels.
Renaming anything is a one-line edit here. Tokens are UPPER_SNAKE.

| Token | Current name | What it is |
|---|---|---|
| `{{HERO}}` | Falke | Hunter's apprentice, seventeen. Bought his father's sword. Has an introduction he doesn't know about. |
| `{{HEALER}}` | Ottilie | Healer, twenty-three. The mentor's other student. Wants to be asked along. |
| `{{SCHOLAR}}` | Frage | One of eight people alive who read any of the ancients' language, and the worst of them. Joins chapter two. |
| `{{THIEF}}` | Elster | Thief, small, loud. Sends money home to a sister a caretaker keeps alive. Joins chapter two. |
| `{{MENTOR}}` | Hart | The old hunter. Bad knee, guild ledger, one chair, a roof he should not be on. |
| `{{RIVAL}}` | Stolz | Nineteen, better, right about it. Takes the lord's coin in chapter one. |
| `{{VILLAIN}}` | Durst | The young lord in the west. Bought a caretaker, had the {{SECOND_MOON}} moved, and is walking to {{THE_DOOR}}. |
| `{{HOME_TOWN}}` | Halm | Nine streets, a grain yard, a well. Where the hero starts. |
| `{{CARETAKER}}` | *(unnamed yet)* | One of the people the ancients made. Few left, all old. Took the water at the {{STAIR}} for generations; bought and taken west before chapter one. |
| `{{STAIR}}` | *(descriptive)* | A staircase the width of a town, down out of the cloud, stopping thirty feet above an empty field. The ancients' work nearest {{HOME_TOWN}}. |
| `{{SECOND_MOON}}` | *(descriptive)* | A moon that isn't a moon. Held one quarter of the sky for a thousand years until last spring. |
| `{{THE_DOOR}}` | *(descriptive)* | The mountain that is a door. The one place the ancients can be woken or overruled; the end of the pilgrimage. |
| `{{SEA_WALL}}` | *(descriptive)* | The edge where the sea stops, with dry seabed beyond it. |

## Reserved, currently described rather than named

These have no proper name and should stay that way. If one ever gets named, add the token here and
tokenize it in the text in the same pass.

| Token | Current text | Note |
|---|---|---|
| `{{ANCIENTS}}` | "the ancients" | The people who built the four great works and then stopped. |
| `{{GUILD}}` | "the guild" | Hunters' guild. Job board, ledger, guarantors. |
| `{{DRY_CITY}}` | "the dry city in the west" | The villain's city. |

## Voices

Voice is energy before it is vocabulary, and **punctuation carries the energy**. Three habits, one
plain sample, one loud sample. Every sample states a fact. Voice changes how a thing is said, never
whether it is said. People interrupt each other with a dash.

**{{HERO}}** — *Energy: blurts and asks.* Short bursts, contractions always, and he says what he
feels the second he feels it. Questions when he is behind, exclamations when he is surprised, and he
repeats the fact he just heard back as a question before he reacts to it. Never asks for pity.
> plain: "He walked out here every year for thirty years and sat under this step."
> loud: "Never once? Not one job, ever?"

**{{HEALER}}** — *Energy: precise questions, rare exclamations that land.* Complete sentences, few
contractions when correcting, and she corrects the number or the date rather than the point. Talks
about what she will have to patch up afterwards. Never raises her voice twice in a scene.
> plain: "He left it the year he died, then. Eleven years under a rock."
> loud: "Do you know he has never once asked me to come anywhere?"

**{{MENTOR}}** — *Energy: orders and grumbles.* Never asks a question. Fragments with the subject
dropped off the front. Four or five words where most people use twelve. Puts a price on everything,
favours included. Never says goodbye and never says he is worried about you.
> plain: "Kept it eleven years. I want paying for the keeping."
> loud: "Died on that road eleven years back. Hold the ladder."

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

- Fourteen named things exist in the whole game. Adding a fifteenth is a decision, not a convenience.
- Speaker labels are tokens too: `{{HERO}}: line`, never `BRON: line`.
- Role-only speakers stay plain English and get no token: `CLERK`, `SOLDIER`, `BAKER`.
- After any rename, grep the v3 files for the retired name and expect zero hits.
