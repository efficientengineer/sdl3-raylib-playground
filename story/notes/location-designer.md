# Notes — location designer

Companion to `story/locations.md` (227 paste-ready `- location (key)` lines across 32 places).
Everything here is a decision the orchestrator or the owner may want to reverse. Nothing here is
expensive to change until art exists.

---

## 1. Places the bible mentions but under-describes, and what I decided

**The Ancient Halls, past scene 003.** The bible gives the corridor and nothing else. Decided: every
Halls surface is *dressed* cut stone with square tooling marks — nothing crumbling, no cobwebs, no
rubble except where something has actually collapsed. At most **one light source in frame**, always
below shoulder height (one guttering wall torch, or one carried lamp), so everything above head
height is black. Staging is frozen to scene 003 everywhere under the hills: doors, guards and deeper
are on the **right**, party and the way out on the **left**.

**`halls_upper_corridor` versus scene 003's frozen line.** Scene 003's line is 18 words, below the
20-40 word floor this file requires of itself. I did not touch scene 003. My key is a strict
expansion of it: same corridor, same open door on the right, same single guttering torch, plus
tooling marks and dust. **Decide:** new Halls scenes use my key; 003 keeps its own line verbatim.

**Thurn's light.** "Lit, swept, warm, fed" with no source given. Decided: **stone lamp columns** down
the street and lamp panels in house ceilings, warm and steady and running for a thousand years. There
is **no fire and no torch anywhere in Thurn** — that is the single visual fact that separates it from
the Halls and from every surface town, and it is why the place reads as maintained rather than
haunted. Water runs clear in an open channel beside the street.

**The Choir chamber.** Named only. Decided: a worked oval chamber of concentric stepped tiers going
down to a sunken floor disc, built up against a raw seam face that fills the back wall. A ring of
brass sockets flush in the disc marks the node. The two light sources — the face at the back, the
company's lamp stanchions at the stair behind the party — face each other, which is the staging of
the whole chapter.

**What a skywell looks like.** "Towers of stacked stone rings" only. Decided: enormous flat dressed
rings stepping slightly inward as they rise, **no ornament, no doors, no windows** on the exterior;
hollow drum inside, open to the sky at the top. A running well is **not brighter** — the only tell is
air trembling at the ring joints. This keeps hard rule "nothing acts" intact visually.

**The Onn cradle (ch. 14).** Decided: a long vaulted room of empty glass-fronted tanks on brass
frames with their pipework cut through and capped, one tank broken inward. The sabotage has to be
legible in one panel, because Ket does not get a second look at it.

**The setting frame and the holding frame.** Decided they are the same object at two scales: a brass
yoke. Portable, on a folding stand, in the undercroft and in care-house intake rooms; doorway-sized,
on a floor gantry, in the sanatorium. The player should recognise the ch. 10 boss as a bigger version
of something they saw in chapter 8.

**Vess architecture.** Not described. Decided: tall narrow stone houses with **shuttered openings,
not glazed windows**; stepped floors; high slit openings that throw light in bars; chant-halls seat
people in rising tiers around a shallow water basin, lit at seat height so faces light from below.

**Kell architecture.** Implied only. Decided: low flat-roofed stone houses on ledges, broad doorways,
everything proportioned wide rather than tall, dry-stone field walls stepping down the slope.

**Care-houses.** "Clean, warm, well-fed" only. Decided all five share one dormitory image — twenty
small beds in two rows, a central stove, **a shelf of identical enamel cups** — so the player
recognises a care-house on sight in Vantage, Emberrow, Windrow, Lomm and Saltmouth. The intake office
holds a cloth-covered brass yoke in the corner, and nobody ever mentions it.

**Company livery, given that images may carry no words.** Decided: slate grey with brass. Painted
company boards are **blank** — the colour and the brass fittings identify the Works, never a name.
This also solves Tellwater's canon "new sign": a freshly painted blank board over the bakery, whose
newness is the tell.

**Tellwater's drowned low ground.** The bible says they drowned the bore and the low ground to make a
flood story true. Decided this is visible from the town: a shallow flooded hollow east of the square
with drowned fence lines and a shed roof above the water. It is Tellwater's one visible problem.

**Vantage Tier Five, "the best-lit rooms in the Basin."** Decided: the Long Office has a **glazed
roof** and flat daylight from directly overhead, and the Registry stacks behind it have no daylight
at all. The contrast is the point and it costs nothing to draw.

**Ostry Bar's Onn.** "Come here on foot and stand for days." Decided: standing figures **spaced
apart**, never grouped, never facing each other. Reads as a place of work, not a vigil.

**Emberrow's stone.** Built from quarried Kestrin. Decided its housefronts are a darker, denser grey-
brown than Rim Hills sandstone, and the Kestrin quarry still shows curved ring blocks bedded in an
arc in the floor, so the two places are visibly the same stone.

---

## 2. Places a twenty-hour game needs that the bible lacks

Per bible decision 14 I have proposed **no new towns and no new skywells**. All of these are camps,
waystations, farms or halts. Six already have keys in `locations.md`; two are proposals only and are
marked as such.

1. **The Fold** *(proposal — no key yet)*. A shepherd's fold and drift camp in the high ground above
   Tellwater. Maren's own line is "you got lost in the drifts above the fold," so the bible already
   assumes it exists. Worth one establishing panel in ch. 3 or ch. 11 so that the ch. 9 recitation
   has a picture attached to it. **This is the only place in the game with a strong reason to exist
   that I did not write, because I did not want to invent the geography of Bron's worst memory
   without the cast writers.**
