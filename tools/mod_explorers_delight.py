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
 # (under the sea, and in the caves)
 ('coral_stone', 'Coral stone', (236, 150, 150, 255), 25), ('dripstone', 'Dripstone', (160, 130, 104, 255), 25), ('geode', 'Geode', (96, 70, 130, 255), 35),
 ('magma_rock', 'Magma rock', (150, 60, 40, 255), 35),
]
ground('red_sand', 'Red sand', 'sand', (214, 122, 70, 255), 10)
ground('salt_crust', 'Salt crust', 'sand', (240, 236, 232, 255), 15)
ground('ash', 'Ash', 'sand', (120, 116, 120, 255), 10)
ground('packed_snow', 'Packed snow', 'sand', (238, 244, 250, 255), 10)
ground('permafrost', 'Permafrost', 'dirt', (140, 150, 166, 255), 10)
ground('mud', 'Mud', 'dirt', (104, 82, 66, 255), 10)
ground('peat', 'Peat', 'dirt', (66, 52, 44, 255), 15, burns=True)
ground('coral_sand', 'Coral sand', 'sand', (250, 214, 200, 255), 10)
ground('sea_gravel', 'Sea gravel', 'sand', (130, 140, 150, 255), 10)
ground('mycelium', 'Mycelium', 'dirt', (140, 110, 160, 255), 15)
turf('highland_grass', 'Highland grass', (150, 150, 110, 255), (116, 86, 60, 255), 10)
turf('dry_grass', 'Dry grass', (206, 180, 96, 255), (150, 96, 60, 255), 10)
turf('moss', 'Moss', (86, 150, 80, 255), (90, 96, 84, 255), 10)
for name, label, c, price in STONES:
    ground(name, label, 'stone', c, price, **({'light': 2} if name == 'glacial_ice' else {'light': 4} if name == 'magma_rock' else {}))
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
rec('builder_bench', 'red_sandstone', 1, [('red_sand', 4)], 20); rec('mixing_bench', 'salt', 4, [('rock_salt', 1)], 10); rec('mixing_bench', 'salt', 1, [('salt_crust', 1)], 10)
rec('builder_bench', 'glacial_ice', 1, [('packed_snow', 4)], 20); rec('compost_bin', 'dirt', 1, [('mud', 1)], 5); rec('builder_bench', 'mossy_stone', 1, [('stone', 1), ('moss', 1)], 10)

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
    rec('woodwork_bench', 'wood', 1, [(w + '_wood', 1)], 5)

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
rec('woodwork_bench', 'stick', 2, [('reed', 2)], 5)

# ---- under the sea, and in the caves: what grows there, and what is made of it
def small(name, label, art, drop, like='emerald', **kw):
    """Something small that stands in the background: it leaves `drop` when it is taken."""
    art.save('tiles', name)
    d = {'name': name, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.15, 'drop': drop, 'texture': 'tiles/%s.png' % name, 'tool': 'pickaxe'}
    d.update(kw); tiles.append(d)
def thing(name, label, art, cat, price, **kw):
    art.save('items', name); item(name, label, cat, price, **kw)
SEABED = ['sand', 'coral_sand', 'sea_gravel']
def brain(c):
    t = P(); t.r(4, 8, 11, 14, c); t.r(3, 10, 12, 13, c); t.r(5, 7, 10, 7, lt(c)); t.r(5, 9, 6, 9, dk(c)); t.r(8, 10, 10, 10, dk(c)); t.r(4, 12, 6, 12, dk(c)); t.r(9, 13, 11, 13, dk(c)); t.r(6, 15, 9, 15, dk(c, 0.6)); return t
def fan(c):
    t = P(); t.r(7, 9, 8, 15, dk(c))
    for x, y in ((4, 4), (6, 3), (9, 2), (11, 4), (3, 7), (12, 7), (5, 6), (10, 6), (7, 5), (8, 7)): t.r(x, y, x, y + 3, c); t.p(x, y, lt(c))
    t.r(4, 8, 11, 8, c); return t
def tube(c):
    t = P()
    for x, top in ((4, 8), (7, 4), (10, 6), (12, 10)): t.r(x, top, x + 1, 15, c); t.r(x, top, x + 1, top, lt(c, 1.4)); t.r(x + 1, top + 1, x + 1, 15, dk(c))
    return t
