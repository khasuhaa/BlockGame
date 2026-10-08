#!/usr/bin/env python3
"""Writes the Alchemie mod: its data files and its textures.

    python3 tools/mod_alchemie.py .

Alchemy: benches that turn things into other things. An alembic draws the six essences out of ordinary stuff;
an athanor uses them to turn one metal, one stone, one material into another; an alchemist's desk does the same
for gems and makes the great works - quicksilver, the philosopher's stone, the Tome of Knowledge. All of it leads
to the Transmutation Table, which makes platinum coins out of nothing at all but time.

Either edit this and run it again, or edit the mod's files by hand and stop using this: not both.
"""
import json, os, sys
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
mod = os.path.join(repo, 'mods/alchemie'); tex = os.path.join(mod, 'textures')
for d in ('tiles', 'items'): os.makedirs(os.path.join(tex, d), exist_ok=True)

INK = (22, 22, 30, 255)
def c(s): return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
def dk(col, f=0.7): return tuple(int(v * f) for v in col[:3]) + (255,)
def lt(col, f=1.3): return tuple(min(255, int(v * f)) for v in col[:3]) + (255,)

class P:
    def __init__(s, fill=(0, 0, 0, 0)): s.im = Image.new('RGBA', (16, 16), fill); s.px = s.im.load()
    def r(s, x0, y0, x1, y1, col):
        for y in range(max(0, y0), min(15, y1) + 1):
            for x in range(max(0, x0), min(15, x1) + 1): s.px[x, y] = col
    def p(s, x, y, col): s.r(x, y, x, y, col)
    def outline(s):
        src = s.im.copy(); q = src.load()
        for y in range(16):
            for x in range(16):
                if q[x, y][3]: continue
                if any(0 <= x + a < 16 and 0 <= y + b < 16 and q[x + a, y + b][3] for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))): s.px[x, y] = INK
        return s
    def save(s, kind, name):
        k = 3 if kind == 'items' else 2
        s.im.resize((16 * k, 16 * k), Image.NEAREST).save(os.path.join(tex, kind, name + '.png')); return s
def bench_icon(name):
    """A bench's icon is the bench, as the game's own are."""
    im = Image.open(os.path.join(tex, 'tiles', name + '.png')).convert('RGBA').resize((16, 16), Image.NEAREST)
    t = P(); t.im = im; t.px = im.load(); t.outline().save('items', name)

GLASS, BRASS, STONE, WOOD = c('a8d8e8'), c('c8a040'), c('6e717c'), c('8c6537')
items, tiles, recipes = [], [], []

# ---- the six essences: a phial of each -------------------------------------------------------------------------
ESSENCES = [('terra', 'Terra essence', '8a6a3a'), ('vita', 'Vita essence', '5ab84a'), ('ignis', 'Ignis essence', 'f07828'),
            ('aqua', 'Aqua essence', '3a86d8'), ('metallum', 'Metallum essence', 'b4bcc8'), ('aether', 'Aether essence', 'b37aec')]
for name, label, col in ESSENCES:
    k = c(col); t = P()
    t.r(7, 2, 8, 3, WOOD); t.r(7, 4, 8, 5, GLASS); t.r(5, 6, 10, 12, GLASS); t.r(4, 8, 11, 11, GLASS); t.r(5, 13, 10, 13, dk(GLASS))
    t.r(5, 8, 10, 12, k); t.r(4, 9, 11, 11, k); t.r(5, 8, 10, 8, lt(k)); t.p(6, 7, c('ffffff')); t.p(6, 10, lt(k)); t.p(9, 11, dk(k))
    t.outline().save('items', name + '_essence')
    items.append({'name': name + '_essence', 'label': label, 'texture': 'items/%s_essence.png' % name, 'category': 'materials', 'price': 120 if name != 'aether' else 400, 'tier': 4})

