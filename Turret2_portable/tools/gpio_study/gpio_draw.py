"""Draws an assignment of route_v2 on the v1 board.

    python gpio_draw.py <A|B|C> <assignment.json|v1|final> <out.png> <title>

Needs Pillow (pip install pillow).
"""
import itertools
import json
import sys

from PIL import Image, ImageDraw, ImageFont

import gpio_study as rv

S = 16
X0, Y0 = 62.0, 80.0


def px(x, y):
    return (x - X0) * S, (y - Y0) * S


def main():
    variant, src, out, title = sys.argv[1:5]
    m = rv.Model(variant)
    assign = rv.load_assignment(src)
    r = m.evaluate(assign, detail=True)
    img = Image.new("RGB", (int(82 * S), int(56 * S)), "white")
    d = ImageDraw.Draw(img)
    try:
        small = ImageFont.truetype("arial.ttf", 11)
        font = ImageFont.truetype("arial.ttf", 13)
        big = ImageFont.truetype("arialbd.ttf", 18)
    except OSError:
        small = font = big = ImageFont.load_default()
    d.rectangle([px(64.4, 86.4), px(141.6, 127.0)], outline=(110, 110, 110), width=2)
    for f in rv.fps:
        if f.ref in ("U7",) or f.ref.startswith(("TP", "H")) or "LOGO" in f.ref or not f.pads:
            continue
        if f.ref in ("J12", "J13", "R13", "R17", "R19", "R20", "C11", "C12"):
            continue  # removed in v2
        xs, ys = [p.x for p in f.pads], [p.y for p in f.pads]
        side = f.side
        if f.ref in ("U6", "U9", "R10", "R11"):
            side = "B"
            xs = [2 * f.x - x for x in xs]
        col = (205, 125, 125) if side == "F" else (120, 140, 215)
        d.rectangle([px(min(xs) - 0.4, min(ys) - 0.4), px(max(xs) + 0.4, max(ys) + 0.4)], outline=col)
        d.text(px(min(xs) - 0.4, min(ys) - 1.6), f.ref, fill=col, font=small)
    names = {"SW_SERVO": "SW_SERVO", "SW_LED": "SW_LED", "SW_RADAR": "SW_RADAR", "Q_HALL": "Q_HALL"}
    if variant == "B":
        names["EXP"] = "EXTENDER"
    elif variant == "C":
        names["EXP_BACK"] = "EXTENDER"
    for key, label in names.items():
        x, y, side = rv.POS[key]
        col = (190, 0, 150)
        d.rectangle([px(x - 1, y - 1), px(x + 1, y + 1)], outline=col, width=2)
        d.text(px(x - 1, y - 2.7), label + (" (arr.)" if side == "B" else ""), fill=col, font=small)
    jx0, jy0, jx1, jy1 = rv.J16_BACK
    d.rectangle([px(jx0, jy0), px(jx1, jy1)], outline=(120, 140, 215), width=1)
    d.rectangle([px(*rv.MOD[:2]), px(*rv.MOD[2:])], outline=(0, 0, 0), width=2)
    d.rectangle([px(*rv.ANTENNA[:2]), px(*rv.ANTENNA[2:])], fill=(235, 235, 235), outline=(0, 0, 0))
    d.text(px(79.5, 88.0), "antenne", fill=(0, 0, 0), font=small)
    for net, (layer, a, b) in r["legs"]:
        col = (220, 40, 40) if layer == "F" else (40, 90, 220)
        if net in rv.FIXED:
            col = (160, 160, 160)
        elif net.startswith("X_"):
            col = (190, 0, 150) if layer == "F" else (120, 60, 200)
        d.line([px(*a), px(*b)], fill=col, width=2)
    for (n1, l1), (n2, l2) in itertools.combinations(r["legs"], 2):
        if n1 == n2 or l1[0] != l2[0] or not rv.cross(l1[1], l1[2], l2[1], l2[2]):
            continue
        (x1, y1), (x2, y2) = l1[1], l1[2]
        (x3, y3), (x4, y4) = l2[1], l2[2]
        den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)
        t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / den
        X, Y = px(x1 + t * (x2 - x1), y1 + t * (y2 - y1))
        d.ellipse([X - 5, Y - 5, X + 5, Y + 5], outline=(0, 0, 0), width=2)
    inv = {v: k for k, v in assign.items()}
    fixed_labels = {"USB_D-": "USB D-", "USB_D+": "USB D+", "TXD0": "UART0 TX", "RXD0": "UART0 RX", "EN": "EN",
                    "IO0": "PROG", "IO45": "(strap)", "IO46": "(strap)"}
    for pin, (x, y, side, num) in rv.PINS.items():
        label = inv.get(pin, fixed_labels.get(pin, ""))
        if label.startswith("SPARE"):
            label = "libre"
        X, Y = px(x, y)
        d.ellipse([X - 3, Y - 3, X + 3, Y + 3], fill=(0, 0, 0))
        text = "%s %s" % (pin, label)
        if side == "L":
            w = d.textlength(text, font=small)
            d.text((X - 8 - w, Y - 7), text, fill=(0, 0, 0), font=small)
        elif side == "R":
            d.text((X + 8, Y - 7), text, fill=(0, 0, 0), font=small)
        else:
            tmp = Image.new("RGBA", (int(d.textlength(text, font=small)) + 4, 14), (255, 255, 255, 0))
            ImageDraw.Draw(tmp).text((0, 0), text, fill=(0, 0, 0), font=small)
            tmp = tmp.rotate(90, expand=True)
            img.paste(tmp, (int(X - 7), int(Y + 8)), tmp)
    d.text((10, 6), "%s : %d croisements (avant %d, arriere %d), %d vias, %.0f mm, %d bloques" % (
        title, len(r["crossings"]), sum(1 for c in r["crossings"] if c[2] == "F"),
        sum(1 for c in r["crossings"] if c[2] == "B"), r["vias"], r["length"], len(r["blocked"])),
        fill=(0, 0, 0), font=big)
    d.text((10, int(54.5 * S)), "rouge = face avant, bleu = face arriere, gris = lignes fixes (USB, UART0, PROG, EN), "
           "violet = sorties de l'extender ; cercle = croisement sur la meme face ; rectangle bleu sous le module = J16",
           fill=(0, 0, 0), font=font)
    img.save(out)
    print(out)


if __name__ == "__main__":
    main()
