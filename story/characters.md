# characters.md — the current cast

One `## Name` per character. The `look` line is pasted **verbatim** into every image
prompt that includes the character, so keep it concrete and visual: build, hair, outfit
colors, one distinguishing feature. Change it only when the character's design changes.
`ref` is the character's reference image for ChatGPT (make it with
`./story_prompt.py refsheet <Name>`, save the result at that path). `status` and `notes` are story state for whoever writes the next scene; they are never
sent to the image model. `people` and `voice` are for writers too; the tool ignores them.

The `## Name` is the **handle**: the one word a scene writer types in a panel description and as a
dialogue speaker. The tool matches handles case-insensitively as whole words inside panel text, so
handles are never common English words (that is why Warden Ivo Teach is `## Ivo` and Ross Kettle is
`## Ross`). Each character's full name is the first thing in `notes`.

**Design direction (owner's call):** the cast should look like the heroes of a 1993 sci-fantasy
anime JRPG (Phantasy Star IV is the touchstone), never like Western tabletop fantasy. So:
- Keep race and class words (**dwarf, halfling, elf, fighter, rogue, cleric, wizard**) and beards out
  of `look`. They drag image models toward braided beards, gritty muscle, and Tolkien armor. `role`
  keeps the story job; `look` describes only what is visible. `story_prompt.py` rejects them.
- Young adults with big expressive eyes and big hair; build comes from silhouette (short and compact,
  tall and slim, petite), not from age, bulk, or facial hair. **Age reads through posture, hair, and
  dress** — a straight back, a combed-flat parting, a coat kept for best — never through wrinkles.
- Costume vocabulary: high collars, bodysuits, long coats, half-capes, a single oversized shoulder
  plate, headbands, circlets, sashes, gauntlets, boots. One strong accent color per character.
- Original designs only. Borrow the era's archetypes, never a specific existing character.

### The four peoples, by what is visible

None of them is a species and none of them gets a fantasy word. Write only what an artist can draw.
To make a person unmistakably of their people, the `look` line names the marked features in this
order: build and height, skin, hair, ears, eyes.

- **Humans.** The baseline. Brown, tan and pale skins, ordinary black, brown, blonde and grey hair,
  round-topped ears, whites to the eyes. What marks a human in a crowd is **cloth**: they layer for
  cold, so a human on a winter street is three garments deep where the person beside them is in one.
- **Kell.** Very short and very wide — head and shoulders below a human, half again as broad through
  the shoulders. Clay-warm skin with a faint reddish undertone. Thick hair in one flat metal colour
  (copper, brass, iron-grey, rarely gold) that stands straight up and never lies flat. Amber eyes
  with a bright ring at the pupil. Broad flat fingernails. They run hot, so they are **bare-armed in
  any weather**: sleeveless coats, cut-off sleeves, no gloves unless the job needs them.
- **Vess.** Very tall and very thin, narrow through the shoulder. Ash-lilac skin with a grey cast.
  **Long backswept fluted ears with visible ribbing** that lamplight shows through. **Eyes in one
  solid colour with no whites at all** — violet, slate, amber-brown. Hair like drawn wire in cold
  indigos, ash-violets and silvers, worn long and pinned with tools or bone pins. Long hands.
- **Onn.** Grown, not born, and identical in build at any age: five foot six to five foot ten,
  narrow, upright. **Matte skin in one flat colour per individual** — bone-white, slate, ochre, dull
  green — with no flush and no variation. **No hair anywhere. No nose: two small slits above the
  mouth.** Large lensed eyes with a visible bright iris ring. **Four small lights set in a vertical
  row down each temple, eight in all**, and the `look` line always says how many are still lit. Lights
  only ever go out, never come back.

### Recurring liveries (defined once, reused)

- **Clement Works livery.** A charcoal-grey stand-collar coat, a small brass number plate stitched at
  the throat, a mustard-yellow band around the left cuff, black boots. Senior staff wear the same cut
  in company black with no band. Field crews cut the sleeves off.
- **The Pale** (company police). A pale ash-grey long coat with a black stand collar and black
  gauntlets. Rank shows on the shoulder: constables an oxblood cord at the right shoulder, sergeants
  a white cross-belt, wardens an oxblood half-cape and a single brass shoulder plate.
- **The Even Hand** (hunters' guild). No uniform at all — that is the point. Every member carries a
  flat brass token stamped with an open hand, worn on the left wrist or the belt.
- **Onn keepers.** A long grey keeper's smock, a tool roll across the chest, bare forearms,
  flat-soled boots. The individual's colour is in the sash or the strap.
- **Kell doorwards.** Sleeveless coats, one heavy shoulder strap, a wide belt of mortar tools, and a
  door-key the length of a forearm.

---

## The party

## Bron
- role: hunter of the Even Hand, axe, party leader
- ref: story/refs/bron.png
- people: Kell
- voice: Short sentences, concrete nouns, the thing first and the reason after; no irony and no idea how funny he is.
- look: Bron, a short, compact, broad-shouldered young man in his early twenties with a youthful clean-shaven face, big spiky copper-red hair swept back under a white headband with long trailing ends and a small red cross mark at its center, sharp amber eyes, a sleeveless burnt-orange high-collared jacket worn open over a black high-necked bodysuit, black trousers, one large rounded steel shoulder plate strapped to his left shoulder, oversized segmented steel gauntlets, a brown belt with a large round silver buckle, white cuffed boots, a big axe slung on his back.
- status: alive, party leader, ch.1-15
- notes: Bron Sallow. Born Tellwater 1084; carried out of the street at nine and given stock template 4 plus two live sleeves drawn from Maren. Believes only what he can put his hands on, which is the one thing that was done to him. Ch.10 he learns his own past was erased, not stored. Ch.11 he takes Lyra's last hour. **Look and ref are approved art — do not change a character of either line.**

## Lyra
- role: Attendance sister of the Order of the Late Hour, mace
- ref: story/refs/lyra.png
- people: human
- voice: Complete unhurried sentences; says the frightening thing plainly and quietly, asks one question too many, and never signals a joke.
- look: Lyra, a tall, slender young woman with very long straight pale-gold hair, long bangs parted in the center, a thin gold circlet with a small red gem on her forehead, calm grey eyes, a white high-collared long coat with gold trim and wide gold-edged shoulder pieces over a dark navy bodysuit, a short white half-cape, a gold sun medallion on her chest, long white gloves, white heeled boots, a slim silver mace at her hip.
- status: alive ch.1-11; shot in the road at Tellwater in ch.11 and dies there
- notes: Sister Lyra, of the Order of the Late Hour, posted to Sallowgate in 1106. Sits the last hour of the dying and asks the Tally, the Order's fifteen questions — which are a strand-integrity check written by the people who built the Seam, a fact she learns in ch.9 and decides makes the prayer better. The gold sun medallion is the Order's hour-token, marked with the fifteen points. **Look and ref are approved art — do not change a character of either line.**

## Zeph
- role: resonance reader, expelled annex scholar, staff
- ref: story/refs/zeph.png
- people: Vess
- voice: Fast and delighted and showing off; repeats your key word back before answering, corrects counts, says "I have it as," and gets quieter and more formal the more it matters.
- look: Zeph, a tall, very slim young man with ash-lilac skin, long backswept fluted ears with visible ribbing that lamplight shows through, solid violet eyes with no whites, long flowing indigo wire-hair with a spiky fringe over one eye, a confident smirk, a single silver earring, a deep blue high-collared long coat with silver clasps and one asymmetric shoulder cape, a wide white sash, fingerless black gloves, and a short staff tipped with a floating blue crystal.
- status: alive, in the party, ch.1-15
- notes: Zeph Quill. Expelled from the Braid annex in 1106 for reading sealed material; the annex is leased from his own people and his mother's signature is on the lease. Will not say the name Quill aloud until ch.7. Ch.15 the Braid chant-lines accept the record from him and he sings for eleven hours.

## Pip
- role: hunter of the Even Hand, locks and blades
- ref: story/refs/pip.png
- people: human
- voice: Jokes when frightened, which is always; fast, rude, generous, and deflects every personal question with a better joke.
- look: Pip, a petite, agile young woman, the shortest of the party, with tan skin, short fluffy chestnut hair under an olive-green bandana, round goggles pushed up on her forehead, large bright brown eyes and a quick grin, a cropped olive-green jacket with a tall collar over a dark grey bodysuit layered at the throat against the cold, fingerless gloves, a belt of small pouches and picks, scuffed knee-high boots resoled four times, twin curved daggers at her lower back.
- status: alive, in the party, ch.1-15
- notes: Pip Rime. Care-house surnames come from the month a foundling arrived. Left the Emberrow care-house at sixteen in 1104, cheerful, employable, and unable to describe a single birthday; she has worked out roughly why and has never said it out loud. Ch.10 she is offered her drawn days back and says no, and the game never suggests she is wrong.

## Ket
- role: skywell keeper of Meddra, joins ch.4 for passage
- ref: story/refs/ket.png
- people: Onn
- voice: Exact, literal, unrounded; says "correct" and "not correct," refuses approximations, and announces her own state without embarrassment.
- look: Ket, a narrow upright figure a head taller than Bron, with matte bone-white skin in one flat tone, no hair anywhere, no nose but two small slits above the mouth, large lensed pale-green eyes with a bright iris ring, four small lights set down each temple with five of the eight still lit, a long grey keeper's smock, a jade-green tool roll strapped across her chest, bare forearms, flat-soled boots, a brass ring-gauge in one hand.
- status: alive, joins ch.4, leaves ch.15 to restart the Vintry well
- notes: Ket-Ossun, 1,261, of the last muster. Keeper of Ossun until the Works metered it in 1082, then reassigned to Meddra. **Her lights go out across the game: five lit when she joins, two when she leaves, and no writer may light one back up.** Has not spoken to Tovin in forty-nine years; they resolve it in four lines in ch.4.

## Hesk
- role: doorward of the Fifth Door, Ninefold Terrace, spear
- ref: story/refs/hesk.png
- people: Kell
- voice: Flat and hard and declarative; names weights and distances, swears by doors, says the unwelcome thing first and does not soften it after.
- look: Hesk, a very short and very broad woman, half again as wide through the shoulders as any human her height, with clay-warm skin, amber eyes with bright rings at the pupils, iron-grey hair cropped into a stiff upright brush, a sleeveless pine-green coat with one heavy shoulder strap, bare arms marked with old rope scars, a wide leather belt hung with mortar tools, and a short broad spear and a door-key the length of her forearm slung at her back.
- status: alive, joins ch.5, stays to ch.15
- notes: Hesk Ninefold, 44. Her inherited job is keeping a sealed hall-mouth shut; three of the nine doors have been opened and re-mortared without her knowing. Joins furious that the party opened one. She is right about almost everything and is overruled every time. Ch.15 she opens the door herself and writes down who goes in.

## Ollo
- role: baker of Tellwater on a company posting, joins ch.11
- ref: story/refs/ollo.png
- people: human
- voice: Warm and talkative, feeds people mid-sentence, apologises for what is not his fault, and speaks about his own life in the third person by accident.
- look: Ollo, a middle-height man heavy through the chest and forearms, with tan skin, greying brown hair tied back off a broad flour-dusted face, kind heavy-lidded eyes, a flat cap, a white collarless shirt with the sleeves rolled past the elbow, a long apron dyed soft sage-green, a small brass numbered badge pinned at his collar that he never takes off, flour dried white on his knuckles, and a long wooden bread peel carried upright like a spear.
- status: alive, joins ch.11, leaves ch.15 to hold Tellwater together
- notes: Ollo Marrick, 47. Eleven years on a renewable posting playing the baker of a town that is a set; he signed a paper and nothing else. Terrible in a fight and surprisingly strong. Ch.13-14 he is the one who talks to the people of Thurn, because he is the only person in the party with practice at being kind to someone who does not know who they are.

## Sefa
- role: Registrar of the Clement Works; joins the party ch.13
- ref: story/refs/sefa.png
- people: Vess
- voice: Elegant, funny and devastating, speaks in filing metaphors and means them; the second tone under her voice goes flat when she lies and she does it anyway.
- look: Sefa, a tall, spare woman with ash-lilac skin gone grey at the temples, long backswept fluted ears, one ear-fan pierced with fourteen small brass rings, solid slate eyes with no whites, silver wire-hair pinned up with two pencils, a company-black long coat cut like a coat and not a uniform, brass-framed reading lenses on a chain, ink on the outside edge of her right hand, and a flat brass-cornered case of forms held under one arm.
- status: alive, one of the five company leaders until ch.12; joins the party ch.13
- notes: Registrar Sefa Quill, 58, Zeph's mother. Drafted the Tellwater resettlement instrument in two days in 1093 and was made Registrar for it in 1094; it is the best work she has ever done and she declines to be sorry in public. Carries a company seal and a pistol she has never fired and does not intend to. Ch.15 Ilven refuses her to her face and she hands the argument to Zeph.

---

## The Fair Copy

## Maren
- role: former company chief surveyor; the only saturated person alive
- ref: story/refs/maren.png
- people: human
- voice: Tired, courteous and entirely sane, with four hundred people trying to use her mouth; her accent moves mid-sentence, she apologises a lot, and under pressure her voice arrives doubled.
- look: Maren, a lean woman of middle height who stands like someone who has walked a very long way, with weathered brown skin, dark hair going grey early and cut short by her own hand, steady dark eyes, a long grey travelling coat open over a brick-red wool scarf and a patched shirt, a canvas satchel stitched in the same brick-red, boots resoled twice, and bare ungloved hands she keeps open and in view.
- status: alive, walking the Basin; met ch.8, spoken to ch.9, reached ch.13, ends ch.15 emptied and not crowded
- notes: Maren Ostry, 36. Twenty-two and standing at the draw head when Bore One inverted on the 9th of Fallow, 1093; she received all of it, four hundred and eleven lives. Held twelve years at the Windrow sanatorium under the file name *the Fair Copy* and has been walking for two. She gives memories back; she never makes them up. Never gloats, never monologues, never raises her voice.

---

## The Clement Works

## Crewe
- role: Chairman of the Clement Works
- ref: story/refs/crewe.png
- people: human
- voice: Courteous, complete sentences, never raised; apologises for the temperature of the room and asks after your family and remembers the answer.
- look: Crewe, a very tall, thinned-out old man who stoops a little, with pale skin, white hair combed flat, pale blue eyes, a long charcoal coat that hangs loose on him over a deep burgundy waistcoat and a high white collar, a black stock at the throat, a brass company seal on a chain at his breast, and a black cane with a brass ferrule which he holds in both hands and taps once before he speaks.
- status: alive; relieved of the chair mid-sentence in ch.12, house arrest ch.13, testifies ch.15
- notes: Chairman Ansel Crewe, 68. Chairman since 1076, the last man in the building who remembers the Hungry Winters. Holds the true list of where Tellwater's four hundred were sent, written in his own hand and never filed. His sin is that he can live with it. Never charged; ends answering letters in a room in Windrow.

## Kerrow
- role: Director of Extraction, then Chairman from ch.12
- ref: story/refs/kerrow.png
- people: human
- voice: Short declaratives and numbers, no small talk at all, jokes so dry that people miss them; she does not lie, she declines to answer.
- look: Kerrow, a small, very upright woman a head shorter than everyone in the room, with pale skin, black hair cut level at the jaw, cold dark eyes, a plain slate-grey coat buttoned to the throat with no braid anywhere on it, an ice-blue wool scarf she keeps on indoors and ice-blue gloves she rarely removes, ink on the outside edge of her right hand, and a slim tally board carried flat under one arm.
- status: alive; takes the chair ch.12, arrives to cap the Seam in ch.15 and is stopped by people, not by an argument
- notes: Director Ottoline Kerrow, 35. Out of the Emberrow care-house at sixteen on a clerical scholarship; has known about Tellwater since she was a twenty-one-year-old clerk who took the message. Holds the buried 1099 study proving every large bore inverts eventually. Ch.6 she stays in the dust at Saltmouth for eleven hours running the count herself, which is the most admirable thing anyone does in Act II. She is partly right and the party never refutes her.

## Ivo
- role: Warden of the Pale
- ref: story/refs/ivo.png
- people: human
- voice: Soft and one beat too slow; asks questions he already knows the answer to, uses people's full names, and never threatens, which is far worse.
- look: Ivo, a very big, slow-moving man who fills a doorway, with ruddy tan skin, hair cropped to the scalp in iron grey, small tired eyes, a healed break across the bridge of his nose, the pale ash-grey coat of the company police hanging badly on him, an oxblood half-cape at the left shoulder over a single brass shoulder plate, black gauntlets too large for anything he holds, and a plain baton at his belt.
- status: alive; dismissed, charged and acquitted after ch.15, runs the Quiet House
- notes: Warden Ivo Teach, 55. Handle is **Ivo**: "Teach" is a common word and would false-match in panel text. Sergeant at Tellwater in 1093; carried a nine-year-old four hundred yards from the bore shed doorway and has never told anyone, including his wife. Keeps a boarding house in Windrow for the widows of men who died on his jobs. Ch.11 his one conscientious refusal puts a nineteen-year-old in the road instead of him.

## Orrin
- role: Field Superintendent of the Clement Works
- ref: story/refs/orrin.png
- people: Kell
- voice: Warm, fast and funny; first-names everybody including people he is about to ruin, tells a joke before bad news and another one after, and says "we" about the crews and means it.
- look: Orrin, a short, extremely wide man with clay-warm skin and amber eyes, brass-coloured hair cropped into a short upright brush, a charcoal company field coat with both sleeves cut off at the shoulder, burn scars up both bare forearms, a mustard-yellow neckerchief knotted at the throat, a brass survey rule stuck through his belt like a sword, heavy boots, and a battered tin whistle on a cord around his neck.
- status: alive; relieved of his post in the street in ch.13, holds the stair in ch.15
- notes: Field Superintendent Orrin Cadder, 52. Handle is **Orrin**: "Cadder" is also a village name and would false-match. Brokered the Cadder Step door-rights sale at twenty-six and has never pretended it was anything but a sale. Posted the burned-seal contract in ch.1 and admits it cheerfully over dinner in ch.2. Ends blacklisted at Cadder Step, running a wage fund out of his pension.

## Hanna
- role: Sergeant of the Pale, Warden Teach's second
- ref: story/refs/hanna.png
- people: human
- voice: Crisp, correct, and obedient; she is the one who says "sir" in a particular tone and never says anything else about it.
- look: Hanna, a compact, square-shouldered woman who stands at parade rest without thinking about it, with brown skin, dark hair scraped back into a tight knot, level brown eyes, the pale ash-grey company police coat fitted properly for once, a white sergeant's cross-belt with a whistle on a chain, a black stand collar and black gauntlets, a short baton at her hip, a folded order book in her left hand, and boots polished to a shine.
- status: alive, in service
- notes: Sergeant Hanna Kesk, 38. Handle is **Hanna** so scene writers never confuse "Kesk" with "Hesk". Decent, competent, obeys. Ch.3 at Tellwater, ch.9 on the Flats sweep, ch.11 on the decommission detail, ch.13 commanding the company at the Long Bore.

## Arden
- role: Constable of the Pale, on a two-day field commission in ch.11
- ref: story/refs/arden.png
- people: human
- voice: Over-polite and audibly frightened; quotes regulations he half-remembers and apologises while he does it.
- look: Arden, a thin, long-necked boy of nineteen whose coat is a size too big for him, with pale freckled skin, sandy hair sticking out from under a grey peaked cap, wide anxious blue eyes, the pale ash-grey company police coat with a single oxblood cord at the right shoulder and the packing creases still in it, black gauntlets he keeps flexing, and a long rifle carried across his body in both hands.
- status: alive; shoots Lyra in ch.11, is never redeemed and never punished
- notes: Constable Wen Arden, 19. Checks the party's papers at a Vantage gate in ch.7 and is on the edge of the Flats sweep in ch.9. In ch.11 he is standing in the road because his warden refused the escort and his superintendent told him "nobody has to get hurt, just stand in the road." He fires once, drops the rifle, and sits down in the dirt, and is still sitting there when the party leave.

## Emmet
- role: Clement Works duty clerk
- ref: story/refs/emmet.png
- people: human
- voice: Mildly annoyed, entirely procedural; answers with the rule and the form number and is not being unkind about it.
- look: Emmet, a mild, narrow-shouldered man of middle height with pale skin, mousy brown hair flattened into a side part, small round wire spectacles, a charcoal company coat with a stand collar, a brass number plate stitched at the throat and a mustard-yellow band around the left cuff, ink on both cuffs, a pencil behind one ear, and a clipboard on a string around his neck that he holds up like a shield.
- status: alive, doing his job, all game
- notes: Emmet Sark, 33. The man at the folding table behind the ancient door in ch.1 who asks whether the party have an entry chit. He recurs doing his job — a counting house at Saltmouth in ch.6, a desk at Vantage in ch.12 — and in ch.15 he produces the 22:02 message log at the hearing because it was in the file. He never does anything but his job and he is the most important minor character in the game.

---

## The Even Hand

## Tovin
- role: guild master of the Even Hand, Sallowgate hall
- ref: story/refs/tovin.png
- people: Onn
- voice: Flat and absolutely literal, keeps a running tally out loud, refuses to round a number, and delivers an extremely filthy joke without any change of expression.
- look: Tovin, a narrow upright figure with matte ochre skin in one flat tone, no hair anywhere, no nose but two small slits above the mouth, large lensed amber eyes with a bright iris ring, four small lights down each temple with only two lit on each side, a long dark brown leather coat older than anyone in the room, a chalk roll and a brass ledger pen strapped across her chest, chalk dust white on both hands, and flat-soled boots.
- status: alive, running the hall; at two lights and going down
- notes: Tovin-Caleth, 1,248, of the same muster as Ket. Keeper of the Caleth skywell until it died in 1058; took a bill because there was nothing else to do and has run the Sallowgate hall since 1073. Calls the hunters collectively "the board." Writes the ledger wall in her own hand and cannot be lied to about a date. Ch.1 she tells Bron the bill stinks and lets him take it. The last image of the game is her chalking a new one.

## Tibb
- role: hunter of the Even Hand, hammer
- ref: story/refs/tibb.png
- people: human
- voice: Loud, friendly, competitive about nothing that matters, and completely without side; asks how the job went before he says how his went.
- look: Tibb, a broad, cheerful young man of middle height with brown skin, black hair pulled up into a short tail, wide dark eyes and an easy grin, a turquoise scarf wound twice at his throat, a short brown leather jacket with a high collar over a quilted grey shirt, bracers on both forearms, a flat brass token stamped with an open hand on his left wrist, and a short-hafted hammer slung at his hip.
- status: alive, on the board
- notes: Tibb Orsk, 28. Takes the bills the party turn down, which is how he ends up at Saltmouth in ch.6 when his partner goes blank. Cheerful rival and a genuine friend. Ch.1, 2, 6, 11, 14, 15.

## Sela
- role: hunter of the Even Hand, polearm; Tibb's partner on the bill
- ref: story/refs/sela.png
- people: human
- voice: Dry and economical before ch.6; afterwards mild, friendly, agreeable, and unable to tell you where she slept.
- look: Sela, a tall, rangy woman with pale skin and ash-blonde hair cropped short above the ears, grey-green eyes, a plum-purple half-cape pinned at one shoulder over a dark travelling bodysuit, a plum sash at the waist, worn leather gauntlets, a flat brass token stamped with an open hand on her left wrist, a single silver stud in one ear, and a long hooked polearm strapped down her back.
- status: blank from ch.6 — healthy, calm, pleasant, and cannot say her own name
- notes: Sela Marrin, 31. Inside the radius when Extraction Bore Four inverted. **Never draw her as a zombie**: upright, warm, looking at you with mild friendly interest. After ch.6 she is a person Tibb visits.

---

## Windrow

## Anneke
- role: laundry presser, Windrow; Bron's mother
- ref: story/refs/anneke.png
- people: human
- voice: Kind and ordinary and slightly shy with strangers; offers food and drink instead of conversation.
- look: Anneke, a sturdy woman of middle height with brown skin reddened at the forearms from steam, greying dark-brown hair pinned up under a faded rose-pink headscarf, warm brown eyes, a sleeveless grey work dress over a cream blouse with the sleeves rolled, a long canvas laundry apron, wooden clogs, a flat iron held in one hand and a folded sheet over the opposite arm.
- status: alive, resettled 1093, filed as having no dependents
- notes: Anneke Brae, 54. One of Tellwater's four hundred, set with a stock template and posted to a Windrow laundry; pairing survivors was more paperwork. Ch.10 she holds a chipped cup out to a stranger in a doorway and he does not take it and does not tell her. In the epilogue she asks Bron to tell her, and he does, and it takes eleven minutes and is not enough.

## Aldo
- role: retired tram inspector, Windrow
- ref: story/refs/aldo.png
- people: human
- voice: Precise, pleased to be asked, and completely unaware of what he is confessing until he has finished saying it.
- look: Aldo, a small, neat old man who stands very straight, with pale skin, thin white hair combed back, bright grey eyes, a faded tram-green inspector's coat kept for best with two rows of brass buttons, a green peaked cap, a brass ticket punch on a chain at his breast pocket, pressed trousers, polished shoes, and a cane he leans on lightly rather than needs.
- status: alive, retired
- notes: Aldo Fessel, 71. The clerk who signed Bron's intake form in 1093 — *A. Fessel*, four marks, two sleeves. He remembers that autumn perfectly, has never told anyone, and thought at the time that he was being kind. Ch.10.

## Ovel
- role: doctor; ran the Clement Works sanatorium at Windrow
- ref: story/refs/ovel.png
- people: human
- voice: Gentle, professional, and entirely certain he behaved well; explains his own kindness at length without being asked.
- look: Ovel, a tall, stooped man with pale skin and steel-grey hair swept back off a high forehead, deep-set pale eyes, a long bone-white sanatorium coat buttoned to a black stand collar over a charcoal suit, brass-ringed reading lenses pushed up onto his head, a ring of small brass keys on his belt, and a closed notebook held flat against his chest with both hands.
- status: alive, in post
- notes: Doctor Ovel Marsh, 60. Handle is **Ovel**: "Marsh" would false-match in panel text. Kept Maren for twelve years, believes he was kind to her, is mostly wrong, and has her handwriting on forty notebooks he never read. Ch.10.

## Arro
- role: keeper of the dark Vintry skywell above Windrow
- ref: story/refs/arro.png
- people: Onn
- voice: Literal, unhurried, and funny by accident; gives exact figures about a machine that has not run in a hundred and sixty-three years.
- look: Arro, a narrow upright figure with matte dull-green skin in one flat tone, no hair anywhere, no nose but two small slits above the mouth, large lensed grey eyes with a bright iris ring, four small lights down each temple with a single one still lit, a threadbare grey keeper's smock patched at both elbows, a copper-green sash, a brass tool roll with most of its loops empty, and a worn-down broom held in one hand.
- status: alive, at one light; restarts the Vintry well in the epilogue
- notes: Arro-Vintry. Has swept a dead machine for a hundred and sixty-three years and is aware of the joke. Ch.10, ch.15, and the epilogue, where Ket comes to bring his well back up. Four of nine wells run within two years.

---

## Tellwater

## Sem
- role: keeper of the dark Dree skywell at Tellwater
- ref: story/refs/sem.png
- people: Onn
- voice: Exact and patient; answers what she is asked, in full, and has been answering the same unasked question for fourteen years.
- look: Sem, a narrow upright figure with matte slate-grey skin in one flat tone, no hair anywhere, no nose but two small slits above the mouth, large lensed pale-gold eyes with a bright iris ring, four small lights down each temple with six of the eight still lit, a long charcoal keeper's smock, an ochre-yellow keeper's sash knotted at the hip, flat-soled boots, and a brass well-key the length of her arm slung across her back.
- status: alive, at her post; the only resident of Tellwater who is not an employee
- notes: Sem-Dree. She watched the whole night of the 9th of Fallow, 1093 from the well platform, and has told anyone who asked for fourteen years, and nobody has asked. Ch.3 the party does not think to ask her. Ch.11 they ask. Ch.15.

## Neve
- role: company shop clerk at Lomm; played "Hesta Sallow" 1093-1097
- ref: story/refs/neve.png
- people: human
- voice: Careful and quiet, in a register she has not used in ten years; will not claim anything and will not deny it either.
- look: Neve, a slight woman of middle height with pale skin, brown hair going grey pulled into a plain low knot, quiet hazel eyes, a cornflower-blue shawl folded over a charcoal company shop coat with a brass number plate at the throat and a mustard-yellow band at the left cuff, a plain dark skirt, flat walking shoes, and a shopping basket held in front of her in both hands.
- status: alive, on the payroll at Lomm
- notes: Neve Ardo, 53. *Ardo, N. — role: Hesta Sallow, widow, one son — posting closed 1097.* The party read her out of the Tellwater payroll in ch.6. In ch.11 she comes back to the square on her own money and without permission, because for four years she was somebody's mother. Optional, and the best optional scene in the game.

---

## Ninefold Terrace

## Dov
- role: Clement Works bore crew, Ninefold Terrace
- ref: story/refs/dov.png
- people: Kell
- voice: Cheerful, stubborn and a little too loud at his own dinner table; argues like his aunt and has not noticed.
- look: Dov, a short, thick-set young man with clay-warm skin, amber eyes with bright rings at the pupils, and pale gold hair cropped into a short upright brush, a broad open face, a charcoal company bore-crew coat with both sleeves cut off at the shoulder and a mustard-yellow band at the left cuff, a harness of brass clips across his chest, heavy gloves tucked into his belt, and a coil of line over one shoulder.
- status: alive, on a company bore crew against his family's wishes
- notes: Dov Ninefold, 26, Hesk's nephew. Alive, cheerful, and on the wrong side of every argument at his own dinner table. Ch.5 the argument, ch.6 on the crew at Saltmouth, ch.13 in the evacuation.

---

## Braid and the shoreline

## Ilven
- role: first chanter of Braid
- ref: story/refs/ilven.png
- people: Vess
- voice: Formal, unhurried, no contractions; acknowledges before she answers and refuses on procedure rather than on feeling.
- look: Ilven, a very tall, very thin elder with ash-lilac skin gone pale at the hands, long backswept fluted ears with visible ribbing, solid amber-brown eyes with no whites, white-silver wire-hair falling to the waist and pinned with nine bone chant-pins, a deep teal-green chant-hall robe with a high collar and long open sleeves, a rope belt hung with small counting tokens, bare feet, and long hands held one inside the other.
- status: alive, holding the line at Braid
- notes: Ilven Tessen, 70. Ch.15 she refuses Sefa's request to take the record to her face, in front of everybody, and accepts Zeph's twenty minutes later, and the difference is not sentiment, it is procedure. Ch.4, 13, 15.

## Cassa
- role: ferry-rail sledge hand, Braid; the party's transport in Act II
- ref: story/refs/cassa.png
- people: Vess
- voice: Talks constantly, counts everything out loud, corrects your numbers happily, and adores Pip within four minutes of meeting her.
- look: Cassa, a tall, whip-thin girl of nineteen with ash-lilac skin, long backswept fluted ears with visible ribbing, solid slate eyes with no whites, short spiky ash-violet wire-hair, a coral-pink sail-cloth jacket with a high collar over a grey bodysuit, cut-off gloves, a crank handle hooked through her belt, a coil of sledge rope across her body, and knee patches worn through to the lining.
- status: alive, running the rail
- notes: Cassa Tessen, 19, Ilven's bond-pair. Sails and hand-cranks a rail sledge across the dry seabed. Ch.4, 5, 6, 13.

---

## The Flats

## Nessa
- role: keeper of the Kettle waystation
- ref: story/refs/nessa.png
- people: human
- voice: Sells you water at the metered rate and tells you at length exactly what she thinks of the rate while she does it.
- look: Nessa, a small, wiry old woman with sun-darkened skin, white-grey hair escaping from under a wide straw hat, sharp dark eyes, a rust-red apron over a layered brown work dress with the sleeves pushed past the elbow, fingerless mitts, a brass water-meter key hanging at her hip on a cord, heavy boots, and a tin cup held out in one hand as though she is about to charge for it.
- status: alive, at the cistern
- notes: Nessa Tull, 66. Posts the ch.2 side contract about bad water at Kettle. Three buildings, eleven people, and a metered cistern on the drove road. Ch.2, 9.

## Ilsa
- role: former Clement Works shift engineer; now at Ostry Bar
- ref: story/refs/ilsa.png
- people: human
- voice: Asks questions constantly, about everything, in a mild and friendly way, and writes the answers down before she asks the next one.
- look: Ilsa, a square, steady woman of middle height with pale skin, iron-grey hair cut blunt at the jaw, calm pale eyes and a mild friendly expression that never changes, a cobalt-blue quilted work coat over engineer's overalls, a thick book strapped flat to her chest on a canvas harness, a pencil tied to the strap on a string, and heavy boots crusted white with salt.
- status: blank since 1093; the only blank in the Basin who asks questions
- notes: Ilsa Tremmel, 49. Entered the written objection at 21:40 on the 9th of Fallow and was found at the winch with her hand still on the brake. Her template did not take cleanly, which is why she asks; she has been writing the answers in a book for eleven years. Ch.9, ch.15.

---

## Emberrow

## Ross
- role: tanner at Emberrow; resettled from Tellwater in 1093
- ref: story/refs/ross.png
- people: human
- voice: Easy and unremarkable before ch.8; afterwards he cannot finish a sentence and keeps starting different ones.
- look: Ross, a big-framed man gone heavy through the middle, with weathered brown skin, straw-blond hair going white at the front and tied back, pale blue eyes, a heavy oxblood-stained leather apron over a faded cream shirt with the sleeves cut away, forearms stained brown to the elbow, a tanner's curved scraper held in one hand, and cracked boots laced to the shin.
- status: alive, restored by Maren in ch.8, and broken by it
- notes: Ross Kettle, 58. Handle is **Ross**: "Kettle" is a common word and a town, and would false-match in panel text. Gets fourteen years of consequence back in one piece, in front of the party and in front of his wife. Restoration is a violence, not a cure. Ch.8, and once more in ch.12, sitting outside his own house.

## Dalla
- role: Ross's wife, Emberrow
- ref: story/refs/dalla.png
- people: human
- voice: Says very little and all of it practical; asks the party what she is supposed to do now and means it as a real question.
- look: Dalla, a short, round-shouldered woman with brown skin and black hair pinned in a low twist, dark watchful eyes, a dusty-pink shawl crossed over a plain grey dress, a long work apron with one stitched pocket, a wedding band worn on a cord at her throat, sturdy flat shoes, and a folded cloth held in both hands that she keeps folding and unfolding.
- status: alive; married nine years to a man who did not know he had been anyone else
- notes: Dalla Kettle, 51. She is standing in the doorway when Maren gives her husband his life back. She is the person the party cannot look at. Ch.8, 12.

---

## Vantage

## Idda
- role: schoolmistress, Vantage Tier Four
- ref: story/refs/idda.png
- people: human
- voice: Brisk and over-prepared, braced to be told she imagined it; when she is believed she stops mid-sentence.
- look: Idda, a slim, upright woman of middle height with pale skin, dark auburn hair pinned into a tight roll, shadowed grey eyes, a plum-purple high-collared teaching dress with a row of small covered buttons, a grey half-cape against the draughts, chalk dust on both cuffs, a leather book-strap of readers carried under one arm, and flat black shoes.
- status: alive, teaching; has not slept properly in six years
- notes: Idda Renn, 44. Posts the ch.7 side contract about something frightening the children at night. The lifting-car cable runs behind the dormitory wall and resonates at the Ossun head's pitch; the children are fine, and she needed one person to say the noise was real. Ch.7, 12.

## Corrie
- role: strand counter clerk, Vantage Tier Three
- ref: story/refs/corrie.png
- people: human
- voice: Relentlessly, genuinely cheerful; recites the counter's patter about sleeves and seasons without a flicker.
- look: Corrie, a small, quick girl of seventeen with brown skin, black hair tied in two short high tails, big dark eyes and a bright unwavering smile, a charcoal company shop coat with a brass number plate stitched at the throat, a bright pink counter apron with rows of narrow pockets holding paper sleeves, fingerless cotton gloves, and a shallow wooden tray carried level at her waist.
- status: alive, on the counter
- notes: Corrie Fallow, 17. Out of a care-house at sixteen, surnamed for the month she arrived, cheerful and incurious and unable to tell you about a single birthday. Pip cannot be in the room with her. Ch.7, 12.

---

## Uniforms and recurring extras

Reusable one-sentence looks for unnamed figures. These are **not** cast entries: paste the sentence
into the panel description, or name the figure generically ("a Pale trooper"), and do not put them on
a `characters:` line.

- **Pale trooper** — a pale ash-grey long coat with a black stand collar, black gauntlets and an oxblood cord at the right shoulder, a grey peaked cap pulled low, a baton at the belt and a long rifle carried across the body.
- **Pale officer** — the same pale ash-grey coat, fitted, with an oxblood half-cape over one brass shoulder plate, black gloves, polished boots, and a whistle on a chain across the chest.
- **Works clerk** — a charcoal-grey stand-collar coat with a small brass number plate stitched at the throat and a mustard-yellow band at the left cuff, ink on both cuffs, a clipboard or a ledger held against the chest.
- **Works bore hand** — a charcoal company coat with the sleeves cut off, a canvas harness of brass clips, a lamp on the shoulder, heavy gloves, salt-crusted boots, and a mustard-yellow band on the left cuff.
- **Care-house child** — a clean grey pinafore or tunic over a white collar, a numbered brass tag at the chest, cropped neat hair, sturdy new boots, and a full, untroubled, well-fed face.
- **Resettled townsperson of Tellwater** — an ordinary human in three layers of plain brown and grey wool, a brass posting badge pinned at the collar, hands busy with a trade tool, and a warm rehearsed smile aimed slightly wrong.
- **Kell terrace farmer** — a very short, very broad figure with clay-warm skin, amber eyes, upright copper or iron-grey hair, a sleeveless brown coat and bare arms in any weather, a wide belt of hand tools, and a barley basket on one hip.
- **Vess ferry-rail hand** — a very tall, thin figure with ash-lilac skin, long backswept fluted ears, solid no-white eyes and silver wire-hair tied back, a grey sail-cloth coat with a high collar, cut-off gloves, and a crank handle hooked through the belt.
- **Onn keeper** — a narrow upright figure with matte skin in one flat colour, no hair, two small slits above the mouth, large lensed eyes with a bright iris ring, a row of four small temple lights of which some are dark, and a long grey keeper's smock with a chest tool roll.
- **Even Hand hunter** — practical mismatched travelling gear in browns and greys, a high-collared jacket or half-cape, bracers, a weapon of their own choosing, and a flat brass token stamped with an open hand at the left wrist.
- **Hall-guard of Thurn** — an armored skeleton standing in an ancient segmented helm with two small steady blue eye-lights, a tattered red cloak hanging still, a halberd held level, and nothing at all inside the armor.