# Quicksilver, the philosopher's stone, and the Tome of Knowledge.
t = P(); t.r(5, 6, 10, 11, c('c8d0dc')); t.r(4, 8, 11, 10, c('c8d0dc')); t.r(6, 5, 9, 5, c('e8eef6')); t.r(5, 11, 10, 12, c('8a94a4')); t.p(6, 7, c('ffffff')); t.p(7, 6, c('ffffff')); t.r(12, 11, 13, 12, c('c8d0dc')); t.p(2, 12, c('c8d0dc'))
t.outline().save('items', 'quicksilver')
items.append({'name': 'quicksilver', 'label': 'Quicksilver', 'texture': 'items/quicksilver.png', 'category': 'materials', 'price': 420, 'tier': 5})
t = P(); R = c('c61e38')
t.r(6, 2, 9, 13, R); t.r(4, 5, 11, 10, R); t.r(5, 3, 10, 12, R); t.r(6, 4, 7, 6, lt(R, 1.5)); t.r(8, 8, 10, 11, dk(R)); t.p(7, 7, c('f6e88a')); t.p(8, 7, c('f6e88a')); t.p(7, 8, c('f6e88a'))
t.p(2, 3, c('f6e88a')); t.p(13, 5, c('f6e88a')); t.p(12, 12, c('f6e88a')); t.p(3, 11, c('f6e88a'))
t.outline().save('items', 'philosophers_stone')
items.append({'name': 'philosophers_stone', 'label': "Philosopher's stone", 'texture': 'items/philosophers_stone.png', 'max_stack': 1, 'category': 'materials', 'price': 24000, 'tier': 6})
t = P(); B = c('4a2a86')
t.r(3, 2, 12, 13, B); t.r(3, 2, 4, 13, dk(B)); t.r(12, 3, 13, 13, c('e8dcc0')); t.r(5, 13, 12, 14, c('e8dcc0')); t.r(6, 5, 10, 9, BRASS); t.r(7, 6, 9, 8, c('f6e88a')); t.p(8, 7, c('ffffff'))
t.p(5, 3, BRASS); t.p(11, 3, BRASS); t.p(5, 11, BRASS); t.p(11, 11, BRASS)
t.outline().save('items', 'tome_of_knowledge')
# (Carried, it makes everything found: every trade portal deals in all it could.)
items.append({'name': 'tome_of_knowledge', 'label': 'Tome of Knowledge', 'texture': 'items/tome_of_knowledge.png', 'max_stack': 1, 'reveals_all': True,
              'category': 'materials', 'price': 18000, 'tier': 6})

# ---- the benches -------------------------------------------------------------------------------------------
# The alembic: a round flask on a stand, with its long neck over to a small one.
t = P(); t.r(2, 13, 9, 14, STONE); t.r(3, 11, 3, 12, dk(STONE)); t.r(8, 11, 8, 12, dk(STONE)); t.r(3, 5, 8, 10, GLASS); t.r(2, 6, 9, 9, GLASS); t.r(3, 7, 8, 10, c('5ab84a')); t.r(2, 7, 9, 9, c('5ab84a'))
t.r(5, 2, 6, 4, GLASS); t.r(6, 2, 12, 2, BRASS); t.r(12, 2, 13, 8, BRASS); t.r(11, 9, 14, 12, GLASS); t.r(11, 11, 14, 12, c('b37aec')); t.r(10, 13, 15, 14, STONE); t.p(4, 6, c('ffffff')); t.r(4, 12, 7, 12, c('f07828')); t.p(5, 11, c('f6dc6a'))
t.save('tiles', 'alembic'); bench_icon('alembic')
# The athanor: a tower of a furnace, banded in brass, with a fire door.
t = P(); BR = c('a0503a')
t.r(3, 4, 12, 15, BR); t.r(4, 2, 11, 3, BR); t.r(6, 0, 9, 1, dk(BR)); t.r(3, 6, 12, 6, BRASS); t.r(3, 11, 12, 11, BRASS); t.r(5, 12, 10, 15, INK); t.r(6, 13, 9, 15, c('f07828')); t.r(7, 14, 8, 15, c('f6dc6a'))
t.r(6, 7, 9, 9, dk(BR, 0.5)); t.r(7, 8, 8, 8, c('f07828')); t.p(4, 5, lt(BR)); t.p(4, 8, lt(BR)); t.r(12, 4, 12, 15, dk(BR)); t.p(7, 0, c('8a8e9a'))
t.save('tiles', 'athanor'); bench_icon('athanor')
# The alchemist's desk: a desk with a book, a candle and a flask on it.
t = P(); t.r(0, 8, 15, 9, WOOD); t.r(0, 8, 15, 8, lt(WOOD)); t.r(1, 10, 2, 15, dk(WOOD)); t.r(13, 10, 14, 15, dk(WOOD)); t.r(3, 10, 12, 12, dk(WOOD, 0.85)); t.p(7, 11, BRASS); t.p(8, 11, BRASS)
t.r(2, 6, 6, 7, c('4a2a86')); t.r(2, 6, 6, 6, c('e8dcc0')); t.r(8, 4, 8, 7, c('e8dcc0')); t.p(8, 3, c('f6dc6a')); t.p(8, 2, c('f07828')); t.r(11, 5, 13, 7, GLASS); t.r(11, 6, 13, 7, c('c61e38')); t.r(12, 3, 12, 4, GLASS)
t.save('tiles', 'alchemists_desk'); bench_icon('alchemists_desk')
# The Transmutation Table: a slab of dark stone on gold feet, a circle cut in its top, lit from within.
t = P(); DKS, GOLD, LIT = c('3a3448'), c('e2c466'), c('f6e88a')
t.r(0, 6, 15, 9, DKS); t.r(0, 6, 15, 6, lt(DKS, 1.5)); t.r(0, 9, 15, 9, dk(DKS)); t.r(1, 10, 3, 15, GOLD); t.r(12, 10, 14, 15, GOLD); t.r(1, 15, 3, 15, dk(GOLD)); t.r(12, 15, 14, 15, dk(GOLD)); t.r(5, 10, 10, 11, dk(DKS))
t.r(3, 7, 12, 8, GOLD); t.r(5, 7, 10, 8, LIT); t.p(7, 7, c('ffffff')); t.p(8, 8, c('ffffff'))
t.r(6, 2, 9, 4, LIT); t.r(7, 1, 8, 5, LIT); t.p(7, 3, c('ffffff')); t.p(4, 4, GOLD); t.p(11, 3, GOLD); t.p(3, 2, LIT); t.p(12, 5, LIT)
t.save('tiles', 'transmutation_table'); bench_icon('transmutation_table')
BENCHES = [('alembic', 'Alembic', 'alembic', 'pickaxe', 260, None), ('athanor', 'Athanor', 'athanor', 'pickaxe', 1900, 5),
           ('alchemists_desk', "Alchemist's desk", 'desk', 'axe', 2600, None), ('transmutation_table', 'Transmutation table', 'table', 'pickaxe', 60000, 8)]
