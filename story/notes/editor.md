# Story editor's report to the orchestrator

Five acts merged into one story. The single page every scene writer now reads after the bible is
**`story/canon.md`** — the fifteen questions, every ruling, Ket's lights ledger, the running facts, the
cross-act plant table, and the name register. Two hand-offs are in
`story/notes/requests-cast.md` and `story/notes/requests-locations.md`.

---

## What I changed, and why

### `story/canon.md` — new

The frozen fifteen questions with a table of every scene that uses them and which ones; every D8
ruling restated as an obeyable fact; a ruling on each of the ~60 further questions in the seven notes
files (nine marked **owner may overrule** — art budget, economy, strand availability, Zeph benched in
ch.15, party-less scenes, Hallam Clement's portrait); the lights ledger; calendar, ages, distances,
party and vehicle per chapter, and what each member knows at the end of each act; the plant table; the
name register.

### `story/bible.md`

- **Section 5:** added the fifteen questions in full (they were quoted by four acts and written
  nowhere), plus two clarifications that do not bend a hard rule — **rehearsal** (the mechanism by
  which giving the record empties Maren, D8.8) and **"taking the hour" is not a device**.
- **Section 8 (22:31):** named the boy in the bore shed doorway as the one the node did not receive.
  The timeline already supplied the mechanism; it never said what it meant.
- **Section 10:** corrected what Maren holds. It said "four hundred and eleven lives, including the two
  hours of Bron's", which contradicts section 12 chapter 10 and decision 4. Decision 4 governs: four
  hundred and ten other lives plus her own, and **nothing of Bron's**.
- **Section 11:** **Anneke Brae is Kell**, with a look line.
- New **"Editor's changes"** list at the end recording all of it.

### `story/outline/act1.md`

- `0115` now quotes canon rather than pointing at a superseded candidate set of questions.
- `0125`: **the Four Lines are burned into the board's timber frame** — chapters 14 and 15 are built on
  that object and it existed nowhere.
- `0275`: Crewe's and Kerrow's countersignatures added to the survey order, on screen, unread.
- `0240` and the Act I threads: the Kettle bill now points at its actual closing scene.
- `0110`: Tovin's light count stated as two a side.

### `story/outline/act2.md`

- **Renumbered 21 scene ids.** Chapters 4, 5 and 6 had each overflowed their own `CCSS` block, giving
  **thirteen duplicate ids** — five of which collided with Act III's chapter 7. Slugs unchanged; full
  mapping in `canon.md` section 9. `outline-act2.md`'s notes still use the old numbers.
- **New scene `0607_bad_water_at_kettle`** (talk, optional): the bible's chapter-2 side contract could
  be posted and never cleared anywhere in the game. D8.6 said chapter 6; now it is.
- `0420`: **planted the Vess second tone going flat when they lie.** Chapters 8 and 15 both pay it off
  and nothing set it up.
- `0405`: stopped re-revealing the terrace stair, the white horizon and the rails, which Act I already
  spends at `0390`/`0395`.
- `0696`: it re-made Bron's promise to Lyra instead of collecting on it; now it collects.
- `0575`/`0580`: fixed which two questions are cut into the wall (7 and 13), and that the band carries a
  numbered line 15 with nothing after it — which unifies Zeph's "inconsistent numbering", the ch.9
  manual's blank, and Ket's shutdown check in ch.15.
- Chapter 6 header: Lyra's ch.5-9 silence made a standing rule; scene counts updated.

### `story/outline/act3.md`

- `0925`: the Kettle bill is cleared in chapter 6, not chapter 2, and the scene must play identically
  either way.
- `0960`: quote canon's wording, three questions at most, recommended 1, 4 and 7.
- Chapter 7 and 8 headers: Lyra's silence as a standing rule.

### `story/outline/act4.md`

- **Dates corrected.** The draft had chapter 13 ending "the 22nd of Dust, eighteen days before the 9th
  of Fallow", which is impossible on a 45-day month, and Act V's clock (ch.15 = 7th of Fallow, 29 hours
  after the Long Bore goes full) put chapters 14-15 a month later than Act IV left them. Chapter 13
  moves to **1st-5th of Fallow**; Kerrow's orders are dated the 20th of Dust and the evacuation runs a
  fortnight. Lyra still dies on the 9th of Dust, exactly one 45-day month before the anniversary.
