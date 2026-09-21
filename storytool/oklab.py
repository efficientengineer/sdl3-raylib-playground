"""Oklab: the perceptual colour space the palette, the lighting and the tone match use.

Nearest-colour matching, tone matching and the colormap's night and lamp casts are all
done here rather than in RGB, because night and lamp tint by ADDING a cast and taking
chroma out — never by rotating hue, which runs a red roof through magenta.

Must never do: rotate a hue.
Public: _to_oklab, _from_oklab, oklab_means, match_tone, _lerp_ang.
Imports: nothing.
"""
import math



_SRGB_LIN = [((v / 255.0 / 12.92) if v / 255.0 <= 0.04045
              else (((v / 255.0 + 0.055) / 1.055) ** 2.4)) for v in range(256)]


def _to_oklab(r, g, b):
    R, G, B = _SRGB_LIN[r], _SRGB_LIN[g], _SRGB_LIN[b]
    l = (0.4122214708 * R + 0.5363325363 * G + 0.0514459929 * B) ** (1 / 3.0)
    m = (0.2119034982 * R + 0.6806995451 * G + 0.1073969566 * B) ** (1 / 3.0)
    s = (0.0883024619 * R + 0.2817188376 * G + 0.6299787005 * B) ** (1 / 3.0)
    return (0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s)


def _from_oklab(L, A, B_):
    l = (L + 0.3963377774 * A + 0.2158037573 * B_) ** 3
    m = (L - 0.1055613458 * A - 0.0638541728 * B_) ** 3
    s = (L - 0.0894841775 * A - 1.2914855480 * B_) ** 3
    out = []
    for lin in (+4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
                -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
                -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s):
        lin = 0.0 if lin < 0 else (1.0 if lin > 1 else lin)
        v = 12.92 * lin if lin <= 0.0031308 else 1.055 * lin ** (1 / 2.4) - 0.055
        out.append(max(0, min(255, int(round(v * 255)))))
    return out


def oklab_means(px, w, h):
    """(mean L, mean chroma) of an opaque tile, in Oklab."""
    n, sL, sC = 0, 0.0, 0.0
    for y in range(h):
        row = px[y]
        for x in range(w):
            L, A, B = _to_oklab(row[x * 4], row[x * 4 + 1], row[x * 4 + 2])
            sL += L
            sC += (A * A + B * B) ** 0.5
            n += 1
    return (sL / n, sC / n) if n else (0.0, 0.0)


def match_tone(px, w, h, want_L, want_C):
    """Shift a variant's mean lightness and scale its mean chroma to the family's first member.

    An offset on L and a gain on chroma: every pixel keeps its own distance from the mean, so the
    tile's detail, its local contrast and its hue relationships are all left exactly as drawn. Only
    the tone the eye reads from three tiles away moves."""
    have_L, have_C = oklab_means(px, w, h)
    dL = want_L - have_L
    gC = (want_C / have_C) if have_C > 1e-6 else 1.0
    gC = max(0.5, min(2.0, gC))
    if abs(dL) < 0.002 and abs(gC - 1) < 0.02:
        return have_L, have_C, have_L, have_C
    for y in range(h):
        row = px[y]
        for x in range(w):
            L, A, B = _to_oklab(row[x * 4], row[x * 4 + 1], row[x * 4 + 2])
            row[x * 4:x * 4 + 3] = bytes(_from_oklab(max(0.0, L + dL), A * gC, B * gC))
    now_L, now_C = oklab_means(px, w, h)
    return have_L, have_C, now_L, now_C


def _lerp_ang(a, b, t):
    d = math.atan2(math.sin(b - a), math.cos(b - a))
    return a + d * t
