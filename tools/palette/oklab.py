"""sRGB <-> Oklab, stdlib only."""
import math

_SRGB_TO_LIN = []
for i in range(256):
    c = i / 255.0
    _SRGB_TO_LIN.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)

_CBRT = None


def srgb_to_oklab(r, g, b):
    lr, lg, lb = _SRGB_TO_LIN[r], _SRGB_TO_LIN[g], _SRGB_TO_LIN[b]
    l = 0.4122214708 * lr + 0.5363325363 * lg + 0.0514459929 * lb
    m = 0.2119034982 * lr + 0.6806995451 * lg + 0.1073969566 * lb
    s = 0.0883024619 * lr + 0.2817188376 * lg + 0.6299787005 * lb
    l_, m_, s_ = l ** (1 / 3), m ** (1 / 3), s ** (1 / 3)
    return (0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
            1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
            0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_)


def oklab_to_srgb(L, a, b):
    l_ = L + 0.3963377774 * a + 0.2158037573 * b
    m_ = L - 0.1055613458 * a - 0.0638541728 * b
    s_ = L - 0.0894841775 * a - 1.2914855480 * b
    l, m, s = l_ ** 3, m_ ** 3, s_ ** 3
    lr = +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s
    lg = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s
    lb = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s
    out = []
    for v in (lr, lg, lb):
        v = max(0.0, min(1.0, v))
        v = 12.92 * v if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055
        out.append(max(0, min(255, int(round(v * 255)))))
    return tuple(out)


def de(p, q):
    return math.sqrt((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2)


def hue(lab):
    return math.degrees(math.atan2(lab[2], lab[1])) % 360.0


def chroma(lab):
    return math.hypot(lab[1], lab[2])
