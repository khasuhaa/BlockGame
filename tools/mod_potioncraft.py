#!/usr/bin/env python3
"""Writes the Potioncraft mod: its data files and its textures.

    python3 tools/mod_potioncraft.py .

Potions for every stage of the game: herbs and fungi to gather and grow, a mortar, a crock, a still and a
cauldron in four tiers, and what they make - draughts that do something for a while (in three strengths),
tonics that do something at once, flasks to throw, and elixirs that never wear off. The plan and what the
owner decided are in ModDescriptions/Alchemy.txt.

Herbs are the game's own crops under new names and colours; everything else is drawn here.

Either edit this and run it again, or edit the mod's files by hand and stop using this: not both.
"""
import colorsys, copy, json, os, subprocess, sys
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
DATA = os.path.join(repo, 'data')
mod = os.path.join(repo, 'mods/potioncraft'); tex = os.path.join(mod, 'textures')
for d in ('tiles', 'items', 'mobs'): os.makedirs(os.path.join(tex, d), exist_ok=True)
BT = {t['name']: t for t in json.load(open(os.path.join(DATA, 'tiles.json')))['tiles']}
BI = {t['name']: t for t in json.load(open(os.path.join(DATA, 'items.json')))['items']}

INK = (22, 22, 30, 255)
def c(s): return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
def dk(col, f=0.72): return tuple(int(v * f) for v in col[:3]) + (255,)
def lt(col, f=1.28): return tuple(min(255, int(v * f + 8)) for v in col[:3]) + (255,)
GLASS, CORK, BRASS, IRON, WOOD, GOLD = c('bfe2ee'), c('b08850'), c('c8a040'), c('7a7e8a'), c('8c6537'), c('f0c840')

class P:
    def __init__(s): s.im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); s.px = s.im.load()
    def r(s, x0, y0, x1, y1, col):
        for y in range(max(0, y0), min(15, y1) + 1):
            for x in range(max(0, x0), min(15, x1) + 1): s.px[x, y] = col
        return s
    def p(s, x, y, col): return s.r(x, y, x, y, col)
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
def icon_of(name):
    t = P(); t.im = Image.open(os.path.join(tex, 'tiles', name + '.png')).convert('RGBA').resize((16, 16), Image.NEAREST); t.px = t.im.load(); t.outline().save('items', name)

def tint(src, dst, col):
    """One of the game's pictures in another colour: every pixel half way to that colour at its own lightness."""
    im = Image.open(src).convert('RGBA'); px = im.load()
    h, s, _ = colorsys.rgb_to_hsv(col[0] / 255, col[1] / 255, col[2] / 255)
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            p = px[x, y]
            if p[3] == 0 or sum(p[:3]) < 90: continue
            v = max(p[:3]) / 255
            r, g, b = colorsys.hsv_to_rgb(h, s, min(1.0, v * 1.05))
            px[x, y] = (int((p[0] + r * 255 * 2) / 3), int((p[1] + g * 255 * 2) / 3), int((p[2] + b * 255 * 2) / 3), p[3])
    os.makedirs(os.path.dirname(dst), exist_ok=True); im.save(dst)

tiles, items, recipes, mobs, veins, decor, plants = [], [], [], [], [], [], []
def tile(name, label, solid=False, **kw):
    d = {'name': name, 'label': label, 'solid': solid, 'removable': True, 'mine_time': 0.3, 'drop': name, 'texture': 'tiles/%s.png' % name, 'tool': 'pickaxe'}
    if not solid: d['layer'] = 'background'
    d.update(kw); tiles.append({k: v for k, v in d.items() if v is not None}); return d
