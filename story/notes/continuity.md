# continuity.md — the continuity editor's pass

Sixteen chapters by fifteen writers, merged into one story. Every scene file was checked; the
chapter joins, every quotation of frozen wording, every repeated line, the two branch files, the
speaker list, the location keys and the playlist were worked end to end.

**Validation.** `./story_prompt.py check story/scenes/*.md` passes on all 326 files except
`000_TEMPLATE.md`, which is the template and fails by design. `stats` and `export` both succeed and
the exported intro is unchanged. `git diff --stat story/scenes/003_the_warning.md` prints nothing.

**Counts.** 326 scenes (was 323): 111 panel, 192 talk, 23 narration. The three extra files are the
branch splits and they are all optional talk scenes, so the art budget is untouched at 111 sheets.

**Every ruling is in `story/canon.md` section 11, one line each**, numbered 1-56. This file is the
narrative account of what changed and where; canon is the list you obey.

---

## What changed, by chapter

### Structural, across the whole game

- **`story/playlist.md` rewritten.** `## intro` is byte-identical. Below it, `## chapter01` to
  `## chapter16`, every scene stem in play order, `(optional)` on 42 of them and `(branch: ...)` on
  five. Chapter 1 keeps the hand-ordered sequence that interleaves the five legacy ids (`p01`,
  `001`, `002`, `003`, `003b`) at their right points; chapters 2-16 are numeric order.
- **`story/canon.md`** gains section 10 (Branching scenes) and section 11 (Continuity rulings), and
  a note under section 8 that the scene count is now 326.
- **`story/locations.md`** gains **39 keys** and a rebuilt alphabetical key index (311 keys).
- **`story/characters.md`** gains five entries and four recurring extras.

### Chapter 1

- `0135_the_roof_tank` now uses a real key, `sallowgate_roof_tank` (Bex's roof, not the yard under a
  raised cistern), and `0190_the_walk_out` uses `sallowgate_hill_path`. Both were flagged by the
  chapter 1 writer.
- `0145_the_stove` is **no longer optional**. It is the only scene in which Sela Marrin is liked
  before chapter 6 takes her, and `0670` and `0689` are both mandatory.
- `0195_under_the_seal` was mid-morning in its Beat and "lamplit evening" in its location line; it
  now uses the new `even_hand_hall_morning` key, which the epilogue also needed.
- New keys for the two ch.1 state variants the writer worked around inline:
  `halls_door_mouth_exterior_early` (no company tent yet) and `halls_broken_corridor_held` (the
  guards still holding the halberds).

### Chapter 2

- The **tram conductor is now Corm Hallet**, a cast entry, and `0225`'s Beat — which described his
  half of the argument as "the laundry's" and never mentioned him — now says who he is.
- Three keys added from the writer's list: `windrow_survey_office_door`, `windrow_drying_yard`,
  `hill_road_above_windrow`. Also `survey_cut_rig_floor_night`.
- Cadder's "Who pays the terraces this winter? / Not you. You're leaving on Thursday." is repeated
  verbatim at `0525` and **stays repeated**; both writers intended it and it is his one argument.
- Arro's flat file (`0255`) is finally collected — see chapter 16.

### Chapter 3

- `0340_can_a_town_be_wrong` loses Lyra's "I'd like to stop arguing and go in. I'd like that to be
  because we decided." The line is given to `0570`, which is where the door is. `0340` ends the
  fight with "That's enough for tonight. None of us is going to be cleverer at one in the morning."
- `0310_the_welcome`'s Beat now describes the townsman and the townswoman who use the nickname, so
  the two one-off speakers are drawable.
