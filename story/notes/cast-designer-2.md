# cast-designer notes 2 — the editor's requests, applied

`story/characters.md` now holds **49 entries** (was 34) plus a `## Uniforms and recurring extras`
section of **seventeen** reusable one-line looks. Every added or changed entry passes
`./story_prompt.py refsheet <Handle>`, and `check` still passes on `003_the_warning` and
`003b_after_the_warning`.

Bron's and Lyra's `look` and `ref` lines are untouched, byte for byte.

---

## 1. What I added

Fifteen entries, in the file's existing format, placed in their group. Two new groups: **Sallowgate**
(Bex) and **Thurn** (Nona), the second sitting between Vantage and the extras section.

**Panel cast (list A)**

- **Ovey** — Recovery Chief Ovey, Clement Works recovery service, ch.8 boss. Company-black senior
  coat, no cuff band, a tobacco-brown scarf, brown gloves, a clipboard flat under one arm. Built for
  `0885`'s `full_body_reveal`: solid, unhurried, open-faced, nothing shadowed and nothing armed. **He
  does not look like a villain** and the design gives an artist no way to make him one.
- **Marek** — Marek Culm, survey cut crew chief, ch.2. Field-crew livery with the sleeves off, a wage
  book in the breast pocket, and the bull's-eye lamp in his own hand, which is what lights him from
  below in `0270`'s `low_angle_menace`. Tired, not angry.
- **Ista** — Ista Fenn, Tellwater weaver. Rose-madder shawl, a sage-green band (Tellwater green), the
  brass posting badge at the collar, a half-woven braid on a belt loom. Warm, neat, a half-beat quick
  — the one named face in `0310`'s `high_angle_down`.
- **Domma** — Domma Sath, Vess, eighties. Seated, straight-backed, long folded hands, white wire-hair
  in one flat plait pinned close, mulberry-red chant wrap. Designed so the hands carry `0487` and the
  portrait carries `1335`. No collision with Ilven (teal, nine bone pins, waist-length, barefoot) or
  Sefa (brass, company black, two pencils).
- **Nona** — the woman of Thurn. Kell of Thurn: clay-warm, pale gold upright hair, floured bare arms,
  and a square-cut woad-blue overdress with bone toggles, centuries out of fashion, spotless and
  invisibly mended. The clothes are the whole design. **No backstory, ever.** See §4 for the handle.
- **Ondra** — Ondra Pell, decommission clerk. Works charcoal, brass number, mustard cuff band, a
  roster book with a brass clasp, and a plum-purple scarf and mittens that are hers and not the
  company's. Likeable, orderly, on a schedule.

**Portrait-only talk cast (list B)** — `Bex` (Kell roofkeeper, Sallowgate), `Orla` (Windrow
boarding-house keeper), `Mir` (Vess hall-keeper, Braid), `Ol` (Onn, one light, Saltmouth), `Corrow`
(restored crawler hand, Ostry Bar), `Jory` (salvage foreman, Tellwater), `Rask` (dying Pale sergeant,
the Quiet House), `Threnn` (Vess, Domma's bond-pair), `Mira` (Ivo Teach's wife).

**New extras lines** (plain bullets, not entries): the dying hunter's hand (Merrit Tack), the bench
man, frame-bearer, standing Onn of the muster (Dur-Sarn at distance), person of Thurn, sanatorium
orderly.

## 2. What I changed

1. **Anneke is Kell.** `people`, `voice`, `look` and `notes` rewritten to canon: very short and very
   broad, clay-warm skin reddened by steam, iron-grey hair that will not lie flat, amber eyes with the
   bright pupil ring, bare arms in a hot room, and the steam complaint in the voice line. The faded
   rose-pink headscarf, the laundry apron, the flat iron and the chipped cup all survive. Notes now
   carry the four-foot-eleven fact and why `1085` is a two-shot about height.
2. **Sem** is **four of eight, two a side** — look and status both corrected, with the reason (Ket at
   five must be the brightest of the four) in her notes.
3. **Tovin**'s status line now reads "two a side, four of eight, and going down". Her look was
   already right and was not touched.