for name, label, c, art in (('red_coral', 'Red coral', (226, 80, 90, 255), brain), ('blue_coral', 'Blue coral', (80, 130, 230, 255), fan), ('yellow_coral', 'Yellow coral', (240, 200, 70, 255), tube)):
    small(name, label, art(c), name, tool='machete', needs_water=True, soil=SEABED, light=3)
    thing(name, label, art(c), 'materials', 20, sows=name)
    b = P(c); rnd = random.Random(name)
    for _ in range(26): b.p(rnd.randrange(16), rnd.randrange(16), rnd.choice((lt(c, 1.25), dk(c, 0.8), dk(c, 0.65))))
    b.r(0, 0, 15, 0, lt(c, 1.15)); b.r(0, 15, 15, 15, dk(c, 0.7))
    b.save('tiles', name + '_block'); block(name + '_block', label + ' block', 'stone', 90, cat='building', texture='tiles/%s_block.png' % name, shade=0, mine_time=0.5)
    rec('builder_bench', name + '_block', 1, [(name, 4)], 30)
g = P()
for x, top in ((3, 9), (5, 5), (8, 7), (10, 3), (12, 8)): g.r(x, top, x, 15, (70, 160, 110, 255)); g.p(x, top, (130, 210, 150, 255))
small('sea_grass', 'Sea grass', g, 'sea_grass', tool='machete', needs_water=True, soil=SEABED); thing('sea_grass', 'Sea grass', g, 'materials', 10, sows='sea_grass')
c = P(); c.r(3, 10, 12, 14, (200, 190, 210, 255)); c.r(4, 8, 11, 9, (226, 216, 232, 255)); c.r(3, 12, 12, 12, (150, 140, 160, 255)); c.r(7, 11, 8, 11, (255, 255, 255, 255)); c.r(5, 15, 10, 15, (150, 140, 160, 255))
small('pearl_clam', 'Pearl clam', c, 'pearl', tool='machete', needs_water=True, soil=SEABED)
pl = P(); pl.r(5, 5, 10, 10, (240, 240, 250, 255)); pl.r(6, 4, 9, 11, (240, 240, 250, 255)); pl.r(4, 6, 11, 9, (240, 240, 250, 255)); pl.r(6, 6, 7, 7, (255, 255, 255, 255)); pl.r(9, 9, 10, 10, (200, 200, 226, 255))
thing('pearl', 'Pearl', pl, 'gems', 250)
polished((236, 236, 248, 255)).save('tiles', 'pearl_block'); block('pearl_block', 'Pearl block', 'stone', 1100, cat='building', texture='tiles/pearl_block.png', shade=0)
rec('builder_bench', 'pearl_block', 1, [('pearl', 4)], 60)
# in the caves
def spike(c, down, n=3):
    t = P()
    for i, x in enumerate((4, 8, 11)[:n]):
        h = (9, 13, 7)[i]
        for k in range(h):
            w2 = 1 if k > h * 0.55 else 2
            y = k if down else 15 - k
            t.r(x, y, x + w2 - 1, y, c if k % 4 else dk(c, 0.85))
        t.p(x, 0 if down else 15, lt(c))
    return t
VINE = copy.deepcopy(T['vine'])
def hanging(name, label, art, drop, **kw):
    art.save('tiles', name); d = copy.deepcopy(VINE); d.update({'name': name, 'label': label, 'drop': drop, 'texture': 'tiles/%s.png' % name}); d.update(kw); tiles.append(d)
