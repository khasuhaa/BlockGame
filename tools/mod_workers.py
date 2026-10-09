#!/usr/bin/env python3
"""Writes mods/workers: seven posts, each with a worker who does one job out of the post's chest - miner, farmer,
lumberjack, dodo breeder, fisher, gatherer, guard - and the hired hand, boundary stake and mine ladder that go with
them. The engine half is src/workers.c. Runs caged_art.py and mob_models.py for the workers themselves.

    python3 tools/mod_workers.py .
"""
import glob, json, os, subprocess, sys
from PIL import Image
repo = sys.argv[1] if len(sys.argv) > 1 else '.'
mod = os.path.join(repo, 'mods/workers')
for d in ('tiles', 'items', 'mobs'): os.makedirs(os.path.join(mod, 'textures', d), exist_ok=True)
for f in glob.glob(os.path.join(mod, 'textures/mobs/*.png')) + glob.glob(os.path.join(mod, 'textures/items/caged_*.png')): os.remove(f)   # (drawn again below, and by the tools)
BASE = {i['name'] for i in json.load(open(os.path.join(repo, 'data/items.json')))['items']}
TAKEN = set(BASE)
for d in glob.glob(os.path.join(repo, 'mods/*')):
    if os.path.basename(d) != 'workers' and os.path.exists(os.path.join(d, 'items.json')): TAKEN |= {x['name'] for x in json.load(open(os.path.join(d, 'items.json')))['items']}
tiles, items, recipes, mobs = [], [], [], []
def c(s): return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
def pic(kind, name, rects):
    im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); px = im.load()
    for x0, y0, x1, y1, col in rects:
        for y in range(max(0, y0), min(15, y1) + 1):
            for x in range(max(0, x0), min(15, x1) + 1): px[x, y] = c(col)
    if kind == 'items': im.resize((48, 48), Image.NEAREST).save(os.path.join(mod, 'textures/items', name + '.png')); return
    im.resize((32, 32), Image.NEAREST).save(os.path.join(mod, 'textures', kind, name + '.png'))
    if kind == 'tiles': icon = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); icon.paste(im.resize((32, 32), Image.NEAREST), (8, 8)); icon.save(os.path.join(mod, 'textures/items', name + '.png'))
def rec(bench, out, ins, count=1):
    for n, _ in ins: assert n in BASE or any(i['name'] == n for i in items), n
    recipes.append({'bench': bench, 'output': {'item': out, 'count': count}, 'inputs': [{'item': i, 'count': n} for i, n in ins], 'time': 80})
W, D = 'a8824e', '6e5230'
def person(coat, hat, step):
    return [(6, 2, 9, 5, 'e0b890'), (7, 4, 7, 4, '20202a'), (9, 4, 9, 4, '20202a'), (5, 1, 10, 2, hat), (6, 0, 9, 0, hat), (5, 6, 10, 11, coat), (3, 6, 4, 9 - step, coat), (11, 6, 12, 8 + step, coat), (5, 9, 10, 9, D),
            (5 + step, 12, 6 + step, 14, '3a3a48'), (9 - step, 12, 10 - step, 14, '3a3a48')]
POSTS = [  # key, label, the worker's label, coat, hat, the emblem on the post, work, what the post is made of
    ('miner', "Miner's post", 'Miner', '6a6e78', 'e8c040', [(4, 3, 11, 4, '9aa4b0'), (7, 4, 8, 9, W)], {'kind': 'mine', 'reach': 48, 'ticks': 30, 'ladder': 'mine_ladder'}, [('wood', 4), ('stone', 4)]),
    ('farmer', "Farmer's post", 'Farmer', '5a8a4a', 'e0c070', [(7, 3, 8, 9, W), (5, 2, 6, 5, 'e0c050'), (9, 2, 10, 5, 'e0c050')], {'kind': 'farm', 'reach': 16, 'ticks': 30}, [('wood', 4), ('wheat', 4)]),
    ('lumberjack', "Lumberjack's post", 'Lumberjack', 'b04030', '5a3a20', [(7, 3, 8, 9, W), (8, 3, 11, 6, '9aa4b0')], {'kind': 'lumber', 'reach': 16, 'ticks': 60}, [('wood', 8)]),
    ('breeder', "Dodo breeder's post", 'Dodo breeder', 'c89a50', 'f0f0e0', [(5, 4, 10, 9, 'f0e8d8'), (6, 3, 9, 3, 'f0e8d8')], {'kind': 'breed', 'reach': 10, 'ticks': 60, 'flock': 6}, [('wood', 4), ('feather', 4)]),
    ('fisher', "Fisher's post", 'Fisher', '3a6aa0', 'c8a868', [(4, 2, 4, 9, W), (5, 2, 11, 2, W), (11, 3, 11, 7, 'e0e0e0'), (10, 8, 12, 9, '60a0e0')], {'kind': 'fish', 'reach': 8, 'ticks': 200, 'catch': [{'item': 'fish', 'weight': 8}, {'item': 'kelp', 'weight': 2}]}, [('wood', 4), ('string', 4)]),
    ('gatherer', "Gatherer's post", 'Gatherer', '8a6a9a', '6a4a2a', [(5, 4, 10, 9, 'c8a868'), (5, 4, 10, 4, 'e0c890'), (7, 2, 8, 3, D)], {'kind': 'gather', 'reach': 14, 'ticks': 20}, [('wood', 4), ('basket', 1)]),
    ('guard', "Guard's post", 'Guard', '4a4a6a', '9aa4b0', [(7, 1, 8, 8, 'c0c8d0'), (5, 8, 10, 9, 'c8a040'), (7, 10, 8, 11, D)], {'kind': 'guard', 'reach': 12, 'ticks': 20}, [('wood', 4), ('iron_ingot', 2)]),
]
for key, label, who, coat, hat, emblem, work, ins in POSTS:
    post, mob_name = key + '_post', 'hired_' + key
    assert post not in TAKEN and mob_name not in TAKEN
    pic('tiles', post, [(2, 10, 13, 15, W), (2, 10, 13, 10, 'c8a868'), (2, 15, 13, 15, D), (7, 12, 8, 13, 'c8a040'), (1, 0, 14, 9, '00000000'[:6])][:4] + [(3, 0, 12, 9, 'e8dcc0'), (3, 0, 12, 0, D), (3, 9, 12, 9, D), (3, 0, 3, 9, D), (12, 0, 12, 9, D)] + emblem)
    tiles.append({'name': post, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.4, 'drop': post, 'texture': 'tiles/%s.png' % post, 'tool': 'axe', 'storage': {'columns': 4, 'rows': 3}, 'work': dict(work, mob=mob_name)})
    items.append({'name': post, 'label': label, 'places': post, 'texture': 'items/%s.png' % post, 'category': 'crafted', 'price': 300, 'tier': 2, 'container': {'slots': 12}}); rec('woodwork_bench', post, ins)
    for step in (0, 1): pic('mobs', mob_name + ('_walk' if step else ''), person(coat, hat, step))
    mobs.append({'name': mob_name, 'label': who, 'texture': 'mobs/%s.png' % mob_name, 'walk': ['mobs/%s_walk.png' % mob_name, 'mobs/%s.png' % mob_name], 'hop': 0.0, 'step_ticks': 4, 'move_chance': 0.0, 'turn_chance': 0.0, 'health': 40, 'worker': True, 'shape': 'person', 'drops': []})
