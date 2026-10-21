"""GPIO assignment study for the Turret2 v2 board, two variants:
  A = every control line from the ESP32 (no expander)
  B = I2C GPIO expander (TCAL6416 / PCAL6416A) next to the new switches,
      driving SERVO_EN, LED_EN, RADAR_EN and HALL_EN_N locally.

Geometry: v1 board (Turret2.kicad_pcb). Module, connectors, AHCT125 and power
parts stay in place; IMU U6 and amplifier U9 (+ R10, R11) flip to the back in
place; new parts at the positions below (assumptions, to be checked in KiCad).

Net model: each net leaves its module pad through an escape point 1.2 mm out;
to a back SMD part, or to a front part hidden behind the module, it takes a via
right there and runs straight on the back (allowed under the module), with a
second via if it ends on a front SMD pad; otherwise it runs straight on the
front. Multi-destination nets are chains. Fixed nets (USB, UART0, PROG, EN)
are obstacles.

Cost = 10 x crossings on the same side + 2 x vias + 0.05 x length (mm)
     + 10 x legs through J16 (back) or the antenna keepout
     + 1 x back legs in the corridor under the module
     + 3 x holes in the pin bundles (amp, NeoPixel, I2C, radar).

Hard constraints: ADC1 = IO1-IO10 (ADC2 is unusable with Wi-Fi); wake from deep
sleep = IO0-IO21; touch = IO1-IO14; IO26 has a pull-up at reset (no
active-high enable, no amplifier line); amplifier gain lines only on pins
without pull-up and without power-up glitch; IO0, IO19, IO20, IO43-IO46 fixed.

    python gpio_study.py A|B|C [restarts] [iterations] [tag]   search, writes route_<tag>.json
    python gpio_study.py eval A|B|C <assignment.json|v1|final>    cost of an assignment

Variant C (expander on the back next to the IMU) is the one kept in
docs/carte-v2-plan.md (21.10.2026): FINAL below. V1 is the v1 pinout carried
over as such (plan of 21.10.2026, before this study).
"""
import itertools
import json
import math
import random
import sys

import kpcb

fps, _, _, _ = kpcb.load()
FP = {f.ref: f for f in fps}


def pad(ref, number):
    p = next(p for p in FP[ref].pads if p.number == number)
    return p.x, p.y


def flipped(ref, number):
    f = FP[ref]
    x, y = pad(ref, number)
    return 2 * f.x - x, y


MOD = (75.15, 86.55, 90.55, 107.05)
PINS = {}
for p in FP["U7"].pads:
    f = p.func
    if f.startswith(("IO", "TXD", "RXD", "USB", "EN")):
        name = f.split("_")[0]
        if name == "USB":
            name = "USB_D-" if p.number == "23" else "USB_D+"
        side = "L" if p.x < 76.5 else ("R" if p.x > 89 else "B")
        PINS[name] = (p.x, p.y, side, int(p.number))

ASSIGNABLE = ["IO%d" % i for i in list(range(1, 19)) + [21, 26] + list(range(33, 43)) + [47, 48]]
RTC = {"IO%d" % i for i in range(0, 22)}
ADC1 = {"IO%d" % i for i in range(1, 11)}
TOUCH = {"IO%d" % i for i in range(1, 15)}
WPU = {"IO26"}
GLITCH = {"IO%d" % i for i in list(range(1, 19))}
QUIET = set(ASSIGNABLE) - WPU - GLITCH
NOT_WPU = set(ASSIGNABLE) - WPU

# --- v2 placement assumptions (front unless noted) ---------------------------
POS = {
    "SW_SERVO": (94.0, 119.0, "F"),   # at the eFuse OUT node (star point), feeds the servo plane
    "SW_LED": (115.0, 94.5, "F"),     # logic +5V, near J14 / J15
    "SW_RADAR": (126.0, 110.0, "B"),  # back, near J5
    "Q_HALL": (122.0, 100.0, "F"),    # AO3401A between J17 and J18
    "EXP": (110.0, 108.0, "F"),       # variant B: expander on the front, between the switches
    "EXP_BACK": (107.0, 105.5, "B"),  # variant C: expander on the back, next to the IMU (I2C already there)
}