2. **The Tellwater relay hut, two miles out** *(proposal — no key yet)*. Canon in the timeline: the
   telegraph acknowledgment failed here and a rider went to Windrow. A single plank hut with a
   telegraph pole would pay off enormously in ch. 15 when the 22:02 log is produced.
3. **A drove halt between Kettle and Saltmouth.** Ch. 6 opens the drove roads and there is currently
   nothing on them. Use `kettle_drove_road` plus `company_meter_box` as a halt until someone wants
   more.
4. **The ferry-rail siding camp** (`ferry_rail_siding`). The bible does not say sledges stop
   overnight, but a hundred miles of dry seabed says they must. This is Act II's campfire scene.
5. **The doorward waystation on the terrace path** between Sallowgate and Ninefold Terrace. Use
   `rim_road` and `terrace_stair` for now; if ch. 5 wants a stop, say so and I will add keys.
6. **The Pale Flats survey camp** (`pale_flats_cordon`). The ch. 9 sweep needs a base; the cordon key
   doubles as it.
7. **Working and failed terrace farms** at Ninefold (`ninefold_fields`). Half bare earth in the same
   frame as half standing barley, so the settlement's problem is in the establishing shot.
8. **A company road halt** (`company_road` plus `pale_post`). Act III travels metalled roads for four
   chapters; without a halt every road scene is the same shot.

---

## 3. The fifteen most visually striking establishing shots in the game

In chapter order. These are the ones worth spending a full panel and the owner's attention on.

1. **Ch. 1 — `halls_door_mouth_exterior` at dawn.** A canvas company tent pitched in the mouth of a
   thousand-year-old door. The whole game in one image, and it is the first one.
2. **Ch. 1 — `halls_upper_corridor`.** The approved one. Cut stone, an open door onto black, one
   torch.
3. **Ch. 3 — `tellwater_square_day`.** Forty scrubbed housefronts and three people. Too clean.
4. **Ch. 4 — `braid_wide`.** A Vess town stepping off a terrace lip onto a white seabed with rails
   running out to a flat horizon. The best pure landscape in the game.
5. **Ch. 4 — `braid_annex_exterior`.** A company grille bolted across a Vess doorway with a queue
   along the wall. One image, one grievance.
6. **Ch. 5 — `ninefold_cliff_wide`.** Nine squared doors in a row along a cliff foot with a house on
   the ledge above each. A whole culture as a single tall face.
7. **Ch. 6 — `bore_four_surface_after`.** A ring of stilled dust spreading across white salt, a
   dropped bucket, no bodies. The bible is right that this must have no violence in it.
8. **Ch. 6 — `semmet_stump`.** A bore derrick driven through the floor of a dead skywell.
9. **Ch. 7 — `vantage_from_below` at dusk.** Six stacked bands of warm windows in a quarry face.
   The only warm thing in the Basin, seen from outside it.
10. **Ch. 8 — `emberrow_ravine_wide`.** Rope-walks strung overhead and smoke lying flat below the
    roofline, sun only on the top courses.
11. **Ch. 9 — `ilder_stump_outside` at dawn.** A mile of toppled rings half sunk in salt, to the
    horizon, with birds on the highest one.
12. **Ch. 10 — `sanatorium_holding_stock`.** Drawers of paper sleeves. The quietest horror image in
    the game.
13. **Ch. 13 — `long_bore_head_chamber`.** A brass head the size of a house bolted across an ancient
    doorway, with the dust not moving.
14. **Ch. 14 — `thurn_first_view`.** A lit, swept, thousand-year-old street underground, and it is in
    better repair than anywhere on the surface.
15. **Ch. 15 — `seam_face_great`.** A wall of packed filament in strata like grain in wood, lighting
    a stone ledge from the back of frame.

---

## 4. Decisions the orchestrator must make

1. **Time of day underground.** Roughly a third of the file is underground, where "time of day" is
   meaningless. I used `no daylight`, `night underground`, or the state of a carried lamp as the time
   marker. Confirm that satisfies the requirement, or I will add a surface time to each line.
2. **Blank company boards.** No image may carry words, so every company board, shop board and notice
   in this file is described as *painted, with no words*. If that reads oddly in a generated image,
   the alternative is to drop boards entirely and identify the Works by slate grey and brass only.
3. **`halls_upper_corridor`.** See section 1. Confirm scene 003 keeps its own 18-word line.
4. **Glazed roofs and window glass.** I gave Vantage Tier Five a glazed roof and Cadder Step's school
   glazed windows (the bible calls that school's glass a luxury). That implies the Works makes flat
   glass at scale, which fits an industry that spins glass thread, but it is an invention.
5. **Where the Ossun machinery lives in the file.** I put the ch. 12 machine hall under Vantage
   (`tier_six_ossun_head`) and cross-referenced it from the skywells section, because it is a dungeon
   before it is a landmark. Easy to move.
6. **Talk-scene backdrops depend on panel scenes existing.** The list at the end of `locations.md`
   names 30 reuses; each needs its source panel actually generated. Someone should reconcile that
   list against the final scene list once the outline writers are done — several entries assume a
   panel that nobody has committed to yet (Saltmouth after the inversion, the survey crawler cabin,
   the maintenance rail tunnel).
7. **Per-chapter index.** If outline writers would rather look places up by chapter than by region, I
   can add a chapter-ordered index of keys at the top of the file. I did not, to keep it short on a
   phone.
8. **Two places the cast writers own more than I do:** the Fold above Tellwater (Bron's lost winter)
   and the Quiet House (Teach's boarding house). I gave the Quiet House two keys and left the Fold
   alone. Tell me if I should write it.
