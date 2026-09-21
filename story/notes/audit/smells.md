# AI-smell audit — chapter one, everything a player can see

Read against `story/v3/SMELLS.md` (convergences C1-C42, the BAN block) and `story/v3/STYLE.md`
(rules 0, 0b, 0c, 6, 12, 14, the BAN blocks, the five rewrite rules).

**Corpus.** 86 dialogue lines across `story/scenes/0110, 0130, 0140, 0150, 0160, 0180`; 56 entries of
`story/field/text.md`; 9 goal lines and the end card in `src/chapter01.h` / `src/star_logic.cpp`;
~40 battle-log strings, 9 enemy names, 7 skill names, 2 goal lines in `src/battle.cpp`; the name
table in `story/v3/NAMES.md`.

**Headline.** The chapter is well above the line the SMELLS document was written to police. The big
convergences are gone: no pale mark, no "Louder", no all-caps, no ellipsis, no "Gods", no polysyndeton
grief, no renege, no mentor confession inverting the catchphrase, and the dead animal is criticised
rather than eulogised. What is left is not retrieval, it is **rhythm**. One sentence shape has become
the chapter's default music, and a second habit — saying a thing in an examine box and then saying it
again in the cutscene — costs nine lines outright.

LOCKED lines are marked **[LOCKED]** and are never proposed for change, but they are counted when a
pattern is measured, because the question is how often the ear hears the shape, not who wrote it.

---

## 1. The five worst habits, with counts

### H1 — The symmetrical pair. **11 instances / 86 dialogue lines + 56 boxes.**

Two clauses of near-identical length, the second inverting or mirroring the first. It is the
chapter's signature and it is used by every speaker, which is the problem: it is a *house* rhythm,
not a voice.

| # | Location | Line |
|---|---|---|
| 1 | 0110 | "I hit it a dozen times!" / "It hit you once." |
| 2 | 0110 | "You fought a training dummy, not a bear." **[LOCKED]** |
| 3 | 0130 | "No sword, no signature." **[LOCKED]** |
| 4 | 0130 | "You do not buy one. You are given one." |
| 5 | 0140 | "He won't tell you. He's never told me either" |
| 6 | 0150 | "Don't tell me how many times. Please don't tell me." |
| 7 | 0160 | "I've been up here in daylight a hundred times. It isn't the same hill." |
| 8 | 0180 | "You killed him, and he'd have killed me." |
| 9 | 0180 | "If it was done to mine, it was done to others." |
| 10 | `hart_yard.ottilie_idle` | "I'll be up here tomorrow and you'll be down there losing." |
| 11 | `hill_path.ottilie_dark` | "I've never been up here after dark. I've never been anywhere after dark." |

That is roughly one every eight boxes, and three of the six scenes carry two. Nos. 1, 6 and 11 are
the good ones and should survive; the argument for cutting 4, 7, 8 and 9 is not that any of them is
bad but that after the first three the player stops hearing meaning and starts hearing metre.
**Target: four in the chapter, and never two in one scene.**

### H2 — Every scene ends on a button. **6 of 6.**

| Scene | Last line | Kind of button |
|---|---|---|
| 0110 | "Not quite ready yet." **[LOCKED]** | the withheld verdict |
| 0130 | "I'll have a sword before you've spent it!" | boast |
| 0140 | "I'll still be out there when you come and laugh at me!" | boast |
| 0150 | "{{HERO}}. That's the evening bell." | cliffhanger |
| 0160 | "She has a name." **[LOCKED]** | mic drop |
| 0180 | "Then we walk. Which way is up your mountain?" | chapter closer — allowed by STYLE |

Nothing here is a *bad* line. But six scenes in a row that land on a snap makes the chapter feel
written rather than overheard, and it makes 0160's genuinely great drop ("She has a name.") land as
the fifth of its kind rather than the first. STYLE R2 — *end one line earlier* — is the fix, and the
two boasts are where it costs nothing. See §3.

### H3 — The sentence that explains the sentence before it. **11 instances.**