def T(ref, n):
    return pad(ref, n) + ("T",)


def F(ref, n):
    return pad(ref, n) + ("F",)


def Bk(ref, n):
    return flipped(ref, n) + ("B",)


def Bv1(ref, n):
    return pad(ref, n) + ("B",)


def P(name):
    x, y, s = POS[name]
    return (x, y, s)


BASE_NETS = {
    "GUN_LEFT": ([T("J3", "2")], None),
    "GUN_RIGHT": ([T("J4", "2")], None),
    "WING_LEFT": ([T("J7", "3")], None),
    "WING_RIGHT": ([T("J8", "3")], None),
    "ROTATE_X": ([T("J10", "3")], None),
    "ROTATE_Z": ([T("J9", "3")], None),
    "HALL_LEFT": ([T("J17", "2")], ADC1),
    "HALL_RIGHT": ([T("J18", "2")], ADC1),
    "NEO_CENTER": ([F("IC2", "2")], None),
    "NEO_LEFT": ([F("IC2", "5")], None),
    "NEO_RIGHT": ([F("IC2", "9")], None),
    "RADAR_RX": ([T("J5", "3")], None),
    "RADAR_TX": ([T("J5", "2")], None),
    "SDA": ([Bk("U6", "14"), F("J11", "3")], None),
    "SCL": ([Bk("U6", "13"), F("J11", "4")], None),
    "IMU_INT1": ([Bk("U6", "4")], RTC),
    "I2S_BCLK": ([Bk("U9", "16")], None),
    "I2S_LRCLK": ([Bk("U9", "14")], None),
    "I2S_DIN": ([Bk("U9", "1")], None),
    "AMP_SD": ([Bk("R10", "1")], NOT_WPU),
    "AMP_GAIN": ([Bk("U9", "2")], QUIET),
    "AMP_GAIN_100K": ([Bk("R11", "1")], QUIET),
    "PWR_FLT": ([F("IC1", "6")], None),
    "LED_GREEN": ([F("R18", "1")], NOT_WPU),
    "LED_RED": ([F("R12", "1")], NOT_WPU),
    "SW1": ([T("SW1", "1")], None),
    "SPARE_TOUCH": ([], TOUCH),
}
CONTROL_NETS = {
    "SERVO_EN": ([P("SW_SERVO")], NOT_WPU),
    "LED_EN": ([P("SW_LED")], NOT_WPU),
    "RADAR_EN": ([P("SW_RADAR")], NOT_WPU),
    "HALL_EN_N": ([P("Q_HALL")], None),
}

FIXED = {
    "USB_D-": ("USB_D-", [F("D2", "6")]),
    "USB_D+": ("USB_D+", [F("D2", "4")]),
    "TXD0": ("TXD0", [Bv1("J16", "1")]),
    "RXD0": ("RXD0", [Bv1("J16", "2")]),
    "PROG": ("IO0", [F("SW2", "2"), Bv1("J16", "5")]),
    "EN": ("EN", [F("R14", "2"), Bv1("J16", "4")]),
}


def make_variant(v):
    nets = dict(BASE_NETS)
    extra = []  # legs that do not start at the module: (name, start, dests)
    if v == "A":
        nets.update(CONTROL_NETS)
    elif v == "B":
        # expander on the front: the bus continues to it; EN goes to its RESET pin
        nets["SDA"] = (nets["SDA"][0] + [P("EXP")], None)
        nets["SCL"] = (nets["SCL"][0] + [P("EXP")], None)
        for name, (dests, _) in CONTROL_NETS.items():
            extra.append(("X_" + name, P("EXP"), dests))
        extra.append(("X_RESET", F("R14", "2"), [P("EXP")]))
    else:
        # expander on the back next to the IMU: module -> IMU -> expander -> J11
        e = P("EXP_BACK")
        nets["SDA"] = ([Bk("U6", "14"), e, F("J11", "3")], None)
        nets["SCL"] = ([Bk("U6", "13"), e, F("J11", "4")], None)
        for name, (dests, _) in CONTROL_NETS.items():
            extra.append(("X_" + name, e, dests))
        extra.append(("X_RESET", F("R14", "2"), [e]))
    n_spare = len(ASSIGNABLE) - len(nets)
    for i in range(n_spare):
        nets["SPARE%d" % (i + 1)] = ([], None)
    return nets, extra


