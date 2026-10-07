"""Writes the Explorer's Delight mod: its data files and its textures. Run with the repository root as the argument."""
import sys, os, json, copy, random
from PIL import Image
repo = sys.argv[1]; mod = os.path.join(repo, 'mods/explorers_delight'); tex = os.path.join(mod, 'textures')
for d in ('tiles', 'items'): os.makedirs(os.path.join(tex, d), exist_ok=True)
base = os.path.join(repo, 'data')
def load(n): return json.load(open(os.path.join(base, n)))
T = {t['name']: t for t in load('tiles.json')['tiles']}
I = {t['name']: t for t in load('items.json')['items']}
TR = {t['name']: t for t in load('trees.json')['trees']}

def dk(c, f=0.72): return tuple(int(v * f) for v in c[:3]) + (255,)
def lt(c, f=1.25): return tuple(min(255, int(v * f)) for v in c[:3]) + (255,)
class P:
    def __init__(s, fill=(0, 0, 0, 0)): s.im = Image.new('RGBA', (16, 16), fill); s.px = s.im.load()
    def r(s, x0, y0, x1, y1, c):
        for y in range(max(0, y0), min(15, y1) + 1):
            for x in range(max(0, x0), min(15, x1) + 1): s.px[x, y] = c
    def p(s, x, y, c): s.r(x, y, x, y, c)
    def save(s, kind, name):
        k = 3 if kind == 'items' else 2
        s.im.resize((16 * k, 16 * k), Image.NEAREST).save(os.path.join(tex, kind, name + '.png')); return s
def block_icon(tile_file, name):
    im = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); im.paste(Image.open(os.path.join(tex, 'tiles', tile_file + '.png')).convert('RGBA').crop((0, 0, 32, 32)), (8, 8))
    im.save(os.path.join(tex, 'items', name + '.png'))
def tint(src, name, colour, strength=1.0, only=None, other=None):
    """A base texture in another colour: each pixel takes the colour, as light as it was. `only` picks which pixels; the rest take `other`, or stay."""
    im = Image.open(os.path.join(base, 'textures', src)).convert('RGBA'); px = im.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, b, a = px[x, y]
            if not a: continue
            c = colour if (only is None or only(r, g, b)) else other
            if c is None: continue
            v = 0.3 + (r + g + b) / 765.0 * 1.15
            n = tuple(min(255, int(k * v)) for k in c[:3])
            px[x, y] = tuple(int(o * (1 - strength) + q * strength) for o, q in zip((r, g, b), n)) + (a,)
    im.save(os.path.join(tex, 'tiles', name + '.png'))
def greenish(r, g, b): return g > r and g > b

# ---- pictures of blocks that are made
def bricks(c, seed):
    rnd = random.Random(seed); t = P(dk(c, 0.55))
    for row in range(4):
        y = row * 4; off = 0 if row % 2 == 0 else 4
        for x0 in range(-8 + off, 16, 8):
            shade = lt(c, rnd.choice((0.9, 1.0, 1.0, 1.1)))
            t.r(x0, y, x0 + 6, y + 2, shade); t.r(x0, y, x0 + 6, y, lt(shade, 1.12)); t.r(x0, y + 2, x0 + 6, y + 2, dk(shade, 0.86))
    return t
def polished(c):
    t = P(c); t.r(0, 0, 15, 0, lt(c, 1.22)); t.r(0, 0, 0, 15, lt(c, 1.12)); t.r(0, 15, 15, 15, dk(c, 0.7)); t.r(15, 0, 15, 15, dk(c, 0.8))
    for i in range(3, 8): t.p(i, 10 - i, lt(c, 1.15)); t.p(i + 5, 14 - i, lt(c, 1.08))
    return t
def tiles_(c):
    t = P(dk(c, 0.6))
    for (x, y) in ((0, 0), (8, 0), (0, 8), (8, 8)):
        s = c if (x + y) % 16 == 0 else dk(c, 0.9)
        t.r(x, y, x + 6, y + 6, s); t.r(x, y, x + 6, y, lt(s, 1.2)); t.r(x, y, x, y + 6, lt(s, 1.1)); t.r(x + 6, y + 1, x + 6, y + 6, dk(s, 0.85))
    return t
