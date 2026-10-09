#!/usr/bin/env python3
"""Writes mods/qol_utils (a few handy things and two kinder rules) and the icon of mods/graphics_quality.

    python3 tools/mod_qol_utils.py .
"""
import json, os, sys
from PIL import Image
repo = sys.argv[1] if len(sys.argv) > 1 else '.'
mod = os.path.join(repo, 'mods/qol_utils')
for d in ('tiles', 'items'): os.makedirs(os.path.join(mod, 'textures', d), exist_ok=True)
base = {i['name'] for i in json.load(open(os.path.join(repo, 'data/items.json')))['items']}
tiles, items, recipes = [], [], []
def c(s): return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
def pic(kind, name, rects):
    im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); px = im.load()
    for x0, y0, x1, y1, col in rects:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1): px[x, y] = c(col)
    if kind == 'tiles':
        im.resize((32, 32), Image.NEAREST).save(os.path.join(mod, 'textures/tiles', name + '.png'))
        icon = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); icon.paste(im.resize((32, 32), Image.NEAREST), (8, 8)); icon.save(os.path.join(mod, 'textures/items', name + '.png'))
    else: im.resize((48, 48), Image.NEAREST).save(os.path.join(mod, 'textures/items', name + '.png'))
def rec(bench, out, ins, count=1):
    for n, _ in ins: assert n in base, n
    recipes.append({'bench': bench, 'output': {'item': out, 'count': count}, 'inputs': [{'item': i, 'count': n} for i, n in ins], 'time': 40})
def block(name, label, price, tier, rects, bench, ins, count=1, iover=None, **kw):
    pic('tiles', name, rects)
    tiles.append(dict({'name': name, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.2, 'drop': name, 'texture': 'tiles/%s.png' % name, 'tool': 'axe'}, **kw))
    items.append(dict({'name': name, 'label': label, 'places': name, 'texture': 'items/%s.png' % name, 'category': 'crafted', 'price': price, 'tier': tier}, **(iover or {}))); rec(bench, name, ins, count)