pic('tiles', 'mine_ladder', [(4, 0, 5, 15, W), (10, 0, 11, 15, W), (4, 2, 11, 2, D), (4, 6, 11, 6, D), (4, 10, 11, 10, D), (4, 14, 11, 14, D)])
tiles.append({'name': 'mine_ladder', 'label': 'Mine ladder', 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.1, 'drop': 'mine_ladder', 'texture': 'tiles/mine_ladder.png', 'tool': 'axe', 'climbable': True, 'attaches': True})
items.append({'name': 'mine_ladder', 'label': 'Mine ladder', 'places': 'mine_ladder', 'texture': 'items/mine_ladder.png', 'category': 'crafted', 'price': 10, 'tier': 1}); rec('woodwork_bench', 'mine_ladder', [('stick', 3)], 3)
pic('tiles', 'boundary_stake', [(7, 4, 8, 15, W), (7, 4, 7, 15, 'c8a868'), (8, 4, 12, 7, 'c84a3a'), (8, 4, 12, 4, 'e06a5a')])
tiles.append({'name': 'boundary_stake', 'label': 'Boundary stake', 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.1, 'drop': 'boundary_stake', 'texture': 'tiles/boundary_stake.png', 'tool': 'axe', 'work_marker': True, 'model': 'panel'})
items.append({'name': 'boundary_stake', 'label': 'Boundary stake', 'places': 'boundary_stake', 'texture': 'items/boundary_stake.png', 'category': 'crafted', 'price': 10, 'tier': 1}); rec('woodwork_bench', 'boundary_stake', [('stick', 2)], 2)
pic('items', 'hired_hand', person('8a6a4a', 'c84a3a', 0) + [(11, 9, 14, 12, 'efe2b8'), (12, 10, 13, 10, '20202a')])
items.append({'name': 'hired_hand', 'label': 'Hired hand', 'texture': 'items/hired_hand.png', 'category': 'crafted', 'price': 900, 'tier': 3, 'hires': True, 'max_stack': 1}); rec('craft_bench', 'hired_hand', [('bread', 3), ('linen_shirt', 1)])
def out(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v)) for k, v in obj.items()) + '\n}\n')
out('tiles.json', {'tiles': tiles}); out('items.json', {'items': items}); out('recipes.json', {'recipes': recipes}); out('mobs.json', {'mobs': mobs})
json.dump({'workers': {'meal_steps': 60}}, open(os.path.join(mod, 'rules.json'), 'w'), indent=1)
json.dump({'label': 'Workers', 'icon': 'icon.png', 'utility': True, 'description': 'Posts with workers: a miner who strip-mines from a laddered shaft, a farmer for a staked patch, a lumberjack, a dodo breeder, a fisher, a gatherer and a guard. Each works out of its post\'s chest once a hired hand is put in it.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
ic = Image.new('RGBA', (64, 64), c('2a3040')); px = ic.load()
for x0, y0, x1, y1, col in person('6a6e78', 'e8c040', 0):
    for y in range(y0 * 4, y1 * 4 + 4):
        for x in range(x0 * 4, x1 * 4 + 4): px[x, y] = c(col)
ic.save(os.path.join(mod, 'icon.png'))
# A cage and a body for each worker, from the tools every mod uses: whatever of anybody else's they change is put back.
kept = {f: open(f, 'rb').read() for f in glob.glob(os.path.join(repo, 'data', '*.json')) + glob.glob(os.path.join(repo, 'mods', '*', '*.json')) if os.sep + 'workers' + os.sep not in f}
for tool in ('caged_art.py', 'mob_models.py'): subprocess.run([sys.executable, os.path.join(repo, 'tools', tool), repo], check=True, stdout=subprocess.DEVNULL)
for f, was in kept.items():
    if open(f, 'rb').read() != was: open(f, 'wb').write(was)
print('%d blocks, %d items, %d recipes, %d workers' % (len(tiles), len(items), len(recipes), len(mobs)))
