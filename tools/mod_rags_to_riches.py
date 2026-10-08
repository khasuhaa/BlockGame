#!/usr/bin/env python3
"""Writes the Rags to Riches mod: its data files and its textures.

    python3 tools/mod_rags_to_riches.py .

Villages - on the ground, in halls under it and on islands over it - with people living in them. Trade is only
done in a village; every village has prices of its own, which move from week to week; and each sells a few
specialities that the others do not, and that the others pay well for. Buy where a thing is cheap, carry it
to where it is dear.

The buildings are pictures in rows of characters (BUILDINGS, below), built once in each of three styles. To
add one, draw it there; to add a style, add a line to STYLES.

Either edit this and run it again, or edit the mod's files by hand and stop using this: not both.
Run tools/worn_art.py and tools/caged_art.py afterwards (this runs the second itself).
"""
import json, os, random, subprocess, sys
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
mod = os.path.join(repo, 'mods/rags_to_riches'); tex = os.path.join(mod, 'textures')
for d in ('tiles', 'items', 'mobs'): os.makedirs(os.path.join(tex, d), exist_ok=True)

INK = (22, 22, 30, 255)
def c(s): return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
def dk(col, f=0.75): return tuple(int(v * f) for v in col[:3]) + (255,)
def lt(col, f=1.25): return tuple(min(255, int(v * f)) for v in col[:3]) + (255,)

class P:
    def __init__(s, fill=(0, 0, 0, 0)): s.im = Image.new('RGBA', (16, 16), fill); s.px = s.im.load()
    def r(s, x0, y0, x1, y1, col):
        for y in range(max(0, y0), min(15, y1) + 1):
            for x in range(max(0, x0), min(15, x1) + 1): s.px[x, y] = col
        return s
    def p(s, x, y, col): return s.r(x, y, x, y, col)
    def speckle(s, seed, cols, chance=0.3):
        rnd = random.Random(seed)
        for y in range(16):
            for x in range(16):
                if s.px[x, y][3] and rnd.random() < chance: s.px[x, y] = rnd.choice(cols)
        return s
    def outline(s):
        q = s.im.copy().load()
        for y in range(16):
            for x in range(16):
                if q[x, y][3]: continue
                if any(0 <= x + a < 16 and 0 <= y + b < 16 and q[x + a, y + b][3] for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))): s.px[x, y] = INK
        return s
    def save(s, kind, name):
        k = 3 if kind == 'items' else 2
        s.im.resize((16 * k, 16 * k), Image.NEAREST).save(os.path.join(tex, kind, name + '.png')); return s

def block_icon(name):
    """A block's icon is its picture, set in the middle of the square, as the game's own are."""
    im = Image.open(os.path.join(tex, 'tiles', name + '.png')).convert('RGBA')
    out = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); out.paste(im, (8, 8), im); out.save(os.path.join(tex, 'items', name + '.png'))
def thing_icon(name):
    """Something that is not a block: its picture with a line round it."""
    t = P(); t.im = Image.open(os.path.join(tex, 'tiles', name + '.png')).convert('RGBA').resize((16, 16), Image.NEAREST); t.px = t.im.load()
    t.outline().save('items', name)

tiles, items, recipes, mobs = [], [], [], []
def tile(name, label, **kw):
    d = {'name': name, 'label': label, 'solid': True, 'removable': True, 'mine_time': 0.5, 'drop': name, 'texture': 'tiles/%s.png' % name, 'tool': 'pickaxe'}
    d.update(kw); tiles.append({k: v for k, v in d.items() if v is not None}); return d
