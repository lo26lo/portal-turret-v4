# Carte Turret2 v2 : verifie que chaque condensateur de decouplage est relie a la broche
# du circuit qu'il decouple (piste directe sur la meme couche, ou zone du meme net si c'est
# voulu), et que son pad GND a une via proche.
# Sur le schema, un condensateur n'est relie qu'aux nets d'alimentation et GND : rien n'y dit
# a quelle broche il appartient. Ce script porte cette intention (table MAP, cf.
# docs/carte-v2-kicad.md, phase 6, « Decouplage »).
# Usage (Python de KiCad) : python check_decap.py chemin/Turret2.kicad_pcb
import pcbnew, sys, math, collections
if len(sys.argv) < 2:
    sys.exit('usage : python check_decap.py Turret2.kicad_pcb')
b = pcbnew.LoadBoard(sys.argv[1])
mm = pcbnew.ToMM
# condensateur -> (cible lisible, [(ref, broche), ...]) ; une seule broche atteinte suffit
MAP = [
 ('C1',  'IC1 IN (3/4), entree eFuse',        [('IC1','3'),('IC1','4')]),
 ('C2',  'IC1 DVDT (1)',                      [('IC1','1')]),
 ('C3',  'U1 VIN (4), TPS631000',             [('U1','4')]),
 ('C4',  'U1 VIN (4), TPS631000',             [('U1','4')]),
 ('C5',  'U1 VOUT (1)',                       [('U1','1')]),
 ('C6',  'U1 VOUT (1)',                       [('U1','1')]),
 ('C7',  'U1 VOUT (1)',                       [('U1','1')]),
 ('C8',  'IC2 VCC (14), AHCT',                [('IC2','14')]),
 ('C9',  'U10 VOUT (7/8), 5V_SERVO',          [('U10','7'),('U10','8')]),
 ('C13', 'U7 EN (45)',                        [('U7','45')]),
 ('C14', 'U6 VDDIO (5) / VDD (8), IMU',       [('U6','5'),('U6','8')]),
 ('C15', 'U7 3V3 (3), module',                [('U7','3')]),
 ('C16', 'U7 3V3 (3), module',                [('U7','3')]),
 ('C17', 'U6 VDDIO (5) / VDD (8), IMU',       [('U6','5'),('U6','8')]),
 ('C24', 'U9 VDD (7/8), ampli',               [('U9','7'),('U9','8')]),
 ('C25', 'U9 VDD (7/8), ampli',               [('U9','7'),('U9','8')]),
 ('C26', 'U9 VDD (7/8), ampli',               [('U9','7'),('U9','8')]),
 ('C27', 'U6 VDD (8) / VDDIO (5), IMU',       [('U6','8'),('U6','5')]),
 ('C33', 'U10 VIN (1/2)',                     [('U10','1'),('U10','2')]),
 ('C34', 'U10 VBIAS (4)',                     [('U10','4')]),
 ('C35', 'U10 CT (6)',                        [('U10','6')]),
 ('C36', 'U11 VIN (1/2)',                     [('U11','1'),('U11','2')]),
 ('C37', 'U11 VBIAS (4)',                     [('U11','4')]),
 ('C38', 'U11 CT (6)',                        [('U11','6')]),
 ('C39', 'U11 VOUT (7/8), 5V_LED',            [('U11','7'),('U11','8')]),
 ('C40', 'U12 VIN (1/2)',                     [('U12','1'),('U12','2')]),
 ('C41', 'U12 VBIAS (4)',                     [('U12','4')]),
 ('C42', 'U12 CT (6)',                        [('U12','6')]),
 ('C43', 'U12 VOUT (7/8), 5V_RADAR',          [('U12','7'),('U12','8')]),
 ('C44', 'U13 VCCI (2), extendeur',           [('U13','2')]),
 ('C45', 'U13 VCCP (24), extendeur',          [('U13','24')]),
 ('C10', 'J4.1 (servo canon droit)',          [('J4','1')]),
 ('C19', 'J3.1 (servo canon gauche)',         [('J3','1')]),
 ('C20', 'J7.2 (servo aile gauche)',          [('J7','2')]),
 ('C21', 'J9.2 (servo ROTATE_Z)',             [('J9','2')]),
 ('C22', 'J8.2 (servo aile droite)',          [('J8','2')]),
 ('C23', 'J10.2 (servo ROTATE_X)',            [('J10','2')]),
 ('C29', 'J5.1 (radar)',                      [('J5','1')]),
 ('C30', 'J14.1 (NeoPixel)',                  [('J14','1')]),
 ('C31', 'J17.1 (Hall gauche)',               [('J17','1')]),
 ('C32', 'J18.1 (Hall droit)',                [('J18','1')]),
]
F = {f.GetReference(): f for f in b.GetFootprints()}