- **Ket's lights fixed.** Act IV assumed she entered chapter 11 at three when Act III left her at four,
  and only two of the three spends were placed. Ledger: `0965` → 4, `1255` → 3, **`1390` → 2** (the
  scene that is literally about her lights, and it lands her at two for the first frame of chapter 14,
  which Act V already assumes). `1140` corrected, `1145`'s panel corrected.
- `1150`/`1175` now quote canon and state the fifteenth.

### `story/outline/act5.md`

- Tovin's light count corrected in three places (two **a side**; Ket is the one at two).
- `1585`: thirteen "correct", one "not correct", and the fifteenth has no written answer so Ket gives
  it herself — which ties `1490`'s blank line to the last decision in the game.
- `1630`: it is a mandatory scene whose entire cast was introduced in optional scenes; it must now
  introduce Arro in its own first lines.
- `1405`: **the Ninth Door is the ninth door-mouth of Ninefold Terrace** — Hesk's own cliff. Chapter 5's
  geography now carries chapter 14's entrance and chapter 15's last image.
- `1560`: Sem-Dree's light count matched to canon.

### Verified

`grep -h '^### ' story/outline/*.md | sort | uniq -d` prints nothing; all **323** scene ids are unique
and each sits inside its own chapter block; every scene entry still carries `when`, `where`, `who`,
`beat`, `purpose`, `mood` and `lines` (`notes` and `panels` where applicable). At most one optional
panel scene per chapter (`0487`, `0555`, `0840`).

---

## The five biggest risks left in the story

1. **Chapters 7-10 are four consecutive documents.** The Registry, the resettlement map, the intake
   forms, Crewe's list, the survey tag, the payroll page — Act III's four chapters each end on a piece
   of paper, and the ladder E rungs are all filing. It is thematically right and it is four hours of
   reading. Mitigation: chapters 8 and 9 must lead with the physical set-pieces (Maren restoring Ross
   Kettle in a street; a mile of fallen rings) and each drop one document beat.
2. **One plant is touched nine times.** Bron's winter is told at `0130`, retold in chapter 4, retold in
   chapter 5, recited back at `0985`, and remembered at `1435`; and his promise to Lyra is made at
   `0395`, collected at `0696`, re-confirmed at `1030`, and left unpaid at `1175`. I fixed the worst of
   it (`0696` now collects rather than re-asks) but the chapter 4 and chapter 5 retellings should be
   cut to one between them. The player will be ahead of the game by chapter 5 as it stands.
3. **The last five chapters have no new information about the protagonist, by design**, and chapter 13
   is the thinnest in the game — evacuate a town, shut down a bore for four hours, watch a woman walk
   through a door. If chapter 11's death and chapter 14's Thurn do not land emotionally, the last six
   hours read as a long errand. Chapter 13 is the one to look at first if anything gets cut or merged.
4. **Two shapes are derivative even though every detail is original.** A healer dying in a quiet town
   two thirds of the way through is the single most recognisable beat either source game has, and
   chapter 11 opens so warm that a genre-literate player will brace at scene one. And a sealed ancient
   underground city of descendants who cannot remember is well-worn ground. Both are saved only by
   execution — the paperwork cause of the death, and Thurn being lit, swept and in better repair than
   the surface. **Nobody may soften either.** If you want to break the first one, Act IV's own note
   offers the swap to Pip.
5. **Six reasonable company people in a row start to blur.** Cadder, Kerrow, Teach, Ovey, Crewe and
   Sark are all courteous, all partly right, and all offer the party a reasonable thing. That is the
   thesis, but it is also six scenes with the same temperature. Let Ovey be brisk to the point of rude,
   keep Kerrow cold, and lean hard on the cast designer's silhouettes; and note that the two coldest
   reveals in the game (the cradle, and that Thurn is inhabited) have **zero** foreshadowing by bible
   instruction, which is a lot of weight on chapter 14 doing it in one go.