for name, label, word, tool, price, light in BENCHES:
    tile = {'name': name, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.4, 'drop': name, 'texture': 'tiles/%s.png' % name,
            'bench': True, 'bench_kind': name, 'bench_level': 1, 'popup_name': word, 'tool': tool}
    if light: tile['light'] = light
    tiles.append(tile)
    items.append({'name': name, 'label': label, 'places': name, 'texture': 'items/%s.png' % name, 'category': 'benches', 'price': price})

def rec(bench, out, n, ins, time, level=1):
    return {'bench': bench, 'level': level, 'output': {'item': out, 'count': n}, 'inputs': [dict({'item': i[0], 'count': i[1]}, **({'full': True} if len(i) > 2 else {})) for i in ins], 'time': time}
E = lambda n: n + '_essence'

# The benches themselves.
recipes += [rec('workbench', 'alembic', 1, [('glass', 4), ('copper_ingot', 2), ('clay', 2)], 60, level=2),
            rec('workbench', 'athanor', 1, [('red_brick', 8), ('iron_ingot', 4), (E('ignis'), 2), (E('terra'), 2)], 120, level=3),
            rec('workbench', 'alchemists_desk', 1, [('wood', 6), ('gold_ingot', 2), (E('aether'), 4), (E('vita'), 2)], 120, level=3)]

# The alembic: essences out of ordinary things. Several ways to each.
for out, ways in (('terra', [('dirt', 16), ('stone', 12), ('sand', 12), ('gravel', 8)]), ('vita', [('wheat', 8), ('flax', 8), ('apple', 6), ('kelp', 6), ('compost', 4)]),
                  ('ignis', [('coal', 4), ('charcoal', 6), ('oil', 2)]), ('aqua', [('ice', 4), ('bucket', 1, True)]),
                  ('metallum', [('copper_ore', 2), ('tin_ore', 2), ('iron_ore', 1)]), ('aether', [('amethyst_shard', 2), ('amethyst', 1)])):
    for way in ways:
        made = 2 if way[0] == 'amethyst' else 1
        recipes.append(rec('alembic', E(out), made, [way], 20))

