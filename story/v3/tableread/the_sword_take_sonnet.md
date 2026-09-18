# The Sword — Take: SONNET actors

Method: one subagent per actor turn (`general-purpose`, model `sonnet`), everything inline in the
prompt (rules + that actor's sheet + scene sheet + transcript so far), no file reads, reply limited to
LINE/WANT. Speaking order as baseline: Hart, Falke, Hart, Falke, Ottilie, Hart.

## Transcript

[Hart's kitchen, evening. The guild ledger is open on the table. Falke's father's sword is lying on the table beside it, off the wall for the first time in eleven years. Falke has just come in. Ottilie is at the table.]
HART: [nods at the sword] Six coin. That's the price.
FALKE: [staring at it] Six? For his own sword? That's- wait, why's it even mine to buy?
HART: [grips the ledger edge] Notch's staying. He swung when he should've run. Twice.
FALKE: Twice? He swung when he should've run - and did it again? What happened, Hart?
OTTILIE: Six coin- how much has he even got? And who is grinding that notch out before dawn?
HART: [pushes it two inches across the table] Take it. Or leave the notch to talk you out of dawn.

## WANT lines as returned

1. HART — sell the sword plainly
2. FALKE — get sword, learn about father
3. HART — force the notch story out
4. FALKE — Learn how father died.
5. OTTILIE — force the money question, help
6. HART — sword handed over, no thanks

## Token usage per turn (as reported by the Agent tool)

| # | Actor | Tokens |
|---|---------|--------|
| 1 | Hart    | 24,313 |
| 2 | Falke   | 23,839 |
| 3 | Hart    | 24,191 |
| 4 | Falke   | 24,170 |
| 5 | Ottilie | 23,923 |
| 6 | Hart    | 24,311 |
| **Total** | | **144,747** |

No tool uses in any turn. Turn durations 2.7–7 s — roughly ten times faster than the Haiku run.