gv = P()
for x in (5, 9, 11): gv.r(x, 0, x, 15, (70, 150, 80, 255)); gv.p(x + 1, 3 + x % 5, (110, 190, 100, 255)); gv.p(x - 1, 9 + x % 4, (110, 190, 100, 255))
for x, y in ((4, 5), (10, 8), (8, 13), (12, 2)): gv.r(x, y, x + 1, y + 1, (250, 220, 90, 255))
hanging('glow_vine', 'Glow vine', gv, 'glow_berries', light=6)
gb = P(); gb.r(5, 7, 8, 10, (250, 220, 90, 255)); gb.r(8, 9, 11, 12, (244, 190, 70, 255)); gb.p(5, 7, (255, 250, 200, 255)); gb.r(7, 4, 7, 6, (70, 150, 80, 255)); gb.r(8, 3, 10, 4, (110, 190, 100, 255))
thing('glow_berries', 'Glow berries', gb, 'food', 20, food=0.08)
DRIP = (160, 130, 104, 255); ICE = (190, 226, 248, 255)
hanging('stalactite', 'Stalactite', spike(DRIP, True), 'dripstone', climbable=False, tool='pickaxe')
small('stalagmite', 'Stalagmite', spike(DRIP, False), 'dripstone')
hanging('icicle', 'Icicle', spike(ICE, True), 'packed_snow', climbable=False, tool='pickaxe')
sh = P(); sh.r(7, 10, 8, 15, (226, 226, 240, 255)); sh.r(4, 6, 11, 9, (90, 210, 220, 255)); sh.r(5, 5, 10, 5, (150, 240, 240, 255)); sh.r(3, 8, 12, 9, (60, 170, 190, 255)); sh.p(6, 7, (255, 255, 255, 255)); sh.p(9, 6, (255, 255, 255, 255))
small('glowshroom', 'Glowshroom', sh, 'glowshroom', tool='machete', light=8, soil=['mycelium', 'moss', 'dirt', 'mud', 'stone'])
thing('glowshroom', 'Glowshroom', sh, 'food', 25, food=0.1, sows='glowshroom')
cr = P()
for x, top, c in ((3, 8, (120, 220, 240, 255)), (6, 3, (200, 130, 240, 255)), (9, 6, (120, 220, 240, 255)), (12, 10, (200, 130, 240, 255))): cr.r(x, top, x + 1, 15, c); cr.p(x, top, (255, 255, 255, 255)); cr.r(x + 1, top + 1, x + 1, 15, dk(c))
small('crystal_cluster', 'Crystal cluster', cr, 'prism_shard', light=10)
ps = P(); ps.r(6, 3, 9, 12, (150, 230, 240, 255)); ps.r(7, 2, 8, 13, (150, 230, 240, 255)); ps.r(6, 4, 6, 11, (255, 255, 255, 255)); ps.r(9, 5, 9, 12, (110, 170, 220, 255))
thing('prism_shard', 'Prism shard', ps, 'gems', 120)
tiles_((150, 230, 240, 255)).save('tiles', 'prism_block'); block('prism_block', 'Prism block', 'stone', 560, cat='building', texture='tiles/prism_block.png', shade=0, light=12)
rec('builder_bench', 'prism_block', 1, [('prism_shard', 4)], 60)
eb = P(); eb.r(7, 10, 8, 15, (90, 40, 30, 255)); eb.r(5, 5, 10, 9, (240, 130, 40, 255)); eb.r(6, 4, 9, 10, (240, 130, 40, 255)); eb.r(7, 6, 8, 8, (255, 230, 120, 255)); eb.p(4, 7, (220, 80, 30, 255)); eb.p(11, 7, (220, 80, 30, 255))
small('ember_bloom', 'Ember bloom', eb, 'ember', tool='machete', light=11)
rec('compost_bin', 'dirt', 1, [('mycelium', 1)], 5)

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
# Three kinds of sea, by how warm the water is: where none of them is, the sea is the one the game has.
biomes += [
 {'name': 'coral_reef', 'label': 'Coral reef', 'below_sea': True, 'heat': [0.15, 1], 'dryness': [-1, 1], 'surface': 'coral_sand', 'soil': 'coral_stone', 'soil_depth': 6,
  'tree_one_in': 16, 'tree_spacing': 3, 'trees': [{'tree': 'kelp', 'weight': 1}],
  'plants': [{'tile': 'red_coral', 'one_in': 7}, {'tile': 'blue_coral', 'one_in': 8}, {'tile': 'yellow_coral', 'one_in': 8}, {'tile': 'sea_grass', 'one_in': 10}, {'tile': 'pearl_clam', 'one_in': 60}]},
 {'name': 'frozen_sea', 'label': 'Frozen sea', 'below_sea': True, 'heat': [-1, -0.45], 'dryness': [-1, 1], 'surface': 'sea_gravel', 'soil': 'glacial_ice', 'soil_depth': 5,
  'tree_one_in': 24, 'tree_spacing': 3, 'trees': [{'tree': 'kelp', 'weight': 1}], 'plants': [{'tile': 'sea_grass', 'one_in': 14}]},
 {'name': 'seagrass_meadow', 'label': 'Seagrass meadow', 'below_sea': True, 'heat': [-0.45, 0.15], 'dryness': [-1, 0.1], 'surface': 'sand', 'soil': 'sand', 'soil_depth': 3,
  'tree_one_in': 12, 'tree_spacing': 3, 'trees': [{'tree': 'kelp', 'weight': 1}],
  'plants': [{'tile': 'sea_grass', 'one_in': 2}, {'tile': 'pearl_clam', 'one_in': 25}, {'tile': 'coral_0', 'one_in': 20}]},
]
# Six kinds of cave, by how deep: stretches of the underground with their own rock round them and their own things growing.
cave_biomes = [
 {'name': 'ice_caves', 'label': 'Ice caves', 'depth': [3, 50], 'share': 0.14, 'wavelength': 220, 'lining': 'glacial_ice', 'roof': [{'tile': 'icicle', 'one_in': 3, 'hang': [1, 3]}]},
 {'name': 'lush_caves', 'label': 'Lush caves', 'depth': [3, 70], 'share': 0.22, 'wavelength': 180, 'lining': 'moss',
  'floor': [{'tile': 'fern', 'one_in': 4}, {'tile': 'glowshroom', 'one_in': 20}], 'roof': [{'tile': 'glow_vine', 'one_in': 4, 'hang': [1, 4]}]},
 {'name': 'dripstone_caves', 'label': 'Dripstone caves', 'depth': [20, 115], 'share': 0.2, 'wavelength': 160, 'lining': 'dripstone',
  'floor': [{'tile': 'stalagmite', 'one_in': 4}], 'roof': [{'tile': 'stalactite', 'one_in': 3, 'hang': [1, 3]}]},
 {'name': 'mushroom_caves', 'label': 'Mushroom caves', 'depth': [30, 135], 'share': 0.18, 'wavelength': 200, 'lining': 'mycelium',
  'floor': [{'tile': 'glowshroom', 'one_in': 3}], 'roof': [{'tile': 'glow_vine', 'one_in': 14, 'hang': [1, 2]}]},
 {'name': 'crystal_caverns', 'label': 'Crystal caverns', 'depth': [70, 170], 'share': 0.16, 'wavelength': 140, 'lining': 'geode',
  'floor': [{'tile': 'crystal_cluster', 'one_in': 5}]},
 {'name': 'magma_caves', 'label': 'Magma caves', 'depth': [125, 198], 'share': 0.25, 'wavelength': 180, 'lining': 'magma_rock',
  'floor': [{'tile': 'ember_bloom', 'one_in': 6}]},
]
# The rocks that lie under a country only at its heart are in the rock everywhere too, in pockets: so that they can be found.
veins = [
 {'name': 'granite_pocket', 'tile': 'granite', 'in': ['stone'], 'size': [20, 56], 'per_100_columns': 2.5, 'depth': [6, 140]},
 {'name': 'mossy_pocket', 'tile': 'mossy_stone', 'in': ['stone'], 'size': [10, 30], 'per_100_columns': 2.0, 'depth': [2, 40]},
 {'name': 'pumice_pocket', 'tile': 'pumice', 'in': ['stone'], 'size': [10, 30], 'per_100_columns': 1.5, 'depth': [90, 190]},
 {'name': 'red_sandstone_pocket', 'tile': 'red_sandstone', 'in': ['sandstone'], 'size': [8, 24], 'per_100_columns': 2.0, 'depth': [2, 30]},
]
rec('builder_bench', 'permafrost', 2, [('dirt', 1), ('packed_snow', 1)], 10)
rec('builder_bench', 'pumice', 2, [('scoria', 1), ('ash', 1)], 10)
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        if isinstance(obj, dict) and len(obj) == 1 and isinstance(list(obj.values())[0], list):
            k, v = list(obj.items())[0]
            f.write('{\n  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]\n}\n')
        else: json.dump(obj, f, indent=1); f.write('\n')
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes}); dump('trees.json', {'trees': trees})
dump('worldgen.json', {'biomes': biomes, 'veins': veins, 'cave_biomes': cave_biomes}); dump('mod.json', {'label': "Explorer's Delight", 'icon': 'icon.png'})
t = P((120, 180, 230, 255)); t.r(0, 11, 15, 15, (214, 122, 70, 255)); t.r(2, 7, 6, 10, (190, 104, 70, 255)); t.r(3, 5, 5, 6, (190, 104, 70, 255)); t.r(9, 8, 14, 10, (76, 88, 108, 255)); t.r(10, 5, 12, 7, (238, 244, 250, 255)); t.r(11, 4, 11, 4, (238, 244, 250, 255))
t.im.resize((64, 64), Image.NEAREST).save(os.path.join(mod, 'icon.png'))
print(len(tiles), 'tiles', len(items), 'items', len(recipes), 'recipes', len(trees), 'trees', len(biomes), 'biomes', len(cave_biomes), 'cave biomes')