STYLE rule 6 ("cut any line that explains the line before it"), rule 14 ("no examine box ends with a
sentence interpreting the box's own first sentence"), and the BAN on summing-up lines. This is the
habit `text.md`'s own draft-three note was written to kill, and eleven survived.

`0130` "That is the whole of the rule." · `0130` "It is the sign, and I look for it." · `0130` "So
don't be up a hill at supper time." · `0150` "I can't swing at it again." · `0150` "He'll write your
name in the book." · `0160` "It isn't the same hill." · `0180` "Both of those are true." · `0180`
"Not ever." · `halm.bell` "It is rung once a day, at the end of it." · `high_pasture.find` "where you
would only find it in the dark" · `hart_yard.ladder` "and something tucked under them out of the rain".

Every one of them is a **free cut**: delete the clause, lose nothing.

### H4 — Said twice: the box and then the clip. **8 duplicated jokes, images or facts.**

STYLE bans "a joke, image or phrase used twice for a neat callback — keep the better one." The
chapter's two-layer authoring (examine text and cutscene written separately) has produced eight.
These cost the most, because the player reads the box and then watches a character say it again
thirty seconds later, which reads as the game not trusting them.

1. **spoon like a hammer** — `hart_yard.supper_2` "you still hold the spoon like a hammer" / 0140
   "watch him hold a spoon like a hammer"
2. **the ring of holes** — `high_pasture.body_2` "all the same size, healed shut a long time ago" /
   0180 "all the same size, healed shut years ago"
3. **before it's light** — 0140 "before it's light" / `hart_yard.bed` "before it is light" / goal line
   "Be at the machines before light." — **three times**
4. **raised from a pup** — 0160 "I raised him from a pup" / 0180 "since he was a pup"
5. **sheets go up at dawn** — 0130 {{RIVAL}} / 0140 {{HERO}}, verbatim
6. **the best one** — 0110 "That's the best one all week" / `hart_yard.day2_d` "That's the best one yet"
7. **the empty hanger** — `hart_yard.workshop_door` "a hook with nothing hanging on it" /
   `hart_yard.room_sword_gap` "Two pegs with nothing across them" / 0150 "It's been on its hook since"
   — **three times**, and this is SMELLS C10, the document's most damning convergence
8. **he stopped knowing her** — 0160 "stopped knowing me" / battle log "looks at Distel, and does not
   know her" / `high_pasture.boss_turn` "He's looking at me the way he looks at a sheep" — **three times**

### H5 — All the energy belongs to one character. **11 of 11 exclamation marks are {{HERO}}'s.**

| Speaker | `!` in cutscenes | `?` in cutscenes |
|---|---|---|
| {{HERO}} | **11** | 5 |
| {{HEALER}} | 0 | 1 |
| {{MENTOR}} | 0 | 0 *(correct — NAMES: never asks a question)* |
| {{RIVAL}} | 0 | 0 *(NAMES says he "asks mocking questions" — he asks none)* |
| Distel | 0 | 0 |
| Clerk | 0 | 1 |

STYLE 0c requires energy *per speaker*. What has happened instead is that {{HERO}} was given all of
it and everyone else was left in level declaratives, so the five non-hero speakers converge on one
dry, unruffled register — the "everyone is wry in the same voice" failure, arrived at from the other
direction. {{HEALER}}'s NAMES sample is *"Falke — put it down! You're bleeding on my bandage!"* and
there is nothing like it in the chapter. {{RIVAL}} never mocks with a question.

Cheapest repair: give {{HEALER}} one real exclamation in 0160 (she is frightened on a black hill and
says nothing louder than a comma), and turn one {{RIVAL}} line in 0130 into the mocking question his
voice is built on.

### Runner-up — the echo tic. **4 clean instances, all {{HERO}}, ~1 per scene.**

`NAMES.md` licenses it ("he repeats the fact he just heard back as a question"), so it is not a
smell by itself. But it fires almost exactly once per scene, and three of the four are his reaction
to that scene's most important line, so it has become the chapter's reflex for *this line matters*:
0110 "That's it? That's all of it?" · 0140 "That's how you hit things!" · 0150 "Say it!" ·
0180 "He forgot you." Keep two, cut two — 0110's second half and 0180's, which is the worst placed.

### What is clean — worth recording

No ellipsis-led lines (0). No ALL-CAPS (0). No "nobody" in player-facing text (0 — the three hits
are Beat prose, which only ChatGPT reads; still worth a sweep since the file's own rule says grep
before done). No "Gods,". No "my whole life" (one near-miss, §2). No rule-of-three list sentence in
dialogue (one in field text). Em dashes: 8 in ~140 boxes, all true em dashes, all doing interruption
work — **not** a crutch; leave them. No adverbs of manner in dialogue except "Quietly" in 0160,
which is an instruction and earns it.

---

## 2. Full findings

### 2a. `story/scenes/0110_the_yard.md`

| Line | Smell | Repair |
|---|---|---|
| {{HEALER}}: "He's at the swing again. Best wall in {{HOME_TOWN}}, this." | Folksy inverted cadence — rule 0 bans country cadence outright. Also the player is 40 seconds into the game and "best wall" is an idea with no referent (rule 14). | "He's at the swing again. I've got the good seat." |
| {{HEALER}}: "Feet over his head. That's the best one all week." | Duplicate of `hart_yard.day2_d` (H4 #6). | Keep this one; change the field box. |
| {{HERO}}: "I hit it a dozen times!" / {{HEALER}}: "It hit you once." | H1. The chapter's best symmetrical pair and the one that sets the rhythm every later pair copies. | **Leave — this one earns it.** It is the reason to cut the other four. |
| {{HEALER}}: "Maybe hit it again?" | — | **[LOCKED]** |
| {{HERO}}: "That's it? That's all of it?" | Echo-question doubled: the second half restates the first. | "That's it?" |
| {{HEALER}}: "You fought a training dummy, not a bear." | "not X, Y" — counts toward H1. | **[LOCKED]** |
| {{HERO}}: "{{MENTOR}} — say it straight. Am I ready or am I not?" | "say it straight" is one word off `BAN: "Say it"`. "Am I ready or am I not?" is a tidy rhetorical binary — SMELLS C25, fake friction; the real blurt is shorter. | "{{MENTOR}}. Am I ready?" |
| {{MENTOR}}: "Not quite ready yet." | — | **[LOCKED]** |
| Panel 2 acting: "enjoying herself" | `BAN: mood adverbs in stage directions`. | "eyes right on {{HERO}}, brows up, mouth open on a laugh" already says it — cut "enjoying herself". |

### 2b. `story/scenes/0130_the_counter.md`

| Line | Smell | Repair |
|---|---|---|
| Clerk: "No sword, no signature." | — | **[LOCKED]** |
| {{HERO}}: "Say that plainly." | A writer stage-managing the exposition. His voice is blurt, not instruction. | "What does that mean?" |
| Clerk: "A hunter's first sword comes from his teacher. It is the sign, and I look for it." | "It is the sign" is grand and aphoristic (BAN: aphorisms), and it is a second fact in one line (rule 14). | "A hunter's first sword comes from his teacher. That is what I look for." |
| {{HERO}}: "Then I'll buy one myself." | Retort opening on "Then" — the reflex behind `BAN: leading "Then you..."`; it slips the grep on the pronoun only. Chapter has **three** "Then"-openers (here, 0160, 0180). | "I'll buy one, then." |
| Clerk: "You do not buy one. You are given one. That is the whole of the rule." | H1 antithesis **and** H3 summing-up closer in one box. | "You do not buy one. Your teacher gives it to you." |
| {{RIVAL}}: "Job sheets go up at dawn and the board shuts on the last ring of the evening bell." | 78 chars, two facts, and it is a character **reciting rules as a list** — rule 14 forbids exactly this. | Split into two boxes: "Job sheets go up at dawn." / "Board shuts on the evening bell's last ring." |
| {{RIVAL}}: "So don't be up a hill at supper time. Here — three coin for your bread." | First sentence explains the one before it (H3). The coin is the good half and the whole gloat. | "Here. Three coin for your bread." |
| {{RIVAL}} — whole scene | NAMES: "gloats, **and asks mocking questions**." He asks none, and has no `!` or `?` (H5). | Turn "He's right. Mine took me two years to get." into "He's right. Mine took two years — how long have you had?" |
| {{HERO}}: "I'll have a sword before you've spent it!" | Scene-ending boast (H2). Good line, wrong job. | See §3 — end on the coin instead. |

### 2c. `story/scenes/0140_supper.md`

| Line | Smell | Repair |
|---|---|---|
| {{HERO}}: "Job sheets go up at dawn. I'll be standing at that board when they do." | Verbatim recap of {{RIVAL}}'s line one scene earlier — rule 6 cuts recaps (H4 #5). | "I'll be standing at that board at dawn." |
| {{MENTOR}}: "The swing is the one that is out there." | **The clearest rule-0 violation in the chapter**: works only by implication, and it is proverb-shaped, which rule 0 bans by name. The Beat calls it "the one oblique sentence the whole chapter turns on" — which is the definition of a line written to be quoted. | "The swing hits back. Everything out there hits back." |
| {{MENTOR}}: "You hit everything as hard as you can. Every single time." | "single" is filler. | "You hit everything as hard as you can. Every time." |
| {{MENTOR}}: "Eat here tomorrow as well. You've eaten alone enough." | Breaks his own register (NAMES: never says he is worried, shows care by handing things over) — deliberately, per the Beat. | **Leave — this one earns it.** It is the warmest line in the chapter and it is seven words. |
| {{HEALER}}: "Someone has to watch him hold a spoon like a hammer." | Duplicate of `hart_yard.supper_2` (H4 #1). | Cut the joke here; keep the examine box. Replace with her deflecting: "I'll be here. Don't make it a thing." |
| {{HEALER}}: "He'll be out there in the dark hitting a post." | Narrating a third party's intentions, and **factually wrong** — the post is a different machine; he is going out to the swing. | "He'll be out there in the dark swinging at it." |
| {{HERO}}: "I'm going out to the machines before it's light." | Said three times across the chapter (H4 #3). | Keep the goal line; cut `hart_yard.bed`'s half. |
| {{HERO}}: "I'll still be out there when you come and laugh at me!" | Scene-ending boast (H2), and the second one in a row. | See §3. |

### 2d. `story/scenes/0150_the_sword.md`

| Line | Smell | Repair |
|---|---|---|
| {{HERO}}: "My arms are gone. I can't swing at it again." | Second sentence **narrates what the player just played** — rule 14's hardest ban, in the one scene whose Beat says "nobody says afterwards what he did." | "My arms are gone." |
| {{HEALER}}: "I had a very good thing ready to say and now I'm not saying it." | A character announcing her own withheld joke: explaining the joke before making it. | "I'm not saying it." |
| {{HEALER}}: "No. You'd have it framed." | Audibly quotable — about quotability. | **Leave — this one earns it.** Best line in the chapter. |
| {{MENTOR}}: "Put the stick down. Both hands." + panel 8 (sword across two open palms, held beat) | **SMELLS C11, a 5-of-5 convergence** — the ceremonial two-handed prop transfer — staged in full, with the held silent beat. This is not a line-level fix; it is the chapter's central set piece sitting on the most-retrieved shot in the document. | Not cheap. Flagged for the orchestrator: if one thing in the chapter is rewritten structurally, it is this staging. The line itself is fine. |
| {{MENTOR}}: "Made it the winter before last. It's been on its hook since." | Second sentence is the third empty-hanger image (H4 #7) **and** SMELLS C5, the prop issued a stated history. | "Made it the winter before last." |
| {{MENTOR}}: "Take it to the clerk. He'll write your name in the book." | Second sentence explains the first and repeats what 0130 already established (H3). | "Take it to the clerk." |
| {{HEALER}}: "It suits you." | — | **Leave.** Flat, short, does not comment on the moment. Exactly right. |
| Panel 2: "comfortable, watching a show she has watched for years" | Mood description in a stage direction. | Cut; the acting line under it already does it. |

### 2e. `story/scenes/0160_the_herder.md`

| Line | Smell | Repair |
|---|---|---|
| {{HEALER}}: "I've been up here in daylight a hundred times. It isn't the same hill." | Exact integer + H1 + H3: the second sentence labels the feeling the first one caused. | "I've been up here in daylight a hundred times. Not at night." |
| Distel: "Don't shout. You'll wake her. She's had a bad night." | — | **Leave — this one earns it.** Three imperatives, third is the joke, and it is the whole character in one box. |
| {{HEALER}}: "I've heard of your people all my life." | One word off `BAN: "my whole life"`. | "I've heard of your people since I was small." |
| Distel: "{{BEAST_NAME}} put them down. My {{GUARD_BEAST}} — I raised him from a pup." | **Two new proper nouns in one line** — rule 14 allows one. Also "from a pup" recurs in 0180 (H4 #4). | Split: "{{BEAST_NAME}} put them down." / "He's my {{GUARD_BEAST}}. I raised him." |
| Distel: "He beds a flock down where he thinks it's safe. He ran in the spring and stopped knowing me." | 93 chars, two facts, and the second is the entire plot of the chapter delivered as a subordinate clause. | Two boxes: "He beds a flock down where he thinks it's safe." / "He ran in the spring. He stopped knowing me." |
| Distel: "Then hold him still and I'll calm him." | Second "Then"-opener. | "Hold him still and I'll calm him." |
| Distel: "She has a name." | — | **[LOCKED]** |
| {{HEALER}} — whole scene | She is on a black hillside at night in front of a drawn sword and her loudest punctuation is a full stop (H5). | "She's a girl. {{HERO}}, she's a girl — put the sword down!" |

### 2f. `story/scenes/0180_what_was_on_it.md`

| Line | Smell | Repair |
|---|---|---|
| Distel: "He's dead. You killed him, and he'd have killed me. Both of those are true." | H1 chiasmus **plus** a tag telling the player how to hold it — `BAN: summing-up lines that restate the feeling`. | "He's dead. You killed him. He'd have killed me." |
| {{HEALER}}: "Nine of them, all the same size, healed shut years ago." | Near-verbatim repeat of `high_pasture.body_2`, read by the player under a minute earlier (H4 #2). | "Nine of them." — and let her count be the reaction, not the report. |
| Distel: "I've known him since he was a pup. I have never seen those." | "I have never" — no contraction, against her NAMES voice; and "pup" repeats 0160. | "I've known him all his life. I've never seen those." |
| Distel: "A {{GUARD_BEAST}} does not forget his handler. Not ever." | Proverb (BAN: aphorisms) closed with a gavel fragment — the C29 shape. | "A {{GUARD_BEAST}} does not forget his handler." |
| {{HERO}}: "He forgot you." | The echo tic at the chapter's most important beat: he repeats her line back with the polarity flipped and nothing is learned (SMELLS C33, C25). | **Cut.** Distel's next line follows perfectly without it. |
| Distel: "So it was done to him. If it was done to mine, it was done to others." | Three-clause anaphora — the chapter's thesis spoken as a syllogism by a fourteen-year-old with no idiom (H1). | "Then somebody did it to him. Somebody's doing it to others." |
| Garbe: "{{SHEPHERD}}, and that's my flock walking home on its own legs." | A stranger announcing her own name as her first word; "on its own legs" is a flourish. | "That's my flock. Walking home on its own." |
| {{HEALER}}: "Put it somewhere you won't sit on it." | — | **Leave — this one earns it.** |
| Distel: "I'm going to cry soon. Keep walking." | — | **[LOCKED]** |
| {{HERO}}: "Then we walk. Which way is up your mountain?" | Third "Then"-opener in the chapter, and it is the last line of the chapter. The question half is good. | "Which way is up your mountain?" |

### 2g. `story/field/text.md`

| Id | Smell | Repair |
|---|---|---|
| `hart_yard.ladder` | "and something tucked under them out of the rain" — narrator winking at the player about a reward (BAN: narrator asides that explain the reward). | "A ladder up to the eaves." |
| `hart_yard.workshop_door` | "a hook on the back wall with nothing hanging on it" — absence-as-image, SMELLS C10 / structural default 9, and the chapter has three (H4 #7). | Cut the clause: "Wood, iron, rope and counterweights." Keep `room_sword_gap`, which is the player's own and pays off. |
| `hart_yard.supper_2` | — | **Leave.** Keep this spoon joke and cut 0140's. |
| `hart_yard.day2_d` | "That's the best one yet" duplicates 0110 (H4 #6). | "You went over the post and everything." |
| `hart_yard.bed` | "before it is light" — third saying (H4 #3), and no contraction in a narrator box that elsewhere uses them. | "Sleep." |
| `halm.bell` | "It is rung once a day, at the end of it." interprets the first sentence (H3), and the bell teaches itself on day two. | Cut the second sentence. |
| `halm.well` | "same as the bell axle and the catch in your hands" — rule-of-three list, and it explains the box's own point. | "Capped and roped, and the winch over it is {{MENTOR}}'s work." |
| `halm.lamp_charm_2` | "sleep like a {{HERDER_PEOPLE}}, wake like a {{HERDER_PEOPLE}}, get nothing done all day" — rule of three **and** an aphorism. | **Leave — this one earns it.** It is flagged in-line as a grandfather's saying, which is the one context where a proverb is a fact about a speaker rather than the writer showing off. |
| `halm.town_healer` | 38 words, three clauses, zero contractions — the longest and most composed box in the file, and it is a briefing. | "I took her in the winter the fever went through. One season's work outside this valley and the standing is hers." |
| `halm.marta` | "You'll get one." — reassurance that flattens the fact in front of it. | Cut the second sentence. |
| `hill_path.ottilie_dark_2` | "She'll be **delighted**" — `delighted` is on STYLE's banned-word line. | "She'll be pleased, and I'm not telling her." |
| `high_pasture.find` | "where you would only find it in the dark" — explains the reward (H3). | "A flat disc of pale glass on a cord, dropped in the grass." |
| `high_pasture.body` | "bigger than he looked standing up" — the `BAN: heavier than it looks` shape with the adjective swapped. | "{{BEAST_NAME}} lying in the flattened grass, still warm. Under the matted hair behind his left ear the skin is not right." |
| `high_pasture.body_2` | Duplicated by 0180 (H4 #2). | Keep this box; cut the clip line. |
| `high_pasture.tracks_stop` | "Whatever left here stopped putting its feet down." — audibly written to be quoted, and the `- what:` note says so. | **Leave — this one earns it.** Best line in the file, and the designer means it. |
| `high_pasture.sleeper_3` | — | **Leave.** "This one is a dog, and it is asleep too." is perfect. |
| `west_road.west_end` | "Your job is up the hill behind you." — the narrator coaching the player (rule 14). | Not played in ch1. Fix when the road west is authored. |
| `high_pasture.boss_break`, `boss_turn` | `- name: Distel` is a bare name where every other entry uses a token (`- name: {{SHEPHERD}}`). NAMES rule: speaker labels are tokens. | `- name: {{HERDER}}` |

### 2h. Consistency (not smells — conventions that drift)

- **Speaker tokens.** `0160` and `0180` use bare `Distel` and `Garbe` as speaker labels; `{{HERDER}}`
  and `{{SHEPHERD}}` exist. `NAMES.md`: *"Speaker labels are tokens too."* Three spellings of the
  clerk exist as well: `Guildclerk` (0130 `characters:`), `Clerk` (0130 speaker), `Clerk` (field).
- **The dash.** Title screen is `"Chapter 1  -  The Last Job Sheet"` — an ASCII hyphen with doubled
  spaces, where every other dash in the project is an em dash.
- **Battle log case.** Enemy display names are lowercase (`"the swing"`, `"a lid"`), so sentence-initial
  positions render lowercase: `"the swing counters."`, `"a lid stops."` Visible in the capture harness
  at `src/battle.cpp:1922,1930`. Either capitalise at format time or give the table sentence-case names.
- **`{{GUARD_BEAST}}` vs the goal line.** The token's value is **drover**, but `chapter01.h`'s goal
  line is hardcoded `"Find the guard beast."` The player is told "guard beast" once and "drover"
  everywhere else. Goal lines do not go through substitution.

---

## 3. Per-scene last-line review

Six scenes, six buttons (H2). STYLE R2 says: when a scene ends on a callback or a snap, end it one
line earlier. Three of the six should end flatter.

| Scene | Current last line | Verdict |
|---|---|---|
| **0110** | "Not quite ready yet." **[LOCKED]** | **Right.** A withheld verdict, not a snap. It is the one ending in the chapter that leaves the scene open. |
| **0130** | "I'll have a sword before you've spent it!" | **Should end flatter.** This is a boast answering a kindness, and it hands {{HERO}} a triumph he has not earned in a scene he lost. **End on {{RIVAL}}'s coin**: "Here. Three coin for your bread." — the player walks out holding the rival's money, which is a worse and better ending. |
| **0140** | "I'll still be out there when you come and laugh at me!" | **Should end flatter**, and it is the second boast in a row, which is what makes it audible. **End on {{HEALER}}**: "He'll be out there in the dark swinging at it." Then {{MENTOR}}'s "Eat." can move down to be the last word — no, better: end on her line and let the scene stop mid-argument. |
| **0150** | "{{HERO}}. That's the evening bell." | **Right.** It is not a snap, it is a clock starting. Functional cliffhanger into the bell run. |
| **0160** | "She has a name." **[LOCKED]** | **Right, and it is the best drop in the chapter** — but it is the fifth button, so it lands as one of a series. Fixing 0130 and 0140 is what protects it. |
| **0180** | "Which way is up your mountain?" *(after cutting "Then we walk.")* | **Right.** STYLE explicitly allows the last line of a chapter to be quotable, and this is a question rather than a statement, which opens instead of closing. |

Note against SMELLS C42 / structural default 12 ("the older man closing every scene"): this chapter
passes cleanly. {{MENTOR}} closes once, the two teenagers close twice each, {{HEALER}} once. Good.

---

## 4. UI, goal and battle strings

### Goal lines — `src/chapter01.h`

"Beat the swing." · "Deliver the part. Buy bread." · "Get home before supper." · "Eat, and go to bed." ·
"Be at the machines before light." · "Get to the guild hall." · "Get up to the pasture." ·
"Follow the sleeping animals." · "Find the guard beast."

**These are the best-written strings in the project.** Verb first, ≤6 words, concrete, no flavour.
One fix only: `"Find the guard beast."` should read `"Find the drover."` — see §2h.

### Battle log — `src/battle.cpp`

| String | Smell | Repair |
|---|---|---|
| `"The counterweight winds up a notch. (%d)"` | Fires on *every* attack against the swing, so it is the most-read string in the game; at 6 words + number it is a touch long for that job. | `"The weight winds up. (%d)"` |
| `"The mane lifts. It looks at Distel, and does not know her."` | 12 words of cutscene prose in a combat log, at the chapter's emotional peak, saying a thing already said twice elsewhere (H4 #8). | `"It looks at Distel."` — her own field line carries the rest. |
| `"Settle does not take."` | "take" reads as a typo for "hold". | `"Settle does not hold."` |
| `"Away."` *(fled outcome)* | Ambiguous out of context — it reads as a command. | `"Got away."` |
| `"Again."` *(yard-loss outcome, and `bt_log` at :859)* | Reuse of {{MENTOR}}'s locked line as a system string — technically the banned twice-used phrase. | **Leave — this one earns it.** In the yard it *is* Hart shouting, and it is the cheapest possible loss message. |
| `"the arm that holds"` *(enemy name)* | Four words with a relative clause, and it goes into sentence slots: `"the arm that holds counters."` | `"the arm"` — the display already sits under a sprite that is visibly a jaw. |
| `"It is not hurt."` / `"It turns the blow aside."` / `"It goes quiet. Its head comes down."` / `"%s is winded. (-%d)"` / `"Nothing to use."` / `"%s has nothing left."` | — | **Leave.** Plain, short, correct register. This is the standard the rest should meet. |
| `"Mend: %s +%d."` / `"Shield: %s, -%d%% for %d."` vs `"It goes quiet."` | Two punctuation conventions in one log: colon-prefixed telegraph style for skills, plain sentences for everything else. | Pick one. The colon style is right for mechanics; make the skill lines consistent and leave the narrative ones alone. |
| Goals `"Hold it open for Distel."` / `"Stop it. It wants Distel."` | — | **Leave.** Both excellent. |

### Title and end card — `src/star_logic.cpp`

| String | Smell | Repair |
|---|---|---|
| `"Chapter 1  -  The Last Job Sheet"` | **"The Last Job Sheet" is SMELLS C14 on the banned list** — `BAN: "one good job left" / "one slip left worth taking"` — scarcity quantified as the cheapest legible stake, and it is the chapter's *title*. It is also inaccurate: the job is the one nobody wanted, not the last one going. | `"Chapter 1 — The Job Nobody Took"` is the same smell. Better: `"Chapter 1 — Eleven Sheep"` or `"Chapter 1 — The Sheep on the Hill"`. Also replace `"  -  "` with a spaced em dash. |
| `"THE FAIR COPY"` | — | **Leave.** |
| `"To be continued"` / `"Tap to return to the title"` / `"Tap to begin"` | — | **Leave.** Plain and correct. |
| Dev panel labels (`"Win##bt"`, `"Know all##bt"`, step/flag readouts) | Not player-facing. | No action. |

---

## 5. Names review

Checked as a German speaker would hear them. The table's stated method — plain German words plus real
old German given names, Frieren-style — is sound and mostly executed well. Three problems.

### Working well — keep

| Name | Word | Verdict |
|---|---|---|
| **Falke** | falcon | Real German surname, real bird, masculine, unforced. The best-chosen name in the table. |
| **Ottilie** | given name | A real, slightly old-fashioned German given name. Exactly the Frieren register. |
| **Elster** | magpie | Real surname, real bird, and *die diebische Elster* is the German idiom for a thief. Very apt — arguably one notch too apt, since a German reader gets the joke before the character does — but it is a bird name doing honest work. **Keep.** |
| **Linde** | lime tree | Real given name *and* surname, and the village lime tree is the German image of the old woman everyone goes to. Perfect for the town healer. |
| **Distel** | thistle | Real word, real surname, feminine, prickly. Correct for a blunt fourteen-year-old. |
| **Klee** | clover | Real surname (Paul Klee), and sheep eat clover. A sheepdog called Clover is lovely and no German would blink. |
| **Rabe** | raven | Real surname, real bird. Fine for a dead father. |
| **Halm** | grain stalk | Real word, works as a place name, and the town has a grain yard. Good. |
| **Hart** | hard | Real German surname; also reads as English *hart* (a stag), which does not undercut it. Slightly on-the-nose for the tough mentor but defensible. |

### Problems

1. **`{{SCHOLAR}}` = Frage — the weakest name in the table. Change it.**
   *Die Frage* is an abstract feminine noun meaning "question". It is not a German surname and does
   not function as one. Frieren's Stark / Fern / Himmel work because they are adjectives or concrete
   nouns that already exist as names; "Frage" reads to a German ear exactly as "Question" reads in
   English — a placeholder, not a person. It is also grammatically feminine for a male character.
   **Repair:** keep the plant/bird rule and take a real one — **Reiher** (heron), **Dohle** (jackdaw,
   the clever corvid), or **Farn** (fern, a real surname).

2. **Two allegory names in a row: `{{RIVAL}}` = Stolz ("pride") and `{{VILLAIN}}` = Durst ("thirst").**
   Both are real German surnames, so neither is *wrong*. But Stolz is the proud rival and Durst is the
   lord of a dry city who moved the water — a German speaker reads "Pride" and "Thirst" as their
   function, instantly, and two of them establishes it as a system. That is the tell of a generated
   table: names that are their own character notes. The rest of the table (Falke, Ottilie, Linde,
   Klee) does not do this, which is why these two stand out.
   **Repair, cheapest:** change **one** of them and the pattern disappears. Durst is the one to keep —
   the villain earns an emblem name, and it lands once, late. Rename {{RIVAL}} to a bird: **Star**
   (starling — real German word, real surname, and it is what a show-off is called), or **Specht**
   (woodpecker). *(Note: "Star" collides with English; **Specht** is safer.)*
   Same tic, minor characters, same repair if convenient: **Bleibe** ("a place to stay" — not a name;
   *die Bleibe* is a noun for lodgings, and she is the woman who stayed) and **Wagen** ("cart"; he kept
   the carters' horses). Both read as common nouns rather than names. Low priority — they are two
   villagers — but if either gets a line, rename first.

3. **`{{HERDER_PEOPLE}}` = Mohn / `{{HERDER_PEOPLE_PL}}` = Mohnen.**
   *Mohn* is poppy, and poppy means sleep, which is the herder people's whole magic — apt. Two issues.
   First, **"Mohnen" is not a German plural**; *Mohn* is largely a mass noun (*Mohnsorten*, *Mohnblumen*).
   For an invented ethnonym this is defensible, but it will read as an error to a German speaker, and
   the token exists in player-facing text (`halm.lamp_charm_2`, 0160). Second, to a German ear *Mohn*
   is first of all **the poppy seed on a bread roll**, so `"sleep like a {{HERDER_PEOPLE}}"` lands
   somewhere near "sleep like a poppyseed" — mildly comic under a line meant to be a grandfather's
   saying. **Repair:** **Schlaf-** compounds are too literal; take the flower rather than the seed —
   **Mohnblume / Mohnblumen** is correct German but long. Cleanest: **Nacht-** is banned by
   on-the-noseness, so use a different night plant — **Nachtkerze** (evening primrose) is too long;
   **Raute** (rue) pluralises cleanly as *Rauten* and carries no breakfast association.
   Lowest-cost option if the owner likes Mohn: keep the singular and make the plural **Mohne**.

4. **`{{SHEPHERD}}` = Garbe ("sheaf").** Real German surname, so it is fine — but *Garbe* is a grain
   word and the town is already **Halm**, a grain word. The shepherd is the one character who has
   nothing to do with grain. Two grain names, one of them on the wrong character. **Repair:** a pasture
   or sheep word — **Wicke** (vetch, a real surname and a real pasture plant) or **Kluft**.

### Creature and item names — `{{GRAIN_CREATURE}}` … `{{LOOT_SHRINE}}`

lid · thumb · milestone · follower · drape · gong · knuckle · sitter · drover · burr · lantern ·
fleece · weight · brace · tooth · green pebble.

**These are the opposite of generated and they are the table's best idea** — flat English nouns, no
capital letters spent, and the joke is always the shape of the thing. No fantasy-generator flavour
anywhere. Two collisions worth fixing:

1. **`{{NIGHT_CREATURE}}` = "lantern"** is overloaded three ways: the party **carries a lantern** on
   the high pasture, **fights "a lantern"** on the same map on the same night, and `lamp`/`lantern` is
   also the name of the point-light colormap table in `PALETTE.md`. The battle log will read
   `"a lantern is blinded."` while the player is holding one. **Repair: rename the creature.** It is a
   pale light that pulls you toward it — **"wick"** keeps the register, the single syllable and the
   plainness, and collides with nothing.
2. **`{{GUARD_BEAST}}` = "drover"** — in English a drover is a **person** who drives livestock. "My
   drover" reads as a hired hand, not an animal, and Distel *is* the drover. **Repair:** **"herd dog"**
   is too plain; **"walker"** collides with the walk-sprite pipeline; **"minder"** or **"shepherd
   beast"** both work, and **"minder"** is one word, plain, and says exactly what it does.

Everything else in the creature table: **leave.**

---

## 6. Summary of proposed edits

23 line-level repairs (19 are pure cuts or one-word swaps), 8 duplicate-removals, 4 battle-string
tweaks, 1 chapter subtitle, 3 name changes (`Frage`, `{{RIVAL}}`, `lantern`) and 3 optional ones
(`Mohnen`, `Garbe`, `drover`), 2 scene endings moved one line earlier, and 1 structural flag
(0150's two-handed sword transfer, SMELLS C11) that is not a cheap fix and is the orchestrator's call.

No LOCKED line is touched.
