"""Aperture logo (6 blades around a hexagon) for the OLED and the web page.

    python tools/aperture_logo.py [size]

Prints the XBM bitmap for src/ui/ApertureLogo.h (default 42 x 42, U8g2
drawXBM: least significant bit = leftmost pixel, 1 = lit), an ASCII preview,
and the SVG used in src/web/page/index.html (header and favicon).

Geometry fitted on 21.10.2026 on the picture given by the user (512 x 512,
1.3 % of the pixels differ, on the smoothed edges): disc of radius 1, white
hexagon of apothem D1 (corners at 0, 60, ... degrees), and six white gaps:
bands between D0 and D1 from the centre, along each side of the hexagon,
running from one end of the side out to the rim. Plain Python, no package.
"""
import math
import sys

D0, D1 = 0.28, 0.34
U0, U1 = -D0 * math.tan(math.pi / 6), 1.2
OVERSAMPLE = 8
COVER = 0.55  # a pixel is lit when the blades cover at least this much of it


def sides():
    for k in range(6):
        t = math.radians(90 + 60 * k)  # normal of the side, y pointing down
        n = (math.cos(t), math.sin(t))
        yield n, (n[1], -n[0])


SIDES = list(sides())


def is_blade(x, y):
    if x * x + y * y > 1.0:
        return False
    in_hexagon = True
    for n, tg in SIDES:
        d = x * n[0] + y * n[1]
        u = x * tg[0] + y * tg[1]
        if D0 < d < D1 and u > U0:
            return False  # in a gap
        in_hexagon = in_hexagon and d < D1
    return not in_hexagon


def bitmap(size):
    m = size * OVERSAMPLE
    rows = []
    for row in range(size):
        bits = []
        for column in range(size):
            hits = 0
            for sy in range(OVERSAMPLE):
                y = (row * OVERSAMPLE + sy + 0.5) / m * 2 - 1
                for sx in range(OVERSAMPLE):
                    x = (column * OVERSAMPLE + sx + 0.5) / m * 2 - 1
                    hits += is_blade(x, y)
            bits.append(hits >= COVER * OVERSAMPLE * OVERSAMPLE)
        rows.append(bits)
    return rows


def xbm(rows):
    data = []
    for bits in rows:
        for byte in range((len(bits) + 7) // 8):
            data.append(sum(1 << b for b in range(8) if byte * 8 + b < len(bits) and bits[byte * 8 + b]))
    return data


def svg():
    def p(x, y):
        return "%.1f,%.1f" % (50 + 50 * x, 50 + 50 * y)

    r = D1 / math.cos(math.pi / 6)
    hexagon = " ".join(p(r * math.cos(math.radians(60 * k)), r * math.sin(math.radians(60 * k))) for k in range(6))
    gaps = ""
    for n, tg in SIDES:
        corners = [(n[0] * d + tg[0] * u, n[1] * d + tg[1] * u) for d, u in ((D0, U0), (D0, U1), (D1, U1), (D1, U0))]
        gaps += '<polygon points="%s"/>' % " ".join(p(*c) for c in corners)
    return ('<svg viewBox="0 0 100 100" aria-hidden="true"><mask id="ap"><circle cx="50" cy="50" r="50" fill="#fff"/>'
            '<g fill="#000"><polygon points="%s"/>%s</g></mask>'
            '<circle cx="50" cy="50" r="50" fill="currentColor" mask="url(#ap)"/></svg>' % (hexagon, gaps))


if __name__ == "__main__":
    size = int(sys.argv[1]) if len(sys.argv) > 1 else 42
    rows = bitmap(size)
    for bits in rows:
        print("// " + "".join("#" if b else "." for b in bits))
    data = xbm(rows)
    for i in range(0, len(data), 12):
        print("    " + ", ".join("0x%02x" % v for v in data[i:i + 12]) + ",")
    print()
    print(svg())