4. **Ket**'s notes now carry the light ledger explicitly: the sheet is drawn at five and is correct
   only for ch.4-8; four in ch.9-11, three in ch.12, two from ch.13, and the panel line must state the
   count from ch.9 on.
5. **Zeph and Pip** — unchanged, as expected.

## 3. Accent colour map (updated)

One strong accent per character, hair colour beside it. The rule is that no two characters who share
a scene match on **both**. Checked against every pairing in the outlines; no violation. New entries
in **bold**.

**Party** — Bron copper-red / burnt orange · Lyra pale-gold / white and gold · Zeph indigo / deep
blue with silver · Pip chestnut / olive green · Ket bone-white skin / jade green · Hesk iron-grey /
pine green · Ollo greying brown / soft sage green · Sefa silver wire / brass

**The Fair Copy** — Maren dark going grey / brick red

**The Clement Works** — Crewe white / deep burgundy · Kerrow black / ice blue · Ivo iron-grey /
oxblood · Orrin brass / mustard yellow · Hanna dark brown / white cross-belt · Arden sandy / oxblood
cord · Emmet mousy brown / mustard livery · **Ovey grey / tobacco brown** · **Marek black going grey
/ mustard livery** · **Ondra dark brown / plum purple**

**The Even Hand** — Tovin ochre skin / chalk white · Tibb black / turquoise · Sela ash-blonde / plum
purple · **Mir iron-silver wire / claret red**

**Sallowgate** — **Bex brass / slate blue**

**Windrow** — Anneke iron-grey / faded rose pink · Aldo white / tram green · Ovel steel-grey / bone
white · Arro dull-green skin / copper green · **Orla grey-brown / brick red** · **Rask white / ochre
yellow** · **Mira grey-streaked black / dusky lavender**

**Tellwater** — Sem slate skin / ochre yellow · Neve brown going grey / cornflower blue · **Ista
brown going white / rose madder** · **Jory black / rust orange**

**Ninefold Terrace** — Dov pale gold / mustard yellow

**Braid and the shoreline** — Ilven white-silver wire / deep teal · Cassa ash-violet wire / coral pink
· **Domma white wire / mulberry red** · **Threnn silver-blue wire / turquoise** · **Ol ochre skin /
deep indigo**

**The Flats** — Nessa white-grey / rust red · Ilsa iron-grey / cobalt blue · **Corrow black going
grey / saffron yellow**

**Emberrow** — Ross straw-blond / oxblood leather · Dalla black / dusty pink

**Vantage** — Idda dark auburn / plum purple · Corrie black / sleeve pink

**Thurn** — **Nona pale gold / woad blue**

### Near-misses worth a second look

- **Marek and Orrin share `0280` and both wear mustard** — but mustard is the livery, not a choice,
  and that is half the point of the panel. They separate on people and silhouette: Orrin is short,
  very wide, brass-haired, scarred and grinning; Marek is lean, human, grey-templed and holding a
  lamp low.
- **Ondra's plum** is the third plum in the cast (Sela, Idda). None of the three ever share a scene,
  and hers is a scarf and mittens over company charcoal.
- **Orla's brick red and Maren's brick red** never meet. Maren is only a rumour in Windrow ch.2, and
  their silhouettes (a long grey travelling coat versus three layers of wool under a laundry apron)
  are nothing alike.
- **Threnn's turquoise and Tibb's turquoise** never meet (ch.13 seabed versus the Sallowgate board).
- **Bex's slate blue beside Zeph's deep indigo** in `0135`, where Zeph is present and silent. Talk
  scenes are portraits, and a Kell in a sleeveless coat cannot be confused with a Vess in a long one.
