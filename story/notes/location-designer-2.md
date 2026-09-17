# Notes — location designer, second pass

Answers `story/notes/requests-locations.md`. `story/locations.md` now holds **272 keys** (227 + 45
new) across 33 places, plus an **Index by chapter** and an alphabetical **Key index** at the foot of
the file. Verified by script: every `- location (` line matches the format, is 20-40 words, no key is
duplicated, and no sentence carries a style, rendering or readable-text word.

---

## 1. What I added (45 keys)

**Sallowgate (1).** `even_hand_bunkroom_night` — `0115`. The same room as `even_hand_bunkroom`, at
night, eleven bunks stripped, one made up, one tallow lamp. Deliberately a lighting change on a room
the player already knows, not a new room.

**Windrow (4).** `windrow_boarding_room` (`0215`, and `0230` on the landing outside it),
`windrow_boarding_parlour` (`1015`), `windrow_chop_house` (`0285`, `0290`),
`quiet_house_upper_room` (`1225`).

**The sanatorium (1).** `sanatorium_day_room` — `1035`, `1040`. Forty chairs in loose rows, not
beds. Separate from `sanatorium_ward` precisely so nobody paints a ward when hard rule 13 wants the
day room.

**Tellwater (9).** `tellwater_lit_window` (`0330`), `tellwater_ridge_road` (`0365`-`0375`),
`tellwater_fair_pole_store` (`0320` and `1140` — one key, pole racks below, loft above),
`tellwater_relay_hut`, `tellwater_posting_office` (`1110`), `tellwater_roster_room`,
`tellwater_posting_yard` (`1185`), `tellwater_south_row` and `tellwater_empty_house` (`1125`).

**The fold, above Tellwater (1, new place).** `fold_camp`. One establishing backdrop, no interior,
no people, as canon permits. Drawn in early-morning autumn light, **not** in snow: the only chapters
that can show it are 3 and 11, and neither is winter. The drifts stay in Bron's and Maren's mouths.

**The Sealed Stair (2 new, 1 revised).** `sealed_stair_gate_chamber` and `sealed_stair_fill`
(`0595`). I revised `sealed_stair_landing` as the requests file allowed: it now carries three empty
armour niches and fallen iron segments, because `0585` is built on the emptiness. It lost "two stone
bench posts" to stay under 40 words.

**Saltmouth (2).** `saltmouth_drain_sump` (`0635`), `saltmouth_relief_tent` (`0688`, `0691`; the
dust-hanging yard of `0686` is still `saltmouth_street_after`).

**Semmet (1).** `semmet_keepers_quarters` (`0640`), filed with the skywells beside `semmet_stump`.

**Kettle (0 new, 1 revised).** `kettle_cistern_house` now shows the tank standing full and an open
inflow channel at the back, so `0607`'s job is visibly in the room. It lost the straw pallet.

**Vantage (3).** `vantage_staff_stair` (`0790`, and the night route into the Registry in `0765`),
`tier_four_cable_house` (`0760`, `1235`/`1240`), `tier_two_hired_office` (`1615`).

**Emberrow (2).** `emberrow_even_hand_hall` (`0820`), `emberrow_tenement_stair` (`0840`).

**The Pale Flats (1).** `survey_depot_yard` (`0905`) — fence, watch hut, and a stencilled company
plate on the crawler they steal, which is the plate nobody deals with for six chapters.

**Ostry Bar (1).** `ostry_bar_far_side` (`0950`).

**Braid (5).** `braid_chant_hall_back_room` (`0487`), `braid_rail_head` and `braid_rail_head_board`
(`1305`, `1310` — the board is a separate insert key because the chapter turns on it),
`braid_tram_halt` (`1320`), `braid_emptied_house` (`1350`).

**The annex (3).** `annex_index_room` (`0480`), `annex_crating_floor` and `annex_sealed_shelf`
(`1315`, `1345`).

**Lomm (1).** `lomm_inquiry_hall` (`1610`). A table across one end, one witness chair, grey light
from high windows. Not the permit counter.

**The Ninth Door and the Deep Stair (2).** `ninth_door_outside` (`1595`, last panel of ch. 15) and
`deep_stair`. The section is renamed and now states the canon correction: the Ninth Door is the
ninth door-mouth of Ninefold Terrace, so `ninefold_cliff_wide` / `ninefold_door_sealed` and
`ninth_approach_gallery` / `ninth_inner_doors` are one cliff seen from two sides.

**Thurn (4).** `thurn_well_court` (`1425`), `thurn_council_quarter` (streets, `1445`-`1455`),
`thurn_council_door_landing` (`1440`, `1450` — the ch. 14 boss room, a mile below
`ninth_muster_hall`), `thurn_choir_archway` (`1495`).

**The Choir (2).** `choir_stair_flight` (`1545`, `1565`) and `choir_stair_lower_landing` (`1570`).

---

## 2. Requests I declined

