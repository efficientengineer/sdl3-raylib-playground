# Palette cycles (D19)

One line a cycle: `name: index index index… @ fps`. The engine rewrites those columns of
the colormap every frame, rotating the listed indices through each other's colours, so a
river moves and a lamp flickers without a second frame of art being drawn. The indices are
consecutive steps of one ramp in `master.hex`; see `master.json` for the ramp spans.

water: 115 116 117 118 @ 6
fire_lamp: 21 22 23 24 @ 10
