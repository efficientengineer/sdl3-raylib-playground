# Table read bake-off: "The Sword"

Three takes of the same six-line clip, same speaking order (Hart, Falke, Hart, Falke, Ottilie, Hart).

- **Opus baseline** — `the_sword_take1.md`, the existing take.
- **Sonnet** — `the_sword_take_sonnet.md`, cheap method: one `general-purpose` subagent per turn, model
  `sonnet`, everything inline in the prompt (rules + sheet + scene + transcript so far), no file reads.
- **Haiku** — `the_sword_take_haiku.md`, same method, model `haiku`.

Both new runs used zero tool calls per turn, so every turn is a single model call on an inline prompt.

---

## 1. The three takes side by side

### Opus (baseline)

```
HART:    [shoves the sword across the table] Your father's. Six coin. Kept it eleven years — I want paying for the keeping.
FALKE:   [lifts the sword] Six coin? For my own father's sword?! Wait — there's a notch in the blade. Where'd that come from?
HART:    [taps the notch] Your father swung at a thing he should have run from. Worked, so he did it again — second time killed him. Notch stays.
FALKE:   [grips the handle] Killed him the second time? You hung it on your wall eleven years and never said a word — here's your six coin.
OTTILIE: [counts the coin] Four, and two from the baker. That is all of it — and I am the one who patches you.
HART:    [pockets the coin] Paid in full. Sharpen it tonight. Run from the big ones. Out — dawn comes early.
```

### Sonnet

```
HART:    [nods at the sword] Six coin. That's the price.
FALKE:   [staring at it] Six? For his own sword? That's- wait, why's it even mine to buy?
HART:    [grips the ledger edge] Notch's staying. He swung when he should've run. Twice.
FALKE:   Twice? He swung when he should've run - and did it again? What happened, Hart?
OTTILIE: Six coin- how much has he even got? And who is grinding that notch out before dawn?
HART:    [pushes it two inches across the table] Take it. Or leave the notch to talk you out of dawn.
```

### Haiku

```
HART:    Blade on the table. Six coin.
FALKE:   Six coin? For my father's blade!
HART:    Kept it eleven years. Won't grind the notch.
FALKE:   Won't grind it? What's the notch from?
OTTILIE: His father fought what he should have run from. The first time worked, so he fought again—and the second time it killed him.
HART:    That's how he died. Take the sword.
```

---

## 2. Token usage

Per-turn totals as reported by the Agent tool (`subagent_tokens`); `tool_uses: 0` on all twelve turns.

| Turn | Actor | Sonnet | Haiku |
|------|---------|--------|--------|
| 1 | Hart    | 24,313 | 20,578 |
| 2 | Falke   | 23,839 | 22,560 |
| 3 | Hart    | 24,191 | 25,161 |
| 4 | Falke   | 24,170 | 21,447 |
| 5 | Ottilie | 23,923 | 23,058 |
| 6 | Hart    | 24,311 | 22,243 |
| **Total** | | **144,747** | **135,047** |
| Mean/turn | | 24,125 | 22,508 |

**Versus the baseline's ~21k per turn (~126k per run): the cheap method did not save tokens — it cost
slightly more.** Sonnet ran ~15% more tokens per turn than the Opus baseline, Haiku ~7% more. The
inline prompt is only ~2.6k tokens of scene material; the rest is the generic `general-purpose`
subagent system prompt, which is larger than the file-reading overhead it was meant to replace. The
saving in this bake-off is entirely the per-token price of the model, not the token count.

Latency did move: Sonnet turns returned in 2.7–7 s, Haiku in 27–77 s (Haiku was consistently the
slowest of the three despite being the smallest model).

### Cost per six-line clip

At current first-party rates (Opus 5 $5/$25 per MTok, Sonnet 5 $2/$10, Haiku 4.5 $1/$5). Output is a
handful of tokens per turn, so these are essentially the input-rate figures plus a few percent.

| Run | Tokens | Approx. cost |
|-----|--------|--------------|
| Opus baseline | ~126,000 | **~$0.69** |
| Sonnet | 144,747 | **~$0.32** |
| Haiku | 135,047 | **~$0.15** |

Director cost sits on top of all three and is unchanged by the actor model: reading the sheets,
assembling six prompts, holding the transcript, and writing the takes is one Opus session of roughly
the same order as a single run.

---

## 3. Quality verdict

### Rule 0 — every required fact stated plainly

| Fact | Opus | Sonnet | Haiku |
|------|------|--------|-------|
| 1. Six coin for his own father's sword | yes, line 1 ("Your father's. Six coin.") | partial — "Six coin. That's the price"; Falke supplies "his own sword" | partial — same shape, Falke supplies it |
| 2. Kept eleven years, wants paying for the keeping | yes, verbatim | **MISSING** — "eleven years" never appears; the keeping is never mentioned | half — "Kept it eleven years", but never "I want paying for the keeping" |
| 3. There is a notch in the blade | yes — Falke finds it and names it | **weak** — Hart says "Notch's staying" before anyone has seen a notch | **weak** — same; the notch is never established, only referred to |
| 4. Swung at what he should have run from; worked, did it again; second time killed him | yes, whole chain in one line | **half** — "He swung when he should've run. Twice." The "it worked, so he did it again" link and the death are both missing; **the clip never says the father is dead** | yes, whole chain — but in the wrong mouth (see below) |
| 5. Hart will not grind the notch out | yes, "Notch stays." | yes, "Notch's staying" — then muddied by Ottilie asking who *is* grinding it out | yes, "Won't grind the notch." |

