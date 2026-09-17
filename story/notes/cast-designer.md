# cast-designer notes — for the orchestrator

`story/characters.md` is rewritten as the whole-game cast: **34 entries**, plus a
`## Uniforms and recurring extras` section of eleven reusable one-line looks for unnamed figures.
Every entry passes `./story_prompt.py refsheet <Handle>`, and scenes 001, 002, 003 and 003b still
pass `check` unchanged.

Bron's and Lyra's `look` and `ref` lines are byte-for-byte what they were. Only their `role`,
`status` and `notes` changed.

---

## 1. Accent colour map

One strong accent per character; the hair colour is given too, because the rule is that no two
characters who share scenes may match on **both**. Checked pairwise against who appears together;
no violation. Near-misses are flagged.

**Party**

- Bron — copper-red hair / **burnt orange** (fixed)
- Lyra — pale-gold hair / **white and gold** (fixed)
- Zeph — indigo wire-hair / **deep blue with silver**
- Pip — chestnut hair / **olive green**
- Ket — no hair, bone-white skin / **jade green**
- Hesk — iron-grey hair / **pine green**
- Ollo — greying brown hair / **soft sage green** ("Tellwater green")
- Sefa — silver wire-hair / **brass**

**The Fair Copy**

- Maren — dark hair going grey / **brick red**

**The Clement Works**