GROUPS = {
    "amp": ["I2S_BCLK", "I2S_LRCLK", "I2S_DIN", "AMP_SD", "AMP_GAIN", "AMP_GAIN_100K"],
    "neo": ["NEO_CENTER", "NEO_LEFT", "NEO_RIGHT"],
    "i2c": ["SDA", "SCL"],
    "radar": ["RADAR_RX", "RADAR_TX"],
}
J16_BACK = (83.0, 93.6, 88.3, 102.0)
ANTENNA = (75.15, 86.55, 90.55, 91.5)
UNDER = (75.15, 91.5, 90.55, 107.05)


def escape(pin):
    x, y, side, _ = PINS[pin]
    return {"L": (x - 1.2, y), "R": (x + 1.2, y), "B": (x, y + 1.2)}[side]


ESCAPES = {escape(p) for p in PINS}


def hits(a, b, r, margin=0.4):
    x0, y0, x1, y1 = r[0] - margin, r[1] - margin, r[2] + margin, r[3] + margin
    for i in range(1, 40):
        t = i / 40
        x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
        if x0 < x < x1 and y0 < y < y1:
            return True
    return False


def chain(start, dests, start_layer="F"):
    legs, vias = [], 0
    layer, pos = start_layer, start
    for i, (x, y, kind) in enumerate(dests):
        d = (x, y)
        if i == 0 and start_layer == "F" and start in ESCAPES and (kind == "B" or hits(start, d, MOD)):
            vias += 1
            layer = "B"
            if kind == "F":
                legs.append(("B", pos, d))
                vias += 1
                layer, pos = "F", d
                continue
        else:
            want = {"F": "F", "B": "B", "T": layer}[kind]
            if want != layer:
                vias += 1
                layer = want
        legs.append((layer, pos, d))
        pos = d
    return legs, vias


def cross(p1, p2, p3, p4):
    def o(a, b, c):
        return (c[1] - a[1]) * (b[0] - a[0]) - (b[1] - a[1]) * (c[0] - a[0])

    d1, d2, d3, d4 = o(p3, p4, p1), o(p3, p4, p2), o(p1, p2, p3), o(p1, p2, p4)
    return (d1 > 0) != (d2 > 0) and (d3 > 0) != (d4 > 0) and min(abs(d1), abs(d2), abs(d3), abs(d4)) > 1e-9


