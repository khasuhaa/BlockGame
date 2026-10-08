#!/usr/bin/env python3
"""Draws every column and every flight of stairs, in the game and in each mod, and their icons.

    python3 tools/stair_column_art.py .

A column of stone is not a column of iron in another colour. Each is drawn in the manner of what it is made of -
stone, plaster, wood, brick, metal, carbon fibre, gem, glass - and within one of those the kinds differ only in
colour. What a thing is made of is told from its name (KINDS, below: anything not named there is stone); its
colours are those it had the first time this was run, kept in tools/stair_column_palettes.json so that running
it again changes nothing. To recolour one, edit its line there; a new one gets a line the first time it is seen.

Run it after adding a column or stairs, and after any mod's generator that makes them.
"""
import glob, json, os, sys
from PIL import Image

ROOT = sys.argv[1] if len(sys.argv) > 1 else '.'
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'stair_column_palettes.json')
INK = (22, 22, 30, 255)

KINDS = {
    'wood': ['wood', 'acacia', 'baobab', 'birch', 'willow', 'bamboo', 'glowcap'],
    'brick': ['brick', 'terracotta'],
    'metal': ['copper', 'tin', 'bronze', 'iron', 'steel', 'gold', 'titanium', 'platinum', 'lead', 'dreadsteel', 'imperial', 'skysteel', 'galvanite', 'aerite',
              'moonsilver', 'deepiron', 'cinderite', 'umbral', 'adamant'],
    'carbon': ['carbon_fiber'],
    'gem': ['amethyst', 'sapphire', 'emerald', 'ruby', 'diamond', 'zephyrite', 'heliodor', 'lumite', 'umbrite', 'emberheart', 'geode'],
    'glass': ['glass', 'ice'],
    'plaster': ['plaster'],
}


def kind_of(material):
    """What a thing is made of, from its name: the longest word of a kind that is in it."""
    best, found = 'stone', 0
    for kind, words in KINDS.items():
        for w in words:
            if w in material.split('_') or material == w or material.endswith('_' + w) or (('_' in w) and w in material):
                if len(w) > found: best, found = kind, len(w)
    return best


def palette(png):
    """Five tones of what a picture is painted in, darkest first, and how solid it is."""
    im = Image.open(png).convert('RGBA')
    px = sorted((p for p in im.getdata() if p[3] > 40 and sum(p[:3]) > 90), key=lambda p: 0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2])
    if not px: return [[60, 60, 70], [90, 90, 100], [130, 130, 140], [170, 170, 180], [210, 210, 220]], 255
    at = lambda f: list(px[min(len(px) - 1, int(f * len(px)))][:3])
    alpha = sorted(p[3] for p in px)[len(px) // 2]
    dark, mid, light = at(0.15), at(0.5), at(0.88)
    return [[int(v * 0.72) for v in dark], dark, mid, light, [min(255, int(v * 1.18 + 14)) for v in light]], alpha


class P:
    def __init__(s, tones, alpha):
        s.im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); s.px = s.im.load()
        s.sh, s.d, s.m, s.l, s.hl = [tuple(t) + (alpha,) for t in tones]
    def r(s, x0, y0, x1, y1, col):
        for y in range(max(0, y0), min(15, y1) + 1):
            for x in range(max(0, x0), min(15, x1) + 1): s.px[x, y] = col
        return s
    def p(s, x, y, col):
        if 0 <= x < 16 and 0 <= y < 16: s.px[x, y] = col
        return s
    def clear(s, x0, y0, x1, y1): return s.r(x0, y0, x1, y1, (0, 0, 0, 0))
    def tile(s, path):
        os.makedirs(os.path.dirname(path), exist_ok=True); s.im.resize((32, 32), Image.NEAREST).save(path)
    def icon(s, path):
        q = s.im.copy(); a = q.load()
        for y in range(16):
            for x in range(16):
                if s.px[x, y][3]: continue
                if any(0 <= x + i < 16 and 0 <= y + j < 16 and s.px[x + i, y + j][3] for i, j in ((1, 0), (-1, 0), (0, 1), (0, -1))): a[x, y] = INK
        os.makedirs(os.path.dirname(path), exist_ok=True); q.resize((48, 48), Image.NEAREST).save(path)


