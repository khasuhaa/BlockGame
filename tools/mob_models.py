#!/usr/bin/env python3
"""Gives every creature in the game and in every mod a body built of boxes, and the skin it is wrapped in.

    python3 tools/mob_models.py .

Each creature gets a "model" in its mobs.json (parts: where each box hangs, how big, how it moves) and a
textures/mobs/<name>_skin.png. What kind of body - four legs, two, wings, fins, a length of segments, a hull with
masts - is told from its name and from what the data says of it (that it swims, flies, is boarded); its colours are
taken from the picture it already has; and how big it is built is how big the data says it is: a creature three
blocks long has parts three times the size, with a skin three times as fine, and more of them - it is not the small
one drawn large. A "shape" in the data settles the kind where the name would not. Run it again after anything that
adds a creature (a mod's generator, for one): rerun with nothing changed it changes nothing.
"""
import json, os, random, re, sys, zlib
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
INK = (22, 22, 30, 255)
def rnd(name): return random.Random(zlib.crc32(name.encode()))
def dk(c, f=0.72): return tuple(int(v * f) for v in c[:3]) + (255,)
def lt(c, f=1.25): return tuple(min(255, int(v * f + 8)) for v in c[:3]) + (255,)
def far(a, b): return sum(abs(a[k] - b[k]) for k in range(3))

WORDS = [
    ('ship', ('_afloat', 'sloop', 'brig', 'galleon', 'cutter', 'frigate', 'man_of_war', 'schooner', 'canoe', 'ghost_ship', 'smack', 'lugger', 'whaling_ship', 'bomb_vessel', 'xebec', 'corvette', 'plague_ship', 'dread_sovereign')),
    ('person', ('captain', 'townsman', 'fishwife', 'islander', 'deckhand', 'buccaneer', 'gunner', 'sailor', 'marine', 'cutthroat', 'villager', 'merchant', 'farmer', 'elder', 'witch', 'disciple', 'immortal', 'heaven_guard', 'puppet',
                'soldier', 'pirate', 'archer', 'caster', 'jester', 'samurai', 'thug', 'alchemist', 'engineer', 'trooper', 'rigger', 'shaman', 'hoplite', 'oracle', 'gladiator', 'slinger', 'initiate', 'hexer', 'singer', 'weaver', 'reader',
                'acolyte', 'priest', 'hunter', 'caller', 'keeper', 'seer', 'walker', 'mage', 'raider', 'mummy', 'warrior', 'guard', 'darksworn', 'sunken_dead', 'oathsworn', 'star_knight', 'shade_of')),
    ('dragon', ('dragon', 'wyrm', 'drake', 'leviathan')),
    ('golem', ('golem', 'colossus', 'guardian', 'warden', 'sentinel', 'giant', 'titan', 'treant', 'ogre', 'brute', 'hulk', 'abomination', 'knight', 'thunder_god', 'elden_heart', 'immortal_tree', 'jotunn', 'oni', 'nightmare', 'constellation', 'inventor')),
    ('serpent', ('serpent', 'eel', 'worm', 'leech', 'slug', 'viper', 'borer', 'seahorse', 'caiman', 'croc')),
    ('octopus', ('octopus', 'kraken')),
    ('fish', ('fish', 'carp', 'tuna', 'snapper', 'grouper', 'marlin', 'barracuda', 'shark', 'piranha', 'whale', 'manta', 'ray', 'swimmer', 'biter')),
    ('floater', ('wisp', 'spirit', 'jelly', 'wraith', 'soul', 'lantern', 'mote', 'sprite', 'devil', 'drifter', 'puffer', 'ghost', 'shade', 'remnant', 'drone', 'probe', 'king_ghost', 'lich', 'dreamer')),
    ('crawler', ('crab', 'scarab', 'beetle', 'mite', 'spider', 'crawler', 'skitter', 'barnacle', 'scorpion', 'strider', 'spawn', 'fly')),
    ('dodo', ('dodo', 'moa', 'chick', 'heron', 'crane', 'wader')),
    ('bird', ('hawk', 'eagle', 'bat', 'moth', 'gull', 'parrot', 'roc', 'thunderbird', 'owl', 'raven', 'fury', 'gargoyle', 'phoenix', 'star_hunter', 'admiral')),
    ('turtle', ('turtle', 'tortoise', 'horror')),
    ('ape', ('ape', 'monkey', 'dropbear', 'imp', 'gremlin')),
    ('hopper', ('rabbit', 'hopper', 'toad', 'frog', 'mole', 'rat', 'squirrel', 'stoat', 'hare')),
    ('horned', ('stag', 'deer', 'elk', 'yak', 'boar', 'qilin', 'hind', 'grazer', 'fleeceling')),
]
def kind_of(m):
    if m.get('shape'): return m['shape']
    n = m['name']
    if m.get('boarding') or (m.get('mount') and m.get('ship')): return 'ship'
    for kind, words in WORDS:
        if any(w in n for w in words): return kind
    if m.get('flies'): return 'bird'
    if m.get('swim') is not None or m.get('swims'): return 'fish'
    return 'quad'