# The athanor: one thing into another, with an essence to do it.
A = lambda out, n, ins, time=30: recipes.append(rec('athanor', out, n, ins, time))
# metals, up the ladder - the old dream, iron into gold - and sideways
A('tin_ingot', 2, [('copper_ingot', 2), (E('metallum'), 1)]); A('copper_ingot', 2, [('tin_ingot', 2), (E('metallum'), 1)])
A('iron_ingot', 1, [('copper_ingot', 3), (E('metallum'), 1), (E('ignis'), 1)], 45)
A('gold_ingot', 1, [('iron_ingot', 3), (E('metallum'), 2), (E('aether'), 1)], 60)
A('titanium_ingot', 1, [('gold_ingot', 2), (E('metallum'), 2), (E('ignis'), 1)], 90)
A('platinum_ingot', 1, [('titanium_ingot', 2), (E('metallum'), 1), (E('aether'), 2)], 120)
# stone
A('gravel', 4, [('stone', 4), (E('terra'), 1)]); A('sand', 4, [('gravel', 4), (E('terra'), 1)]); A('stone', 4, [('sand', 4), (E('terra'), 1)])
A('limestone', 4, [('stone', 4), (E('aqua'), 1)]); A('sandstone', 4, [('sand', 4), (E('terra'), 1), (E('ignis'), 1)]); A('basalt', 4, [('stone', 4), (E('ignis'), 1)])
A('marble', 4, [('limestone', 4), (E('ignis'), 1)]); A('red_marble', 4, [('marble', 4), (E('metallum'), 1)]); A('lapis_lazuli', 4, [('marble', 4), (E('aether'), 1)])
A('glass', 4, [('sand', 4), (E('ignis'), 1)]); A('black_sand', 4, [('sand', 4), (E('metallum'), 1)]); A('ice', 4, [(E('aqua'), 2)])
# what burns, and what grows
A('clay', 4, [('dirt', 4), (E('aqua'), 1)]); A('compost', 4, [('dirt', 4), (E('vita'), 1)]); A('coal', 2, [('charcoal', 4), (E('terra'), 1)]); A('oil', 2, [('coal', 2), (E('aqua'), 1)])

# The alchemist's desk: gems, each made of the one before; and the great works.
D = lambda out, n, ins, time=60: recipes.append(rec('alchemists_desk', out, n, ins, time))
for a, b, k in (('amethyst', 'sapphire', 1), ('sapphire', 'emerald', 2), ('emerald', 'ruby', 2), ('ruby', 'diamond', 3)): D(b, 1, [(a, 2), (E('aether'), k)], 90)
D('quicksilver', 1, [(E('metallum'), 2), (E('aqua'), 1)], 45)
D('philosophers_stone', 1, [('diamond', 1), ('quicksilver', 4)] + [(E(n), 2) for n, _, _ in ESSENCES], 600)
D('tome_of_knowledge', 1, [('linen', 8), (E('aether'), 4), (E('vita'), 4), ('gold_ingot', 1)], 300)
D('transmutation_table', 1, [('philosophers_stone', 1), ('gold_block', 2), ('marble', 8), (E('aether'), 8)], 600)

# The Transmutation Table: out of nothing. A platinum coin takes a quarter of an hour; aether, two minutes.
recipes.append(rec('transmutation_table', 'platinum_coin', 1, [], 900))
recipes.append(rec('transmutation_table', E('aether'), 1, [], 120))

def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        if len(obj) == 1 and isinstance(next(iter(obj.values())), list):
            k, v = next(iter(obj.items()))
            f.write('{\n  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]\n}\n')
        else: json.dump(obj, f, indent=1); f.write('\n')
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes}); dump('mod.json', {'label': 'Alchemie', 'icon': 'icon.png'})
# Its icon: the red stone over a gold circle.
t = P(c('2a2438')); t.r(3, 3, 12, 12, GOLD); t.r(4, 4, 11, 11, c('2a2438')); t.r(6, 5, 9, 10, R); t.r(5, 6, 10, 9, R); t.p(7, 6, c('f6e88a')); t.p(2, 2, LIT); t.p(13, 2, LIT); t.p(2, 13, LIT); t.p(13, 13, LIT)
t.im.resize((64, 64), Image.NEAREST).save(os.path.join(mod, 'icon.png'))
print(len(tiles), 'tiles', len(items), 'items', len(recipes), 'recipes')
