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

## Rules

- Fourteen named things exist in the whole game. Adding a fifteenth is a decision, not a convenience.
- Speaker labels are tokens too: `{{HERO}}: line`, never `BRON: line`.
- Role-only speakers stay plain English and get no token: `CLERK`, `SOLDIER`, `BAKER`.
- After any rename, grep the v3 files for the retired name and expect zero hits.