class Model:
    def __init__(self, variant):
        self.variant = variant
        self.nets, self.extra = make_variant(variant)
        self.fixed_legs = []
        for name, (pin, dests) in FIXED.items():
            legs, _ = chain(escape(pin), dests)
            self.fixed_legs += [(name, l) for l in legs]
        # slow lines that do not start at the module: two options each (run on the front
        # or on the back, vias where needed); evaluate() keeps the one that crosses less
        self.extra_opts = []
        for name, start, dests in self.extra:
            opts = []
            for run in "FB":
                legs, vias, layer, pos = [], 0, start[2] if start[2] in "FB" else "F", start[:2]
                for (x, y, kind) in dests:
                    if layer != run:
                        vias += 1
                        layer = run
                    legs.append((run, pos, (x, y)))
                    pos = (x, y)
                    end = {"F": "F", "B": "B", "T": run}[kind]
                    if end != run:
                        vias += 1
                        layer = end
                opts.append(([(name, l) for l in legs], vias))
            self.extra_opts.append(opts)
        self.extra_legs, self.extra_vias = [], 0

    def allowed(self, net, pin):
        c = self.nets[net][1]
        return c is None or pin in c

    def evaluate(self, assign, detail=False):
        legs, vias, length = list(self.fixed_legs), 0, 0.0
        for net, (dests, _) in self.nets.items():
            if not dests:
                continue
            l, v = chain(escape(assign[net]), dests)
            vias += v
            legs += [(net, x) for x in l]
        for opts in self.extra_opts:
            def n_cross(option):
                return sum(1 for (n1, l1) in option[0] for (n2, l2) in legs
                           if l1[0] == l2[0] and cross(l1[1], l1[2], l2[1], l2[2]))
            option = min(opts, key=lambda o: (n_cross(o), o[1]))
            legs += option[0]
            vias += option[1]
        for name, (layer, a, b) in legs:
            if name not in FIXED:
                length += math.dist(a, b)
        crossings = []
        for (n1, l1), (n2, l2) in itertools.combinations(legs, 2):
            if n1 != n2 and l1[0] == l2[0] and cross(l1[1], l1[2], l2[1], l2[2]):
                crossings.append((n1, n2, l1[0]))
        blocked, under = [], 0
        for name, (layer, a, b) in legs:
            if name in FIXED:
                continue
            if hits(a, b, ANTENNA, 0.0) or (layer == "B" and hits(a, b, J16_BACK, 0.2)):
                blocked.append(name)
            if layer == "B" and hits(a, b, UNDER, 0.0):
                under += 1
        holes = 0
        for g in GROUPS.values():
            idx = sorted(PINS[assign[n]][3] for n in g)
            holes += idx[-1] - idx[0] + 1 - len(idx)
        cost = 10 * len(crossings) + 2 * vias + 0.05 * length + 10 * len(blocked) + under + 3 * holes
        if detail:
            return dict(cost=cost, crossings=crossings, vias=vias, length=length, blocked=blocked, under=under,
                        holes=holes, legs=legs)
        return cost

    def initial(self, seed=0):
        rng = random.Random(seed)
        for _ in range(10000):
            pins = list(ASSIGNABLE)
            rng.shuffle(pins)
            assign = {}
            # most constrained first
            order = sorted(self.nets, key=lambda n: len(self.nets[n][1]) if self.nets[n][1] else 99)
            ok = True
            for net in order:
                choices = [p for p in pins if self.allowed(net, p)]
                if not choices:
                    ok = False
                    break
                p = choices[0]
                assign[net] = p
                pins.remove(p)
            if ok:
                return assign
        raise RuntimeError("no feasible assignment")

    def anneal(self, start, iters, seed):
        rng = random.Random(seed)
        cur = dict(start)
        cur_cost = self.evaluate(cur)
        best, best_cost = dict(cur), cur_cost
        nets = list(cur)
        temp = 30.0
        decay = (0.05 / 30.0) ** (1.0 / iters)
        for _ in range(iters):
            a, b = rng.sample(nets, 2)
            pa, pb = cur[a], cur[b]
            if not (self.allowed(a, pb) and self.allowed(b, pa)):
                continue
            cur[a], cur[b] = pb, pa
            c = self.evaluate(cur)
            if c < cur_cost or rng.random() < math.exp((cur_cost - c) / temp):
                cur_cost = c
                if c < best_cost:
                    best, best_cost = dict(cur), c
            else:
                cur[a], cur[b] = pa, pb
            temp *= decay
        return best, best_cost


V1 = {
    "GUN_LEFT": "IO1",
    "GUN_RIGHT": "IO2",
    "SW1": "IO3",
    "WING_LEFT": "IO4",
    "WING_RIGHT": "IO5",
    "ROTATE_X": "IO6",
    "ROTATE_Z": "IO7",
    "HALL_LEFT": "IO8",
    "HALL_RIGHT": "IO9",
    "IMU_INT1": "IO10",
    "SPARE_TOUCH": "IO11",
    "SERVO_EN": "IO12",
    "LED_EN": "IO13",
    "NEO_CENTER": "IO14",
    "NEO_LEFT": "IO15",
    "NEO_RIGHT": "IO16",
    "RADAR_RX": "IO17",
    "RADAR_TX": "IO18",
    "AMP_GAIN": "IO21",
    "HALL_EN_N": "IO26",
    "LED_GREEN": "IO33",
    "SPARE1": "IO34",
    "SDA": "IO35",
    "SCL": "IO36",
    "RADAR_EN": "IO37",
    "PWR_FLT": "IO38",
    "I2S_BCLK": "IO39",
    "I2S_LRCLK": "IO40",
    "I2S_DIN": "IO41",
    "AMP_SD": "IO42",
    "AMP_GAIN_100K": "IO47",
    "LED_RED": "IO48"
}