- **Ovey's tobacco brown** is the only brown accent in the cast, chosen because every warm colour in
  `0885` was already taken (Bron orange, Ross oxblood, Dalla dusty pink, Lyra gold) and the greens
  were three deep (Hesk, Ket, and Pip's olive).
- **Nona's pale gold hair against Thurn's lamp-column gold.** The woad-blue overdress is what carries
  her off the background; do not let an artist put her in a warm colour.

## 4. Requests I declined, and why

1. **The handle `Nine` — declined. Her handle is `Nona`.** `story_prompt.py` matches handles
   case-insensitively as whole words in panel text, so a handle of `Nine` would fire on "the Nine
   Doors" (ch.5's chapter title and every door panel), "Nine hundred tallies" (ch.14's side contract),
   and Ilven's nine bone chant-pins — each one a hard `R6 known cast` error in a scene she is not in.
   `Nona` is a number, which the Act V outline requires, and is not an English word. **Her name on
   screen is still Nine**: write the dialogue speaker label as `Nine` (the tool only checks speaker
   names in talk scenes, and only warns), and use `Nona` on the `characters:` line and in panel text.
   If a later week's name is wanted, it must still be a number and the handle stays `Nona`.
2. **Merrit Tack — no reference sheet**, as recommended. He is an extras line: a broad weathered hand
   and forearm palm up on a grey blanket, with the Even Hand token loose at the wrist.
3. **The bench man — no entry**, as requested. Extras line, and it carries hard rule 13 in its own
   words: eyes clear and focused, an open friendly half-smile, never vacant, never slack, never a
   stare.
4. **Dur-Sarn — no entry.** Covered by a new extras line for standing Onn at distance: small, spaced
   far apart, never grouped, never facing one another. Give him an entry only if somebody gives him a
   line, which nobody should.

## 5. Reference sheet priority (updated; chapters 1-3 first)

Bron and Lyra exist. Fourteen in tier 1 covers every face in the first three hours.

**Tier 1 — chapters 1-3**
1. Zeph · 2. Pip · 3. Tovin · 4. Emmet · 5. **Marek** (two ch.2 panels) · 6. Orrin · 7. Ollo ·
8. **Ista** (ch.3 panel) · 9. Ivo · 10. Sem · 11. Hanna · 12. Tibb and Sela · 13. **Bex** ·
14. **Orla**

**Tier 2 — Act II (ch.4-6)**
15. Ket · 16. Hesk · 17. Cassa · 18. Ilven · 19. **Mir** · 20. **Domma** · 21. Dov · 22. Kerrow ·
23. Maren (glimpsed ch.6, so the design must exist before the reveal) · 24. Nessa · 25. **Ol**

**Tier 3 — Act III (ch.7-10)**
26. **Ovey** (ch.8 panel boss — first in this tier if the tier is split) · 27. Sefa · 28. Crewe ·
29. Ross · 30. Dalla · 31. Corrie · 32. Idda · 33. Ilsa · 34. **Corrow** ·
35. **Anneke — re-sheet, she is Kell now** · 36. Aldo · 37. Ovel · 38. Arro

**Tier 4 — Acts IV-V**
39. **Ondra** (ch.11 panel) · 40. Arden · 41. **Nona** (ch.14 talk, ch.15 panel — do not leave her
last) · 42. **Rask** · 43. **Threnn** · 44. **Jory** · 45. Neve · 46. **Mira**

Everything else is covered by the seventeen extras lines and needs no sheet.

## 6. Flags for the orchestrator

- **`Ovey` and `Ovel` are one letter apart** (Recovery Chief Ovey, ch.8; Doctor Ovel Marsh, ch.10).
  They never share a scene and neither handle false-matches, but it is the Kesk/Hesk problem again.
  If it bothers a scene writer, rename the doctor, not the chief — Ovey is the name in canon and in
  the bible.
- **Anneke's `ref` path is unchanged** and no art exists at it yet, so the Kell rewrite costs nothing.
  If anyone has generated her since, that sheet is dead.
- **Threnn's sex** is not fixed anywhere in canon; I wrote him male so the bonded pair reads as a pair
  of chanters rather than a matched set with Domma. Reverse freely.
- **Mir Lirr is filed under The Even Hand**, not Braid, because her job is the guild and the entry
  should sit beside Tovin's. Domma, Threnn and Ol-Semmet are under Braid and the shoreline; Ol is at
  Saltmouth, which has no group of its own.
- Two open points from the first notes file still stand: scene 001's staging calls Pip "he", and
  scene 002's "old magic" is banned. Canon section 2 rules on both; neither is my file.
