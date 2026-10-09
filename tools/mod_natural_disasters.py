#!/usr/bin/env python3
"""Writes mods/natural_disasters: six disasters (in its rules.json - the engine half is src/disaster.c) and the few
things that go with them: meteorites and what is made of them, a lightning rod, sandbags.

    python3 tools/mod_natural_disasters.py .
"""
import glob, json, os, sys
from PIL import Image
repo = sys.argv[1] if len(sys.argv) > 1 else '.'
mod = os.path.join(repo, 'mods/natural_disasters')
for d in ('tiles', 'items'): os.makedirs(os.path.join(mod, 'textures', d), exist_ok=True)
BASE = {i['name'] for i in json.load(open(os.path.join(repo, 'data/items.json')))['items']}
TAKEN = set(BASE)
for d in glob.glob(os.path.join(repo, 'mods/*')):
    if os.path.basename(d) != 'natural_disasters' and os.path.exists(os.path.join(d, 'items.json')): TAKEN |= {x['name'] for x in json.load(open(os.path.join(d, 'items.json')))['items']}
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
def item(name, label, price, tier, cat='crafted', **kw):
    assert name not in TAKEN, name; items.append(dict({'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'category': cat, 'price': price, 'tier': tier}, **kw))
def rec(bench, out, ins, count=1):
    for n, _ in ins: assert n in BASE or any(i['name'] == n for i in items), n
    recipes.append({'bench': bench, 'output': {'item': out, 'count': count}, 'inputs': [{'item': i, 'count': n} for i, n in ins], 'time': 60})
ROCK = [(0, 0, 15, 15, '3a3440'), (2, 3, 4, 5, '5a4a6a'), (9, 2, 12, 4, '2a2630'), (6, 8, 8, 10, 'e08040'), (11, 10, 13, 12, '5a4a6a'), (3, 11, 5, 13, '2a2630'), (7, 9, 7, 9, 'ffd070'), (13, 6, 13, 6, 'e08040')]
pic('tiles', 'meteorite', ROCK)
tiles.append({'name': 'meteorite', 'label': 'Meteorite', 'solid': True, 'removable': True, 'mine_time': 1.4, 'texture': 'tiles/meteorite.png', 'shade': 12, 'tool': 'pickaxe', 'light': 4, 'drops': [{'item': 'meteoric_iron', 'count': 2}]})
item('meteorite', 'Meteorite', 400, 4, 'ores', places='meteorite')
pic('items', 'meteoric_iron', [(4, 6, 11, 12, '4a4458'), (6, 4, 10, 6, '4a4458'), (5, 7, 7, 8, '8a80a8'), (9, 10, 10, 11, 'e08040'), (4, 12, 11, 12, '2a2630')]); item('meteoric_iron', 'Meteoric iron', 300, 4, 'ores')
pic('items', 'star_steel_ingot', [(3, 8, 12, 11, '8a90c0'), (4, 7, 11, 7, 'c0c8f0'), (3, 11, 12, 11, '5a6090'), (4, 12, 11, 12, '3a4068'), (5, 8, 5, 8, 'ffffff')]); item('star_steel_ingot', 'Star-steel ingot', 900, 5, 'ores')
rec('furnace', 'star_steel_ingot', [('meteoric_iron', 2), ('coal', 1)])
HANDLE = [(4, 12, 4, 12, '8a6a3c'), (5, 11, 5, 11, '8a6a3c'), (6, 10, 6, 10, '8a6a3c'), (7, 9, 7, 9, '8a6a3c'), (8, 8, 8, 8, '8a6a3c'), (9, 7, 9, 7, '8a6a3c'), (3, 13, 3, 13, '8a6a3c')]
pic('items', 'star_steel_pickaxe', HANDLE + [(5, 3, 12, 4, '8a90c0'), (11, 4, 12, 10, '8a90c0'), (4, 4, 4, 5, '8a90c0'), (12, 11, 12, 11, '5a6090'), (6, 3, 10, 3, 'c0c8f0')]); item('star_steel_pickaxe', 'Star-steel pickaxe', 2400, 5, 'tools', tool={'class': 'pickaxe', 'speed': 7.0}, max_stack=1)
pic('items', 'star_steel_sword', [(3, 12, 4, 13, '8a6a3c'), (4, 10, 6, 11, 'c8a040'), (5, 11, 5, 12, 'c8a040')] + [(5 + k, 9 - k, 6 + k, 10 - k, '8a90c0') for k in range(1, 8)] + [(6 + k, 9 - k, 6 + k, 9 - k, 'c0c8f0') for k in range(1, 8)]); item('star_steel_sword', 'Star-steel sword', 2400, 5, 'tools', damage=14, max_stack=1)
rec('tool_bench', 'star_steel_pickaxe', [('star_steel_ingot', 3), ('stick', 2)]); rec('tool_bench', 'star_steel_sword', [('star_steel_ingot', 2), ('stick', 1)])
pic('tiles', 'storm_rod', [(7, 2, 8, 15, 'c87a4a'), (7, 2, 7, 15, 'e0a070'), (6, 0, 9, 1, 'e0a070'), (7, 0, 8, 0, 'ffffff'), (5, 14, 10, 15, '7a4a2a'), (5, 6, 6, 6, 'c87a4a'), (9, 9, 10, 9, 'c87a4a')])
tiles.append({'name': 'storm_rod', 'label': 'Lightning rod', 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.3, 'drop': 'storm_rod', 'texture': 'tiles/storm_rod.png', 'tool': 'pickaxe', 'wards': 12, 'model': 'panel'})
item('storm_rod', 'Lightning rod', 400, 2, places='storm_rod'); rec('metalwork_bench', 'storm_rod', [('copper_ingot', 3)])
pic('tiles', 'sandbags', [(0, 0, 15, 15, 'c8b080'), (0, 7, 15, 8, '8a7448'), (0, 15, 15, 15, '8a7448'), (7, 0, 8, 7, '8a7448'), (3, 8, 4, 15, '8a7448'), (11, 8, 12, 15, '8a7448'), (1, 1, 5, 1, 'e0cca0'), (9, 1, 13, 1, 'e0cca0'), (5, 9, 9, 9, 'e0cca0')])
tiles.append({'name': 'sandbags', 'label': 'Sandbags', 'solid': True, 'removable': True, 'mine_time': 0.25, 'drop': 'sandbags', 'texture': 'tiles/sandbags.png', 'shade': 10, 'tool': 'shovel'})
item('sandbags', 'Sandbags', 30, 1, 'blocks', places='sandbags'); rec('tailor_bench', 'sandbags', [('flax', 1), ('sand', 2)], 2)
DISASTERS = {'period_ticks': 300, 'list': [
    {'name': 'thunderstorm', 'label': 'Thunderstorm', 'chance': 0.02, 'ticks': 600, 'every': 40, 'reach': 30, 'bolt': True, 'burn': 1, 'hurt': 0.2, 'hurt_radius': 1},
    {'name': 'meteor_shower', 'label': 'Meteor shower', 'chance': 0.008, 'time': 'night', 'ticks': 300, 'every': 60, 'reach': 40, 'blast': 2, 'leaves': 'meteorite', 'hurt': 0.2, 'hurt_radius': 3},
    {'name': 'earthquake', 'label': 'Earthquake', 'chance': 0.008, 'ticks': 200, 'every': 10, 'reach': 25, 'breaks': 1, 'depth': [1, 24]},
    {'name': 'flash_flood', 'label': 'Flash flood', 'chance': 0.008, 'ticks': 300, 'every': 10, 'reach': 20, 'water': 8},
    {'name': 'wildfire', 'label': 'Wildfire', 'chance': 0.006, 'time': 'day', 'ticks': 300, 'every': 15, 'reach': 25, 'burn': 2},
    {'name': 'hailstorm', 'label': 'Hailstorm', 'chance': 0.01, 'ticks': 300, 'every': 30, 'reach': 10, 'exposed': True, 'hurt': 0.03}]}
def out(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v)) for k, v in obj.items()) + '\n}\n')
out('tiles.json', {'tiles': tiles}); out('items.json', {'items': items}); out('recipes.json', {'recipes': recipes})
json.dump({'disasters': DISASTERS}, open(os.path.join(mod, 'rules.json'), 'w'), indent=1)
json.dump({'label': 'Natural Disasters', 'icon': 'icon.png', 'utility': True, 'description': 'Now and then: a thunderstorm, a meteor shower, an earthquake, a flash flood, a wildfire, hail. Lightning rods and sandbags against them, and star-steel from what falls.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
ic = Image.new('RGBA', (64, 64), c('2a2c3c')); px = ic.load()
for k, (x, y) in enumerate([(36, 6), (30, 22), (38, 22), (26, 42), (34, 42), (24, 58)]):
    if k: 
        x0, y0 = [(36, 6), (30, 22), (38, 22), (26, 42), (34, 42), (24, 58)][k - 1]
        for t in range(40):
            xx, yy = int(x0 + (x - x0) * t / 39), int(y0 + (y - y0) * t / 39)
            for a in range(-1, 2): px[min(63, max(0, xx + a)), yy] = (255, 232, 120, 255)
ic.save(os.path.join(mod, 'icon.png'))
print('%d blocks, %d items, %d recipes, %d disasters' % (len(tiles), len(items), len(recipes), len(DISASTERS['list'])))
