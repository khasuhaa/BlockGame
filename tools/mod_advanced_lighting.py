#!/usr/bin/env python3
"""Writes mods/advanced_lighting: some fifty lights - lanterns, sconces and floor lamps in every metal the game has,
gem lamps, cloth lanterns, candles, braziers, floodlights, a street lamp - and two things that glow in caves.

    python3 tools/mod_advanced_lighting.py .
"""
import glob, json, os, sys
from PIL import Image
repo = sys.argv[1] if len(sys.argv) > 1 else '.'
mod = os.path.join(repo, 'mods/advanced_lighting')
for d in ('tiles', 'items'): os.makedirs(os.path.join(mod, 'textures', d), exist_ok=True)
BASE = {i['name']: i for i in json.load(open(os.path.join(repo, 'data/items.json')))['items']}
TAKEN = set(BASE) | {t['name'] for t in json.load(open(os.path.join(repo, 'data/tiles.json')))['tiles']}
for d in glob.glob(os.path.join(repo, 'mods/*')):
    if os.path.basename(d) == 'advanced_lighting': continue
    for f, k in (('tiles', 'tiles'), ('items', 'items')):
        if os.path.exists(os.path.join(d, f + '.json')): TAKEN |= {x['name'] for x in json.load(open(os.path.join(d, f + '.json')))[k]}
tiles, items, recipes = [], [], []
def c(s): return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
def dk(s, f=0.65): return '%02x%02x%02x' % tuple(int(int(s[i:i + 2], 16) * f) for i in (0, 2, 4))
def lt(s, f=1.3): return '%02x%02x%02x' % tuple(min(255, int(int(s[i:i + 2], 16) * f + 12)) for i in (0, 2, 4))
def draw(name, rects, more=()):
    """One picture, or several (a fire's frames): 16 by 16, drawn double, with the icon from the first."""
    for k, rs in enumerate([rects] + list(more)):
        im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); px = im.load()
        for x0, y0, x1, y1, col in rs:
            for y in range(max(0, y0), min(15, y1) + 1):
                for x in range(max(0, x0), min(15, x1) + 1): px[x, y] = c(col)
        big = im.resize((32, 32), Image.NEAREST); big.save(os.path.join(mod, 'textures/tiles', name + ('' if k == 0 else '_lit_%d' % (k - 1)) + '.png'))
        if k == 0: icon = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); icon.paste(big, (8, 8)); icon.save(os.path.join(mod, 'textures/items', name + '.png'))
def light(name, label, level, price, tier, rects, bench, ins, count=1, lit=(), cat=None, **kw):
    assert name not in TAKEN and len(name) <= 31, name
    for n, _ in ins: assert n in BASE or any(i['name'] == n for i in items), n
    draw(name, rects, lit)
    t = {'name': name, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.2, 'drop': name, 'texture': 'tiles/%s.png' % name, 'light': level, 'tool': 'pickaxe'}
    if lit: t.update(texture_lit=['tiles/%s_lit_%d.png' % (name, k) for k in range(len(lit))], lit_fps=6)
    t.update(kw); tiles.append({k: v for k, v in t.items() if v is not None})
    items.append({'name': name, 'label': label, 'places': name, 'texture': 'items/%s.png' % name, 'category': cat or ('energy' if lit or 'beam' in kw or 'candle' in name or 'torch' in name else 'blocks' if kw.get('solid') or kw.get('supports') or 'cloth' in name else 'crafted'), 'price': max(10, price), 'tier': tier})   # (fires and beams with the game's other sources of light and power; what is built with among the blocks)
    recipes.append({'bench': bench, 'output': {'item': name, 'count': count}, 'inputs': [{'item': i, 'count': n} for i, n in ins], 'time': 60})