def colours(path, name):
    """What it is coloured: the commonest colour of its picture, the commonest unlike that, and the commonest unlike both."""
    try: im = Image.open(path).convert('RGBA')
    except OSError: im = None
    count = {}
    if im:
        for p in im.getdata():
            if p[3] > 200 and far(p, INK) > 60: q = (p[0] // 16 * 16 + 8, p[1] // 16 * 16 + 8, p[2] // 16 * 16 + 8, 255); count[q] = count.get(q, 0) + 1
    order = sorted(count, key=lambda c: -count[c])
    k = rnd(name); one = order[0] if order else tuple(k.randint(80, 200) for _ in range(3)) + (255,)
    two = next((c for c in order[1:] if far(c, one) > 90), lt(one, 1.35) if sum(one[:3]) < 420 else dk(one, 0.6))
    three = next((c for c in order[1:] if far(c, one) > 60 and far(c, two) > 60), dk(one, 0.55))
    return one, two, three

class Skin:
    """The boxes of a body, and their skin: each box is given room on it as it unfolds, and painted."""
    def __init__(s, name): s.parts, s.boxes, s.k, s.name = [], [], rnd(name), name
    def box(s, at, box, col, move=None, by=0, lag=0, under=None, pattern=None, eye=None, mirror_of=None):
        x, y, z, w, h, d = box
        part = {'at': [round(v, 2) for v in at], 'box': [round(v, 2) for v in box]}
        if move: part['move'] = move; part['by'] = by
        if lag: part['lag'] = round(lag, 2)
        s.parts.append(part); s.boxes.append((part, max(1, int(round(w))), max(1, int(round(h))), max(1, int(round(d))), col, under, pattern, eye))
        return part
    def paint(s, path):
        # (Shelves: the widest boxes first, each row as tall as its tallest.)
        order = sorted(range(len(s.boxes)), key=lambda i: -(s.boxes[i][2] + s.boxes[i][3]))
        wide = 64
        while True:
            x = y = row = 0; place = {}; ok = True
            for i in order:
                _, w, h, d, *_ = s.boxes[i]; bw, bh = 2 * (w + d), h + d
                if bw > wide: ok = False; break
                if x + bw > wide: x, y, row = 0, y + row, 0
                place[i] = (x, y); x += bw; row = max(row, bh)
            if ok and y + row <= wide: break
            wide *= 2
        im = Image.new('RGBA', (wide, wide), (0, 0, 0, 0)); px = im.load()
        for i, (part, w, h, d, col, under, pattern, eye) in enumerate(s.boxes):
            u, v = place[i]; part['uv'] = [u, v]
            def fill(x0, y0, fw, fh, c, shade_rows=True):
                for yy in range(fh):
                    for xx in range(fw):
                        f = 1.0 + s.k.uniform(-0.07, 0.07) - (0.10 * yy / max(1, fh - 1) if shade_rows else 0.0)
                        cc = c
                        if pattern and pattern[0] == 'stripe' and (xx + (yy // 2)) % max(3, fw // 3 + 2) == 0: cc = pattern[1]
                        if pattern and pattern[0] == 'spot' and s.k.random() < 0.09: cc = pattern[1]
                        if pattern and pattern[0] == 'band' and yy % max(3, fh // 3 + 1) == 0: cc = pattern[1]
                        if pattern and pattern[0] == 'plate' and (xx % max(3, fw // 2) == 0 or yy % max(3, fh // 2) == 0): cc = pattern[1]
                        px[x0 + xx, y0 + yy] = tuple(max(0, min(255, int(cc[k] * f))) for k in range(3)) + (255,)
            fill(u + d, v, w, d, lt(col, 1.08), False)                 # top
            fill(u + d + w, v, w, d, under or dk(col, 0.8), False)     # underside
            fill(u, v + d, d, h, col); fill(u + d, v + d, w, h, col)   # right, front
            fill(u + d + w, v + d, d, h, col); fill(u + d + w + d, v + d, w, h, dk(col, 0.9))   # left, back
            if eye:
                # (An eye on each side, toward the front, and a pair on the face.)
                size = max(1, min(d, h) // 5); ey = v + d + max(0, h // 3 - size // 2)
                for xx in range(size):
                    for yy in range(size):
                        px[u + max(0, d // 5) + xx, ey + yy] = eye; px[u + d + w + d - 1 - max(0, d // 5) - xx, ey + yy] = eye
                        px[u + d + max(0, w // 5) + xx, ey + yy] = eye; px[u + d + w - 1 - max(0, w // 5) - xx, ey + yy] = eye
        im.save(path)
        return wide

def build(m, kind, one, two, three):
    name = m['name']; S = max(1, (m.get('size') or [1, 1])[0]); k = rnd(name + 'model')
    afloat = bool(m.get('flies') or m.get('swims') or m.get('swim') is not None)
    s = Skin(name); bob = hop = 0.0
    pat = k.choice([None, None, ('spot', two), ('stripe', two), ('band', dk(one, 0.8))]) if S == 1 else k.choice([('plate', dk(one, 0.8)), ('stripe', two), ('spot', two), ('band', two)])
    base = 8 - 4 * S if afloat else 0     # (what floats is built about the middle of its square)
    if kind == 'ship':
        L, masts = 13 * S, 0 if S < 2 or 'canoe' in name or 'raft' in name else 1 if S < 3 else 2 if S < 5 else 3
        # (The darkest of its colours is its hull and the lightest its sails; the other flies from the masthead.)
        by_light = sorted((one, two, three), key=lambda c_: sum(c_[:3]))
        hull, flag, sail = by_light[0], by_light[1], by_light[2]
        if far(hull, sail) < 80: hull = dk(sail, 0.45)
        s.box((0, 5, 0), (-2 * S, 0, -L / 2, 4 * S, 3 * S, L), hull, pattern=('band', dk(hull, 0.75)))
        s.box((0, 5, L / 2), (-1.5 * S, 0.5 * S, 0, 3 * S, 2.5 * S, 2 * S), hull)                      # the bow
        s.box((0, 5 + 3 * S, -L / 2), (-2 * S, 0, 0, 4 * S, 1.5 * S, 3 * S), dk(hull, 0.85))         # the stern castle
        s.box((0, 5 + 3 * S, 0), (-1.8 * S, -0.2, -L / 2 + 1, 3.6 * S, 0.5, L - 2), lt(hull, 1.3))   # the deck
        for i in range(masts):
            z = (i - (masts - 1) / 2) * L / (masts + 0.3); high = (7 + 2 * S) * (1.0 if i != masts - 1 or masts == 1 else 0.85)
            s.box((0, 5 + 3 * S, z), (-0.5, 0, -0.5, 1, high, 1), dk(hull, 0.7))
            for tier in range(1 if S < 4 else 2):
                s.box((0.7, 5 + 3 * S + 1.5 + tier * high * 0.48, z), (0, 0, -1.9 * S, 0.6, high * 0.42, 3.8 * S), sail, move='sway', by=2, lag=i)   # (fore and aft: the ship is seen from the side)
            s.box((0, 5 + 3 * S + high, z), (-0.3, 0, -2.5, 0.6, 1.5, 2.5), flag, move='tail', by=18, lag=i)
        if masts == 0 and 'raft' not in name: s.box((0, 5 + 3 * S, 0), (-0.4, 0, -0.4, 0.8, 7, 0.8), dk(hull, 0.7)); s.box((0.6, 5 + 3 * S + 1.5, 0), (0, 0, -2.5, 0.5, 5, 5), sail, move='sway', by=3)
        if S >= 3: s.box((0, 5 + 2 * S, L / 2 + 2 * S), (-0.4, 0, 0, 0.8, 0.8, 3 * S), dk(hull, 0.7))   # the bowsprit
        bob = 0.8
    elif kind in ('person', 'ape', 'golem'):
        if kind == 'person':
            u = S; skin_c = (224, 184, 144, 255); coat, hat = one, two
            s.box((0, 24 * u, 0), (-4 * u, 0, -4 * u, 8 * u, 8 * u, 8 * u), skin_c, move='head', by=4, eye=INK)
            s.box((0, 31 * u, 0), (-4.4 * u, 0, -4.4 * u, 8.8 * u, 2.2 * u, 8.8 * u), hat)
            s.box((0, 12 * u, 0), (-4 * u, 0, -2 * u, 8 * u, 12 * u, 4 * u), coat, pattern=('band', dk(coat, 0.8)) if S > 1 else None)
            for side, mv in ((-1, 'leg'), (1, 'leg2')):
                s.box((6 * side * u, 22 * u, 0), (-2 * u, -10 * u, -2 * u, 4 * u, 12 * u, 4 * u), coat, move=mv, by=30)
                s.box((2 * side * u, 12 * u, 0), (-2 * u, -12 * u, -2 * u, 4 * u, 12 * u, 4 * u), dk(coat, 0.6) if far(three, coat) < 60 else three, move='leg2' if mv == 'leg' else 'leg', by=32)
        elif kind == 'ape':
            u = S
            s.box((0, 13 * u, 1 * u), (-4 * u, 0, -3 * u, 8 * u, 7 * u, 7 * u), one, move='head', by=4, eye=INK)
            s.box((0, 6 * u, 0), (-4.5 * u, 0, -3 * u, 9 * u, 8 * u, 6 * u), one, under=two, pattern=pat)
            for side, mv in ((-1, 'leg'), (1, 'leg2')):
                s.box((6 * side * u, 13 * u, 0), (-1.5 * u, -11 * u, -1.5 * u, 3 * u, 12 * u, 3 * u), dk(one, 0.85), move=mv, by=34)
                s.box((2.5 * side * u, 6 * u, 0), (-2 * u, -6 * u, -2 * u, 4 * u, 6 * u, 4 * u), dk(one, 0.8), move='leg2' if mv == 'leg' else 'leg', by=28)
        else:
            u = S * 1.15
            s.box((0, 20 * u, 1 * u), (-3 * u, 0, -3 * u, 6 * u, 6 * u, 6 * u), lt(one, 1.1), move='head', by=3, eye=two)
            s.box((0, 9 * u, 0), (-6 * u, 0, -4 * u, 12 * u, 11 * u, 8 * u), one, pattern=('plate', dk(one, 0.75)))
            s.box((0, 13 * u, 4 * u), (-2 * u, 0, 0, 4 * u, 4 * u, 0.8 * u), two)                      # what glows in its chest
            for side, mv in ((-1, 'leg'), (1, 'leg2')):
                s.box((8 * side * u, 19 * u, 0), (-2.5 * u, -13 * u, -2.5 * u, 5 * u, 14 * u, 5 * u), dk(one, 0.9), move=mv, by=22, pattern=('plate', dk(one, 0.7)))
                s.box((8 * side * u, 20 * u, 0), (-3.2 * u, 0, -3.2 * u, 6.4 * u, 3 * u, 6.4 * u), lt(one, 1.15))   # shoulders
                s.box((3.2 * side * u, 9 * u, 0), (-2.5 * u, -9 * u, -2.5 * u, 5 * u, 9 * u, 5 * u), dk(one, 0.8), move='leg2' if mv == 'leg' else 'leg', by=20)
            if S >= 2:
                for i in range(3): s.box((0, 20 * u, (i - 1) * 3 * u - u), (-1 * u, 0, -1 * u, 2 * u, 3 * u, 2 * u), two)   # a ridge of spikes down the back
        hop = 0.6
    elif kind in ('quad', 'horned', 'hopper', 'turtle', 'dragon'):
        u = S * 1.3; low = kind in ('hopper', 'turtle'); legs = 3 * u if low else 5 * u
        L, Hh, W = (9 if low else 11) * u, (5 if low else 6) * u, 6 * u
        if kind == 'dragon': L, Hh, W, legs = 13 * u, 6 * u, 6 * u, 4 * u
        y0 = base + legs
        s.box((0, y0, 0), (-W / 2, 0, -L / 2, W, Hh, L), one, under=two, pattern=pat)
        neck = y0 + Hh * 0.55
        if kind == 'dragon':
            for i in range(2 + S // 2): s.box((0, neck + i * 2.2 * u, L / 2 + i * 1.6 * u - u), (-1.6 * u, 0, 0, 3.2 * u, 3 * u, 3 * u), one, under=two, move='wave', by=5, lag=i * 0.7)
            hy, hz = neck + (2 + S // 2) * 2.2 * u, L / 2 + (2 + S // 2) * 1.6 * u - u
        else: hy, hz = neck, L / 2 - u * 0.5
        s.box((0, hy, hz), (-W * 0.42, -Hh * 0.25, 0, W * 0.84, Hh * 0.85, 4.5 * u), one, move='head', by=5, eye=INK)
        s.box((0, hy, hz + 4.5 * u), (-W * 0.25, -Hh * 0.2, 0, W * 0.5, Hh * 0.45, 2 * u), dk(one, 0.85) if kind != 'dragon' else two, move='head', by=5)   # a muzzle
        if kind in ('horned', 'dragon'):
            for side in (-1, 1): s.box((side * W * 0.3, hy + Hh * 0.6, hz + 1.5 * u), (-0.5 * u, 0, -0.5 * u, 1 * u, (3 if kind == 'horned' else 2.5) * u, 1 * u), two if far(two, one) > 60 else lt(one, 1.5), move='head', by=5)
        else:
            for side in (-1, 1): s.box((side * W * 0.3, hy + Hh * 0.6, hz + 1 * u), (-0.7 * u, 0, -0.5 * u, 1.4 * u, 1.6 * u, 1 * u), dk(one, 0.8), move='head', by=5)   # ears
        for i, (sx, sz) in enumerate(((-1, 1), (1, 1), (-1, -1), (1, -1))):
            big = 1.3 if kind == 'hopper' and sz < 0 else 1.0
            s.box((sx * (W / 2 - u), y0, sz * (L / 2 - 1.5 * u)), (-u * big, -legs, -u * big, 2 * u * big, legs, 2 * u * big), dk(one, 0.85), move='leg' if (sx * sz) > 0 else 'leg2', by=30)
        if kind == 'turtle': s.box((0, y0 + Hh, 0), (-W * 0.6, 0, -L * 0.45, W * 1.2, 3 * u, L * 0.9), two if far(two, one) > 60 else dk(one, 0.6), pattern=('plate', dk(one, 0.5)))
        tail = 3 if kind == 'dragon' else 1
        for i in range(tail): s.box((0, y0 + Hh * 0.6, -L / 2 - i * 3 * u), (-u * (1.2 - 0.25 * i), -u, -3.5 * u, 2 * u * (1.2 - 0.25 * i), 2 * u * (1 - 0.2 * i), 3.5 * u), one if i < tail - 1 or kind != 'dragon' else two, move='tail', by=16 + 6 * i, lag=i * 0.6)
        if kind == 'dragon':
            for side, mv in ((-1, 'wing'), (1, 'wing2')): s.box((side * W / 2, y0 + Hh, 0), (0 if side > 0 else -9 * u, 0, -3 * u, 9 * u, 0.7 * u, 7 * u), two, move=mv, by=22, pattern=('stripe', dk(two, 0.7)))
            for i in range(4 + S): s.box((0, y0 + Hh, L / 2 - (i + 0.5) * L / (4 + S)), (-0.5 * u, 0, -0.7 * u, 1 * u, 1.6 * u, 1.4 * u), two)   # spines
        elif S >= 2:
            s.box((0, y0 + Hh, L * 0.15), (-W * 0.35, 0, -L * 0.25, W * 0.7, 1.5 * u, L * 0.5), dk(one, 0.8), pattern=('plate', dk(one, 0.6)))        # a hump, or a mane
            for side in (-1, 1): s.box((side * W * 0.25, hy - Hh * 0.1, hz + 4 * u), (-0.4 * u, -1.5 * u, 0, 0.8 * u, 1.5 * u, 0.8 * u), (240, 236, 220, 255))   # teeth
        hop = 16 * m.get('hop', 0.05) + (2.5 if kind == 'hopper' else 0)
    elif kind in ('bird', 'dodo'):
        u = S * 1.25; stand = kind == 'dodo' or not m.get('flies'); legs = (4 if kind == 'dodo' else 2) * u if stand else 0
        y0 = (0 if stand else base) + legs
        bw, bh, bl = (6, 7, 7) if kind == 'dodo' else (4, 4, 7)
        s.box((0, y0, 0), (-bw * u / 2, 0, -bl * u / 2, bw * u, bh * u, bl * u), one, under=two, pattern=pat)
        hy = y0 + bh * u * (0.9 if kind == 'dodo' else 0.6)
        s.box((0, hy, bl * u / 2 - u), (-1.8 * u, 0, 0, 3.6 * u, 3.6 * u, 3.6 * u), one, move='head', by=6, eye=INK)
        s.box((0, hy + 0.8 * u, bl * u / 2 + 2.6 * u), (-0.7 * u, 0, 0, 1.4 * u, 1.3 * u, 2.4 * u), three if far(three, one) > 60 else (232, 180, 60, 255), move='head', by=6)   # the beak
        flap = 42 if m.get('flies') else 8
        for side, mv in ((-1, 'wing'), (1, 'wing2')):
            span = (2 if kind == 'dodo' else 6) * u
            s.box((side * bw * u / 2, y0 + bh * u * 0.75, 0), (0 if side > 0 else -span, -0.5 * u, -bl * u * 0.35, span, 0.8 * u, bl * u * 0.7), two if far(two, one) > 50 else dk(one, 0.8), move=mv, by=flap)
        s.box((0, y0 + bh * u * 0.6, -bl * u / 2), (-1.5 * u, 0, -2.5 * u, 3 * u, 0.8 * u, 2.5 * u), two, move='tail', by=10)
        if stand:
            for side, mv in ((-1, 'leg'), (1, 'leg2')): s.box((side * 1.2 * u, y0, 0), (-0.5 * u, -legs, -0.5 * u, 1 * u, legs, 1 * u), (232, 180, 60, 255), move=mv, by=32)
        bob, hop = (1.2 if m.get('flies') else 0.0), 16 * m.get('hop', 0.0)
    elif kind == 'fish':
        u = S * 1.15; flat = 'ray' in name or 'manta' in name
        W, Hh, L = (9, 2, 8) if flat else (3, 5, 9)
        y0 = 8 - Hh * u / 2
        s.box((0, y0, 0), (-W * u / 2, 0, -L * u / 2, W * u, Hh * u, L * u), one, under=two, pattern=pat, eye=INK)
        s.box((0, y0 + Hh * u / 2, -L * u / 2), (-0.5 * u, -Hh * u * 0.3, -3 * u, 1 * u, Hh * u * 0.6, 3 * u), one, move='wave', by=18, lag=0.8)
        s.box((0, y0 + Hh * u / 2, -L * u / 2 - 3 * u), (-0.4 * u, -Hh * u * 0.7, -2.5 * u, 0.8 * u, Hh * u * 1.4, 2.5 * u), two, move='wave', by=32, lag=1.6)
        if not flat: s.box((0, y0 + Hh * u, -u), (-0.4 * u, 0, -2 * u, 0.8 * u, 2 * u, 4 * u), two)
        for side, mv in ((-1, 'wing'), (1, 'wing2')): s.box((side * W * u / 2, y0 + Hh * u * 0.3, u), (0 if side > 0 else -2 * u, 0, -1.5 * u, 2 * u, 0.6 * u, 3 * u), two, move=mv, by=14)
        if S >= 2:
            s.box((0, y0 + Hh * u * 0.15, L * u / 2), (-W * u * 0.4, 0, 0, W * u * 0.8, Hh * u * 0.5, 2 * u), dk(one, 0.8))                            # a jaw
            for i in range(S + 1): s.box((0, y0 + Hh * u, L * u / 2 - (i + 1) * L * u / (S + 3)), (-0.3 * u, 0, -0.6 * u, 0.6 * u, 1.2 * u, 1.2 * u), two)
        bob = 0.5
    elif kind == 'serpent':
        u = S; n = 6 + 2 * S; y0 = 8 - 1.6 * u if afloat else 0
        for i in range(n):
            t = 1.0 - 0.5 * i / n
            s.box((0, y0, (n / 2 - i) * 2.6 * u), (-1.6 * u * t, 0, -1.4 * u, 3.2 * u * t, 3.2 * u * t, 2.8 * u), one if i % 2 == 0 or not pat else pat[1], under=two, move='wave', by=10, lag=i * 0.7, eye=INK if i == 0 else None)
        s.box((0, y0 + 0.4 * u, (n / 2) * 2.6 * u + 1.4 * u), (-1.3 * u, 0, 0, 2.6 * u, 2 * u, 2.2 * u), dk(one, 0.85), move='wave', by=10)
        if S >= 2:
            for i in range(0, n, 2): s.box((0, y0 + 3.2 * u * (1.0 - 0.5 * i / n), (n / 2 - i) * 2.6 * u), (-0.4 * u, 0, -0.8 * u, 0.8 * u, 1.6 * u, 1.6 * u), two, move='wave', by=10, lag=i * 0.7)
        bob = 0.4 if afloat else 0.0
    elif kind == 'octopus':
        u = S; y0 = 8 - 2 * u
        s.box((0, y0 + 2 * u, 0), (-3 * u, 0, -3 * u, 6 * u, 6 * u, 6 * u), one, pattern=pat, eye=INK)
        for i in range(6):
            sx, sz = (-1 if i % 2 == 0 else 1) * (1 + (i // 2) % 2) * 1.2 * u, ((i // 2) - 1) * 2.2 * u
            for j in range(2 + S // 2): s.box((sx, y0 + 2 * u - j * 2.4 * u, sz), (-0.8 * u, -2.6 * u, -0.8 * u, 1.6 * u, 2.6 * u, 1.6 * u), one if j % 2 == 0 else two, move='sway', by=10 + 6 * j, lag=i * 0.9 + j * 0.5)
        bob = 1.0
    elif kind == 'crawler':
        u = S * 1.2; y0 = base + 2.5 * u
        s.box((0, y0, 0), (-4 * u, 0, -4 * u, 8 * u, 3.5 * u, 8 * u), one, under=two, pattern=pat or ('plate', dk(one, 0.75)), eye=INK)
        for i in range(6):
            sx, sz = (-1 if i % 2 == 0 else 1), ((i // 2) - 1) * 2.6 * u
            s.box((sx * 4 * u, y0 + u, sz), (0 if sx > 0 else -2.5 * u, -3.5 * u, -0.5 * u, 2.5 * u, 1 * u, 1 * u), dk(one, 0.7), move='leg' if i % 4 in (0, 3) else 'leg2', by=24)
        for side in (-1, 1): s.box((side * 2.5 * u, y0 + u, 4 * u), (-1 * u, -0.5 * u, 0, 2 * u, 1.5 * u, 3 * u), two if far(two, one) > 50 else dk(one, 0.8), move='head', by=10)   # claws, or feelers
        if 'scorpion' in name or S >= 2:
            for j in range(3): s.box((0, y0 + 3 * u + j * 2 * u, -4 * u + j * u), (-0.8 * u, 0, -0.8 * u, 1.6 * u, 2.2 * u, 1.6 * u), two, move='sway', by=6, lag=j * 0.5)   # a tail carried over the back
        hop = 0.4
    else:   # floater
        u = S; y0 = 8 - 1 * u if not afloat and 'drone' not in name and 'probe' not in name else 8 - 2 * u
        y0 = max(y0, 3 * u)
        s.box((0, y0, 0), (-3 * u, 0, -3 * u, 6 * u, 6 * u, 6 * u), one, pattern=pat, eye=two if far(two, one) > 60 else INK)
        s.box((0, y0 + 6 * u, 0), (-2 * u, 0, -2 * u, 4 * u, 1.5 * u, 4 * u), lt(one, 1.2))
        if 'drone' in name or 'probe' in name:
            for side, mv in ((-1, 'wing'), (1, 'wing2')): s.box((side * 3 * u, y0 + 5 * u, 0), (0 if side > 0 else -4 * u, 0, -1 * u, 4 * u, 0.5 * u, 2 * u), two, move=mv, by=12)
        for i in range(3 + S):
            sx = ((i % 3) - 1) * 1.8 * u
            s.box((sx, y0, ((i // 3) - 0.5) * 2 * u), (-0.8 * u, -(3 + i % 2) * u, -0.8 * u, 1.6 * u, (3 + i % 2) * u, 1.6 * u), two if i % 2 else dk(one, 0.8), move='sway', by=14, lag=i * 1.1)
        bob = 1.6
    return s, round(bob, 2), round(hop, 2)

def write(d):
    path = os.path.join(d, 'mobs.json')
    if not os.path.exists(path): return 0
    lines = open(path).read().split('\n'); made = 0
    tex = os.path.join(d, 'textures')
    for n, line in enumerate(lines):
        body = line.strip().rstrip(',')
        if not body.startswith('{"name"'): continue
        try: m = json.loads(body)
        except ValueError: continue
        if 'texture' not in m: continue
        kind = kind_of(m)
        one, two, three = colours(os.path.join(tex, m['texture']), m['name'])
        skin, bob, hop = build(m, kind, one, two, three)
        assert len(skin.parts) <= 40, (m['name'], len(skin.parts))
        out = 'mobs/%s_skin.png' % m['name']; os.makedirs(os.path.join(tex, 'mobs'), exist_ok=True)
        wide = skin.paint(os.path.join(tex, out))
        model = {'skin': out, 'atlas': [wide, wide], 'parts': skin.parts}
        if bob: model['bob'] = bob
        if hop: model['hop'] = hop
        m.pop('model', None); m['model'] = model
        lines[n] = re.match(r'\s*', line).group(0) + json.dumps(m) + (',' if line.rstrip().endswith(',') else '')
        made += 1
    open(path, 'w').write('\n'.join(lines))
    return made

total = write(os.path.join(repo, 'data'))
mods = os.path.join(repo, 'mods')
for mod_id in sorted(os.listdir(mods)): total += write(os.path.join(mods, mod_id))
print('built %d creatures' % total)