**Opus: 5/5. Sonnet: 2/5 clean. Haiku: 3/5 clean.**

Both cheap runs also fumble the beat. The scene's one beat is *the sword changes hands*. In Opus the
sale actually closes: Ottilie counts out four coin plus two from the baker, Hart pockets it, "Paid in
full." In Sonnet and Haiku the money is never paid — Hart just says "Take it" / "Take the sword," so
the six coin is set up and then dropped. The sale-not-a-gift idea, which is the whole reason Hart is
behaving this way, does not land in either cheap take.

### The twelve-year-old test

- **Opus — passes.** A child can say: Hart sold Falke his dead father's sword for six coin because he
  kept it eleven years; the dent in the blade is from the fight that killed his father; Hart won't fix
  it; Falke paid and is leaving at dawn.
- **Sonnet — fails.** The child does not learn that Falke's father is dead, does not learn what the
  notch is a notch *in*, and cannot say whether the sword was bought or given. The closing line,
  "leave the notch to talk you out of dawn," is a riddle; ask a twelve-year-old what it means and you
  get a shrug.
- **Haiku — partial.** The child does get the death and the two swings, and gets "six coin." But "the
  notch" arrives out of nowhere in line 3 with no one having pointed at a blade, and the money never
  changes hands, so "what happened" comes out fuzzy.

### Voice fidelity to the sheets

**Hart** survives both downgrades best. Sonnet's Hart is genuinely good — dropped subjects, four-word
fragments, physical business ("grips the ledger edge", "pushes it two inches across the table"), no
questions. Haiku's Hart is on-model but flat, and never once puts a price on the keeping, which is the
single most Hart-like thing on his sheet. Sonnet's last line breaks him: "leave the notch to talk you
out of dawn" is a figure of speech, and Hart's sheet says four or five plain words. It is also
uncomfortably close in sentiment to the forbidden "you're not ready" — not a violation in words, but
the exact idea the prohibition exists to keep out of his mouth.

**Falke** is the best actor in the Sonnet run and the weakest link in neither. Sonnet's line 4 nails
the sheet's signature move — repeats the fact back as a question before reacting ("Twice? He swung
when he should've run - and did it again?"), contractions throughout, blurts. Haiku's Falke is
serviceable but thin, and his one exclamation is the only "!" in either cheap run.

**Ottilie breaks in both, and badly.** Her sheet is specific: complete sentences, corrects the number
or the date rather than the point, talks about what she will have to patch up afterwards, and she
privately knows Falke has four coin plus whatever the baker paid. Opus uses every piece of that in one
line. Haiku's Ottilie instead narrates how Falke's father died — knowledge her sheet never gives her,
delivered in third person to the dead man's son's face, in the longest line of the scene. That is a
straight fidelity break: she steals Hart's only real card and plays it from information she does not
have. Sonnet's Ottilie is closer to the sheet's *energy* (precise questions) but corrects nothing,
patches nobody, and asks about Falke's money in the third person while he is standing there — she
gestures at her private knowledge instead of spending it. Neither cheap Ottilie is castable.

### Energy (? and !)

| | "?" | "!" | interruption dashes |
|---|---|---|---|
| Opus | 3 | 1 (as "?!") | 3 |
| Sonnet | 5 | 0 | 3 |
| Haiku | 2 | 1 | 1 |

Sonnet is question-rich but has no exclamation anywhere — the scene never spikes. Haiku is flat on
both axes; its Ottilie, whose sheet leads with "precise questions," asks nothing at all. Opus is the
only take with a real peak, and it lands on the right line (Falke discovering the price).

### Banned lists

All three clean. No "nobody", "somebody", or "anyway" in any take. Hart's NEVER SAY held in both cheap
runs: the words "you're not ready" never appear, and neither Hart mentions paying the carters —
including on the last turn, where the pressure to explain himself is highest. Format compliance was
good; the one slip was Haiku's Ottilie wrapping her LINE and WANT in quotation marks.

**Summary:** Opus clean sweep. Sonnet gives the best individual lines of the two cheap runs and a
genuinely good Hart and Falke, but loses two required facts outright and never states that the father
died — the clip is unusable as written. Haiku carries more facts across the line but puts the biggest
one in the wrong character's mouth and reads as a list of statements rather than a scene.

---

## 4. Recommendation

Use **Sonnet for actors and Opus for the director**, with the director doing real work between turns
rather than just relaying. At ~$0.32 per six-line clip Sonnet is less than half the baseline's ~$0.69
and produces Hart and Falke lines that are castable as-is; Haiku's ~$0.15 is not worth it, because its
Ottilie invented knowledge her sheet withholds, and a cheap actor that leaks private information
corrupts the scene in a way no amount of re-rolling fixes cheaply. But the honest reading of this
bake-off is that **the cheap prompting method, not the cheap model, is what broke the takes**: inlining
everything saved nothing (both runs ran *more* tokens per turn than the ~21k baseline, so the entire
saving was the price tag), and firing each actor blind with no one checking rule 0 between turns is why
Sonnet reached its last line with the father's death still unstated. The fix is a director that tracks
which of the five facts are still outstanding and says so in the next actor's prompt, and that re-rolls
a turn when an actor speaks past its sheet — one Opus director, five or six Sonnet actor calls, plus
maybe one re-roll, lands around **$0.35–$0.40 per six-line clip** in actor spend against ~$0.69 for
all-Opus. Keep Opus on any scene whose beat is carried by Ottilie, and never let a Haiku actor hold a
PRIVATE block that matters.