def back(name, label, **kw): return tile(name, label, solid=False, layer='background', mine_time=0.3, **kw)
def item(name, label, cat, price, tier=None, **kw):
    d = {'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'category': cat, 'price': price}
    if tier: d['tier'] = tier
    d.update(kw); items.append(d); return d
def rec(bench, out, count, ins, time=60, level=None):
    r = {'bench': bench, 'output': {'item': out, 'count': count}, 'inputs': [{'item': i, 'count': n} for i, n in ins], 'time': time}
    if level: r['level'] = level
    recipes.append(r)

# ---- what houses are built of ----------------------------------------------------------------------------------
def roof(name, a):
    """Shingles: rows of them, each row half a shingle along from the last."""
    t = P(a)
    for row in range(4):
        y = row * 4
        t.r(0, y + 3, 15, y + 3, dk(a, 0.62)); t.r(0, y, 15, y, lt(a, 1.18))
        for x in range(0, 16, 4): t.p((x + (2 if row % 2 else 0)) % 16, y + 1, dk(a, 0.62)); t.p((x + (2 if row % 2 else 0)) % 16, y + 2, dk(a, 0.62))
    t.save('tiles', name); block_icon(name)
def thatch():
    a = c('d2aa48'); t = P(a)
    for x in range(16):
        for y in range(16):
            if (x * 7 + y * 3) % 5 == 0: t.p(x, y, dk(a, 0.8))
            elif (x * 5 + y) % 7 == 0: t.p(x, y, lt(a, 1.2))
    for y in (5, 11): t.r(0, y, 15, y, dk(a, 0.6))
    t.save('tiles', 'thatch'); block_icon('thatch')
def wall(name, plaster, beam, stones=False):
    """A wall: plaster between beams - or, for one of stone, courses of blocks."""
    t = P(plaster)
    if stones:
        for row in range(4):
            y = row * 4
            t.r(0, y + 3, 15, y + 3, dk(plaster, 0.6))
            for x in range(0, 16, 8): t.r((x + (4 if row % 2 else 0)) % 16, y, (x + (4 if row % 2 else 0)) % 16, y + 2, dk(plaster, 0.6))
        t.speckle(name, [lt(plaster, 1.1), dk(plaster, 0.9)], 0.18)
    else:
        # (A beam along the top and one down the left, and no more: set side by side they make one frame of
        # squares, with a single beam between any two panels and none doubled.)
        t.speckle(name, [dk(plaster, 0.95), lt(plaster, 1.03)], 0.22)
        t.r(0, 0, 15, 1, beam); t.r(0, 0, 1, 15, beam)
        t.r(0, 0, 15, 0, lt(beam, 1.2)); t.r(0, 2, 0, 15, lt(beam, 1.2)); t.r(2, 2, 15, 2, dk(plaster, 0.82)); t.r(2, 3, 2, 15, dk(plaster, 0.82))
    t.save('tiles', name); block_icon(name)
def cobbles(name, a):
    t = P(dk(a, 0.55)); rnd = random.Random(name)
    for gy in range(4):
        for gx in range(4):
            x, y = gx * 4 + (2 if gy % 2 else 0), gy * 4
            col = rnd.choice([a, lt(a, 1.12), dk(a, 0.88)])
            for dx in range(3):
                for dy in range(3): t.p((x + dx) % 16, y + dy, col)
            t.p(x % 16, y, lt(col, 1.15))
    t.save('tiles', name); block_icon(name)
def planks():
    a = c('a8763e'); t = P(a)
    for y in (3, 7, 11, 15): t.r(0, y, 15, y, dk(a, 0.65))
    for row, xs in enumerate(((5,), (11,), (2,), (8,))):
        for x in xs: t.r(x, row * 4, x, row * 4 + 2, dk(a, 0.65))
    t.speckle('planks', [lt(a, 1.1), dk(a, 0.92)], 0.2); t.save('tiles', 'village_planks'); block_icon('village_planks')

thatch(); roof('slate_roof', c('5a6478')); roof('azure_roof', c('3a78c8'))
wall('timber_wall', c('e6dcc0'), c('6a4526')); wall('hewn_stone', c('7c7f8a'), None, True); wall('whitewash_wall', c('f2f0ea'), c('9aa6ba'))
cobbles('cobblestone', c('8a8d96')); cobbles('sky_cobble', c('d8dce8')); planks()
for n, lab, tool in [('thatch', 'Thatch', 'machete'), ('slate_roof', 'Slate roof', 'pickaxe'), ('azure_roof', 'Azure roof', 'pickaxe'), ('timber_wall', 'Timber wall', 'axe'),
                     ('hewn_stone', 'Hewn stone', 'pickaxe'), ('whitewash_wall', 'Whitewashed wall', 'pickaxe'), ('cobblestone', 'Cobblestone', 'pickaxe'),
                     ('sky_cobble', 'Sky cobble', 'pickaxe'), ('village_planks', 'Floorboards', 'axe')]:
    tile(n, lab, tool=tool, wall=None if n.endswith('roof') or n == 'thatch' else True, burns=True if n in ('thatch', 'timber_wall', 'village_planks') else None)
    item(n, lab, 'blocks', 30, 2, places=n)
rec('woodwork_bench', 'thatch', 4, [('wheat', 4)]); rec('builder_bench', 'slate_roof', 4, [('basalt', 2)]); rec('builder_bench', 'azure_roof', 4, [('clay', 2), ('lapis_lazuli', 1)])
rec('woodwork_bench', 'timber_wall', 4, [('wood', 2), ('limestone', 2)]); rec('builder_bench', 'hewn_stone', 4, [('stone', 4)]); rec('builder_bench', 'whitewash_wall', 4, [('marble', 2), ('limestone', 2)])
rec('builder_bench', 'cobblestone', 4, [('stone', 2), ('gravel', 2)]); rec('builder_bench', 'sky_cobble', 4, [('marble', 2), ('gravel', 2)]); rec('woodwork_bench', 'village_planks', 4, [('wood', 2)])

# ---- what stands about in a village ----------------------------------------------------------------------------
def awning():
    t = P()
    for x in range(16): t.r(x, 3, x, 10, c('c83c3c') if (x // 4) % 2 == 0 else c('f0ece0'))
    t.r(0, 2, 15, 3, c('7a2424')); t.r(0, 10, 15, 10, c('8a2c2c'))
    for x in range(0, 16, 4): t.r(x, 11, x + 1, 12, c('c83c3c') if (x // 4) % 2 == 0 else c('f0ece0'))
    t.save('tiles', 'awning'); thing_icon('awning')
def bell():
    brass = c('d8a838'); t = P()
    t.r(7, 0, 8, 2, c('6a4526')); t.r(5, 3, 10, 4, lt(brass)); t.r(4, 5, 11, 10, brass); t.r(3, 11, 12, 12, dk(brass)); t.r(7, 13, 8, 14, dk(brass, 0.6))
    t.r(5, 5, 6, 9, lt(brass, 1.3)); t.r(10, 6, 11, 10, dk(brass, 0.85))
    t.save('tiles', 'village_bell'); thing_icon('village_bell')
def lamp():
    iron = c('3a3c46'); t = P()
    t.r(7, 6, 8, 15, iron); t.r(5, 15, 10, 15, iron); t.r(5, 1, 10, 5, c('ffd870')); t.r(6, 2, 9, 4, c('fff4c0')); t.r(4, 0, 11, 0, iron); t.r(4, 5, 11, 5, iron); t.p(5, 1, iron); t.p(10, 1, iron)
    t.save('tiles', 'street_lamp'); thing_icon('street_lamp')
def banner(name, a):
    t = P(); t.r(2, 0, 13, 1, c('6a4526')); t.r(3, 2, 12, 11, a); t.r(3, 2, 12, 2, lt(a)); t.r(3, 12, 5, 13, a); t.r(10, 12, 12, 13, a); t.r(6, 12, 9, 12, a)
    t.r(6, 5, 9, 8, c('f0d060')); t.p(7, 4, c('f0d060')); t.p(8, 9, c('f0d060'))
    t.save('tiles', name); thing_icon(name)
def crate():
    a = c('9a6a34'); t = P(); t.r(1, 3, 14, 15, a); t.r(1, 3, 14, 4, lt(a)); t.r(1, 14, 14, 15, dk(a)); t.r(1, 3, 2, 15, dk(a)); t.r(13, 3, 14, 15, dk(a))
    for i in range(3, 13): t.p(i, i + 1, dk(a, 0.7)); t.p(15 - i, i + 1, dk(a, 0.7))
    t.r(1, 8, 14, 9, dk(a, 0.85)); t.save('tiles', 'village_crate'); thing_icon('village_crate')
def well():
    st = c('8a8d96'); t = P(); t.r(2, 9, 13, 15, st); t.r(2, 9, 13, 9, lt(st)); t.r(4, 11, 11, 13, c('2a5aa8')); t.r(3, 2, 4, 9, c('6a4526')); t.r(11, 2, 12, 9, c('6a4526'))
    t.r(2, 0, 13, 2, c('8a2c2c')); t.r(7, 3, 8, 8, c('c8c0a0')); t.r(6, 8, 9, 9, c('6a4526')); t.save('tiles', 'village_well'); thing_icon('village_well')
awning(); bell(); lamp(); banner('village_banner', c('3a62b8')); crate(); well()
back('awning', 'Market awning', tool='machete', attaches=True, floats=True, model='panel'); item('awning', 'Market awning', 'crafted', 60, 2, places='awning')
back('village_bell', 'Village bell', removable=False, drop=None, village=46, light=8, attaches=True, floats=True, model='panel')
item('village_bell', 'Village bell', 'special', 50000, places='village_bell')
back('street_lamp', 'Street lamp', light=14, attaches=True, model='panel'); item('street_lamp', 'Street lamp', 'crafted', 240, 3, places='street_lamp')
back('village_banner', 'Village banner', tool='machete', attaches=True, floats=True, model='panel'); item('village_banner', 'Village banner', 'crafted', 120, 2, places='village_banner')
back('village_well', 'Well', attaches=True, model='panel'); item('village_well', 'Well', 'crafted', 300, 3, places='village_well')
COIN = {'one_of': [{'item': 'copper_coin', 'weight': 6}, {'item': 'gold_coin', 'weight': 3}], 'min': 2, 'max': 20}
back('village_crate', 'Crate', tool='axe', burns=True, storage={'columns': 4, 'rows': 2},
     loot=[COIN, {'one_of': [{'item': n, 'weight': w} for n, w in (('bread', 4), ('apple', 4), ('wheat', 4), ('carrot', 3), ('cooked_fish', 2), ('flour', 3), ('coffee_bean', 2))], 'min': 1, 'max': 5},
           {'one_of': [{'item': n, 'weight': w} for n, w in (('linen', 4), ('string', 4), ('leather', 3), ('iron_ingot', 2), ('copper_ingot', 3), ('coal', 4), ('torch', 4), ('glass', 2))], 'min': 1, 'max': 4}])
item('village_crate', 'Crate', 'crafted', 60, 2, places='village_crate', container={'slots': 8})
rec('tailor_bench', 'awning', 2, [('linen', 2), ('stick', 1)]); rec('metalwork_bench', 'street_lamp', 1, [('iron_ingot', 1), ('torch', 1)]); rec('tailor_bench', 'village_banner', 1, [('linen', 2), ('ultramarine_blue', 1)])
rec('builder_bench', 'village_well', 1, [('stone', 6), ('bucket', 1), ('wood', 2)]); rec('woodwork_bench', 'village_crate', 1, [('wood', 3)])

# ---- the buildings ---------------------------------------------------------------------------------------------
# One picture each, in characters: # an outside wall, . a room (wall behind, air in front), R roof, F floor,
# D door, W window, L lamp, B b a bed, C a crate, S a shelf, s steps, = an upper floor, c a post, A awning,
# X the bell, P the trade portal, o a street lamp, f a banner, w the well, ~ the world as it was, space open air.
BUILDINGS = {
    'cottage': ["~~~~RR~~~~", "~~~RRRR~~~", "~~RRRRRR~~", "~RRRRRRRR~", "RRRRRRRRRR", "~#.W..L.#~", "~D......D~", "~DBb..C.D~", "~FFFFFFFF~"],
    'longhouse': ["~~~RRRRRRRRRR~~~", "~~RRRRRRRRRRRR~~", "~RRRRRRRRRRRRRR~", "RRRRRRRRRRRRRRRR", "~#.L.W..#.W.L.#~", "~D......D.....#~", "~DBb.S.CD.CBb.#~", "~FFFFFFFFFFFFFF~"],
    'two_storey': ["~~~~RR~~~~", "~~~RRRR~~~", "~~RRRRRR~~", "~RRRRRRRR~", "RRRRRRRRRR", "~#.W..L.#~", "~#......#~", "~#Bb..s.#~", "~#====s=#~", "~#L.W.s.#~", "~D...s..D~", "~DC.s.S.D~", "~FFFFFFFF~"],
    'tower': ["~~RR~~", "~RRRR~", "RRRRRR", "~#W.#~", "~#..#~", "~#sC#~", "~#s=#~", "~#sL#~", "~#s.#~", "~#s=#~", "~#sW#~", "~D..#~", "~DsS#~", "~FFFF~"],
    'storehouse': ["RRRRRRRR", "~#L..L#~", "~D....#~", "~DCCCC#~", "~FFFFFF~"],
    'well': ["~~~~~", " o w ", "FFFFF"],
    'garden': ["       ", " f   f ", "FFFFFFF"],
    'market': ["~AAAAAAAAAAAAA~", "~c           c~", "~c  P     X  c~", "~c  P  f  c  c~", "~co P     c Cc~", "~FFFFFFFFFFFFF~"],
}
# The styles: the same buildings in different stuff. (name, where, wall, roof, floor, street, ground, portal)
STYLES = [('hamlet', 'ground', 'timber_wall', 'thatch', 'village_planks', 'cobblestone', 'dirt', 'trade_portal_3'),
          ('delve', 'cave', 'hewn_stone', 'slate_roof', 'village_planks', 'cobblestone', 'stone', 'trade_portal_4'),
          ('aerie', 'island', 'whitewash_wall', 'azure_roof', 'village_planks', 'sky_cobble', 'dirt', 'trade_portal_6')]
structures, villages = [], []
for style, where, wall_, roof_, floor, street, ground, portal in STYLES:
    room = {'wall': wall_}
    legend = {'#': {'front': wall_, 'wall': wall_}, '.': room, 'R': {'front': roof_}, 'F': {'front': floor, 'wall': floor}, '=': {'front': floor, 'wall': wall_},
              'D': {'front': 'wooden_door', 'wall': wall_}, 'W': dict(room, back='window'), 'L': dict(room, back='torch'), 'B': dict(room, back='wooden_bed'),
              'b': dict(room, back='wooden_bed_foot'), 'C': dict(room, back='village_crate'), 'S': dict(room, back='shelf'), 's': dict(room, back='wood_stairs'),
              'c': {'back': 'wood_column'}, 'A': {'back': 'awning'}, 'X': {'back': 'village_bell'}, 'P': {'back': portal}, 'o': {'back': 'street_lamp'},
              'f': {'back': 'village_banner'}, 'w': {'back': 'village_well'}}
    # (Under the ground there is nothing to see out of a window at: a wall like the rest.)
    if where == 'cave': legend['W'] = room
    # (A crate under the market's awning has no wall behind it, nor the things that stand in the open.)
    for name, rows in BUILDINGS.items():
        lg = dict(legend)
        if name in ('market', 'well', 'garden'): lg['C'] = {'back': 'village_crate'}; lg['F'] = {'front': street}
        structures.append({'name': '%s_%s' % (style, name), 'rows': rows, 'legend': {k: v for k, v in lg.items() if any(k in r for r in rows)}})
    villages.append({'name': style, 'where': where, 'at_least': 1, 'per_1000_columns': {'ground': 1.2, 'cave': 0.5, 'island': 0.4}[where], 'houses': [4, 8],
                     'centre': style + '_market', 'street': street, 'ground': ground,
                     'buildings': [style + '_' + b for b in ('cottage', 'cottage', 'longhouse', 'two_storey', 'tower', 'storehouse', 'well', 'garden')]})

# ---- the people ------------------------------------------------------------------------------------------------
def person(name, hair, shirt, legs, step):
    skin = c('e8b88a'); t = P()
    t.r(6, 1, 9, 2, hair); t.r(6, 3, 9, 5, skin); t.p(6, 3, hair); t.p(8, 4, INK)
    t.r(5, 6, 10, 10, shirt); t.r(5, 6, 10, 6, lt(shirt)); t.r(4, 7, 4, 9, shirt); t.r(11, 7, 11, 9, shirt); t.p(4, 10, skin); t.p(11, 10, skin)
    if step == 0: t.r(6, 11, 7, 14, legs); t.r(8, 11, 9, 14, legs); t.r(6, 15, 7, 15, INK); t.r(8, 15, 9, 15, INK)
    else:
        a, b = (5, 9) if step == 1 else (7, 10)
        t.r(6, 11, 9, 12, legs); t.r(a, 13, a + 1, 14, legs); t.r(b - 1, 13, b, 13, legs); t.r(a, 15, a + 1, 15, INK); t.r(b - 1, 14, b, 14, INK)
    t.outline().save('mobs', name)
FOLK = [('villager', 'Villager', c('5a3a22'), c('4a7ab8'), c('5a4a3a')), ('merchant', 'Merchant', c('2a2a30'), c('a83c3c'), c('3a3a48')),
        ('farmer', 'Farmer', c('c8a040'), c('6a9a4a'), c('7a5a34')), ('elder', 'Elder', c('d8d8d8'), c('7a5aa8'), c('4a4a52'))]
for name, label, hair, shirt, legs in FOLK:
    for step, suffix in ((0, ''), (1, '_walk_0'), (2, '_walk_1')): person(name + suffix, hair, shirt, legs, step)
    mobs.append({'name': name, 'label': label, 'texture': 'mobs/%s.png' % name, 'walk': ['mobs/%s_walk_0.png' % name, 'mobs/%s.png' % name, 'mobs/%s_walk_1.png' % name, 'mobs/%s.png' % name],
                 'hop': 0.03, 'step_ticks': 5, 'move_chance': 0.12, 'turn_chance': 0.3, 'health': 20, 'cap': 3,
                 'spawn': {'above': ['village_planks', 'cobblestone', 'sky_cobble'], 'chance': 0.5}, 'drops': [{'item': 'copper_coin', 'min': 0, 'max': 3}]})

# ---- specialities: sold in a few villages each, and nowhere else ------------------------------------------------
def special(name, label, cat, price, draw, **kw):
    draw(); item(name, label, cat, price, 6, speciality=True, **kw)
def boots(a):
    def go():
        t = P(); t.r(3, 5, 6, 12, a); t.r(3, 12, 8, 13, a); t.r(9, 5, 12, 12, a); t.r(9, 12, 14, 13, a); t.r(3, 5, 6, 5, lt(a)); t.r(9, 5, 12, 5, lt(a)); t.r(3, 13, 8, 13, dk(a)); t.r(9, 13, 14, 13, dk(a)); t.outline()
        return t
    return go
def save_as(name, maker): return lambda: maker().save('items', name)
def bowl(a, b):
    def go():
        t = P(); t.r(2, 8, 13, 9, c('c8b890')); t.r(3, 10, 12, 12, c('a89868')); t.r(5, 13, 10, 13, c('8a7a50')); t.r(3, 7, 12, 8, a); t.p(5, 7, b); t.p(9, 7, b); t.p(7, 6, b); t.outline()
        return t
    return go
def cup(a):
    def go():
        t = P(); t.r(4, 6, 10, 13, c('e8e4d8')); t.r(4, 6, 10, 7, a); t.r(11, 8, 12, 11, c('e8e4d8')); t.r(5, 13, 9, 13, c('b8b4a8')); t.p(6, 3, c('d0d0d0')); t.p(8, 4, c('d0d0d0')); t.p(7, 2, c('d0d0d0')); t.outline()
        return t
    return go
def cake(a, b):
    def go():
        t = P(); t.r(3, 7, 12, 12, a); t.r(3, 6, 12, 7, b); t.r(3, 12, 12, 12, dk(a)); t.p(5, 8, b); t.p(9, 9, b); t.p(7, 5, c('c83c3c')); t.outline()
        return t
    return go
def tool(head, kind):
    def go():
        wood = c('8c6537'); t = P()
        for i in range(3, 13): t.p(i, 15 - i, wood); t.p(i + 1, 15 - i, dk(wood))
        if kind == 'pick': t.r(8, 2, 13, 3, head); t.r(12, 3, 13, 7, head); t.p(7, 3, head); t.p(13, 8, head); t.r(8, 2, 13, 2, lt(head))
        elif kind == 'axe': t.r(9, 1, 13, 6, head); t.r(13, 2, 14, 5, lt(head)); t.r(9, 1, 13, 1, lt(head))
        else: t.r(6, 1, 13, 2, head); t.r(12, 3, 13, 6, head); t.r(4, 2, 6, 3, head); t.r(6, 1, 13, 1, lt(head))
        t.outline(); return t
    return go
def cloth(a, kind):
    def go():
        t = P()
        if kind == 'hat': t.r(4, 4, 11, 9, a); t.r(1, 10, 14, 11, a); t.r(4, 8, 11, 9, c('f0d060')); t.r(4, 4, 11, 4, lt(a)); t.r(1, 11, 14, 11, dk(a))
        elif kind == 'cloak': t.r(4, 2, 11, 4, a); t.r(3, 5, 12, 13, a); t.r(2, 9, 13, 13, a); t.r(7, 3, 8, 12, dk(a)); t.r(3, 5, 12, 5, lt(a)); t.p(7, 3, c('f0d060')); t.p(8, 3, c('f0d060'))
        else: t.r(4, 3, 11, 13, a); t.r(1, 3, 3, 8, a); t.r(12, 3, 14, 8, a); t.r(6, 3, 9, 4, dk(a, 0.6)); t.r(4, 13, 11, 13, dk(a)); t.r(4, 3, 11, 3, lt(a)); t.r(7, 5, 8, 12, c('f0d060'))
        t.outline(); return t
    return go
def placed(name, maker):
    """Something to put down: the same picture is the block and the icon."""
    def go():
        t = maker(); t.save('tiles', name); thing_icon(name)
    return go
def lantern(a):
    def go():
        t = P(); t.r(6, 1, 9, 2, c('d8a838')); t.r(4, 3, 11, 4, c('d8a838')); t.r(5, 5, 10, 11, a); t.r(6, 6, 9, 10, lt(a, 1.4)); t.r(4, 12, 11, 13, c('d8a838')); t.r(4, 5, 4, 11, c('a87a20')); t.r(11, 5, 11, 11, c('a87a20'))
        return t
    return go
def glass(cols):
    def go():
        t = P(c('3a3c46'))
        for i, col in enumerate(cols): t.r(1 + (i % 2) * 7, 1 + (i // 2) * 7, 7 + (i % 2) * 7, 7 + (i // 2) * 7, col)
        t.r(7, 0, 8, 15, c('3a3c46')); t.r(0, 7, 15, 8, c('3a3c46')); t.r(0, 0, 15, 0, c('3a3c46')); t.r(0, 15, 15, 15, c('3a3c46')); t.r(0, 0, 0, 15, c('3a3c46')); t.r(15, 0, 15, 15, c('3a3c46'))
        return t
    return go
special('traders_boots', "Trader's boots", 'clothing', 14000, save_as('traders_boots', boots(c('8a5a2a'))), equip='boots', max_stack=1, armor=0.06, warmth=0.3, walk_speed=1.3)
special('merchants_hat', "Merchant's hat", 'clothing', 18000, save_as('merchants_hat', cloth(c('3a2a5a'), 'hat')), equip='head', max_stack=1, armor=0.04, warmth=0.3, boons={'luck': 0.12})
special('silk_tunic', 'Silk tunic', 'clothing', 22000, save_as('silk_tunic', cloth(c('c84a7a'), 'tunic')), equip='chest', max_stack=1, armor=0.05, warmth=0.45, boons={'regen': 1.4})
special('gliding_cloak', 'Gliding cloak', 'clothing', 32000, save_as('gliding_cloak', cloth(c('4a8ab8'), 'cloak')), equip='chest', max_stack=1, warmth=0.35, glides=True)
special('masons_pick', "Mason's pick", 'tools', 16000, save_as('masons_pick', tool(c('b8c0cc'), 'pick')), tool={'class': 'pickaxe', 'speed': 9.5})
special('woodsmans_axe', "Woodsman's axe", 'tools', 12000, save_as('woodsmans_axe', tool(c('b8c0cc'), 'axe')), tool={'class': 'axe', 'speed': 10.0})
special('reapers_scythe', "Reaper's scythe", 'tools', 9000, save_as('reapers_scythe', tool(c('d0d4dc'), 'scythe')), tool={'class': 'machete', 'speed': 11.0})
special('village_stew', 'Village stew', 'nature', 900, save_as('village_stew', bowl(c('a85a2a'), c('e0a040'))), food=1.0, food_health=0.5, cooked=True)
special('honey_cake', 'Honey cake', 'nature', 700, save_as('honey_cake', cake(c('e0a83a'), c('f8e8b0'))), food=0.6, food_energy=0.4, energy_over=True, cooked=True)
special('spiced_tea', 'Spiced tea', 'nature', 500, save_as('spiced_tea', cup(c('a8501c'))), food=0.1, food_energy=0.7, cooked=True)
special('festival_pie', 'Festival pie', 'nature', 1200, save_as('festival_pie', cake(c('b06a2a'), c('c83c3c'))), food=0.9, food_health=0.3, food_energy=0.3, cooked=True)
special('lantern_of_plenty', 'Lantern of plenty', 'crafted', 6000, placed('lantern_of_plenty', lantern(c('ffc850'))), places='lantern_of_plenty')
back('lantern_of_plenty', 'Lantern of plenty', light=15, attaches=True, floats=True, model='panel', craft_speed=1.25, craft_radius=6)
special('stained_glass', 'Stained glass', 'crafted', 2400, placed('stained_glass', glass([c('c83c3c'), c('3a62b8'), c('e0b030'), c('3a9a5a')])), places='stained_glass')
back('stained_glass', 'Stained glass', window={}, attaches=True)
special('golden_banner', 'Golden banner', 'crafted', 3200, lambda: (banner('golden_banner', c('a8201c')), None)[1], places='golden_banner')
back('golden_banner', 'Golden banner', tool='machete', attaches=True, floats=True, model='panel')

# ---- written out -------------------------------------------------------------------------------------------------
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v))
                                    for k, v in obj.items()) + '\n}\n')
for n, i in enumerate(items):
    if i.get('equip'):
        o = {}
        for k, v in i.items():
            o[k] = v
            if k == 'equip': o['worn'] = 'worn/%s.png' % i['name']
        items[n] = o
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes}); dump('mobs.json', {'mobs': mobs})
dump('worldgen.json', {'structures': structures, 'villages': villages})
dump('trade.json', {'villages': {'only': True, 'swing': 0.2, 'share': 0.6, 'days': 7, 'specialities': 4}})
json.dump({'label': 'Rags to Riches', 'icon': 'icon.png', 'description': 'Villages, the people in them, and trade that is only done there: every village with its own prices.'},
          open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
Image.open(os.path.join(tex, 'items', 'village_bell.png')).save(os.path.join(mod, 'icon.png'))
print('%d tiles, %d items, %d recipes, %d creatures, %d structures, %d kinds of village' % (len(tiles), len(items), len(recipes), len(mobs), len(structures), len(villages)))
subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'caged_art.py'), repo], check=True)
