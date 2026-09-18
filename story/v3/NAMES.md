# NAMES — the token table

Every proper noun in the v3 story text is a token in double braces, including speaker labels.
Renaming anything is a one-line edit here. Tokens are UPPER_SNAKE.

| Token | Current name | What it is |
|---|---|---|
| `{{HERO}}` | Bron | Hunter's apprentice, seventeen. Carries his father's axe. Hears the machines. |
| `{{HEALER}}` | Lyra | Healer, twenty-three. The mentor's other student. Wants to be asked along. |
| `{{SCHOLAR}}` | Zeph | Scholar of the ancients. Reads a third of their marks. Paid to be the survey crew. |
| `{{THIEF}}` | Pip | Thief, small, loud. Sends money home to a sister on an ancient lamp. |
| `{{MENTOR}}` | Dorn | The old hunter. Bad knee, guild ledger, one chair, a roof he should not be on. |
| `{{RIVAL}}` | Cray | Nineteen, better, right about it. Takes the lord's coin in chapter one. |
| `{{VILLAIN}}` | Sevran | The young lord in the west who turned an engine back on and made it rain. |
| `{{HOME_TOWN}}` | Fallow | Nine streets, a grain yard, a well. Where the hero starts. |
| `{{TOWER}}` | *(described, unnamed)* | The tower you can see from three towns. Reserved in case it gets a name. |

## Reserved, currently described rather than named

These have no proper name and should stay that way. If one ever gets named, add the token here and
tokenize it in the text in the same pass.

| Token | Current text | Note |
|---|---|---|
| `{{OLD_HALLS}}` | "the old halls" | The ruin in chapter one, and the type of place generally. |
| `{{ANCIENTS}}` | "the ancients" | The people who built the engines and shut them down. |
| `{{GUILD}}` | "the guild" | Hunters' guild. Job board, ledger, guarantors. |
| `{{DRY_CITY}}` | "the dry city in the west" | The villain's city. |

## Rules

- Nine named things exist in the whole game. Adding a tenth is a decision, not a convenience.
- Speaker labels are tokens too: `{{HERO}}: line`, never `BRON: line`.
- Role-only speakers stay plain English and get no token: `CLERK`, `SOLDIER`, `BAKER`.
- After any rename, grep the v3 files for the retired name and expect zero hits.