FINAL = {
    "HALL_LEFT": "IO8",
    "HALL_RIGHT": "IO10",
    "AMP_GAIN": "IO38",
    "AMP_GAIN_100K": "IO40",
    "IMU_INT1": "IO15",
    "AMP_SD": "IO37",
    "LED_GREEN": "IO13",
    "LED_RED": "IO47",
    "GUN_LEFT": "IO33",
    "GUN_RIGHT": "IO26",
    "WING_LEFT": "IO34",
    "WING_RIGHT": "IO36",
    "ROTATE_X": "IO1",
    "ROTATE_Z": "IO17",
    "NEO_CENTER": "IO18",
    "NEO_LEFT": "IO16",
    "NEO_RIGHT": "IO14",
    "RADAR_RX": "IO48",
    "RADAR_TX": "IO35",
    "SDA": "IO12",
    "SCL": "IO11",
    "I2S_BCLK": "IO41",
    "I2S_LRCLK": "IO42",
    "I2S_DIN": "IO39",
    "PWR_FLT": "IO21",
    "SW1": "IO9",
    "SPARE_TOUCH": "IO2",
    "SPARE1": "IO3",
    "SPARE2": "IO4",
    "SPARE3": "IO5",
    "SPARE4": "IO6",
    "SPARE5": "IO7"
}


def summary(m, assign, title):
    r = m.evaluate(assign, detail=True)
    return ("%s: cost %.0f | crossings %d (front %d, back %d) | vias %d | length %.0f mm | blocked %d | "
            "under module %d | bundle holes %d" % (title, r["cost"], len(r["crossings"]),
                                                   sum(1 for c in r["crossings"] if c[2] == "F"),
                                                   sum(1 for c in r["crossings"] if c[2] == "B"), r["vias"],
                                                   r["length"], len(r["blocked"]), r["under"], r["holes"]))


def load_assignment(src):
    return dict(V1) if src == "v1" else dict(FINAL) if src == "final" else json.load(open(src))


if __name__ == "__main__":
    if sys.argv[1] == "eval":
        m = Model(sys.argv[2])
        a = load_assignment(sys.argv[3])
        print(summary(m, a, "variant %s, %s" % (sys.argv[2], sys.argv[3])))
        r = m.evaluate(a, detail=True)
        print("crossings:", r["crossings"])
        print("blocked:", r["blocked"])
        sys.exit(0)
    variant = sys.argv[1]
    restarts = int(sys.argv[2]) if len(sys.argv) > 2 else 6
    iters = int(sys.argv[3]) if len(sys.argv) > 3 else 60000
    m = Model(variant)
    results = []
    import os
    seed0 = int(os.environ.get("SEED0", "0"))
    for seed in range(seed0, seed0 + restarts):
        best, cost = m.anneal(m.initial(seed), iters, seed)
        results.append((cost, seed, best))
        print("seed %d: %.1f" % (seed, cost), flush=True)
    results.sort(key=lambda r: r[0])
    cost, seed, best = results[0]
    print(summary(m, best, "variant %s best (seed %d)" % (variant, seed)))
    r = m.evaluate(best, detail=True)
    print("crossings:", r["crossings"])
    print("blocked:", r["blocked"])
    for side in "LBR":
        pins = sorted((PINS[p][3], p) for p in ASSIGNABLE if PINS[p][2] == side)
        inv = {v: k for k, v in best.items()}
        print(side, " ".join("%s=%s" % (p, inv.get(p, "-")) for _, p in pins))
    tag = sys.argv[4] if len(sys.argv) > 4 else variant
    json.dump(best, open("route_%s.json" % tag, "w"), indent=1)