def item(name, label, cat, price, tier, **kw):
    d = {'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'category': cat, 'price': price}
    if cat != 'benches': d['tier'] = tier
    d.update(kw); items.append({k: v for k, v in d.items() if v is not None}); return d
def rec(bench, out, count, ins, time=60, level=None, **kw):
    r = {'bench': bench, 'output': {'item': out, 'count': count}, 'inputs': [{'item': i, 'count': n} for i, n in ins], 'time': time}
    if level and level > 1: r['level'] = level
    r.update(kw); recipes.append(r)

# ---- bottles ---------------------------------------------------------------------------------------------------
def bottle(liquid, shape='round', stopper=CORK, trim=None):
    """A bottle with something in it. round: a draught. vial: a tonic. bulb: a flask to throw. urn: an elixir."""
    t = P()
    if shape == 'round':
        t.r(7, 2, 8, 3, stopper); t.r(7, 4, 8, 5, GLASS); t.r(5, 6, 10, 13, GLASS); t.r(4, 8, 11, 12, GLASS)
        t.r(5, 9, 10, 13, liquid); t.r(4, 9, 11, 12, liquid); t.r(5, 9, 10, 9, lt(liquid)); t.r(5, 13, 10, 13, dk(liquid)); t.p(6, 7, c('ffffff')); t.p(6, 11, lt(liquid))
    elif shape == 'vial':
        t.r(7, 1, 8, 2, stopper); t.r(6, 3, 9, 13, GLASS); t.r(6, 7, 9, 13, liquid); t.r(6, 7, 9, 7, lt(liquid)); t.r(6, 13, 9, 13, dk(liquid)); t.p(7, 4, c('ffffff')); t.r(5, 3, 10, 3, GLASS)
    elif shape == 'bulb':
        t.r(7, 1, 8, 4, stopper); t.r(4, 5, 11, 13, GLASS); t.r(3, 7, 12, 11, GLASS); t.r(4, 6, 11, 13, liquid); t.r(3, 7, 12, 11, liquid)
        t.r(5, 6, 10, 6, lt(liquid)); t.r(5, 13, 10, 13, dk(liquid)); t.p(5, 8, c('ffffff')); t.p(6, 7, c('ffffff')); t.r(9, 0, 10, 1, c('e8dcc0')); t.p(11, 0, c('f08030'))
    else:
        t.r(6, 1, 9, 2, trim or GOLD); t.r(7, 3, 8, 4, GLASS); t.r(4, 5, 11, 13, GLASS); t.r(5, 14, 10, 14, trim or GOLD); t.r(5, 7, 10, 13, liquid); t.r(4, 8, 11, 12, liquid)
        t.r(5, 7, 10, 7, lt(liquid)); t.r(4, 6, 11, 6, trim or GOLD); t.p(6, 9, c('ffffff')); t.p(9, 11, lt(liquid)); t.p(7, 10, c('ffffff'))
    return t.outline()
bottle(GLASS).save('items', 'glass_bottle')
item('glass_bottle', 'Glass bottle', 'crafted', 30, 2); rec('kiln', 'glass_bottle', 3, [('glass', 1)], 40)

# ---- herbs: the game's crops in other colours, each wild somewhere -------------------------------------------------
def chain(start):
    seen, todo = [], [start]
    while todo:
        n = todo.pop()
        if n in seen or n not in BT: continue
        seen.append(n); todo += [BT[n].get(k) for k in ('grows_into', 'grows_top', 'topped_by', 'cut_to') if BT[n].get(k)]
    return seen
# name, label, the crop it grows like, colour, where it is wild (and how thinly)
HERBS = [('feverfew', 'Feverfew', 'flax', 'f4f0d0', 'plains', 40), ('bloodroot', 'Bloodroot', 'carrot', 'c02838', 'cherry_grove', 40), ('marshmallow', 'Marshmallow', 'flax', 'f0c8d8', 'beach', 30),
         ('nightshade', 'Nightshade', 'tomato', '5a3a9a', 'maple_forest', 45), ('sunwort', 'Sunwort', 'sunflower', 'f0b020', 'desert', 30), ('frostcap', 'Frostcap', 'carrot', '90d8f0', 'pine_forest', 35),
         ('kingsfoil', 'Kingsfoil', 'wheat', '4aa860', 'plains', 55), ('mandrake', 'Mandrake', 'carrot', '8a6a4a', 'orchard', 70), ('dreamleaf', 'Dreamleaf', 'flax', '8a8af0', 'orchard', 40),
         ('firethorn', 'Firethorn', 'chilli', 'f05020', 'desert', 35), ('sea_lavender', 'Sea lavender', 'flax', 'a070d0', 'beach', 35), ('wolfsbane', 'Wolfsbane', 'tomato', '3a5ac8', 'pine_forest', 45),
         ('ghost_orchid', 'Ghost orchid', 'sunflower', 'e8f0f8', 'tropics', 70), ('goldenseal', 'Goldenseal', 'corn', 'e0c040', 'maple_forest', 50), ('bitterwort', 'Bitterwort', 'wheat', '7a8a3a', 'tropics', 40)]
for n, lab, base, col, biome, one_in in HERBS:
    k = c(col)
    for t in chain(base + '_0'):
        d = copy.deepcopy(BT[t]); new = t.replace(base, n)
        tint(os.path.join(DATA, 'textures', d['texture']), os.path.join(tex, 'tiles', new + '.png'), k)
        d.update(name=new, label=lab, texture='tiles/%s.png' % new)
        for key in ('grows_into', 'grows_top', 'topped_by', 'cut_to'):
            if key in d: d[key] = d[key].replace(base, n)
        d.pop('drops', None); d['drop'] = n
        if BT[t].get('drops'): d['drops'] = [{'item': n, 'count': dr.get('count', 1)} for dr in BT[t]['drops'][:1]]; d.pop('drop')
        tiles.append(d)
    src = base if base in BI else 'wheat'
    tint(os.path.join(DATA, 'textures', BI[src]['texture']), os.path.join(tex, 'items', n + '.png'), k)
    item(n, lab, 'nature', 60, 2, sows=n + '_0')
    plants.append({'biome': biome, 'tile': n + '_0', 'one_in': one_in})
# (A world made before this was on has none growing: they can be had from seed, which is ground from any crop.)
rec('herb_mortar', 'feverfew', 1, [('wheat', 2), ('flax', 1)], 60, output={'one_of': [{'item': n, 'weight': 4 if one_in < 50 else 2} for n, _, _, _, _, one_in in HERBS], 'count': 1})
rec('herb_mortar', 'feverfew', 1, [('carrot', 2), ('tulip_seed', 1)], 60, output={'one_of': [{'item': n, 'weight': 4 if one_in < 50 else 2} for n, _, _, _, _, one_in in HERBS], 'count': 1})

# ---- fungi and minerals ------------------------------------------------------------------------------------------
def cap(col, stem=c('e8dcc0'), tall=False):
    t = P(); t.r(7, 9 if not tall else 7, 8, 14, stem); t.r(4, 6 if not tall else 4, 11, 8 if not tall else 6, col); t.r(5, 5 if not tall else 3, 10, 5 if not tall else 3, col)
    t.r(5, 5 if not tall else 3, 10, 5 if not tall else 3, lt(col)); t.p(6, 7 if not tall else 5, lt(col, 1.5)); t.p(9, 7 if not tall else 5, lt(col, 1.5)); t.r(4, 9 if not tall else 7, 11, 9 if not tall else 7, dk(col)); return t
FUNGI = [('inkcap', 'Inkcap', '2a2a3a', False), ('puffball', 'Puffball', 'e8e0c8', False), ('stinkhorn', 'Stinkhorn', 'a85a2a', True), ('brain_fungus', 'Brain fungus', 'e0a0b0', False), ('death_cap', 'Death cap', '8aa860', True)]
for n, lab, col, tall in FUNGI:
    cap(c(col), tall=tall).save('tiles', n); icon_of(n)
    tile(n, lab, mine_time=0.1, tool='machete', model='cross', on_floor=True); item(n, lab, 'nature', 80, 3, places=n)
    decor.append({'name': 'potion_' + n, 'tile': n, 'per_100_columns': 4, 'depth': [6, 180], 'on_floor': True})
rec('herb_mortar', 'inkcap', 1, [('compost', 2), ('dirt', 1)], 80, output={'one_of': [{'item': n, 'weight': 1} for n, _, _, _ in FUNGI], 'count': 1})
def ore(name, col):
    for k in range(2):
        t = P(); t.im = Image.open(os.path.join(DATA, 'textures/tiles/stone_%d.png' % k)).convert('RGBA').resize((16, 16), Image.NEAREST); t.px = t.im.load()
        for i in range(7):
            x, y = (i * 5 + k * 3) % 13 + 1, (i * 7 + k * 5) % 13 + 1; t.p(x, y, col); t.p(x + 1, y, lt(col)); t.p(x, y + 1, dk(col))
        t.save('tiles', '%s_%d' % (name, k))
    im = Image.open(os.path.join(tex, 'tiles', name + '_0.png')); out = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); out.paste(im, (8, 8)); out.save(os.path.join(tex, 'items', name + '_ore.png'))
