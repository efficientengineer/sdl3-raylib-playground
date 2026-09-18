# NAMES — the token table

Every proper noun in the v3 story text is a token in double braces, including speaker labels.
Renaming anything is a one-line edit here. Tokens are UPPER_SNAKE.

| Token | Current name | What it is |
|---|---|---|
| `{{HERO}}` | Bron | Hunter's apprentice, seventeen. Bought his father's axe. Has an introduction he doesn't know about. |
| `{{HEALER}}` | Lyra | Healer, twenty-three. The mentor's other student. Wants to be asked along. |
| `{{SCHOLAR}}` | Zeph | One of eight people alive who read any of the ancients' language, and the worst of them. Joins chapter two. |
| `{{THIEF}}` | Pip | Thief, small, loud. Sends money home to a sister a caretaker keeps alive. Joins chapter two. |
| `{{MENTOR}}` | Dorn | The old hunter. Bad knee, guild ledger, one chair, a roof he should not be on. |
| `{{RIVAL}}` | Cray | Nineteen, better, right about it. Takes the lord's coin in chapter one. |
| `{{VILLAIN}}` | Sevran | The young lord in the west. Bought a caretaker, had the {{SECOND_MOON}} moved, and is walking to {{THE_DOOR}}. |
| `{{HOME_TOWN}}` | Fallow | Nine streets, a grain yard, a well. Where the hero starts. |
| `{{CARETAKER}}` | *(unnamed yet)* | One of the people the ancients made. Few left, all old. Took the bread at the {{STAIR}} for generations; bought and taken west before chapter one. |
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

Three habits and two sample lines each. Every sample states a plain fact. Voice changes how a thing
is said, never whether it is said.

**{{HERO}}** — Blurts. Short bursts, contractions always, says the obvious thing first. Repeats the
fact he just heard back as a flat half-sentence before he reacts to it. Talks about work and money
because those are the things he knows. Never asks for pity and never says "please" to get something.
> "Never once. Right. So what's left?"
> "You're selling me my father's axe."

**{{HEALER}}** — Dry and exact. Complete sentences, few contractions when she is correcting someone,
and she corrects the number or the date rather than the point. When she could talk about anything she
talks about what she will have to patch up afterwards. Never raises her voice, never sulks in silence.
> "He left it the year he died, then. Eleven years under a rock."
> "And she hasn't taken the bread for one. Whatever happened, she's been gone a year."

**{{MENTOR}}** — Clipped. Four or five words where most people use twelve, and he drops the subject
off the front of a sentence. Puts a price on everything, favours included. Never says goodbye and
never says he is worried about you.
> "Kept it eleven years. I want paying for the keeping."
> "No. Your father died on that road."

**{{RIVAL}}** — Pleased with himself. Easy long sentences, contractions, and a number in every speech:
what he was paid, what it cost, what he has that you do not. He compliments you while beating you and
never insults you to your face.
> "Eighty coin a week. I signed with him the same morning."
> "You're carrying a loaf of bread, and I'm riding in a cart."

**CLERK** — Talks in rules. States the rule, then the fact, then what follows. No contractions, no
opinions, no sympathy. Says "the rule is" the way other people say "well".
> "The rule is that every job needs a hunter who will vouch for you."
> "It has sat on that board four months because it is dull work."

**{{SCHOLAR}}** *(chapter two)* — Says it once for the room and once for the record. Long sentences.
Given any subject he tells you who first wrote it down. Never says "I don't know" without immediately
saying what he does know.
> "That is a staircase. I am writing that down: a staircase, and no one has ever stood on it."
> "Eight people alive read any of their language. I am the eighth best of the eight."

**{{THIEF}}** *(chapter two)* — Fast and short. Contractions always. Jokes by understatement. Given
anything she tells you what it is worth and who is watching it. Never says thank you.
> "That rope's worth more than the bread is."
> "Someone's been here. Cart tracks, a day old."

## Rules

- Fourteen named things exist in the whole game. Adding a fifteenth is a decision, not a convenience.
- Speaker labels are tokens too: `{{HERO}}: line`, never `BRON: line`.
- Role-only speakers stay plain English and get no token: `CLERK`, `SOLDIER`, `BAKER`.
- After any rename, grep the v3 files for the retired name and expect zero hits.