GLOW, HOT, IRONC = 'ffe890', 'fff8d0', '5a5e68'
# ---- a lantern to hang, a sconce for the wall and a lamp for the floor, in each metal: the brighter the dearer ---------------
METALS = [('copper', 'Copper', 'c87a4a', 11), ('tin', 'Tin', 'b8c0c8', 11), ('bronze', 'Bronze', 'b88a4a', 12), ('iron', 'Iron', '7a7e8a', 13), ('steel', 'Steel', '9aa4b0', 13), ('gold', 'Gold', 'f0c840', 14), ('titanium', 'Titanium', 'd0d8e8', 15), ('platinum', 'Platinum', 'e8e8f0', 15)]
for key, lab, col, level in METALS:
    ingot = key + '_ingot'; price, tier = BASE[ingot].get('price', 100) * 2 + 60, BASE[ingot].get('tier', 3)
    light(key + '_hanging_lantern', lab + ' hanging lantern', level, price, tier, [(7, 0, 8, 3, dk(col)), (5, 4, 10, 5, col), (5, 6, 10, 11, GLOW), (7, 7, 8, 10, HOT), (5, 6, 5, 11, col), (10, 6, 10, 11, col), (4, 12, 11, 13, col), (6, 14, 9, 14, dk(col))],
          'metalwork_bench', [(ingot, 1), ('glass', 1), ('oil', 1)], hangs=True)
    light(key + '_wall_sconce', lab + ' wall sconce', level - 2, price - 40, tier, [(6, 9, 9, 13, col), (5, 13, 10, 14, dk(col)), (7, 14, 8, 15, dk(col)), (4, 8, 11, 9, col), (6, 3, 9, 7, GLOW), (7, 2, 8, 6, HOT), (7, 1, 7, 1, 'ffb040')],
          'metalwork_bench', [(ingot, 1), ('oil', 1)], 2, model='panel')
    light(key + '_floor_lamp', lab + ' floor lamp', level, price + 40, tier, [(7, 7, 8, 14, col), (5, 14, 10, 15, dk(col)), (4, 1, 11, 6, GLOW), (5, 0, 10, 0, col), (4, 6, 11, 7, col), (6, 2, 9, 5, HOT), (4, 1, 4, 6, lt(col)), (11, 1, 11, 6, dk(col))],
          'metalwork_bench', [(ingot, 2), ('glass', 1), ('oil', 1)])
# ---- gems: a lamp of each, the colour of its stone -------------------------------------------------------------------------------
for key, lab, col in [('amethyst', 'Amethyst', 'b060e0'), ('sapphire', 'Sapphire', '4070e0'), ('emerald', 'Emerald', '40c070'), ('ruby', 'Ruby', 'e04050'), ('diamond', 'Diamond', 'c0f0f8')]:
    shard = key + '_shard'
    light(key + '_gem_lamp', lab + ' lamp', 14, BASE[shard].get('price', 200) * 3 + 100, BASE[shard].get('tier', 4), [(5, 12, 10, 14, IRONC), (4, 14, 11, 15, dk(IRONC)), (6, 3, 9, 11, col), (5, 5, 10, 9, col), (7, 1, 8, 2, lt(col)), (6, 4, 7, 6, lt(col, 1.6)), (9, 9, 9, 10, dk(col))],
          'craft_bench', [(shard, 3), ('iron_ingot', 1), ('oil', 1)])
# ---- cloth lanterns, hung --------------------------------------------------------------------------------------------------------------
for key, lab, col, dye in [('white', 'White', 'f0e8d8', None), ('red', 'Red', 'c84a3a', 'red_ochre'), ('green', 'Green', '4aa860', 'emerald_green'), ('yellow', 'Yellow', 'e8c040', 'indian_yellow'), ('blue', 'Blue', '4a70c8', 'ultramarine_blue')]:
    light(key + '_cloth_lantern', lab + ' cloth lantern', 11, 120, 2, [(7, 0, 8, 2, 'b89a60'), (5, 3, 10, 3, dk(col)), (4, 4, 11, 11, col), (5, 5, 10, 10, lt(col, 1.15)), (7, 4, 7, 11, dk(col, 0.85)), (9, 4, 9, 11, dk(col, 0.85)), (5, 12, 10, 12, dk(col)), (7, 13, 8, 15, 'c84a3a')],
          'tailor_bench', [('flax', 2), ('stick', 1), ('oil', 1)] + ([(dye, 1)] if dye else []), hangs=True, model='panel')