def powder(col):
    # A little heap of it.
    t = P(); t.r(3, 11, 12, 13, col); t.r(4, 9, 11, 10, col); t.r(6, 7, 9, 8, col); t.r(3, 13, 12, 13, dk(col)); t.r(6, 7, 8, 7, lt(col)); t.r(4, 9, 6, 9, lt(col))
    for i in range(8): t.p((i * 5) % 8 + 4, (i * 3) % 4 + 9, [lt(col), dk(col)][i % 2])
    return t.outline()
ore('sulphur', c('e8d020')); ore('saltpetre', c('f0f0f8'))
for n, lab, col in (('sulphur', 'Sulphur', 'e8d020'), ('saltpetre', 'Saltpetre', 'f0f0f8')):
    tile(n + '_ore', lab + ' ore', True, texture=['tiles/%s_0.png' % n, 'tiles/%s_1.png' % n], shade=12, ore=True, mine_time=1.0, drop=n)
    item(n + '_ore', lab + ' ore', 'ores', 60, 3, places=n + '_ore'); powder(c(col)).save('items', n); item(n, lab, 'ores', 60, 3)
    veins.append({'name': 'potion_' + n, 'tile': n + '_ore', 'in': ['stone'], 'size': [2, 6], 'per_100_columns': 10, 'depth': [10, 180]})