def planks(c):
    t = P(c)
    for y in (3, 7, 11, 15): t.r(0, y, 15, y, dk(c, 0.62))
    for y in (0, 4, 8, 12): t.r(0, y, 15, y, lt(c, 1.14))
    for x, y in ((5, 1), (12, 5), (3, 9), (9, 13)): t.r(x, y, x, y + 1, dk(c, 0.75))
    return t

tiles = []; items = []; recipes = []; trees = []
def item(name, label, cat, price, **kw):
    d = {'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'category': cat, 'price': price}; d.update(kw); items.append(d)
def rec(bench, out, n, ins, time, level=1):
    d = {'bench': bench, 'output': {'item': out, 'count': n}, 'inputs': [{'item': a, 'count': b} for a, b in ins], 'time': time}
    if level > 1: d['level'] = level
    recipes.append(d)
def block(name, label, like, price, cat='blocks', **kw):
    """A plain block and the item that is it; `like` is the base block it behaves as."""
    d = copy.deepcopy(T[like]); d.pop('drops', None); d.update({'name': name, 'label': label, 'drop': name}); d.update(kw); tiles.append(d)
    f = d['texture'][0] if isinstance(d['texture'], list) else d['texture']
    block_icon(os.path.basename(f)[:-4], name)
    item(name, label, cat, price, places=name)

# ---- the ground of the new country
def ground(name, label, like, colour, price, strength=1.0, **kw):
    src = T[like]['texture']; files = src if isinstance(src, list) else [src]; files = files[:2]
    out = []
    for k, f in enumerate(files):
        n = name if len(files) == 1 else '%s_%d' % (name, k)
        tint(f, n, colour, strength); out.append('tiles/%s.png' % n)
    block(name, label, like, price, texture=out if len(out) > 1 else out[0], **kw)
def turf(name, label, top, under, price):
    tint('tiles/grass.png', name, top, 1.0, only=greenish, other=under)
    d = copy.deepcopy(T['grass']); d.update({'name': name, 'label': label, 'texture': 'tiles/%s.png' % name, 'drop': name}); tiles.append(d)
    block_icon(name, name); item(name, label, 'blocks', price, places=name)

STONES = [  # name, label, colour, price: each is found in the ground, and each has its bricks, its polished block, its tiles, its column and its stairs
 ('terracotta', 'Terracotta', (190, 104, 70, 255), 25), ('red_sandstone', 'Red sandstone', (204, 110, 60, 255), 20), ('rock_salt', 'Rock salt', (236, 214, 214, 255), 30),
 ('scoria', 'Scoria', (120, 56, 56, 255), 25), ('pumice', 'Pumice', (186, 180, 170, 255), 25), ('slate', 'Slate', (76, 88, 108, 255), 25),
 ('granite', 'Granite', (170, 124, 114, 255), 25), ('mossy_stone', 'Mossy stone', (104, 136, 96, 255), 25), ('glacial_ice', 'Glacial ice', (160, 208, 240, 255), 25),
]
ground('red_sand', 'Red sand', 'sand', (214, 122, 70, 255), 10)
ground('salt_crust', 'Salt crust', 'sand', (240, 236, 232, 255), 15)
ground('ash', 'Ash', 'sand', (120, 116, 120, 255), 10)
ground('packed_snow', 'Packed snow', 'sand', (238, 244, 250, 255), 10)
ground('permafrost', 'Permafrost', 'dirt', (140, 150, 166, 255), 10)
ground('mud', 'Mud', 'dirt', (104, 82, 66, 255), 10)
ground('peat', 'Peat', 'dirt', (66, 52, 44, 255), 15, burns=True)
turf('highland_grass', 'Highland grass', (150, 150, 110, 255), (116, 86, 60, 255), 10)
turf('dry_grass', 'Dry grass', (206, 180, 96, 255), (150, 96, 60, 255), 10)
turf('moss', 'Moss', (86, 150, 80, 255), (90, 96, 84, 255), 10)
for name, label, c, price in STONES:
    ground(name, label, 'stone', c, price, **({'light': 2} if name == 'glacial_ice' else {}))
    for kind, klabel, art, mult in (('bricks', '%s bricks', bricks(c, name), 1.2), ('polished', 'Polished %s', polished(c), 1.3), ('tiles', '%s tiles', tiles_(c), 1.5)):
        n = '%s_%s' % (name, kind) if kind != 'polished' else 'polished_' + name
        art.save('tiles', n)
        l = klabel % (label if kind == 'bricks' or kind == 'tiles' else label.lower())
        block(n, l, 'stone', int(price * mult / 5) * 5, cat='building', texture='tiles/%s.png' % n, shade=0)
    for part in ('column', 'stairs'):
        n = '%s_%s' % (name, part); d = copy.deepcopy(T['limestone_' + part]); d.update({'name': n, 'label': '%s %s' % (label, part), 'drop': n, 'texture': 'tiles/%s.png' % n}); tiles.append(d)
        tint('tiles/platinum_%s.png' % part, n, c); block_icon(n, n); item(n, '%s %s' % (label, part), 'building', int(price * 0.6 / 5) * 5 + 5, places=n)
    rec('builder_bench', name + '_bricks', 4, [(name, 4)], 40)
    rec('builder_bench', 'polished_' + name, 1, [(name, 1)], 20)
    rec('builder_bench', name + '_tiles', 4, [('polished_' + name, 4)], 40)
    rec('builder_bench', name + '_column', 2, [(name, 1)], 80)
    rec('builder_bench', name + '_stairs', 2, [(name, 1)], 80)
# mud bricks, and what else the ground gives
MUDB = (150, 116, 84, 255)
bricks(MUDB, 'mud').save('tiles', 'mud_bricks'); block('mud_bricks', 'Mud bricks', 'stone', 20, cat='building', texture='tiles/mud_bricks.png', shade=0, mine_time=0.5)
for part in ('column', 'stairs'):
    n = 'mud_brick_' + part; d = copy.deepcopy(T['limestone_' + part]); d.update({'name': n, 'label': 'Mud brick ' + part, 'drop': n, 'texture': 'tiles/%s.png' % n}); tiles.append(d)
    tint('tiles/platinum_%s.png' % part, n, MUDB); block_icon(n, n); item(n, 'Mud brick ' + part, 'building', 15, places=n)
rec('builder_bench', 'mud_bricks', 4, [('mud', 4), ('dry_grass', 1)], 40)
rec('builder_bench', 'mud_brick_column', 2, [('mud_bricks', 1)], 80); rec('builder_bench', 'mud_brick_stairs', 2, [('mud_bricks', 1)], 80)
rec('workbench', 'red_sandstone', 1, [('red_sand', 4)], 20); rec('workbench', 'salt', 4, [('rock_salt', 1)], 10); rec('workbench', 'salt', 1, [('salt_crust', 1)], 10)
rec('workbench', 'glacial_ice', 1, [('packed_snow', 4)], 20); rec('workbench', 'dirt', 1, [('mud', 1)], 5); rec('workbench', 'mossy_stone', 1, [('stone', 1), ('moss', 1)], 10)

# ---- trees: name, label, bark, leaves, the base tree whose shape it takes, what it grows in, its seed (name, label, food)
WOODS = [
 ('acacia', 'Acacia', (176, 110, 70, 255), (130, 160, 70, 255), 'mango', ['dry_grass', 'dirt', 'grass', 'red_sand'], ('acacia_seed', 'Acacia seed', None)),
 ('baobab', 'Baobab', (150, 130, 110, 255), (110, 150, 80, 255), 'oak', ['dry_grass', 'dirt', 'grass', 'red_sand'], ('baobab_fruit', 'Baobab fruit', 0.15)),
 ('birch', 'Birch', (226, 222, 206, 255), (140, 190, 90, 255), 'cherry', ['grass', 'dirt', 'compost', 'highland_grass'], ('birch_catkin', 'Birch catkin', None)),
 ('willow', 'Willow', (110, 100, 80, 255), (120, 160, 110, 255), 'oak', ['mud', 'peat', 'grass', 'dirt'], ('willow_catkin', 'Willow catkin', None)),
 ('bamboo', 'Bamboo', (140, 180, 80, 255), (110, 190, 90, 255), 'coconut', ['grass', 'dirt', 'mud', 'compost'], ('bamboo_shoot', 'Bamboo shoot', 0.1)),
 ('glowcap', 'Glowcap', (226, 214, 190, 255), (200, 80, 90, 255), 'apple', ['moss', 'dirt', 'grass', 'mud'], ('glowcap_spore', 'Glowcap spore', None)),
]
def seed_art(c):
    t = P(); t.r(6, 5, 9, 11, c); t.r(5, 7, 10, 9, c); t.r(7, 4, 8, 12, c); t.r(6, 6, 7, 7, lt(c, 1.3)); t.r(8, 2, 8, 3, dk(c, 0.6)); return t
for w, label, bark, leaf, like, soil, (seed, seed_label, food) in WOODS:
    tree = w + '_tree'
    logd = copy.deepcopy(T['apple_log']); logd.update({'name': w + '_log', 'label': label + (' stem' if w in ('bamboo', 'glowcap') else ' trunk'), 'drop': w + '_wood',
                                                         'texture': ['tiles/%s_log_%d.png' % (w, k) for k in (0, 1)]}); tiles.append(logd)
    lead = copy.deepcopy(T['apple_leaves']); lead.pop('texture_seasons', None)
    lead.update({'name': w + '_leaves', 'label': label + (' cap' if w == 'glowcap' else ' leaves'), 'texture': ['tiles/%s_leaves_%d.png' % (w, k) for k in (0, 1, 2)]})
    if w == 'glowcap': lead['light'] = 6
    tiles.append(lead)
    for k in (0, 1):
        if w == 'bamboo':
            t = P()
            for x in (2, 7, 12): t.r(x, 0, x + 2, 15, bark); t.r(x, 0, x, 15, lt(bark)); t.r(x + 2, 0, x + 2, 15, dk(bark)); t.r(x, 4 + 3 * k + (x % 3), x + 2, 4 + 3 * k + (x % 3), dk(bark, 0.6))
            t.save('tiles', '%s_log_%d' % (w, k))
        else: tint('tiles/apple_log_%d.png' % k, '%s_log_%d' % (w, k), bark)
    for k in (0, 1, 2):
        tint('tiles/apple_leaves_%d.png' % k, '%s_leaves_%d' % (w, k), leaf)
        if w == 'glowcap':
            im = Image.open(os.path.join(tex, 'tiles', '%s_leaves_%d.png' % (w, k))); px = im.load()
            for (x, y) in ((4 + 6 * k, 6), (20, 10 + 4 * k), (10, 22), (26, 26 - 6 * k)):
                for dx in range(4):
                    for dy in range(4):
                        if px[(x + dx) % 32, (y + dy) % 32][3]: px[(x + dx) % 32, (y + dy) % 32] = (250, 240, 220, 255)
            im.save(os.path.join(tex, 'tiles', '%s_leaves_%d.png' % (w, k)))
    d = copy.deepcopy(TR[like]); d.update({'name': tree, 'log': w + '_log', 'leaves': w + '_leaves', 'fruit': seed, 'soil': soil}); trees.append(d)
    seed_art(leaf if food is None else bark).save('items', seed)
    it = {'plants': tree}
    if food is not None: it['food'] = food
    item(seed, seed_label, 'food' if food is not None else 'materials', 15, **it)
    # its wood, as a block of planks, and what is built of that
    planks(bark).save('tiles', w + '_wood')
    block(w + '_wood', label + ' wood', 'wood', 20, texture='tiles/%s_wood.png' % w)
    items[-1]['fuel'] = 300
    for part in ('column', 'stairs'):
        n = '%s_%s' % (w, part); dd = copy.deepcopy(T['wood_' + part]); dd.update({'name': n, 'label': '%s %s' % (label, part), 'drop': n, 'texture': 'tiles/%s.png' % n}); tiles.append(dd)
        tint('tiles/platinum_%s.png' % part, n, bark); block_icon(n, n); item(n, '%s %s' % (label, part), 'building', 30, places=n)
        rec('builder_bench', n, 2, [(w + '_wood', 1)], 80)
    rec('workbench', 'wood', 1, [(w + '_wood', 1)], 5)

# ---- small things that grow wild: name, label, what it leaves, colour, what it grows in
def bush(c, tall=False, dots=None):
    t = P()
    for x in (4, 7, 10, 12) if tall else (5, 8, 11):
        top = (3 if tall else 8) + (x % 3)
        t.r(x, top, x, 15, c); t.p(x - 1, top + 2, lt(c)); t.p(x + 1, top + 4, dk(c))
    if dots:
        for x, y in ((5, 8), (8, 7), (11, 9), (7, 11)): t.p(x, y, dots)
    return t
for name, label, c, tall, dots, soil in (('reed', 'Reeds', (150, 170, 90, 255), True, (120, 80, 50, 255), ['mud', 'peat', 'grass', 'dirt', 'sand']),
                                         ('fern', 'Fern', (70, 150, 80, 255), False, None, ['moss', 'grass', 'dirt', 'mud']),
                                         ('heather', 'Heather', (110, 130, 90, 255), False, (190, 110, 180, 255), ['highland_grass', 'grass', 'dirt']),
                                         ('dead_bush', 'Dead bush', (130, 100, 70, 255), False, None, ['red_sand', 'salt_crust', 'ash', 'sand', 'dry_grass'])):
    bush(c, tall, dots).save('tiles', name)
    drop = 'stick' if name == 'dead_bush' else name
    tiles.append({'name': name, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'texture': 'tiles/%s.png' % name, 'mine_time': 0.1, 'tool': 'machete',
                  'drops': [{'item': drop, 'count': 1}], 'soil': soil})
    if drop == name:
        bush(c, tall, dots).save('items', name); item(name, label, 'materials', 10, sows=name)
rec('workbench', 'stick', 2, [('reed', 2)], 5)

# ---- the new country itself: where it lies by heat and dryness, what it is made of, and what grows there
# (A biome's own rock is its "soil": a block of it at the edge of the country, deepening to the full depth inside. The engine's
# "under" layer only comes in at the very heart of a biome, so it is kept for the rarer rock.)
biomes = [
 {'name': 'red_mesa', 'label': 'Red mesa', 'heat': [0.5, 1], 'dryness': [0.05, 0.5], 'surface': 'red_sand', 'soil': 'terracotta', 'soil_depth': 9,
  'under': {'tile': 'red_sandstone', 'depth': 8}, 'tree_one_in': 30, 'trees': [{'tree': 'baobab_tree', 'weight': 1}],
  'plants': [{'tile': 'dead_bush', 'one_in': 14}]},
 {'name': 'salt_flats', 'label': 'Salt flats', 'heat': [0.1, 1], 'dryness': [0.78, 1], 'surface': 'salt_crust', 'soil': 'rock_salt', 'soil_depth': 7,
  'plants': [{'tile': 'dead_bush', 'one_in': 40}]},
 {'name': 'volcanic_wastes', 'label': 'Volcanic wastes', 'heat': [0.3, 1], 'dryness': [0.5, 0.78], 'surface': 'ash', 'soil': 'scoria', 'soil_depth': 9,
  'under': {'tile': 'pumice', 'depth': 10}, 'plants': [{'tile': 'dead_bush', 'one_in': 30}]},
 {'name': 'frozen_tundra', 'label': 'Frozen tundra', 'heat': [-1, -0.72], 'dryness': [0.1, 1], 'surface': 'packed_snow', 'soil': 'glacial_ice', 'soil_depth': 7,
  'under': {'tile': 'permafrost', 'depth': 8}},
 {'name': 'slate_highlands', 'label': 'Slate highlands', 'heat': [-0.72, -0.4], 'dryness': [0.55, 1], 'surface': 'highland_grass', 'soil': 'slate', 'soil_depth': 9,
  'tree_one_in': 18, 'trees': [{'tree': 'birch_tree', 'weight': 1}],
  'plants': [{'tile': 'heather', 'one_in': 6}, {'tile': 'fern', 'one_in': 30}]},
 {'name': 'misty_marsh', 'label': 'Misty marsh', 'heat': [-1, -0.4], 'dryness': [-1, -0.62], 'surface': 'mud', 'soil': 'peat', 'soil_depth': 4, 'tree_one_in': 8,
  'trees': [{'tree': 'willow_tree', 'weight': 1}], 'plants': [{'tile': 'reed', 'one_in': 5}, {'tile': 'fern', 'one_in': 20}]},
 {'name': 'savanna', 'label': 'Savanna', 'heat': [0.55, 1], 'dryness': [-0.45, 0.05], 'surface': 'dry_grass', 'soil': 'dirt', 'tree_one_in': 14, 'tree_spacing': 5,
  'trees': [{'tree': 'acacia_tree', 'weight': 4}, {'tree': 'baobab_tree', 'weight': 1}], 'plants': [{'tile': 'dead_bush', 'one_in': 26}]},
 {'name': 'birch_woods', 'label': 'Birch woods', 'heat': [-0.4, 0.1], 'dryness': [0.55, 1], 'surface': 'grass', 'soil': 'dirt', 'under': {'tile': 'granite', 'depth': 12},
  'tree_one_in': 5, 'tree_spacing': 3, 'trees': [{'tree': 'birch_tree', 'weight': 1}], 'plants': [{'tile': 'fern', 'one_in': 14}, {'tile': 'heather', 'one_in': 30}]},
 {'name': 'mossy_hollow', 'label': 'Mossy hollow', 'heat': [-0.4, 0.1], 'dryness': [-0.6, -0.35], 'surface': 'moss', 'soil': 'dirt', 'under': {'tile': 'mossy_stone', 'depth': 10},
  'tree_one_in': 6, 'tree_spacing': 3, 'trees': [{'tree': 'glowcap_tree', 'weight': 3}, {'tree': 'oak', 'weight': 1}], 'plants': [{'tile': 'fern', 'one_in': 5}]},
 {'name': 'bamboo_grove', 'label': 'Bamboo grove', 'heat': [0.1, 0.55], 'dryness': [-1, -0.45], 'surface': 'grass', 'soil': 'dirt', 'tree_one_in': 3, 'tree_spacing': 2,
  'trees': [{'tree': 'bamboo_tree', 'weight': 1}], 'plants': [{'tile': 'fern', 'one_in': 16}, {'tile': 'reed', 'one_in': 26}]},
]
# The rocks that lie under a country only at its heart are in the rock everywhere too, in pockets: so that they can be found.
veins = [
 {'name': 'granite_pocket', 'tile': 'granite', 'in': ['stone'], 'size': [20, 56], 'per_100_columns': 2.5, 'depth': [6, 140]},
 {'name': 'mossy_pocket', 'tile': 'mossy_stone', 'in': ['stone'], 'size': [10, 30], 'per_100_columns': 2.0, 'depth': [2, 40]},
 {'name': 'pumice_pocket', 'tile': 'pumice', 'in': ['stone'], 'size': [10, 30], 'per_100_columns': 1.5, 'depth': [90, 190]},
 {'name': 'red_sandstone_pocket', 'tile': 'red_sandstone', 'in': ['sandstone'], 'size': [8, 24], 'per_100_columns': 2.0, 'depth': [2, 30]},
]
rec('workbench', 'permafrost', 2, [('dirt', 1), ('packed_snow', 1)], 10)
rec('workbench', 'pumice', 2, [('scoria', 1), ('ash', 1)], 10)
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        if isinstance(obj, dict) and len(obj) == 1 and isinstance(list(obj.values())[0], list):
            k, v = list(obj.items())[0]
            f.write('{\n  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]\n}\n')
        else: json.dump(obj, f, indent=1); f.write('\n')
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes}); dump('trees.json', {'trees': trees})
dump('worldgen.json', {'biomes': biomes, 'veins': veins}); dump('mod.json', {'label': "Explorer's Delight", 'icon': 'icon.png'})
t = P((120, 180, 230, 255)); t.r(0, 11, 15, 15, (214, 122, 70, 255)); t.r(2, 7, 6, 10, (190, 104, 70, 255)); t.r(3, 5, 5, 6, (190, 104, 70, 255)); t.r(9, 8, 14, 10, (76, 88, 108, 255)); t.r(10, 5, 12, 7, (238, 244, 250, 255)); t.r(11, 4, 11, 4, (238, 244, 250, 255))
t.im.resize((64, 64), Image.NEAREST).save(os.path.join(mod, 'icon.png'))
print(len(tiles), 'tiles', len(items), 'items', len(recipes), 'recipes', len(trees), 'trees', len(biomes), 'biomes')
