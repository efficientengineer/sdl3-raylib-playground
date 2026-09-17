# characters.md — the current cast

One `## Name` per character. The `look` line is pasted **verbatim** into every image
prompt that includes the character, so keep it concrete and visual: build, hair, outfit
colors, one distinguishing feature. Change it only when the character's design changes.
`ref` is the character's reference image for ChatGPT (make it with
`./story_prompt.py refsheet <Name>`, save the result at that path). `status` and `notes` are story state for whoever writes the next scene; they are never
sent to the image model.

Names match the default party in `src/game_logic.cpp`. Accent colors follow `CLASS_COLORS`.

**Design direction (owner's call):** the cast should look like the heroes of a 1993 sci-fantasy
anime JRPG (Phantasy Star IV is the touchstone), never like Western tabletop fantasy. So:
- Keep race and class words (**dwarf, halfling, elf, fighter, rogue, cleric, wizard**) and beards out
  of `look`. They drag image models toward braided beards, gritty muscle, and Tolkien armor. `role`
  keeps them for the story; `look` describes only what is visible. `story_prompt.py` rejects them.
- Young adults with big expressive eyes and big hair; build comes from silhouette (short and compact,
  tall and slim, petite), not from age, bulk, or facial hair.
- Costume vocabulary: high collars, bodysuits, long coats, half-capes, a single oversized shoulder
  plate, headbands, circlets, sashes, gauntlets, boots. One strong accent color per character.
- Original designs only. Borrow the era's archetypes, never a specific existing character.

## Bron
- role: Dwarf Fighter, party leader
- ref: story/refs/bron.png
- look: Bron, a short, compact, broad-shouldered young man in his early twenties with a youthful clean-shaven face, big spiky copper-red hair swept back under a white headband with long trailing ends, sharp amber eyes, a small scar through his left eyebrow, a sleeveless burnt-orange high-collared jacket over a black bodysuit, one large rounded steel shoulder plate on his left shoulder, oversized steel gauntlets, a wide belt with a round buckle, white boots, a big double-bladed axe slung on his back.
- status: alive, in the party
- notes: blunt, protective, distrusts magic. Speaks in short sentences. Shortest of the party after Pip, and touchy about it.

## Lyra
- role: Human Cleric
- ref: story/refs/lyra.png
- look: Lyra, a tall, slender young woman with very long straight pale-gold hair, long bangs parted in the center, a thin gold circlet with a small red gem on her forehead, calm grey eyes, a white high-collared long coat with gold trim and wide gold-edged shoulder pieces, a short white half-cape, a gold sun medallion on her chest, long white gloves, a slim silver mace at her hip.
- status: alive, in the party
- notes: patient, the party's conscience. Hides doubt about her order.

## Zeph
- role: Elf Wizard
- ref: story/refs/zeph.png
- look: Zeph, a tall, slim young man with long pointed ears, long flowing indigo hair with a spiky fringe falling over one eye, narrow violet eyes and a confident smirk, a single silver earring, a deep blue high-collared long coat with silver clasps and one asymmetric shoulder cape, a wide white sash at the waist, fingerless black gloves, a short staff tipped with a floating blue crystal.
- status: alive, in the party
- notes: clever, reckless, reads everything he shouldn't.

## Pip
- role: Halfling Rogue
- ref: story/refs/pip.png
- look: Pip, a petite, agile young woman with short fluffy chestnut hair under an olive-green bandana, round goggles pushed up on her forehead, large bright brown eyes and a quick grin, a cropped olive-green jacket with a tall collar over a dark bodysuit, fingerless gloves, a belt of small pouches and lockpicks, knee-high boots, twin curved daggers sheathed at her lower back.
- status: alive, in the party
- notes: jokes when scared. Always the first to touch the thing. Smallest of the party.
