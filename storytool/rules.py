"""The numbers that mirror story/STYLE.md, in one place.

Panel counts, word caps, the banned-word lists, the shot shapes, the expression-sheet ids
and their order, the scene kinds and the music moods. The prose lives in STYLE.md; these
are the same thresholds written as constants, and the two are changed together — the
comment on each block names the STYLE.md rule it mirrors.

Must never do: read a file, define a function, or import anything of this tool's.
Public: constants only. Imports: nothing.
"""



# ── Rule thresholds (mirror STYLE.md "Composition rules") ──
PANELS_MIN, PANELS_MAX = 2, 4                       # R1, per page


SCENE_MAX_PANELS = 8                                # R1, per scene (one shot sheet)


PAGES_MAX = 3                                       # R1, per scene ('---' in ## Panels starts a page)


ANCHOR_SCALES = {"wide", "full", "medium"}          # R5


PUNCH_SCALES = {"close", "extreme", "insert"}       # R5


MAX_PANEL_WORDS = 40                                # R7


MAX_ACTING_WORDS = 25                               # R11


GAZE_WORDS = ["look", "glar", "star", "gaz", "glanc", "watch", "eyes", "eye ", "facing", "faces ", "turned",
              "peer", "squint", "fixed on"]          # R11: an acting line must say where the eyes point


STYLE_WORDS = ["gradient", "photorealistic", "3d", "blur", "glow", "painterly",
               "realistic", "hd", "4k", "smooth"]   # R8


TEXT_WORDS = ["text", "caption", "speech bubble", "lettering", "written", "inscription reading",
              "sign saying", "sign that says", "sign reading", "the words", "subtitle"]  # R9


MOODS = ["wonder", "dread", "tense", "confront", "sorrow", "hope"]      # music moods; order = CsMood enum in the game


NARRATOR = "narrator"                               # speaker for an unattributed box: no name, no portrait


LOOK_BANNED = ["dwarf", "dwarven", "halfling", "hobbit", "elf", "elven", "gnome", "orc", "fighter",
               "rogue", "cleric", "wizard", "ranger", "barbarian", "paladin", "beard", "bearded"]  # see characters.md


REQUIRED_BLOCKS = ["header", "layout", "framing", "acting", "character_design", "rendering",
                   "dialogue_box", "negative", "sheet_layout", "sheet_avoid", "refsheet", "refsheet_avoid",
                   "expressions", "expressions_avoid"]


# ── Dialogue-box expressions (mirror STYLE.md "Expression sheet") ──
#
# One expression sheet a character: ten head-and-shoulders portraits in the same framing, so a
# dialogue line can switch the face without switching the character. The ids and their ORDER are the
# contract — the slot number on the template is the index into this list, the file name is the id,
# and a scene tags a line with `- Name (biglaugh): ...`. Never reorder or rename one: the sheets
# already drawn are cut by slot number.
EXPRESSIONS = (
    ("neutral", "calm and level, mouth closed, eyes open and steady, looking at the viewer"),
    ("smile", "a small warm closed-mouth smile, eyes softened, one brow slightly raised"),
    ("laugh", "laughing openly, mouth wide and grinning, eyes crinkled nearly shut, head tipped back a little"),
    ("biglaugh", "laughing extremely hard, mouth wide open, eyes squeezed shut, head thrown back, "
                 "one shoulder up and a tear at the corner of one eye"),
    ("concern", "concerned, brows drawn together and up in the middle, mouth a small flat line, "
                "eyes searching slightly off to one side"),
    ("sorrow", "extreme sorrow, head lowered, brows up in the middle, eyes shut, mouth trembling open, "
               "tears running on both cheeks"),
    ("annoyed", "annoyed, one brow down and one up, eyes half lidded and turned aside, mouth pulled "
                "flat to one side"),
    ("angry", "furious, both brows hard down, eyes wide and fixed on the viewer, teeth bared, jaw set"),
    ("shock", "shocked, eyes wide with small pupils, brows high, mouth open, head pulled back"),
    ("resolve", "resolved, chin lifted, brows level and firm, eyes narrowed and fixed ahead, mouth "
                "closed and set"),
)


EXPR_IDS = tuple(e for e, _ in EXPRESSIONS)


EXPR_COLS, EXPR_ROWS = 5, 2           # the template is 2 rows of 5, numbered in reading order


EXPR_ASPECT = 2 / 3                   # width / height of a slot, the dialogue portrait's own shape


EXPR_OUT_H = 288                      # ...and the height a portrait ships at (PORTRAIT_H)


EXPR_OUT_W = 192


# ── Sheet mechanics (layout maths, not style) ──
SHEET_MAX_PANELS = 8


SHAPE_ASPECT = {"wide": 2.0, "tall": 0.5, "square": 1.0, "slit": 4.0}   # width / height


SCENE_KINDS = ("panels", "narration", "talk")  # order = CsKind enum in the game


PORTRAIT_TRIM = 0.022         # fraction of the panel's short side shaved off, to lose the white border


PORTRAIT_H = 288              # a portrait ships this tall, aspect kept (WORLD.md, "Sizes": the


                              # dialogue box draws it about 206 px tall on the phone and 275 at
                              # 1440p, so a ~850-px source was four times the size it is ever shown)
BLACK_MAX = 40                # a pixel is background if max(r,g,b) <= this


GRID = 4                      # slicer works on a 1/GRID downsample, then refines


MIN_PANEL_AREA = 0.012        # ignore specks smaller than this fraction of the image