def thing(name, label, price, tier, rects, bench, ins, **kw):
    pic('items', name, rects); items.append(dict({'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'category': 'crafted', 'price': price, 'tier': tier}, **kw)); rec(bench, name, ins)
W, D, R = 'a8824e', '6e5230', 'c8a868'
block('handy_scaffold', 'Scaffolding', 10, 1, [(1, 0, 2, 15, W), (13, 0, 14, 15, W), (0, 3, 15, 4, D), (0, 11, 15, 12, D), (3, 5, 4, 5, W), (5, 6, 6, 6, W), (7, 7, 8, 7, W), (9, 8, 10, 8, W), (11, 9, 12, 10, W)], 'woodwork_bench', [('stick', 4)], 4, climbable=True, attaches=True, supports=True, mine_time=0.05)
block('handy_rope_ladder', 'Rope ladder', 15, 1, [(4, 0, 5, 15, R), (10, 0, 11, 15, R), (4, 2, 11, 2, W), (4, 6, 11, 6, W), (4, 10, 11, 10, W), (4, 14, 11, 14, W)], 'woodwork_bench', [('string', 2), ('stick', 2)], 3, climbable=True, attaches=True)
block('handy_bright_lantern', 'Bright lantern', 90, 2, [(6, 1, 9, 2, '5a5e68'), (5, 3, 10, 4, '5a5e68'), (5, 5, 10, 11, 'fff0a0'), (7, 6, 8, 10, 'ffffff'), (5, 5, 5, 11, '5a5e68'), (10, 5, 10, 11, '5a5e68'), (4, 12, 11, 13, '5a5e68')], 'craft_bench', [('glass', 1), ('torch', 2)], 1, light=15)
block('handy_bedroll', 'Bedroll', 60, 1, [(1, 10, 14, 13, '8a5a4a'), (1, 10, 14, 10, 'b07a66'), (1, 9, 4, 12, 'e8e0d0'), (12, 9, 14, 13, '6a4234'), (1, 14, 14, 14, '5a3a2c')], 'tailor_bench', [('flax', 6)], 1, bed={'speed': 1.5})
block('handy_crate', 'Stacking crate', 45, 1, [(1, 3, 14, 14, W), (1, 3, 14, 3, R), (1, 14, 14, 14, D), (1, 8, 14, 8, D), (4, 3, 5, 14, D), (10, 3, 11, 14, D)], 'woodwork_bench', [('wood', 3), ('stick', 2)], 1, iover={'container': {'slots': 16}}, storage={'columns': 4, 'rows': 4})
thing('handy_pack_basket', 'Pack basket', 180, 2, [(3, 5, 12, 13, R), (3, 5, 12, 5, 'e0c890'), (3, 9, 12, 9, W), (5, 5, 5, 13, W), (8, 5, 8, 13, W), (11, 5, 11, 13, W), (5, 2, 10, 2, D), (4, 3, 4, 4, D), (11, 3, 11, 4, D)], 'woodwork_bench', [('basket', 2), ('string', 4)], max_stack=1, container={'slots': 16, 'open_in_inventory': True})
CHARMS = [('magnet', 'Magnet charm', 'c83030', {'magnet': 4}, 'metalwork_bench', [('iron_ingot', 3), ('string', 1)], 600, 4), ('swift', 'Swift charm', '60c0f0', {'walk': 1.1}, 'tailor_bench', [('feather', 6), ('leather', 2)], 400, 3),
          ('diver', 'Diver charm', '3a78c0', {'lung': 2.0}, 'craft_bench', [('glass', 2), ('leather', 1)], 300, 3), ('miner', 'Miner charm', 'c87a4a', {'mine': 1.25}, 'metalwork_bench', [('copper_ingot', 3), ('coal', 2)], 400, 3),
          ('maker', 'Maker charm', 'b060e0', {'craft': 1.25}, 'craft_bench', [('tin_ingot', 2), ('amethyst_shard', 1)], 500, 4)]
for key, label, col, boons, bench, ins, price, tier in CHARMS:
    thing('handy_%s_charm' % key, label, price, tier, [(7, 1, 8, 4, R), (5, 5, 10, 12, col), (4, 7, 11, 10, col), (6, 6, 7, 7, 'ffffff'), (6, 13, 9, 13, col)], bench, ins, max_stack=1, relic=True, boons=boons)
def out(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v)) for k, v in obj.items()) + '\n}\n')
out('tiles.json', {'tiles': tiles}); out('items.json', {'items': items}); out('recipes.json', {'recipes': recipes})
# (Kinder rules: what is dropped lies three times as long, and is picked up sooner.)
json.dump({'drops': {'lifetime': 1800, 'pickup_delay': 0.25}}, open(os.path.join(mod, 'rules.json'), 'w'), indent=1)
json.dump({'label': 'QOL Utils', 'icon': 'icon.png', 'utility': True, 'description': 'Handy things: scaffolding, a rope ladder, a bright lantern, a bedroll, a stacking crate, a pack basket and five charms for the relic slots. What is dropped lies longer and is picked up sooner.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
def icon(path, draw):
    im = Image.new('RGBA', (64, 64), c('2a3040')); px = im.load(); draw(px); im.save(path)
def qol(px):
    for x in range(20, 44):
        for y in range(24, 50): px[x, y] = c(R) if (x // 4 + y // 4) % 2 else c(W)
    for x in range(24, 40): px[x, 16] = px[x, 17] = c(D)
def gq(px):
    for x in range(64):
        for y in range(64):
            d = ((x - 32) ** 2 + (y - 30) ** 2) ** 0.5
            px[x, y] = (255, 236, 150, 255) if d < 9 else (int(255 - d * 4), int(200 - d * 3), int(120 + d), 255) if d < 26 else c('2a3040')
icon(os.path.join(mod, 'icon.png'), qol); icon(os.path.join(repo, 'mods/graphics_quality/icon.png'), gq)
print('%d blocks, %d items, %d recipes' % (len(tiles), len(items), len(recipes)))
