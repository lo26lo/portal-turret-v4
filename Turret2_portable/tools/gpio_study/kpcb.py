"""Minimal reader of a KiCad .kicad_pcb file (S-expressions): footprints with
their position, rotation and side, pads in board coordinates, tracks and vias
per net, board outline. Used by route_study.py."""
import math
import os
import re

# default: the board of the repository; set TURRET_PCB to study another file
PCB = os.environ.get("TURRET_PCB", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                                                "Turret2.kicad_pcb"))

TOKEN = re.compile(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+')


def parse(text):
    stack = [[]]
    for m in TOKEN.finditer(text):
        t = m.group(0)
        if t == "(":
            stack.append([])
        elif t == ")":
            node = stack.pop()
            stack[-1].append(node)
        else:
            stack[-1].append(t[1:-1] if t.startswith('"') else t)
    return stack[0][0]


def find(node, name):
    return [c for c in node if isinstance(c, list) and c and c[0] == name]


def first(node, name):
    r = find(node, name)
    return r[0] if r else None


def prop(fp, key):
    for p in find(fp, "property"):
        if len(p) > 2 and p[1] == key:
            return p[2]
    return None


class Pad:
    def __init__(self, ref, number, x, y, net, func, smd, layers):
        self.ref, self.number, self.x, self.y = ref, number, x, y
        self.net, self.func, self.smd, self.layers = net, func, smd, layers

    def __repr__(self):
        return "%s.%s(%.2f,%.2f %s)" % (self.ref, self.number, self.x, self.y, self.net)


class Footprint:
    def __init__(self, ref, value, lib, x, y, rot, side, pads):
        self.ref, self.value, self.lib = ref, value, lib
        self.x, self.y, self.rot, self.side, self.pads = x, y, rot, side, pads


def to_board(fx, fy, rot, px, py):
    a = math.radians(rot)
    return fx + px * math.cos(a) + py * math.sin(a), fy - px * math.sin(a) + py * math.cos(a)


def load(path=None):
    root = parse(open(path or PCB, encoding="utf-8").read())
    fps = []
    for fp in find(root, "footprint"):
        at = first(fp, "at")
        fx, fy = float(at[1]), float(at[2])
        rot = float(at[3]) if len(at) > 3 else 0.0
        side = "B" if first(fp, "layer")[1].startswith("B.") else "F"
        ref = prop(fp, "Reference") or "?"
        pads = []
        for p in find(fp, "pad"):
            pat = first(p, "at")
            x, y = to_board(fx, fy, rot, float(pat[1]), float(pat[2]))
            net = first(p, "net")
            netname = (net[-1] if net else "") if net else ""
            func = first(p, "pinfunction")
            layers = first(p, "layers")
            pads.append(Pad(ref, p[1], x, y, netname, func[1] if func else "", p[2] == "smd", layers[1:] if layers else []))
        fps.append(Footprint(ref, prop(fp, "Value") or "", fp[1], fx, fy, rot, side, pads))
    tracks, vias = [], []
    for s in find(root, "segment"):
        a, b = first(s, "start"), first(s, "end")
        tracks.append((first(s, "net")[-1], first(s, "layer")[1], float(a[1]), float(a[2]), float(b[1]), float(b[2]),
                       float(first(s, "width")[1])))
    for s in find(root, "arc"):
        a, b = first(s, "start"), first(s, "end")
        tracks.append((first(s, "net")[-1], first(s, "layer")[1], float(a[1]), float(a[2]), float(b[1]), float(b[2]),
                       float(first(s, "width")[1])))
    for v in find(root, "via"):
        at = first(v, "at")
        vias.append((first(v, "net")[-1], float(at[1]), float(at[2])))
    edges = []
    for g in find(root, "gr_line") + find(root, "gr_rect") + find(root, "gr_arc") + find(root, "gr_poly"):
        layer = first(g, "layer")
        if layer and layer[1] == "Edge.Cuts":
            for k in ("start", "end", "mid"):
                q = first(g, k)
                if q:
                    edges.append((float(q[1]), float(q[2])))
            pts = first(g, "pts")
            if pts:
                for xy in find(pts, "xy"):
                    edges.append((float(xy[1]), float(xy[2])))
    return fps, tracks, vias, edges