# ---- candles and fires ---------------------------------------------------------------------------------------------------------------------
FLAME = lambda x, y: [(x, y - 2, x, y - 1, 'ffb040'), (x, y - 3, x, y - 3, HOT)]
light('oil_candle', 'Candle', 8, 30, 1, [(7, 9, 8, 14, 'f0e8d0'), (6, 14, 9, 15, 'b88a4a')] + FLAME(7, 9), 'craft_bench', [('oil', 1), ('string', 1)], 2, model='panel')
light('bronze_candelabra', 'Candelabra', 12, 400, 3, [(7, 8, 8, 14, 'b88a4a'), (5, 14, 10, 15, dk('b88a4a')), (3, 7, 12, 8, 'b88a4a'), (3, 5, 4, 7, 'f0e8d0'), (7, 4, 8, 7, 'f0e8d0'), (11, 5, 12, 7, 'f0e8d0')] + FLAME(3, 5) + FLAME(7, 4) + FLAME(11, 5), 'metalwork_bench', [('bronze_ingot', 2), ('oil_candle', 3)])
light('iron_candle_ring', 'Candle ring', 14, 700, 4, [(7, 0, 8, 6, IRONC), (2, 9, 13, 10, IRONC), (2, 7, 3, 9, 'f0e8d0'), (7, 7, 8, 9, 'f0e8d0'), (12, 7, 13, 9, 'f0e8d0'), (4, 6, 7, 6, IRONC), (8, 6, 11, 6, IRONC)] + FLAME(2, 7) + FLAME(7, 7) + FLAME(12, 7), 'metalwork_bench', [('iron_ingot', 2), ('oil_candle', 4)], hangs=True)
def fire(x0, y0, k): return [(x0, y0 - 3 + k % 2, x0 + 5, y0, 'f08030'), (x0 + 1, y0 - 5 + k, x0 + 3, y0 - 1, 'ffb040'), (x0 + 2 + k % 2, y0 - 6 + k, x0 + 2 + k % 2, y0 - 4, HOT)]
BOWL = [(3, 9, 12, 11, IRONC), (4, 12, 11, 12, dk(IRONC)), (7, 13, 8, 14, IRONC), (4, 15, 11, 15, dk(IRONC))]
light('iron_brazier', 'Brazier', 15, 500, 4, BOWL + [(5, 8, 10, 8, '3a3430')], 'metalwork_bench', [('iron_ingot', 3), ('coal', 2)], lit=[BOWL + fire(5, 8, k) for k in range(3)], fuel_capacity=6000, light_needs_fuel=True)
STONE = [(2, 10, 13, 13, '7c7e84'), (3, 14, 12, 15, dk('7c7e84')), (2, 10, 13, 10, lt('7c7e84', 1.1))]
light('stone_fire_bowl', 'Fire bowl', 14, 80, 1, STONE + [(4, 9, 11, 9, '3a3430')], 'kiln', [('stone', 4), ('coal', 1)], lit=[STONE + fire(5, 9, k) for k in range(3)], fuel_capacity=4000, light_needs_fuel=True)
light('tiki_torch', 'Standing torch', 12, 40, 1, [(7, 6, 8, 15, '8a6a3c'), (6, 4, 9, 6, 'b89a60'), (6, 5, 9, 5, '6a4a2a')] + FLAME(7, 4) + [(8, 2, 8, 3, 'ffb040')], 'woodwork_bench', [('stick', 3), ('oil', 1)], 2)
# ---- floodlights: a beam up or down (the engine's beams go no other way) -----------------------------------------------------------------------------------------------------------
for way, lab, body, lens in [('up', 'Floodlight, upward', [(5, 8, 10, 14, 'd0d8e8'), (4, 14, 11, 15, IRONC)], (5, 6, 10, 7)), ('down', 'Floodlight, downward', [(5, 1, 10, 7, 'd0d8e8'), (4, 0, 11, 1, IRONC)], (5, 8, 10, 9))]:
    light('titanium_floodlight_' + way, lab, 9, 2400, 6, body + [lens + (HOT,)], 'metalwork_bench', [('titanium_ingot', 1), ('glass', 2), ('oil', 2)], beam={'direction': way, 'light': 15})
