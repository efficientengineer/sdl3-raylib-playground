# The Sword — Take: HAIKU actors

Method: one subagent per actor turn (`general-purpose`, model `haiku`), everything inline in the
prompt (rules + that actor's sheet + scene sheet + transcript so far), no file reads, reply limited to
LINE/WANT. Speaking order as baseline: Hart, Falke, Hart, Falke, Ottilie, Hart.

## Transcript

[Hart's kitchen, evening. The guild ledger is open on the table. Falke's father's sword is lying on the table beside it, off the wall for the first time in eleven years. Falke has just come in. Ottilie is at the table.]
HART: Blade on the table. Six coin.
FALKE: Six coin? For my father's blade!
HART: Kept it eleven years. Won't grind the notch.
FALKE: Won't grind it? What's the notch from?
OTTILIE: His father fought what he should have run from. The first time worked, so he fought again—and the second time it killed him.
HART: That's how he died. Take the sword.

## WANT lines as returned

1. HART — Make it business, no emotion
2. FALKE — To claim my father's sword
3. HART — make Falke accept the blade
4. FALKE — Hart to explain the notch
5. OTTILIE — "Get them to settle now"  *(returned wrapped in quotes — format slip)*
6. HART — Sword in his hands, no thanks

## Token usage per turn (as reported by the Agent tool)

| # | Actor | Tokens |
|---|---------|--------|
| 1 | Hart    | 20,578 |
| 2 | Falke   | 22,560 |
| 3 | Hart    | 25,161 |
| 4 | Falke   | 21,447 |
| 5 | Ottilie | 23,058 |
| 6 | Hart    | 22,243 |
| **Total** | | **135,047** |

No tool uses in any turn. Turn durations 27–77 s.