rec('herb_mortar', 'sulphur', 2, [('coal', 2), ('ember', 1)], 80); rec('herb_mortar', 'saltpetre', 2, [('compost', 3)], 80)
DUSTS = [('amethyst', 'a060e0'), ('sapphire', '3a70e0'), ('emerald', '30b860'), ('ruby', 'e03048'), ('diamond', 'a0e8f8')]
for g, col in DUSTS:
    powder(c(col)).save('items', g + '_dust'); item(g + '_dust', g.capitalize() + ' dust', 'ores', BI[g]['price'] // 3, min(6, BI[g].get('tier', 6)))
    rec('herb_mortar', g + '_dust', 4, [(g + '_shard', 2)], 60); rec('herb_mortar', g + '_dust', 4, [(g, 1)], 60)

# ---- benches -----------------------------------------------------------------------------------------------------
def pot(body, rim, fire, steam):
    t = P(); t.r(2, 6, 13, 12, body); t.r(3, 13, 12, 13, body); t.r(1, 5, 14, 6, rim); t.r(1, 5, 14, 5, lt(rim)); t.r(3, 7, 4, 11, lt(body)); t.r(2, 12, 13, 12, dk(body))
    t.r(3, 14, 4, 15, IRON); t.r(11, 14, 12, 15, IRON); t.r(6, 14, 9, 15, fire); t.p(7, 13, lt(fire)); t.r(4, 6, 11, 6, steam); t.p(6, 3, steam); t.p(9, 2, steam); t.p(7, 1, steam); return t
CAULDRONS = [('cauldron', 'Cauldron', c('4a4c56'), c('6a6c76'), None), ('cauldron_2', 'Copper cauldron', c('c06a38'), c('e08a50'), [('copper_ingot', 6), ('amethyst', 1)]),
             ('cauldron_3', 'Iron cauldron', c('8a8e9a'), c('b0b4c0'), [('iron_ingot', 6), ('sapphire', 1)]), ('cauldron_4', 'Golden cauldron', c('e0b030'), c('f8d860'), [('gold_ingot', 6), ('ruby', 1)])]
for i, (n, lab, body, rim, cost) in enumerate(CAULDRONS):
    pot(body, rim, c('f08030'), c('70c880')).save('tiles', n); icon_of(n)
    up = {'to': CAULDRONS[i + 1][0], 'cost': [{'item': a, 'count': b} for a, b in CAULDRONS[i + 1][4]]} if i + 1 < len(CAULDRONS) else None
    tile(n, 'Cauldron', bench=True, bench_kind='cauldron', bench_level=i + 1, upgrade=up, light=6)
    item(n, lab, 'benches', 800 * (i + 1) ** 2, 0, places=n)
def bench_pic(name, draw): t = P(); draw(t); t.save('tiles', name); icon_of(name)
bench_pic('herb_mortar', lambda t: (t.r(3, 9, 12, 14, c('a8a090')), t.r(2, 8, 13, 9, c('c8c0b0')), t.r(4, 14, 11, 15, c('8a8478')), t.r(9, 2, 10, 9, WOOD), t.r(8, 1, 11, 2, WOOD)))
bench_pic('fermenting_crock', lambda t: (t.r(4, 5, 11, 14, c('9a6a44')), t.r(3, 7, 12, 12, c('9a6a44')), t.r(5, 3, 10, 4, c('7a4e30')), t.r(4, 5, 11, 5, c('c08a5a')), t.r(4, 9, 11, 9, c('6a4428')), t.r(5, 14, 10, 14, c('6a4428'))))
bench_pic('potion_still', lambda t: (t.r(2, 7, 8, 13, c('c06a38')), t.r(3, 5, 7, 6, c('e08a50')), t.r(5, 2, 6, 4, c('c06a38')), t.r(6, 2, 12, 3, c('c06a38')), t.r(11, 3, 12, 9, c('c06a38')),
                                     t.r(10, 10, 13, 14, GLASS), t.r(10, 12, 13, 14, c('70c880')), t.r(3, 14, 7, 15, c('f08030')), t.p(4, 8, c('f0b080'))))
for n, lab in (('herb_mortar', 'Mortar and pestle'), ('fermenting_crock', 'Fermenting crock'), ('potion_still', 'Still')):
    tile(n, lab, bench=True, bench_kind=n, bench_level=1); item(n, lab, 'benches', 600, 0, places=n)
bench_pic('apothecary_cabinet', lambda t: (t.r(1, 1, 14, 15, WOOD), t.r(1, 1, 14, 1, lt(WOOD)), [t.r(2 + (i % 3) * 4, 3 + (i // 3) * 4, 4 + (i % 3) * 4, 5 + (i // 3) * 4, c(col)) for i, col in enumerate(('c02838', '3a70e0', '30b860', 'f0b020', 'a060e0', 'e8f0f8', 'f05020', '5a3a9a', '70c880'))],
                                           t.r(1, 15, 14, 15, dk(WOOD))))
tile('apothecary_cabinet', "Apothecary's cabinet", storage={'columns': 4, 'rows': 4}, tool='axe', burns=True)
item('apothecary_cabinet', "Apothecary's cabinet", 'crafted', 900, 3, places='apothecary_cabinet', container={'slots': 16})
rec('workbench', 'herb_mortar', 1, [('stone', 4), ('stick', 1)]); rec('workbench', 'cauldron', 1, [('stone', 6), ('flint', 2)]); rec('workbench', 'fermenting_crock', 1, [('clay', 6), ('wood', 2)], level=2)
rec('workbench', 'potion_still', 1, [('copper_ingot', 5), ('glass', 3), ('charcoal', 2)], level=2); rec('woodwork_bench', 'apothecary_cabinet', 1, [('wood', 8), ('glass_bottle', 4)])

# ---- what goes into potions besides what grows -------------------------------------------------------------------
def jar(col):
    t = P(); t.r(5, 4, 10, 13, GLASS); t.r(4, 6, 11, 12, GLASS); t.r(5, 2, 10, 3, CORK); t.r(5, 7, 10, 13, col); t.r(4, 7, 11, 12, col); t.r(5, 7, 10, 7, lt(col)); t.p(6, 5, c('ffffff')); return t.outline()
for n, lab, col, price, where, ins, count in (('spirit', 'Spirit of wine', 'e8e0b0', 120, 'fermenting_crock', [('wheat', 4), ('glass_bottle', 1)], 1), ('sour_wine', 'Sour wine', 'c0a060', 90, 'fermenting_crock', [('apple', 3), ('glass_bottle', 1)], 1),
                                              ('herbal_extract', 'Herbal extract', '60b870', 400, 'potion_still', [('spirit', 1), ('kingsfoil', 2), ('feverfew', 2)], 1), ('fish_oil', 'Fish oil', 'e0c060', 150, 'potion_still', [('fish', 2), ('glass_bottle', 1)], 1),
                                              ('quicksilver_drop', 'Quicksilver drop', 'c8d0dc', 2400, 'potion_still', [('gold_ore', 2), ('sulphur', 2), ('spirit', 1)], 1), ('lye', 'Lye', 'd8d8c8', 100, 'cauldron', [('charcoal', 2), ('glass_bottle', 1)], 1)):
    jar(c(col)).save('items', n); item(n, lab, 'crafted', price, 3 if price < 1000 else 5); rec(where, n, count, ins, 120)
rec('fermenting_crock', 'spirit', 1, [('corn', 4), ('glass_bottle', 1)], 120)

# ---- the potions -------------------------------------------------------------------------------------------------
MIN = 20 * 60
def draught(name, label, col, level, ins, boons, minutes, strengths=3, price=200):
    """A draught, and its stronger forms: each made of the one before and something to strengthen it."""
    k = c(col)
    for s in range(strengths):
        n = name if s == 0 else '%s_%d' % (name, s + 1)
        scale = 1 + 0.5 * s
        b = {key: (round(1 + (v - 1) * scale, 2) if key in ('walk', 'mine', 'craft', 'regen', 'lung') else round(1 - (1 - v) * min(1, 0.8 + 0.1 * s), 2) if key in ('appetite', 'stamina') else
                   round(v * scale, 2) if key in ('armor', 'warmth', 'luck') else int(round(v * scale)) if key in ('strength', 'thorns', 'magnet', 'sight') else v) for key, v in boons.items()}
        bottle(k if s == 0 else lt(k, 1.0 + 0.12 * s), 'round', [CORK, c('c0c8d4'), GOLD][s]).save('items', n)
        item(n, label + ['', ' II', ' III'][s], 'potions', price * (1, 4, 12)[s], min(6, level + 1 + s), max_stack=10, leaves='glass_bottle',
             effect={'ticks': int(minutes * MIN * (1, 1.5, 2)[s]), 'boons': b})
        if s == 0: rec('cauldron', n, 1, [('glass_bottle', 1)] + ins, 120, level=level)
        elif s == 1: rec('cauldron', n, 1, [(name, 2), ('spirit', 1), ('ember', 1)], 180, level=max(level, 3))
        else: rec('cauldron', n, 1, [(name + '_2', 2), ('herbal_extract', 1), (('amethyst', 'sapphire', 'emerald', 'ruby')[min(3, level - 1)] + '_dust', 1)], 240, level=4)
# Stage 1: the first days. One herb and water, over a fire.
draught('willowbark_tea', 'Willowbark tea', 'b89860', 1, [('stick', 2), ('feverfew', 1)], {'regen': 2.0}, 3, 1, 60)
draught('feverfew_tea', 'Feverfew tea', 'e8e0a0', 1, [('feverfew', 2)], {'warmth': 0.5}, 5, 1, 60)
draught('wakeful_tea', 'Wakeful tea', '8a5a30', 1, [('bitterwort', 2)], {'stamina': 0.05}, 3, 1, 60)
# Stage 2: copper and tin. The standard draughts.
draught('swiftness_draught', 'Draught of swiftness', '60d0f0', 2, [('sea_lavender', 2), ('feather', 1)], {'walk': 1.25}, 3)
draught('miners_draught', "Miner's draught", 'c08040', 2, [('kingsfoil', 2), ('flint', 1)], {'mine': 1.25}, 3)
draught('stoneskin_draught', 'Stoneskin draught', '9a9aa8', 2, [('mandrake', 1), ('stone', 2)], {'armor': 0.2}, 3)
draught('gills_draught', 'Draught of gills', '3a86d8', 2, [('marshmallow', 2), ('fish', 1)], {'lung': 4.0}, 3)
draught('owl_sight_draught', 'Owl sight draught', 'f0e060', 2, [('nightshade', 1), ('inkcap', 1)], {'sight': 8}, 4)
draught('stillness_draught', 'Draught of stillness', 'a0c8a0', 2, [('dreamleaf', 2), ('milk', 1)], {'appetite': 0.5}, 5)
# Stage 3: iron. Stronger stuff.
draught('strength_draught', 'Draught of strength', 'd03030', 3, [('bloodroot', 2), ('yak_horn', 1)], {'strength': 3}, 3, price=600)
draught('thorns_draught', 'Draught of thorns', '4a8a3a', 3, [('firethorn', 2), ('poison', 1)], {'thorns': 3}, 3, price=600)
draught('feather_fall_draught', 'Feather fall draught', 'f0f0f8', 3, [('feather', 4), ('puffball', 1)], {'glide': 1}, 3, 2, 600)
draught('craftsman_draught', "Craftsman's draught", 'e0a040', 3, [('goldenseal', 2), ('coffee_bean', 2)], {'craft': 1.3}, 4, price=600)
draught('fortune_draught', 'Draught of fortune', '40d080', 3, [('goldenseal', 2), ('gold_ore', 1)], {'luck': 0.12}, 3, price=800)
draught('whetstone_oil', 'Whetstone oil', '707888', 3, [('fish_oil', 1), ('flint', 2)], {'mine': 1.5}, 1.5, 1, 500)
draught('venom_oil', 'Venom oil', '8ac030', 3, [('fish_oil', 1), ('poison', 2), ('death_cap', 1)], {'strength': 5}, 1.5, 1, 700)
# Stage 4: steel and gold. The strange ones.
draught('invisibility_draught', 'Draught of invisibility', 'd8e8f0', 4, [('ghost_orchid', 2), ('herbal_extract', 1)], {'unseen': 1}, 2, 2, 2400)
draught('magnetism_draught', 'Draught of magnetism', 'a04040', 4, [('wolfsbane', 2), ('iron_ingot', 1), ('herbal_extract', 1)], {'magnet': 6}, 4, 2, 1800)
draught('warming_salve', 'Warming salve', 'f08040', 4, [('firethorn', 2), ('fur', 1), ('fish_oil', 1)], {'warmth': 0.8}, 10, 1, 1500)
draught('frostward_draught', 'Frostward draught', '90d8f0', 4, [('frostcap', 3), ('herbal_extract', 1)], {'warmth': 0.4, 'regen': 1.3}, 5, 2, 1600)

def tonic(name, label, col, level, ins, price, tier, **kw):
    bottle(c(col), 'vial').save('items', name); item(name, label, 'potions', price, tier, max_stack=10, leaves='glass_bottle', **kw); rec('cauldron', name, 1, [('glass_bottle', 1)] + ins, 120, level=level)
tonic('poultice', 'Poultice', '70b860', 1, [('kingsfoil', 2)], 80, 2, effect={'ticks': 200, 'boons': {'regen': 8.0}})
tonic('bitter_draught', 'Bitter draught', '6a7a2a', 1, [('bitterwort', 2), ('charcoal', 1)], 120, 2, cures=True, effect={'ticks': 100, 'boons': {'regen': 2.0}})
tonic('healing_tonic', 'Healing tonic', 'e04060', 2, [('bloodroot', 2), ('kingsfoil', 1), ('spirit', 1)], 500, 3, effect={'ticks': 300, 'boons': {'regen': 16.0}})
tonic('recall_tonic', 'Tonic of recall', '9060e0', 4, [('dreamleaf', 2), ('time_crystal', 3), ('herbal_extract', 1)], 3000, 5, recalls=True)
tonic('panacea', 'Panacea', 'fff0a0', 4, [('healing_tonic', 2), ('bitter_draught', 2), ('diamond_dust', 1), ('quicksilver_drop', 1)], 30000, 6, cures=True, food=1.0, food_health=1.0, food_energy=1.0,
      cooked=True, effect={'ticks': 600, 'boons': {'regen': 20.0}})

# Flasks: thrown. Each is a spell with one use in it.
t = P(); t.r(6, 6, 9, 9, c('ffe890')); t.r(7, 5, 8, 10, c('ffe890')); t.r(5, 7, 10, 8, c('ffe890')); t.r(7, 7, 8, 8, c('ffffff')); t.save('tiles', 'glow_mote')
tile('glow_mote', 'Glow mote', mine_time=0.05, light=13, attaches=True, floats=True, model='panel', drop=None, pulse=0.2)
def flask(name, label, col, level, ins, price, tier, reach, cast, count=2):
    bottle(c(col), 'bulb').save('items', name); item(name, label, 'potions', price, tier, uses=1, range=reach, cast=cast); rec('cauldron', name, count, [('glass_bottle', count)] + ins, 160, level=level)
flask('torchwater', 'Torchwater', 'ffe890', 1, [('sunwort', 2), ('charcoal', 1)], 60, 2, 8, {'places': 'glow_mote'}, 4)
flask('growth_flask', 'Growth flask', '60d060', 2, [('compost', 2), ('kingsfoil', 2), ('saltpetre', 1)], 300, 3, 8, {'grow': 2})
flask('snare_flask', 'Snare flask', 'a0a060', 2, [('marshmallow', 2), ('string', 2), ('stinkhorn', 1)], 300, 3, 8, {'hold': 200, 'splash': 3})
flask('frost_flask', 'Frost flask', 'a0e0f8', 3, [('frostcap', 3), ('saltpetre', 2)], 700, 4, 9, {'freeze': 2, 'damage': 3, 'splash': 2})
flask('fire_flask', 'Fire flask', 'f06020', 3, [('firethorn', 2), ('sulphur', 2), ('spirit', 1)], 700, 4, 9, {'damage': 12, 'splash': 2, 'cooks': True})
flask('acid_flask', 'Acid flask', 'b0e030', 3, [('sour_wine', 1), ('sulphur', 2), ('death_cap', 1)], 900, 4, 8, {'dig': 1})
flask('rain_bottle', 'Rain bottle', '5080c0', 4, [('sea_lavender', 3), ('saltpetre', 2), ('herbal_extract', 1)], 2000, 5, 0, {'weather': 'rain', 'weather_ticks': 12000}, 1)
flask('sun_bottle', 'Sun bottle', 'ffd040', 4, [('sunwort', 3), ('sulphur', 2), ('herbal_extract', 1)], 2000, 5, 0, {'weather': 'clear', 'weather_ticks': 24000}, 1)
flask('tide_flask', 'Flask of the tide', '2060c0', 4, [('marshmallow', 3), ('sapphire_dust', 1), ('herbal_extract', 1)], 3000, 6, 9, {'water': 40}, 1)
flask('drought_flask', 'Flask of drought', 'e0c080', 4, [('sunwort', 3), ('ruby_dust', 1), ('lye', 1)], 3000, 6, 9, {'drain': 6}, 1)
flask('quake_flask', 'Quake flask', '8a6a4a', 4, [('acid_flask', 2), ('sulphur', 3), ('quicksilver_drop', 1)], 6000, 6, 9, {'dig': 3, 'damage': 8, 'splash': 3}, 1)

# Elixirs: the great works. Each lasts for good, and can be taken once. One long making leads to all of them.
def step(name, label, col, where, ins, level=None):
    jar(c(col)).save('items', name); item(name, label, 'crafted', 5000, 6); rec(where, name, 1, ins, 300, level=level)
OPUS = [('prima_materia', 'Prima materia', '5a5a60', 'herb_mortar', [('mandrake', 2), ('death_cap', 2), ('sulphur', 4), ('saltpetre', 4)]),
        ('black_work', 'The black work', '1a1a22', 'fermenting_crock', [('sour_wine', 2), ('inkcap', 3)]),
        ('white_work', 'The white work', 'f0f0f8', 'potion_still', [('spirit', 2), ('puffball', 3), ('diamond_dust', 1)]),
        ('yellow_work', 'The yellow work', 'f0d040', 'cauldron', [('goldenseal', 4), ('gold_ingot', 2), ('sunwort', 3)]),
        ('red_work', 'The red work', 'd02838', 'cauldron', [('bloodroot', 4), ('ruby_dust', 2), ('quicksilver_drop', 1)]),
        ('quintessence', 'Quintessence', 'c0a0ff', 'potion_still', [('herbal_extract', 3), ('ghost_orchid', 2), ('time_crystal', 10)]),
        ('elixir_base', 'Elixir base', 'ffe8b0', 'cauldron', [('panacea', 1), ('amethyst_dust', 2), ('sapphire_dust', 2), ('emerald_dust', 2)])]
last = None
for n, lab, col, where, ins in OPUS:
    step(n, lab, col, where, ([(last, 1)] if last else []) + ins, level=4 if where == 'cauldron' else None); last = n
def elixir(name, label, col, ins, boons, **kw):
    bottle(c(col), 'urn').save('items', name); item(name, label, 'potions', 120000, 6, max_stack=1, leaves='glass_bottle', effect={'forever': True, 'boons': boons})
    rec('cauldron', name, 1, [('elixir_base', 1)] + ins, 600, level=4, **kw)
elixir('elixir_of_vigour', 'Elixir of vigour', 'e03050', [('healing_tonic', 4), ('bloodroot', 8)], {'regen': 1.5})
elixir('elixir_of_deep_breath', 'Elixir of the deep breath', '3080e0', [('gills_draught_3', 2), ('marshmallow', 8)], {'lung': 2.0})
elixir('elixir_of_the_long_road', 'Elixir of the long road', '60d8f0', [('swiftness_draught_3', 2), ('sea_lavender', 8)], {'walk': 1.1})
elixir('elixir_of_stone', 'Elixir of stone', '9a9aa8', [('stoneskin_draught_3', 2), ('mandrake', 6)], {'armor': 0.1})
elixir('elixir_of_fortune', 'Elixir of fortune', '40d080', [('fortune_draught_3', 2), ('goldenseal', 8)], {'luck': 0.08})
# With the Dimensions mod on as well: four more, of what the Aether and the Deep have. (Their recipes name its
# things, and are left out when it is not there.)
bottle(c('c8f0ff'), 'round', GOLD).save('items', 'skywalker_draught')
item('skywalker_draught', "Skywalker's draught", 'potions', 40000, 6, max_stack=10, leaves='glass_bottle', effect={'ticks': 600, 'boons': {'flight': 1}})
rec('cauldron', 'skywalker_draught', 1, [('glass_bottle', 1), ('wind_essence', 2), ('sky_feather', 2), ('zephyrite_shard', 1)], 300, level=4, optional=True)
bottle(c('ff7020'), 'round', GOLD).save('items', 'abyss_draught')
item('abyss_draught', 'Draught of the abyss', 'potions', 60000, 6, max_stack=10, leaves='glass_bottle', effect={'ticks': 3 * MIN, 'boons': {'fireproof': 1}})
rec('cauldron', 'abyss_draught', 1, [('glass_bottle', 1), ('slug_cinder', 2), ('ember_pepper', 2), ('lumite', 1)], 300, level=4, optional=True)
bottle(c('f8f060'), 'bulb').save('items', 'stormcaller_flask')
item('stormcaller_flask', 'Stormcaller', 'potions', 50000, 6, uses=1, range=10, cast={'dig': 2, 'damage': 30, 'splash': 3, 'cooks': True})
rec('cauldron', 'stormcaller_flask', 1, [('glass_bottle', 1), ('storm_feather', 1), ('galvanite_ingot', 2), ('stardust', 2)], 300, level=4, optional=True)
bottle(c('8050c0'), 'urn', trim=c('c0c8d4')).save('items', 'elixir_of_second_shadow')
item('elixir_of_second_shadow', 'Elixir of the second shadow', 'potions', 400000, 6, max_stack=1, leaves='glass_bottle', effect={'forever': True, 'boons': {'craft': 1.2, 'mine': 1.1}})
rec('cauldron', 'elixir_of_second_shadow', 1, [('elixir_base', 1), ('shadow_essence', 4), ('umbrite', 1), ('heliodor', 1)], 600, level=4, optional=True)

# ---- the bog witch -----------------------------------------------------------------------------------------------
def witch(step):
    skin, robe, hat = c('8ab870'), c('3a2a4a'), c('2a1a3a'); t = P()
    t.r(6, 0, 9, 1, hat); t.r(4, 2, 11, 2, hat); t.r(6, 3, 9, 5, skin); t.p(8, 4, INK); t.p(9, 5, c('6a9850'))
    t.r(5, 6, 10, 13, robe); t.r(4, 8, 4, 10, robe); t.r(11, 8, 11, 10, robe); t.r(5, 6, 10, 6, lt(robe)); t.p(4, 11, skin); t.p(11, 11, skin)
    if step == 0: t.r(5, 14, 10, 14, robe); t.r(6, 15, 7, 15, INK); t.r(8, 15, 9, 15, INK)
    else: t.r(5 if step == 1 else 6, 14, 9 if step == 1 else 10, 14, robe); t.r(5 if step == 1 else 7, 15, 6 if step == 1 else 8, 15, INK); t.r(9, 15, 10, 15, INK)
    return t.outline()
for step_, suffix in ((0, ''), (1, '_walk_0'), (2, '_walk_1')): witch(step_).save('mobs', 'bog_witch' + suffix)
mobs.append({'name': 'bog_witch', 'label': 'Bog witch', 'texture': 'mobs/bog_witch.png', 'walk': ['mobs/bog_witch_walk_0.png', 'mobs/bog_witch.png', 'mobs/bog_witch_walk_1.png', 'mobs/bog_witch.png'],
             'hop': 0.03, 'step_ticks': 4, 'move_chance': 0.2, 'turn_chance': 0.3, 'health': 40, 'cap': 1, 'spawn': {'above': ['sand', 'grass'], 'biome': ['beach', 'tropics'], 'time': 'night', 'chance': 0.02},
             'sting': {'ticks': 80, 'rest_ticks': 60, 'sight': 8},
             'drops': [{'item': 'healing_tonic', 'min': 0, 'max': 2}, {'item': 'snare_flask', 'min': 0, 'max': 2}, {'item': 'ghost_orchid', 'min': 1, 'max': 2}, {'item': 'mandrake', 'min': 1, 'max': 2},
                       {'item': 'invisibility_draught', 'min': 1, 'max': 1, 'chance': 0.2}, {'item': 'herbal_extract', 'min': 1, 'max': 2, 'chance': 0.5}]})

# ---- written out -------------------------------------------------------------------------------------------------
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v))
                                    for k, v in obj.items()) + '\n}\n')
names = [t['name'] for t in tiles]; assert len(names) == len(set(names)), [n for n in names if names.count(n) > 1]
names = [t['name'] for t in items]; assert len(names) == len(set(names)), [n for n in names if names.count(n) > 1]
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes}); dump('mobs.json', {'mobs': mobs})
dump('worldgen.json', {'veins': veins, 'cave_decor': decor, 'biome_plants': plants})
dump('trade.json', {'categories': [{'name': 'potions', 'label': 'Potions'}]})
json.dump({'label': 'Potioncraft', 'icon': 'icon.png', 'description': 'Herbs, cauldrons and potions: something to drink or to throw at every stage of the game.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
Image.open(os.path.join(tex, 'items', 'healing_tonic.png')).save(os.path.join(mod, 'icon.png'))
print('%d tiles, %d items, %d recipes, %d potions' % (len(tiles), len(items), len(recipes), sum(1 for i in items if i['category'] == 'potions')))
subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'caged_art.py'), repo], check=True)