- `0405_down_the_stair` (ch.4's opening narration, but the ch.3→4 join) no longer ends on a sail
  "coming on fast" that `0410` cannot see and `0415` is supposed to introduce.
- `tellwater_inn_back_room` added as a key; chapters 11 and 15 will want it.

### Chapter 4

- `0492`: Cassa's "under the hills by three" is now "by ten", which is where a four-hour run from
  first light actually lands, and matches `0505`'s early sun.
- `0440`/`0455` now use a real key, `braid_even_hand_hall`, instead of the generic
  `even_hand_chapter_board` fixture. `braid_chant_hall_steps` added for `0460`.
- `salt_open` added for `0410`; the ch.4 writer asked for it by name.
- The **money substitution is confirmed** (canon ruling 43): the poster never paid the hall, and
  Tovin walks three days to hand back the hall's own tenth.
- `0465`'s Beat called the rail hand a "shed hand"; it now matches its own speaker label and says he
  is Vess.

### Chapter 5

- `0597_pip_and_the_key` loses "Sixteen years and one pair of boots. They were very good boots." The
  line belongs to `0694`, which is where her jokes run out. `0597` keeps its shape with a joke about
  the two objects instead: *"Yours has got the whole village cut into it. Mine's got a cobbler's
  mark inside the heel."*
- `0505`: the sledge sets **five** people down, not six. Cassa is the sixth and she stays with it.
- **Tarn** (Tarn Bly) gets a cast entry — seven lines, and every line the council speaks — and
  `0530`'s Beat now says who he is rather than calling him "the council".
- Keys added: `ninefold_steps`, `caleth_stump_dusk`, `hill_path_above_the_flats`.

### Chapter 6

- `0605_the_crossing` now covers **four days** (two off the hill paths, two across the crust), which
  is what `0598` promised; it said two.
- `0694` keeps the boots line (see chapter 5).
- Keys added: `saltmouth_counting_house_alley` (the ch.6 writer's one invented sentence that
  reaches the art), `saltmouth_town_edge`, `bore_four_head_night`, `bore_four_gate_office`.
- `0607`'s fouling is confirmed as written — the blocked inflow is the work, the man in the cistern
  house is present, does not speak, and nobody looks further.

### Chapter 7

- **The Vantage founding-hall keeper is now Stobb** (Garrick Stobb), one man across `0725`, `0795`
  and chapter 12's `1220`, which previously carried two different spellings of "Hall-keeper" and
  read as two people.
- `0795_wet_paste` moves to the new `tier_two_notice_wall` key — three of its five panels are the
  pasted wall, and it was borrowing the founding hall's interior line.
- `0760_the_school_wall` is **no longer optional**: its fee is collected on screen in the mandatory
  `0795`, and the mandatory `1235` hands over a tier pass on the strength of it.
- `0705`'s narration had the party riding a cart and walking on the same three mornings. They ride.
- Keys added: `tier_three_fair_closed`, `registry_catalogue_room`, `vantage_stair_flight_night` was
  not needed (the inline light change is allowed).
- Sefa's fee mechanism, the mirrored Vantage staging and the decision not to use the winter story at
  `0785` are all confirmed. Chapters 10 and 12 already stage Vantage the same way; nothing flips.

### Chapter 8

- **`0845_after_the_round` is split into `0845a` / `0845b` / `0845c`**, one file per outcome of the
  side contract "Collection at Emberrow", each a complete valid scene, each stating its selector in
  the first paragraph of its `## Beat`. `0845c` (they did the job) is the default if the game cannot
  yet choose. The original file is deleted.
- **The collector is now Venn** (Sennet Venn), a cast entry — eleven lines across two scenes. Named
  Venn rather than the outline's "Aldo Venn" because Aldo is a handle and would false-match.
- **Ross Kettle's first wife is Enna**, not Wenna. `0880` is corrected to match `1230`.
- Keys added: `emberrow_kettle_kitchen`, `emberrow_kettle_house_night`, `emberrow_main_street_night`,
  `undercroft_stair_foot`.
- `0905`'s "she's four days out" is now two: Maren is still in the Kettle kitchen at the end of
  `0895` and the party leaves the next morning.

### Chapter 9

- `0940_eleven_years_of_answers` **cuts the third telling of Bron's winter to a glance**: "Being
  nine, and lost above Tellwater in the snow. That's the first one I'm sure of." The story is now
  told once at `0130`, retold once at `0545`, recited by Maren at `0985` and named as hers at `1435`
  — which is exactly what the story editor asked for and what four chapters had quietly broken.
- **The salt company agent is now Bost** (Ivet Bost), a cast entry.
- `0920_the_roof_at_night` uses the new `survey_crawler_roof_deck` key, which the writer said every
  Act IV travel scene would want.
- `0935`'s Beat now describes the three camp speakers; its `Mother` is relabelled `Camp mother` so
  she cannot be read as chapter 8's mother.

### Chapter 10

- `1090_the_tram_platform` put the party on a tram to Vantage; chapter 11 opens in Tellwater. The
  tram is now the **eight-ten east, calling at Tellwater and nowhere else**. Bron's closing line
  about the crawler plate "before Vantage" still stands, and `1195` still points there.
- The intake form's "Tellwater, house thirty-one" is blessed and stays the only invented field.

### Chapter 11

- `1105`: Tellwater welcomed Bron **three** months ago, not five. Chapter 1 opens on the 2nd of Turn
  and chapter 11 on the 6th of Dust; on 45-day months that is three.
- `1145` panel 7 drew the flooded hollow with "nothing standing in it", which contradicts chapter 3
  and the `tellwater_low_ground` key. The intended meaning was *no memorial*, and the panel now says
  so: "the broken roof of a shed above the surface … nothing put up on the bank, no marker".
- `1195`: Tibb has the cart back at Sallowgate **by the fifteenth**. The eleventh is impossible on
  the story's own four-day figure from Tellwater.

### Chapter 12

- `1220`'s "Hall keeper" is Stobb (see chapter 7).
- `1295`: Braid is **seven thousand** people, not four, matching `1305` and `1310`.
- Keys added: `tier_six_corridor`, `tier_four_dormitory_corridor`, `windrow_pale_post_room`
  (the only drawn location in the chapter that was not a key), `sanatorium_office`,
  `windrow_boarding_kitchen`.
- The Cap's house guard is added to the recurring extras.

### Chapter 13

- **`1340_four_hours` is split into `1340a` / `1340b`**, one file per outcome of "Walk the line",
  each carrying the common closing beat. The original file is deleted. Neither is the default.
- `1300`: the clearance runs **fourteen days from the thirty-fourth of Dust**, which lands day
  thirteen on the 1st of Fallow, where the chapter opens. Kerrow's orders stay dated the 20th.
- `1385` stands exactly as written: the inner doors shut, the guards re-formed, the party turned
  back. See the join ruling below.
- The "eleven words / thirty words" conflict was already resolved in this chapter's favour of
  chapter 14 before I arrived; `1380` reads "Thirty words". Only `story/outline/act4.md` is stale.

### Chapter 14

- **The `1385` / `1405` join is ruled** (canon ruling 1): the Ninth Door is the **outer hall-mouth**
  at the maintenance-rail landing, with the mile of Deep Stair dropping from it; `1380`/`1385`'s
  inner doors are at the end of a separate level gallery off the same line. Chapter 13's party ran
  down the gallery and was stopped; chapter 14 comes back to the landing and takes the stair. The
  inner doors stay shut and are never passed, which is what keeps `1440`/`1445` the first use of the
  relief formula in the game — the alternative readings all spend that set-piece a chapter early.
  `1405` now reads as a return in its Beat and in one new line of Bron's.
- `1400`: **ninety-six** hours since Braid walked out, not thirty-one.
- `1415`: the lit street has a hundred and ninety-one lamp columns, not two hundred and six, which
  is Lyra's count of hours and the Braid sledge count and was doing too much work.
- `thurn_choir_archway_barred` added for `1430`, which was pasting the opened state with the bar
  put back.
- **`Nine` resolves to `Nona`** through the existing alias. `stats` reports no warning and `check`
  does not flag it. Nothing in chapters 14 or 15 needs changing.

### Chapter 15

- `1520`: Zeph was putting **question 7 of the Tally, verbatim**, to Maren, which breaks canon's
  standing rule that no scene outside the Tally table quotes it and borrows chapter 4's tool in
  chapter 1's words. It now uses `0465`'s own phrasing: *"Maren. Yesterday, then. And the day before
  that one?"* — which is also the correct instrument, since the scene is the edges test.
- `1520` also carried a silently invented location sentence blending two Choir keys; it now uses
  `choir_chamber_floor`. `choir_chamber_gallery` is added for `1510` and `1535`, which shared a
  byte-identical invented sentence.
- `1585` keeps Ket's responses only; `1490` owns the maintenance wording.

### Chapter 16 (epilogue)

- **Eleven weeks stands.** No scene ever said eleven months; only `story/outline/act5.md` does.
- `1605`: the Long Bore head went cold on the **eighth** of Fallow. On the seventh it was still
  running at full (`1500`), and Cadder shuts it off on the way down in `1565`.
- `1655` uses the new `even_hand_hall_morning` key — the last image of the game, and the writer
  asked for it by name.
- `1620` uses the new `cadder_step_school_yard` key, which puts the school, the clinic and the
  bunkhouses in one frame without a derrick in it.
- **`1630` collects Arro-Vintry's one request in the whole game** — a flat file, second cut, eight
  inches, asked for at `0255` in chapter 2 and never paid. One box, written so it plays whether or
  not the player took the optional scene.

---

## What I could not resolve

1. **The story is not internally dated between chapters 2 and 10.** Canon forbids inventing dates
   there, so several elapsed-time figures ("three months", "two days out", "four days from the
   crust") are reconciled to each other and to the calendar at both ends, but they are not anchored.
   If the owner ever fixes a date in that stretch, they should be re-checked as a set.
2. **`story/outline/act4.md` and `story/outline/act5.md` are stale** on "eleven words" and "eleven
   months". The outlines are outside my edit permissions and are explicitly history, but anyone
   reading them will trip.
3. **The branch selector does not exist.** I have split the files and marked them, but the game
   still plays a playlist top to bottom. Until a selector lands, `0845a`/`0845b` and
   `1340a`/`1340b` must not be in a shipping list; only `0845c` is safe to default to.
4. **`Clerk` (34 lines, six scenes) and `Guard` (17 lines, four scenes)** are each several different
   people wearing one label. No alias can fix that and naming six clerks would be worse than the
   problem. They stay one-off labels with their Beats describing them, and a script editor should
   know that a player will see "Clerk" six times and may reasonably think it is one person.
5. **Optional plants.** Chapter 3's `0320`/`0325` are optional and are inside the range canon's
   plant table treats as the setup for chapters 11 and 16; chapter 4's `0489` is optional and is
   where strands become buyable. I made `0145` and `0760` mandatory because their payoffs are
   on-screen and mandatory, but I did not touch these two, because both are genuinely game-systems
   decisions and the canon rulings already mark the second one "owner may overrule".
6. **The art budget is still 111 panel sheets** against D3's 90-110 target. Nothing I did moved it,
   and canon's cut order (`0555` first, then `0487`) still stands.
7. **Five new cast entries mean five new portrait sheets.** All are marked "portrait only; low art
   priority" and none is named in any panel, so none of them reaches a panel prompt — but somebody
   has to generate five more reference images, or accept five speaker boxes with no portrait.

---

## The ten things a script editor should read first

In order. These are the weakest places in the manuscript, not the least finished.

1. **`1385_the_inner_doors` and `1405_the_ninth_door` together.** The join now works, but it works
   because of a ruling about geography that no scene states out loud, carried by one line of Bron's.
   If a reader gets the two doors confused, the last third of the game has a hole in it. This is the
   single most fragile thing I touched.
2. **`0845a` / `0845b` / `0845c`.** Three endings to a scene that the game cannot yet choose
   between. `0845b` in particular is now a five-box scene that has to carry a cost the player may
   never have seen coming, and it is the branch with the least on the page.
3. **Chapter 13 entire, and `1350_ollo_asks` above all.** The story editor called 13 the thinnest
   chapter in the game and nothing since has thickened it. Its centre is a conversation about a dead
   woman between a man who never met her and a man who cannot describe her, and it either carries
   the chapter or the chapter is an errand.
4. **`0940_eleven_years_of_answers`.** I cut the winter story out of it an hour ago. Ilsa Tremmel's
   scene now turns on a glance instead of a telling, and somebody who knows the chapter should read
   whether her delight still has enough to land on.
5. **`1520_everything_we_are_good_at`.** Eight characters each fail at the thing they are best at,
   in fifteen boxes. It is the most compressed scene in the game and the one most likely to read as
   a list. Zeph's probe changed today, too.
6. **`1275_the_confession`.** Eleven boxes of a man reading a document, deliberately exempted from
   the line limit. It is either the best scene in the game or four minutes of a chairman talking,
   and there is no version of it that is nearly right.
7. **`1175_the_hour`.** Fifteen boxes, eight of the fifteen questions, a death. Everything in
   chapters 12-15 is paid out of it. It cannot be nearly right either.
8. **`0115_the_last_hour`.** The other end of the same rope: the only time the player hears all
   fifteen questions, in chapter 1, at a bedside, before any of it means anything. If the player
   skims it the whole ladder is weaker.
9. **`1435_the_boy_who_got_cold`.** Bron's decision about whose childhood he is carrying, in four
   boxes, with a joke in front of it. It is the payoff of the most-planted thing in the game and it
   is very short.
10. **`1085_the_tea`.** Eight boxes, two people the same height who do not know what they are to
    each other, and nothing explained. It is the quietest scene in the game and the easiest to
    ruin by adding one line.

---

## Where to look things up

- **Rulings, numbered:** `story/canon.md` section 11.
- **Branch selectors the game needs:** `story/canon.md` section 10.
- **Play order, optional scenes and branches:** `story/playlist.md`.
- **New cast:** `story/characters.md` — Stobb, Venn, Bost, Hallet, Tarn, plus four extras.
- **New location keys:** `story/locations.md` — 39 added, key index rebuilt.