def pad(ref, num):
    return [p for p in F[ref].Pads() if p.GetNumber() == num][0]

def on_seg(pt, t):
    ax, ay, bx, by = t.GetStart().x, t.GetStart().y, t.GetEnd().x, t.GetEnd().y
    dx, dy = bx - ax, by - ay; L = dx * dx + dy * dy
    u = 0 if L == 0 else max(0, min(1, ((pt.x - ax) * dx + (pt.y - ay) * dy) / L))
    return math.hypot(ax + u * dx - pt.x, ay + u * dy - pt.y) <= t.GetWidth() / 2 + 1000

def copper_path(src, dsts, layer):
    """Vrai si src rejoint une des broches dsts par des pistes (et pads) du meme net, sur la meme couche."""
    net = src.GetNetCode()
    segs = [t for t in b.GetTracks() if t.GetClass() == 'PCB_TRACK' and t.GetNetCode() == net and t.GetLayer() == layer]
    pads = [p for f in b.GetFootprints() for p in f.Pads() if p.GetNetCode() == net and p.IsOnLayer(layer)]
    nodes = segs + pads
    def touch(a, c):
        if isinstance(a, pcbnew.PAD) and isinstance(c, pcbnew.PAD): return False
        if isinstance(a, pcbnew.PAD): a, c = c, a
        if isinstance(c, pcbnew.PAD): return c.HitTest(a.GetStart()) or c.HitTest(a.GetEnd()) or on_seg(c.GetPosition(), a)
        return on_seg(a.GetStart(), c) or on_seg(a.GetEnd(), c) or on_seg(c.GetStart(), a) or on_seg(c.GetEnd(), a)
    # les objets SWIG changent d'identite a chaque acces : cle stable
    K = lambda o: (o.GetParentFootprint().GetReference(), o.GetNumber()) if isinstance(o, pcbnew.PAD) else o.m_Uuid.AsString()
    seen = {K(src)}; q = collections.deque([src])
    while q:
        n = q.popleft()
        for m in nodes:
            if K(m) in seen: continue
            if touch(n, m):
                seen.add(K(m)); q.append(m)
    return any(K(d) in seen for d in dsts)

def in_zone(p, layer):
    return any(z.GetNetCode() == p.GetNetCode() and z.IsOnLayer(layer) and z.HitTestFilledArea(layer, p.GetPosition()) for z in b.Zones())

vias = [t for t in b.GetTracks() if t.GetClass() == 'PCB_VIA' and t.GetNetname() == 'GND']
def gnd_via(p, layer, r=1.6):
    # via GND a moins de r mm du pad, ou reliee au pad par une piste de la meme couche
    c = p.GetPosition()
    if any(mm(math.hypot(v.GetPosition().x - c.x, v.GetPosition().y - c.y)) <= r for v in vias): return True
    segs = [t for t in b.GetTracks() if t.GetClass() == 'PCB_TRACK' and t.GetNetname() == 'GND' and t.GetLayer() == layer]
    reach = [t for t in segs if p.HitTest(t.GetStart()) or p.HitTest(t.GetEnd())]
    return any(on_seg(v.GetPosition(), t) for t in reach for v in vias)

bad = 0
print('%-4s %-38s %-12s %s' % ('Cond', 'Cible', 'Alim', 'GND'))
for c, label, tgt in MAP:
    f = F[c]; layer = f.GetLayer()
    sp = [p for p in f.Pads() if p.GetNetname() != 'GND'][0]
    gp = [p for p in f.Pads() if p.GetNetname() == 'GND']
    dsts = [pad(r, n) for r, n in tgt]
    other = pcbnew.B_Cu if layer == pcbnew.F_Cu else pcbnew.F_Cu
    if copper_path(sp, dsts, layer):
        a = 'OK piste'
    elif any(d.IsOnLayer(other) for d in dsts) and not any(d.IsOnLayer(layer) for d in dsts):
        a = 'autre face'          # broche sur l'autre face : vias au plus pres de la broche
    elif in_zone(sp, layer) and all(in_zone(d, d.GetParentFootprint().GetLayer()) for d in dsts):
        a = 'OK zone'
    else:
        a = 'A RELIER'
    g = ('OK' if gnd_via(gp[0], layer) else 'via ?') if gp else '-'
    if not a.startswith('OK') or g == 'via ?': bad += 1
    print('%-4s %-38s %-12s %s' % (c, label, a, g))
print('\n%d condensateur(s) a reprendre sur %d' % (bad, len(MAP)))