- Crewe — white hair / **deep burgundy** (with a brass seal and ferrule)
- Kerrow — black hair / **ice blue**
- Ivo — iron-grey cropped to the scalp / **oxblood** (warden's half-cape)
- Orrin — brass-coloured hair / **mustard yellow**
- Hanna — dark brown hair / **white** (sergeant's cross-belt)
- Arden — sandy hair / **oxblood** (constable's cord)
- Emmet — mousy brown hair / **mustard yellow** (livery cuff band)

**The Even Hand**

- Tovin — no hair, ochre skin / **chalk white**
- Tibb — black hair / **turquoise**
- Sela — ash-blonde hair / **plum purple**

**Windrow**

- Anneke — greying dark-brown hair / **faded rose pink**
- Aldo — white hair / **tram green**
- Ovel — steel-grey hair / **bone white** (sanatorium coat)
- Arro — no hair, dull-green skin / **copper green**

**Tellwater**

- Sem — no hair, slate-grey skin / **ochre yellow**
- Neve — brown going grey / **cornflower blue**

**Ninefold Terrace**

- Dov — pale gold hair / **mustard yellow** (he is wearing the company at his aunt's table, which is
  the point of the design)

**Braid and the shoreline**

- Ilven — white-silver wire-hair / **deep teal green**
- Cassa — ash-violet wire-hair / **coral pink**

**The Flats**

- Nessa — white-grey hair / **rust red**
- Ilsa — iron-grey hair / **cobalt blue**

**Emberrow**

- Ross — straw-blond going white at the front / **oxblood leather**
- Dalla — black hair / **dusty pink**

**Vantage**

- Idda — dark auburn hair / **plum purple**
- Corrie — black hair / **sleeve pink**

**Near-misses worth a second look if the art comes back muddy**

- Bron (copper-red / burnt orange) beside **Orrin** (brass / mustard) — both warm metallic Kell in
  the same panels from ch.2. Silhouettes carry it: Bron's big spiky hair and headband against
  Orrin's cropped brush, cut-off sleeves, burn scars and survey rule.
- **Ivo** and **Arden** are both oxblood, deliberately: it is the same uniform, and rank is the only
  difference. Hair (iron-grey shaved vs sandy under a too-big cap) and build separate them.
- **Idda** (plum) and **Sela** (plum) never share a scene; Sela is blank from ch.6 and Idda is ch.7+.
- **Sefa** and **Ilven** are both silver-haired Vess elders and they share the ch.15 refusal. Sefa is
  pinned up with two pencils in company black and brass; Ilven is waist-length with nine bone pins
  in a teal robe, barefoot.

---

## 2. Reference sheet priority order

Bron and Lyra already exist. Generate in this order; the first block covers chapters 1-3 completely.

**Tier 1 — chapters 1-3 (needed before any more scenes are drawn)**

1. **Zeph** — rewritten to full Vess; the existing design is not usable as-is.
2. **Pip** — light rewrite, but she is in every Act I panel.
3. **Tovin** — ch.1 hall, and the last image of the game.
4. **Emmet** — ch.1's punchline, the tent behind the door.
5. **Orrin** — ch.2's entrance and dinner; the first face of the Works.
6. **Ollo** — ch.3's floury hand on Bron's shoulder, then a party member from ch.11.
7. **Ivo** — ch.3, stepping aside on the ridge.
8. **Sem** — ch.3 at the dark well, ch.11, ch.15.
9. **Hanna** — ch.3 with the Pale, and recurs to ch.13.
10. **Tibb** and **Sela** — ch.1 hall banter, and ch.6 is only a punch if we have seen them.

**Tier 2 — Act II (chapters 4-6)**

11. **Ket**, 12. **Hesk**, 13. **Cassa**, 14. **Ilven**, 15. **Dov**, 16. **Kerrow**
(ch.6 in the dust), 17. **Maren** (glimpsed in ch.6, so the design must exist before the reveal),
18. **Nessa**.

**Tier 3 — Act III (chapters 7-10)**

19. **Sefa**, 20. **Crewe**, 21. **Corrie**, 22. **Idda**, 23. **Ross**, 24. **Dalla**,
25. **Ilsa**, 26. **Anneke**, 27. **Aldo**, 28. **Ovel**, 29. **Arro**.

**Tier 4 — Acts IV-V**

30. **Arden** (ch.7 gate, but his only panel that matters is ch.11), 31. **Neve**.

Everything else in the game is covered by the `Uniforms and recurring extras` lines and needs no
sheet.

---

## 3. Ambiguities resolved (reverse any of these freely)

1. **Handles.** The tool matches a handle as a whole word anywhere in a panel description, so
   English words are unusable as names. Renamed for that reason: Warden Ivo Teach → **Ivo**;
   Orrin Cadder → **Orrin** ("Cadder Step" is a village and would false-match); Doctor Ovel Marsh →
   **Ovel**; Ross and Dalla Kettle → **Ross** / **Dalla** ("kettle" is a town, a cistern town, and a
   thing that boils); Corrie Fallow → **Corrie** ("Fallow" is a month). Sergeant Hanna Kesk →
   **Hanna**, not for the tool but so no writer confuses *Kesk* with *Hesk*. Hyphenated Onn names
   shorten to the personal syllable: **Ket**, **Tovin**, **Sem**, **Arro**.
2. **Sefa Quill is filed once, in the party**, since she becomes playable in ch.13. The Works
   section therefore lists four leaders, not five.
3. **Bron's look line is 90 words**, over the 45-80 word rule I applied to everyone else. It is
   approved art and was not touched.
4. **Bible looks were kept and only extended where it was silent about colour.** Invented: Kerrow's
   ice-blue scarf and gloves (the visible form of the one fear the bible gives her); Orrin's mustard
   neckerchief; Ivo's oxblood half-cape; Crewe's burgundy waistcoat.
5. **Clement Works livery is now fixed** in the Design direction: charcoal stand-collar coat, brass
   number plate at the throat, mustard-yellow band at the left cuff, black boots; senior staff the
   same cut in company black with no band; field crews cut the sleeves off. The bible only said
   "a grey coat with a stitched brass number at the collar."
6. **The Pale get rank marks**, invented, because the bible only names the grey coat: constable an
   oxblood shoulder cord, sergeant a white cross-belt, warden an oxblood half-cape over one brass
   shoulder plate. This lets a scene writer say "a Pale officer" and get a readable silhouette.
7. **The Even Hand gets one shared object and no uniform**: a flat brass token stamped with an open
   hand, at the left wrist. It is the only thing a patronless guild would agree to wear.
8. **Onn skin colours and lights.** Ket is bone-white at five lights (bible). Tovin is ochre at two
   lights each side (bible). Arro is dull green at one light (bible). **Sem-Dree's colour and count
   are invented** — slate grey, six of eight — because the bible gives her neither; change freely,
   but she should be the brightest of the four, since she is the only one still on a well she can
   reach.
9. **"Tellwater green" is defined as a soft sage green**, so Ollo's apron does not collide with
   Pip's olive or Hesk's pine.
10. **Sela's look does not change when she goes blank in ch.6.** Same sheet, different acting: the
    bible's instruction is that a blank must never read as a zombie, so the difference is entirely
    in the eyes and mouth of the acting line, never in the design.
11. **Ages are carried by posture, hair and dress only** — a straight back, a combed-flat parting, a
    coat kept for best, a cane leaned on rather than needed. No wrinkles anywhere, since the
    refsheet AVOID block rejects them.

---

## 4. Things the bible does not have, and someone will want

Listed in the order they will bite.

1. **A named resident of Thurn.** Ch.14's whole emotional load is Ollo talking to the people of
   Thurn, and there is nobody for him to talk to. One handle, one look, one voice would fix it.
   Suggestion: an adult who has lived twenty-eight generations' worth of city and has never held a
   memory older than a week; the look should be *old clothes in perfect repair*.
2. **The ch.2 crew chief** ("a cutting rig and a crew chief who will not stop work for anybody") is
   a boss with a face and no name. Either name him, or the Works bore hand extra covers him.
3. **Recovery Chief Ovey** (ch.8's boss) is named in the bible but never described and appears once.
   Covered for now by the Pale officer extra. If he gets panels, he needs an entry; the
   frame-bearers are Pale troopers with a brass yoke in a case.
4. **The nine-year-old girl in side contract 5** ("Collection at Emberrow"). She is the most
   important face in that contract and has no name. The care-house child extra will draw her, but
   the scene writer should be told whether she may be named.
5. **The dying old Pale sergeant in side contract 7** ("Sit with him"). Same problem: a whole scene
   built on a man with no name and no face.
6. **Hallam Clement**, the founder. He is dead, but a company city will have his portrait on a wall
   somewhere in Tier Five or Six, and somebody will write that panel.
7. **Tovin's counterpart at the Braid guild hall**, referenced in side contract 2, unnamed.
8. **A named Onn at Ostry Bar.** The bible's sample names include Dur-Sarn; ch.9 has Onn standing
   for days at the sunken cap ring and no one to speak.

---

## 5. Two small things for the orchestrator

- **Scene 001's staging line calls Pip "he"** ("Pip is closest to the door, Bron stands guard behind
  him"), while its acting lines call her "she". Not my file to edit; it should be fixed before that
  scene is drawn, because the staging text goes into the prompt.
- **Scene 002's dialogue calls the hall-guard "old magic."** The bible bans the word *magic* and
  hard-rule 14 makes the guards a running recording, not sorcery. The brief says 001 and 002 may be
  rewritten; Zeph's new voice (the Vess "I have it as", the count-correcting) would carry that line
  much better as something like "That is not old. That is nine hundred years past old."