# ---- columns ---------------------------------------------------------------------------------------------------
def column(kind, tones, alpha):
    t = P(tones, alpha)
    if kind == 'stone':
        # A classical column: a capital, a fluted shaft, a base.
        t.r(2, 0, 13, 1, t.l); t.r(2, 1, 13, 1, t.m); t.r(3, 2, 12, 2, t.d); t.r(4, 3, 11, 13, t.m)
        for x in (5, 7, 9): t.r(x, 3, x, 13, t.d)
        t.r(4, 3, 4, 13, t.l); t.r(11, 3, 11, 13, t.sh); t.r(3, 13, 12, 13, t.d); t.r(2, 14, 13, 15, t.m); t.r(2, 14, 13, 14, t.l); t.r(2, 15, 13, 15, t.sh)
    elif kind == 'plaster':
        # Smooth and round, with a plain ring top and bottom.
        t.r(4, 0, 11, 15, t.m); t.r(4, 0, 5, 15, t.l); t.r(6, 0, 6, 15, t.hl); t.r(10, 0, 11, 15, t.d)
        for y in (1, 14): t.r(3, y, 12, y, t.l); t.r(3, y + (1 if y == 1 else -1), 12, y + (1 if y == 1 else -1), t.d)
        t.r(3, 0, 12, 0, t.m); t.r(3, 15, 12, 15, t.sh)
    elif kind == 'wood':
        # A timber post under a beam, with a knee brace either side.
        t.r(1, 0, 14, 1, t.m); t.r(1, 0, 14, 0, t.l); t.r(1, 1, 14, 1, t.d); t.r(5, 2, 10, 15, t.m)
        for x in (6, 8): t.r(x, 2, x, 15, t.d)
        t.r(5, 2, 5, 15, t.l); t.r(10, 2, 10, 15, t.sh)
        for i in range(3): t.p(4 - i, 2 + i, t.d); t.p(4 - i, 3 + i, t.m); t.p(11 + i, 2 + i, t.d); t.p(11 + i, 3 + i, t.m)
        t.p(7, 6, t.sh); t.p(9, 11, t.sh); t.r(4, 15, 11, 15, t.d)
    elif kind == 'brick':
        # A pier of bricks in courses.
        t.r(3, 0, 12, 15, t.m); t.r(2, 0, 13, 0, t.l); t.r(2, 15, 13, 15, t.sh)
        for y in (3, 7, 11): t.r(3, y, 12, y, t.sh)
        for y0, xs in ((1, (7,)), (4, (5, 10)), (8, (7,)), (12, (5, 10))):
            for x in xs: t.r(x, y0, x, y0 + 2, t.sh)
        t.r(3, 1, 3, 14, t.l); t.r(12, 1, 12, 14, t.d)
    elif kind == 'metal':
        # A girder: two uprights and the lattice between them, riveted plates top and bottom.
        t.r(2, 0, 13, 1, t.m); t.r(2, 0, 13, 0, t.hl); t.r(2, 14, 13, 15, t.m); t.r(2, 15, 13, 15, t.sh)
        t.r(3, 2, 4, 13, t.m); t.r(3, 2, 3, 13, t.l); t.r(11, 2, 12, 13, t.m); t.r(12, 2, 12, 13, t.d)
        for i in range(6): t.p(5 + i, 2 + i, t.d); t.p(10 - i, 8 + i, t.d)
        for i in range(6): t.p(5 + i, 3 + i, t.l); t.p(10 - i, 9 + i if 9 + i < 14 else 13, t.l)
        for x in (3, 7, 12): t.p(x, 1, t.sh); t.p(x, 14, t.sh)
    elif kind == 'carbon':
        # A woven tube, banded.
        t.r(5, 0, 10, 15, t.d)
        for y in range(16):
            for x in range(5, 11):
                if (x + y) % 2 == 0: t.p(x, y, t.m)
        t.r(5, 0, 5, 15, t.l); t.r(10, 0, 10, 15, t.sh)
        for y in (0, 5, 10, 15): t.r(4, y, 11, y, t.l)
    elif kind == 'gem':
        # A crystal: six-sided, cut to a point at each end, light on one face and dark on the other.
        for y in range(16):
            taper = max(0, 3 - y, y - 12)
            x0, x1 = 4 + taper, 11 - taper
            t.r(x0, y, x1, y, t.m); t.r(x0, y, x0 + 1, y, t.l); t.r(x1 - 1, y, x1, y, t.d)
            t.p(x0 + 2 if x0 + 2 < x1 - 1 else x0 + 1, y, t.hl)
        for i in range(4): t.p(7 + i // 2, 4 + i * 2, t.hl)
        t.p(8, 2, t.hl); t.p(9, 12, t.sh); t.p(5, 9, t.hl)
    else:
        # Glass: a clear tube with the light down one side.
        t.r(4, 0, 11, 15, t.m); t.r(4, 0, 4, 15, t.l); t.r(11, 0, 11, 15, t.d); t.r(6, 1, 6, 14, t.hl); t.r(9, 3, 9, 6, t.l)
        t.r(3, 0, 12, 0, t.l); t.r(3, 15, 12, 15, t.d)
    return t


# ---- stairs ----------------------------------------------------------------------------------------------------
def stairs(kind, tones, alpha):
    """Two steps, rising to the right: the upper in the right half of the top, the lower all the way across."""
    t = P(tones, alpha)
    upper, lower = (8, 0, 15, 7), (0, 8, 15, 15)
    if kind in ('stone', 'plaster', 'brick', 'gem', 'glass', 'carbon'):
        t.r(*upper, t.m); t.r(*lower, t.m)
        t.r(8, 0, 15, 0, t.hl); t.r(0, 8, 7, 8, t.hl); t.r(8, 1, 8, 7, t.l); t.r(0, 9, 0, 15, t.l); t.r(15, 1, 15, 15, t.d); t.r(0, 15, 15, 15, t.sh)
    if kind == 'stone':
        # Dressed blocks, with a worn nosing to each tread.
        t.r(8, 1, 15, 1, t.l); t.r(0, 9, 7, 9, t.l); t.r(9, 4, 15, 4, t.d); t.r(1, 12, 15, 12, t.d)
        for x, y0, y1 in ((12, 2, 3), (10, 5, 7), (4, 10, 11), (11, 9, 11), (7, 13, 14), (13, 13, 14)): t.r(x, y0, x, y1, t.d)
    elif kind == 'plaster':
        t.r(9, 2, 9, 7, t.hl); t.r(1, 10, 1, 14, t.hl); t.r(14, 2, 14, 14, t.d)
    elif kind == 'brick':
        for y in (3, 6, 11, 14): t.r(8 if y < 8 else 0, y, 15, y, t.sh)
        for x, y0 in ((11, 1), (13, 4), (10, 7), (3, 9), (11, 9), (6, 12), (14, 12), (2, 15), (9, 15)):
            if y0 < 15: t.r(x, y0, x, min(y0 + 1, 15), t.sh)
    elif kind == 'gem':
        # Cut faces: a light one and a dark one to each step.
        for i in range(6): t.r(9 + i, 1 + i, 9 + i, 1 + i, t.hl); t.r(1 + i, 9 + i, 1 + i, 9 + i, t.hl)
        for i in range(5): t.r(10 + i, 3 + i, 14, 3 + i, t.d) if i > 2 else None
        t.r(9, 12, 14, 14, t.d); t.r(9, 11, 14, 11, t.m); t.p(11, 3, t.hl); t.p(4, 11, t.hl); t.p(13, 10, t.hl); t.p(12, 5, t.l)
    elif kind == 'glass':
        for i in range(5): t.p(10 + i, 6 - i, t.hl); t.p(2 + i, 14 - i, t.hl); t.p(9 + i, 14 - i, t.l)
    elif kind == 'carbon':
        for y in range(1, 15):
            for x in range(1 if y > 8 else 9, 15):
                if (x + y) % 2 == 0 and t.px[x, y][3]: t.p(x, y, t.d)
    elif kind == 'wood':
        # Boards for treads, with the risers set back under them and a string down the side.
        t.r(7, 0, 15, 1, t.l); t.r(7, 0, 15, 0, t.hl); t.r(0, 8, 8, 9, t.l); t.r(0, 8, 8, 8, t.hl); t.r(7, 1, 15, 1, t.m); t.r(0, 9, 8, 9, t.m)
        t.r(8, 2, 15, 7, t.d); t.r(0, 10, 15, 15, t.d)
        for x in (11, 14): t.r(x, 2, x, 7, t.sh)
        for x in (3, 7, 11, 14): t.r(x, 10, x, 15, t.sh)
        t.r(9, 8, 15, 9, t.m); t.r(0, 15, 15, 15, t.sh); t.p(9, 0, t.d); t.p(13, 0, t.d); t.p(2, 8, t.d); t.p(6, 8, t.d)
    elif kind == 'metal':
        # Plate treads on an open frame: legs, and a brace under each step.
        t.r(7, 0, 15, 1, t.m); t.r(7, 0, 15, 0, t.hl); t.r(0, 8, 15, 9, t.m); t.r(0, 8, 8, 8, t.hl); t.r(7, 1, 15, 1, t.d); t.r(0, 9, 15, 9, t.d)
        for x in (9, 12, 14): t.p(x, 0, t.sh)
        for x in (1, 4, 7, 11): t.p(x, 8, t.sh)
        t.r(8, 2, 9, 7, t.m); t.r(14, 2, 15, 15, t.m); t.r(15, 2, 15, 15, t.d); t.r(0, 10, 1, 15, t.m); t.r(0, 10, 0, 15, t.l); t.r(8, 2, 8, 7, t.l)
        for i in range(5): t.p(10 + i if 10 + i < 14 else 13, 3 + i, t.d); t.p(2 + i * 2, 10 + i, t.d); t.p(3 + i * 2, 10 + i, t.d)
        t.r(0, 15, 15, 15, t.sh); t.r(7, 10, 8, 15, t.m); t.r(7, 10, 7, 15, t.l)
    return t


# ---- every one there is ----------------------------------------------------------------------------------------
kept = json.load(open(CACHE)) if os.path.exists(CACHE) else {}
done = {}
for home in [os.path.join(ROOT, 'data')] + sorted(glob.glob(os.path.join(ROOT, 'mods/*'))):
    tiles_path, items_path = os.path.join(home, 'tiles.json'), os.path.join(home, 'items.json')
    if not os.path.exists(tiles_path): continue
    tiles = {t['name']: t for t in json.load(open(tiles_path))['tiles']}
    icons = {i['name']: i.get('texture') for i in json.load(open(items_path))['items']} if os.path.exists(items_path) else {}
    where = os.path.relpath(home, ROOT)
    for name, t in tiles.items():
        shape = 'column' if name.endswith('_column') else 'stairs' if name.endswith('_stairs') else None
        if not shape or not isinstance(t.get('texture'), str): continue
        material = name[:-len('_' + shape)]
        key = '%s/%s' % (where, material)
        png = os.path.join(home, 'textures', t['texture'])
        if key not in kept:
            # (Its colours as they stand: from the column, if it has one, so that the two of a kind agree.)
            first = os.path.join(home, 'textures', tiles.get(material + '_column', t)['texture'])
            tones, alpha = palette(first if os.path.exists(first) else png)
            kept[key] = {'kind': kind_of(material), 'tones': tones, 'alpha': alpha}
        entry = kept[key]
        pic = (column if shape == 'column' else stairs)(entry['kind'], entry['tones'], entry['alpha'])
        pic.tile(png)
        if icons.get(name): pic.icon(os.path.join(home, 'textures', icons[name]))
        done[entry['kind']] = done.get(entry['kind'], 0) + 1
with open(CACHE, 'w') as f:
    f.write('{\n' + ',\n'.join('  %s: %s' % (json.dumps(k), json.dumps(v)) for k, v in sorted(kept.items())) + '\n}\n')
print('drew %d columns and stairs: %s' % (sum(done.values()), ', '.join('%d %s' % (n, k) for k, n in sorted(done.items()))))