Smaller, worth a line: **111 panel sheets** against D3's 90-110 target. If it must come down, cut
`0555_company_mortar` first (downgrades to talk cleanly) then `0487_domma_sath` (loses a lot).

---

## Scene counts per chapter after my edits

| Ch | Total | Panel | Talk | Narr | Optional |
|---|---|---|---|---|---|
| 1 | 24 | 8 | 14 | 2 | 4 |
| 2 | 19 | 6 | 12 | 1 | 5 |
| 3 | 19 | 6 | 11 | 2 | 4 |
| 4 | 26 | 8 | 16 | 2 | 4 |
| 5 | 22 | 7 | 14 | 1 | 3 |
| 6 | 26 | 8 | 16 | 2 | 3 |
| 7 | 19 | 7 | 11 | 1 | 2 |
| 8 | 19 | 7 | 11 | 1 | 4 |
| 9 | 19 | 7 | 11 | 1 | 3 |
| 10 | 19 | 7 | 10 | 2 | 1 |
| 11 | 20 | 7 | 11 | 2 | 3 |
| 12 | 20 | 7 | 12 | 1 | 3 |
| 13 | 20 | 7 | 11 | 2 | 2 |
| 14 | 20 | 8 | 11 | 1 | 1 |
| 15 | 20 | 8 | 11 | 1 | 0 |
| 16 (epilogue) | 11 | 3 | 7 | 1 | 0 |
| **Total** | **323** | **111** | **189** | **23** | **42** |

Chapter 1 includes the five legacy ids; one of its eight panel scenes is already drawn. Chapter 6 gains
one scene (`0607`) over D8's count of 322.

---

## Recommended writing order and batching

The constraint is **quotation**, not chronology. A chapter must be written before any chapter that
quotes its words. Five waves.

**Wave 0 — one writer, alone. Chapter 1.**
Everything downstream quotes it: the fifteen questions in full, the Four Lines on the board frame, the
ledger wall, the burned seal, Emmet Sark's entry chit, and Bron's winter story. **Freeze it on
delivery** and hand its finished dialogue to every subsequent writer. Nothing else starts until this
lands.

**Wave 1 — three writers. Chapters 4, 5, 11.**
Each owns text that later chapters quote and must not re-derive.
- **4** owns the edges test and the Vess second tone (both new plants for Acts III and V).
- **5** owns the Sealed Stair inscription, the oath's terms, and "that's a word off a map".
- **11** owns "Ask me the fifteenth / I don't know it / You will", and Lyra's answer to question 15,
  which chapters 14 and 15 both build on. Chapter 11 is also the hardest scene in the game; give it
  your best writer and the most time.

**Wave 2 — four writers. Chapters 2, 3, 6, 10.**
- **2** and **3** finish Act I's ladder rungs and depend on chapter 1's wording.
- **6** owns the payroll page's exact line (*Ardo, N. — role: Hesta Sallow…*), quoted in 7 and 14.
- **10** owns the intake form's exact line (*template 4, two sleeves, source O, four marks*), quoted in
  11, 13, 14 and 15, and Crewe's list entry for Anneke Brae.

**Wave 3 — four writers. Chapters 7, 8, 9, 12.**
- **9** needs 1 and 5 finished (Ket reading the Tally off the manual beside Lyra).
- **12** owns Crewe's eleven-box confession, which chapter 15's hearing quotes.
- 7 and 8 have no downstream quotations and can slot anywhere from here.

**Wave 4 — three writers. Chapters 13, 14, 15.**
- **14 owns the relief formula's exact wording** — it is printed to the player for the first time at
  `1440` and nowhere earlier; `1380` in chapter 13 shows Maren speaking and prints nothing, so 13 and
  14 can be written in parallel as long as 13's writer honours that.
- **15** needs 1, 11, 12 and 14 in hand.

**Wave 5 — one writer. The epilogue (ch.16).**
Last, and ideally **the chapter 1 writer**: it is chapter 1's mirror — the same hall, the same board,
the same ledger wall, the same guild master, and the last joke needs the voice that established her.

**Two standing instructions for all fifteen.** Read `story/canon.md` before the outline, and treat the
seven files in `story/notes/` as history — where they disagree with canon, canon wins.