1. **Ilsa Tremmel's shack** (`0935`, `0940`). It already exists: `ostry_bar_tent` is a crate desk
   stacked with identical bound notebooks in a canvas shelter. Writing a second description of the
   same shack is exactly what the file exists to prevent. `0935`'s cook fire is `ostry_bar_camp`. I
   added a note under Ostry Bar saying so.
2. **A Clement Works road halt.** Written into the file as a ruling, not a key: `company_road` plus
   `pale_post`. The file now says so under Routes on foot so writers stop inventing one.
3. **A second Windrow boarding-house kitchen** (`1015`). `quiet_house_kitchen` is that image. I wrote
   the parlour only. If the orchestrator wants Orla Winch's house visibly distinct from Teach's, say
   so and I will split it; right now they are the same grey brick and the same range.
4. **`0945`, the salt company's shed.** Use `saltmouth_counting_house` — plank office, counter,
   pigeonholes, brass tally frame. Noted in the file.
5. **A separate key for `0686`.** The yard with the dust still hanging is `saltmouth_street_after`,
   which was written for exactly that shot.

---

## 3. Scene ids in the requests file that do not match the outlines

I wrote the key the described room needs and pointed it at the scene that actually uses it. None of
these changed a key, only the note beside it.

- **chop-house** — requested against `0230`/`0235`; those are a boarding-house landing and a closed
  survey-office doorway. The chop-house is `0285`/`0290` (Cadder's dinner).
- **annex index room** — requested against `0470`/`0475` (the flooded aisles and a dry landing,
  already `annex_flooded_hall` and `annex_upper_stair`). The index room is `0480`.
- **Vantage staff stair** — requested against `0745`, which is the *public* stair between Tiers One
  and Two (`vantage_stair_flight`). The staff stair shaft is `0790`.
- **Emberrow** — the hall over a rope-walk is `0820`, not `0850`; the tenement stair is `0840`, not
  `0845`.
- **`1090`** — listed under "boarding-house kitchen and parlour"; it is the tram platform, and
  `windrow_tram_platform` already covers it.
- **emptied Braid house** — requested against `1315` (which is the crating floor); it is `1350`.
- **chant-hall back room "recurs as `1335`/`1340`"** — those two are outdoors, on the laid rail at a
  halted sledge west of Braid (`ferry_rail_siding` fits). The back room recurs in Act V's chant
  scenes.
- **Choir stair, lower landing** — of the four ids given, `1495` is the barred archway at the low end
  of Thurn's street (ch. 14, now `thurn_choir_archway`), `1545` and `1565` are fought on the **upper**
  flight, and only `1570` is the lower landing. I wrote all three.

---

## 4. Places two outlines described incompatibly, and what I kept

1. **The Ninth Door.** Act IV wants "the approach corridor to the inner doors" reached from behind
   and below off the maintenance rail; Act V has people walking out of it into grey daylight down a
   rail cut. **Kept canon's ruling** (`canon.md` §2): one cliff, two sides — the rail arrives behind
   it, the door opens on the terrace face. Written into the section header so nobody draws two doors.
2. **The day room, twice.** Act II asks for a Saltmouth care-house day room (`0670`, forty-one calm
   people) and Act III for the sanatorium day room (`1035`), and both call themselves the most
   important visual in the game. **Kept one new key, the sanatorium's**, because that is the one the
   requests file asked for and the one hard rule 13 names. Saltmouth's should be shot as
   `saltmouth_care_house` with the beds out of frame; if the ch. 6 writer wants a real second key,
   ask and I will add `care_house_day_room` to the recurring fixtures instead of two town-specific
   ones.
3. **Tellwater's town sign (`1195`).** The outline's panel calls for "the painted town sign down and
   leaning on the well". No image may carry words, so it is a **painted board with no words**, as
   every other board in this file is. The dialogue carries what was on it.
4. **Thurn's light versus the hall-guards.** Act V gives the Standing Muster "blue points steady in
   the helms" in a room I have lit by lamp columns. I kept the columns as the only light source and
   the helm points as points of colour, not a light. A ch. 14 writer should not ask for them to light
   anything.

---

## 5. Still open

1. **Gaps outside my work list**, flagged by the act outlines but not by the requests file: a small
   **Pale post room with a duty board** (Windrow, ch. 12 — `pale_post` is the exterior only); the
   **Emberrow Kettle house interior** (`emberrow_kettle_house` is the doorstep); a **street between
   the Braid rail sheds**. Say the word and they are three more keys.
2. **The per-chapter index** is by *place*, not by key — one line per chapter, pointing at sections.
   Listing 272 keys twice would have doubled the file on a phone. The alphabetical Key index at the
   foot is the exact lookup.
3. **`even_hand_bunkroom_night` versus `even_hand_bunkroom`.** Two keys for one room, differing only
   in light and occupancy. That is the first time this file does it. If the orchestrator would rather
   have one key and a lighting note in the scene file, this is the one to collapse.
4. The reuse list at the end of `locations.md` is unchanged; the three flagged reuses now have
   committed panels per the editor's section 3, so nothing there needs cutting.