# ---- the street, and the grand --------------------------------------------------------------------------------------------------------------
light('iron_lamp_post', 'Lamp post', 0, 150, 4, [(7, 0, 8, 15, IRONC), (7, 0, 7, 15, lt(IRONC)), (6, 13, 9, 15, dk(IRONC))], 'metalwork_bench', [('iron_ingot', 1)], 3, supports=True, attaches=True, climbable=True)
light('iron_street_lamp', 'Street lamp', 15, 800, 4, [(7, 11, 8, 15, IRONC), (4, 3, 11, 10, GLOW), (6, 5, 9, 8, HOT), (3, 2, 12, 3, IRONC), (5, 0, 10, 1, IRONC), (4, 10, 11, 11, IRONC), (4, 3, 4, 10, IRONC), (11, 3, 11, 10, IRONC)], 'metalwork_bench', [('iron_ingot', 2), ('glass', 2), ('oil', 2)], attaches=True)
light('gold_beacon', 'Beacon', 15, 9000, 6, [(4, 12, 11, 15, 'f0c840'), (4, 12, 11, 12, lt('f0c840')), (5, 4, 10, 11, 'c0f0f8'), (6, 5, 9, 10, HOT), (5, 3, 10, 3, 'f0c840'), (7, 1, 8, 2, 'f0c840')], 'craft_bench', [('gold_ingot', 3), ('diamond_shard', 2), ('glass', 2)], beam={'direction': 'up', 'light': 15})
light('lit_glass_block', 'Lit glass', 12, 200, 3, [(0, 0, 15, 15, 'f8f0c0'), (0, 0, 15, 0, 'ffffff'), (0, 0, 0, 15, 'ffffff'), (0, 15, 15, 15, 'c8b870'), (15, 0, 15, 15, 'c8b870'), (5, 5, 10, 10, HOT)], 'kiln', [('glass', 2), ('oil', 1)], 2, solid=True, layer=None, see_through=True)
# ---- what glows of itself: found in caves when the mod is on, and made into lamps -------------------------------------------------------------
def grown(name, label, level, rects, per, item_extra):
    assert name not in TAKEN; draw(name, rects)
    tiles.append({'name': name, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.1, 'drop': name, 'texture': 'tiles/%s.png' % name, 'light': level, 'tool': 'machete', 'model': 'panel'})
    items.append(dict({'name': name, 'label': label, 'places': name, 'texture': 'items/%s.png' % name, 'category': 'nature', 'price': 20, 'tier': 1}, **item_extra)); TAKEN.add(name)
    return {'name': name, 'tile': name, 'per_100_columns': per, 'depth': [5, 185]}
decor = [grown('glimmercap', 'Glimmercap', 7, [(7, 10, 8, 15, 'd8e8d0'), (4, 6, 11, 9, '50e0b0'), (5, 5, 10, 5, '80f0d0'), (6, 7, 6, 7, 'e0fff0'), (9, 6, 9, 6, 'e0fff0'), (3, 11, 4, 15, 'c0d8c0'), (2, 9, 5, 10, '40c8a0')], 7, {'food': 0.05}),
         grown('glimmer_moss', 'Glimmer moss', 5, [(0, 12, 15, 15, '3a8a5a'), (1, 11, 4, 11, '50b070'), (7, 10, 9, 11, '50b070'), (12, 11, 14, 11, '50b070'), (2, 13, 2, 13, 'c0ffa0'), (8, 12, 8, 12, 'c0ffa0'), (13, 14, 13, 14, 'c0ffa0')], 5, {})]
JAR = [(5, 3, 10, 4, '8a6a3c'), (4, 5, 11, 14, 'c0e0e0'), (4, 5, 4, 14, 'e8f8f8'), (4, 15, 11, 15, '90b0b0')]
light('glimmercap_jar', 'Glimmercap jar', 10, 90, 1, JAR + [(6, 9, 9, 11, '50e0b0'), (7, 12, 8, 14, 'd8e8d0'), (7, 8, 8, 8, 'e0fff0')], 'craft_bench', [('glass', 1), ('glimmercap', 2)])
light('moss_lamp', 'Moss lamp', 9, 80, 1, JAR + [(5, 10, 10, 14, '3a8a5a'), (6, 9, 8, 9, '50b070'), (6, 11, 6, 11, 'c0ffa0'), (9, 12, 9, 12, 'c0ffa0')], 'craft_bench', [('glass', 1), ('glimmer_moss', 3)])
def out(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v)) for k, v in obj.items()) + '\n}\n')
out('tiles.json', {'tiles': tiles}); out('items.json', {'items': items}); out('recipes.json', {'recipes': recipes}); out('worldgen.json', {'cave_decor': decor})
json.dump({'label': 'Advanced Lighting', 'icon': 'icon.png', 'description': 'Lights: a hanging lantern, a wall sconce and a floor lamp in every metal, gem lamps, cloth lanterns, candles, braziers, floodlights, a street lamp, a beacon - and glimmercaps and glimmer moss in the caves of new worlds.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
ic = Image.new('RGBA', (64, 64), c('1a1c28')); px = ic.load()
for x in range(64):
    for y in range(64):
        d = ((x - 32) ** 2 + (y - 30) ** 2) ** 0.5
        if d < 22: px[x, y] = (255, 232, 144, 255) if d < 8 else (int(255 - d * 9), int(232 - d * 9), int(144 - d * 5), 255)
ic.save(os.path.join(mod, 'icon.png'))
print('%d blocks, %d items, %d recipes, %d that light' % (len(tiles), len(items), len(recipes), sum(1 for t in tiles if t.get('light', 0) > 0)))
