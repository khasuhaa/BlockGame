#!/usr/bin/env python3
"""Writes mods/skyblock: a new world is empty sky with one small island in it, and everything the game - and every
mod it comes with - would have given from the ground, the sea, its creatures and its chests can be had another way.

    python3 tools/mod_skyblock.py .

RERUN IT WHEN ANY MOD'S ITEMS OR RECIPES CHANGE: what has to be given is worked out here from the data. For the
game and then for each mod in turn it finds every item that could not be come by starting from a tree and a patch
of dirt, and gives it: by chance at the sieve, and (the game's own) for certain at the lodestone table. Creatures
are hatched. At the end it asserts that nothing of the game's is left out of reach."""
import glob, json, os, sys
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
DATA, mod = os.path.join(repo, 'data'), os.path.join(repo, 'mods/skyblock')
for d in ('tiles', 'items'): os.makedirs(os.path.join(mod, 'textures', d), exist_ok=True)
load = lambda d, f, k: json.load(open(os.path.join(d, f + '.json'))).get(k, []) if os.path.exists(os.path.join(d, f + '.json')) else []
FREE = {'benches', 'portals', 'creatures', 'special'}
tiles, items, recipes, structures = [], [], [], []

# ---- pictures: 16 by 16, drawn double -------------------------------------------------------------------------------
def c(s): return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
def pic(kind, name, rects):
    im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); px = im.load()
    for x0, y0, x1, y1, col in rects:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1): px[x, y] = c(col) if col else (0, 0, 0, 0)
    if kind == 'tiles':
        im.resize((32, 32), Image.NEAREST).save(os.path.join(mod, 'textures/tiles', name + '.png'))
        icon = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); icon.paste(im.resize((32, 32), Image.NEAREST), (8, 8)); icon.save(os.path.join(mod, 'textures/items', name + '.png'))
    else: im.resize((48, 48), Image.NEAREST).save(os.path.join(mod, 'textures/items', name + '.png'))
W, D, S, I_, G = '9a7444', '6a4e2c', '7c7e84', '5a5e68', 'c8a040'
LEGS = [(2, 11, 3, 15, D), (12, 11, 13, 15, D)]
pic('tiles', 'cloud_condenser', LEGS + [(1, 9, 14, 10, W), (4, 2, 11, 8, 'e8f0f8'), (5, 1, 9, 1, 'ffffff'), (3, 4, 12, 6, 'f4f8fc'), (6, 7, 9, 8, 'c0d0e0'), (7, 9, 8, 9, '80c0f0')])
pic('tiles', 'sky_sieve', LEGS + [(1, 5, 14, 6, W), (1, 9, 14, 10, W), (1, 5, 2, 10, D), (13, 5, 14, 10, D)] + [(x, 7, x, 8, 'c8c0a0') for x in range(3, 13, 2)])
pic('tiles', 'lodestone_table', LEGS + [(1, 9, 14, 10, S), (5, 3, 10, 8, '3a3e5a'), (6, 2, 9, 2, '5a60a0'), (7, 5, 8, 6, 'c080f0'), (5, 8, 10, 8, '2a2c40')])
pic('tiles', 'sky_hatchery', [(2, 12, 13, 15, W), (2, 12, 13, 12, 'c8a060'), (5, 4, 10, 11, 'f0e8d8'), (6, 3, 9, 3, 'f0e8d8'), (6, 6, 7, 7, 'd0c0a0'), (9, 9, 9, 10, 'd0c0a0'), (3, 11, 12, 11, 'e0c060')])
pic('tiles', 'sky_composter', [(2, 4, 13, 15, W), (2, 4, 2, 15, D), (13, 4, 13, 15, D), (7, 4, 8, 15, D), (3, 2, 12, 4, '4a3420'), (5, 1, 6, 2, '5a9a48'), (9, 2, 10, 2, '5a9a48'), (2, 9, 13, 9, D)])
pic('tiles', 'sky_well', [(0, 0, 15, 15, None), (2, 2, 13, 3, S), (2, 2, 3, 15, S), (12, 2, 13, 15, S), (4, 6, 11, 15, '3a78c0'), (4, 6, 11, 6, '80b8f0'), (7, 0, 8, 2, I_)])
pic('tiles', 'void_floor', [(0, 0, 15, 15, '14161e'), (3, 4, 4, 4, '2a2e44'), (10, 11, 11, 11, '2a2e44'), (13, 2, 13, 2, '3a4060')])
pic('items', 'sky_dust', [(4, 9, 11, 12, 'd8e4f0'), (5, 7, 9, 8, 'e8f0f8'), (6, 12, 12, 13, 'b0c0d4'), (7, 6, 7, 6, 'ffffff'), (11, 8, 11, 8, 'ffffff')])
pic('items', 'star_grit', [(4, 8, 11, 12, 'e8c860'), (5, 6, 9, 7, 'f8e080'), (6, 12, 12, 13, 'b89030'), (7, 5, 7, 5, 'ffffff'), (3, 10, 3, 10, 'ffffff'), (12, 7, 12, 7, 'ffffff')])
pic('items', 'void_essence', [(5, 4, 10, 12, '3a2e6a'), (4, 6, 11, 10, '3a2e6a'), (6, 5, 8, 7, '8060d0'), (7, 6, 7, 6, 'f0e0ff'), (9, 10, 10, 11, '241c48')])

def bench(name, label):
    tiles.append({'name': name, 'label': label, 'solid': False, 'removable': True, 'mine_time': 0.4, 'drop': name, 'texture': 'tiles/%s.png' % name, 'bench': True, 'bench_kind': name, 'bench_level': 1, 'popup_name': 'bench', 'tool': 'axe', 'layer': 'background'})
    items.append({'name': name, 'label': label, 'places': name, 'texture': 'items/%s.png' % name, 'category': 'benches', 'price': 10})
def rec(b, out, ins, count=1, time=60, **kw): recipes.append(dict({'bench': b, 'output': out if isinstance(out, dict) else {'item': out, 'count': count}, 'inputs': [{'item': i, 'count': n} for i, n in ins], 'time': time}, **kw))
for n, lab, ins in (('cloud_condenser', 'Cloud condenser', [('wood', 4), ('dirt', 2)]), ('sky_sieve', 'Sieve', [('wood', 4), ('stick', 6)]), ('sky_composter', 'Composter', [('wood', 6)]),
                    ('sky_hatchery', 'Hatchery', [('wood', 6), ('sky_dust', 20)]), ('lodestone_table', 'Lodestone table', [('wood', 6), ('star_grit', 6)])):
    bench(n, lab); rec('workbench', n, ins, time=20)
tiles.append({'name': 'sky_well', 'label': 'Well', 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.4, 'drop': 'sky_well', 'texture': 'tiles/sky_well.png', 'tool': 'pickaxe', 'spring': 1})
items.append({'name': 'sky_well', 'label': 'Well', 'places': 'sky_well', 'texture': 'items/sky_well.png', 'category': 'crafted', 'price': 200, 'tier': 2})
tiles.append({'name': 'void_floor', 'label': 'The bottom of the sky', 'solid': True, 'texture': 'tiles/void_floor.png'})
for n, lab, price, tier in (('sky_dust', 'Sky dust', 10, 1), ('star_grit', 'Star grit', 90, 2), ('void_essence', 'Void essence', 800, 4)): items.append({'name': n, 'label': lab, 'texture': 'items/%s.png' % n, 'category': 'crafted', 'price': price, 'tier': tier})
rec('cloud_condenser', 'sky_dust', [], time=100); rec('cloud_condenser', 'star_grit', [('sky_dust', 8)]); rec('cloud_condenser', 'void_essence', [('star_grit', 8)])
rec('sky_composter', 'dirt', [('stick', 6)]); rec('sky_composter', 'dirt', [('wood', 2)]); rec('lodestone_table', 'sky_well', [('star_grit', 4), ('dirt', 4)])

# ---- what has to be given: worked out from the data --------------------------------------------------------------------
SEED = {'wood', 'stick', 'dirt', 'acorn'}   # (a tree and the ground it stands in: both come again)
def outs(r): return [r['output']['item']] if 'item' in r['output'] else [o['item'] for o in r['output'].get('one_of', [])]
def reach(recs):
    got, more = set(SEED), True
    while more:
        more = False
        for r in recs:
            if all(i['item'] in got for i in r.get('inputs', [])) and all(n in got for n in r.get('random_inputs', {}).get('of', [])):
                for o in outs(r):
                    if o not in got: got.add(o); more = True
    return got
def cost(price):
    return [('sky_dust', max(2, price // 12))] if price <= 60 else [('star_grit', max(2, price // 110))] if price <= 700 else [('void_essence', max(2, min(40, price // 1200)))]
def band(price): return 0 if price <= 60 else 1 if price <= 700 else 2 if price <= 6000 else 3
BAND_COST = [[('sky_dust', 3)], [('star_grit', 3)], [('void_essence', 2)], [('void_essence', 12)]]
def uncovered(want, recs):
    """Every wanted item that cannot be come by, given in the fewest gifts: first what nothing makes at all."""
    given, recs = [], list(recs)
    while True:
        got = reach(recs); miss = [i for i in want if i['name'] not in got]
        if not miss: return given
        made = {o for r in recs for o in outs(r)}
        pick = [i for i in miss if i['name'] not in made] or [min(miss, key=lambda i: i.get('price', 10))]
        given += pick; recs += [{'output': {'item': i['name']}, 'inputs': []} for i in pick]
def pools(b, given, **kw):
    for k in range(4):
        group = [i for i in given if band(i.get('price', 10)) == k]
        for a in range(0, len(group), 40): rec(b, {'one_of': [{'item': i['name'], 'weight': 1} for i in group[a:a + 40]], 'count': 1}, BAND_COST[k], time=40, **kw)
base_items, base_recipes = load(DATA, 'items', 'items'), load(DATA, 'recipes', 'recipes')
for n in SEED: assert any(i['name'] == n for i in base_items), n
want = [i for i in base_items if i.get('category') and i['category'] not in FREE]
given = uncovered(want, base_recipes + recipes)
pools('sky_sieve', given)
for i in given: rec('lodestone_table', i['name'], [(n, k * 4) for n, k in cost(i.get('price', 10))], time=80)
base_mobs = [m for m in load(DATA, 'mobs', 'mobs') if m.get('caged')]
for m in base_mobs: rec('sky_hatchery', m['caged'], [('star_grit', 6), ('dirt', 2)], time=200)
left = [i['name'] for i in want if i['name'] not in reach(base_recipes + recipes)]
assert not left, 'still out of reach: %s' % left
print('the game: %d of %d things given (%s...), %d creatures hatched' % (len(given), len(want), ', '.join(i['name'] for i in given[:8]), len(base_mobs)))
# Every mod the game comes with, the same way: its recipes here are "optional", and are simply not there when it is off.
mine = list(recipes)
for d in sorted(glob.glob(os.path.join(repo, 'mods/*'))):
    name = os.path.basename(d)
    if name in ('skyblock', 'questbook') or json.load(open(os.path.join(d, 'mod.json'))).get('standalone'): continue
    its, names = load(d, 'items', 'items'), {i['name'] for i in base_items}
    its = [i for i in its if i['name'] not in names]
    # (What is rolled is a dungeon's loot, and the dungeons are still there to be gone into: that is not given.)
    g = uncovered([i for i in its if i.get('category') and i['category'] not in FREE and 'rolled' not in i], base_recipes + load(d, 'recipes', 'recipes') + mine)
    pools('sky_sieve', g, optional=True)
    cages = [{'name': m['caged'], 'price': 100} for m in load(d, 'mobs', 'mobs') if m.get('caged') and not any(b['name'] == m['name'] for b in base_mobs)]
    for a in range(0, len(cages), 40): rec('sky_hatchery', {'one_of': [{'item': i['name'], 'weight': 1} for i in cages[a:a + 40]], 'count': 1}, [('star_grit', 8), ('dirt', 2)], time=200, optional=True)
    print('%s: %d things given, %d creatures hatched' % (name, len(g), len(cages)))

# ---- the islands ---------------------------------------------------------------------------------------------------------
BT = {t['name'] for t in load(DATA, 'tiles', 'tiles')}
def built(name, rows, legend):
    for v in legend.values():
        for t in v.values(): assert t in BT, t
    structures.append({'name': name, 'rows': rows, 'legend': legend})
F = lambda t: {'front': t}
built('sky_home', ['~~~~~~LLL~~', '~~~~~LLLLL~', '~~~~~LLTLL~', '~~~~~~~T~~~', '~~~~~~~T~~~', 'GGGGGGGGGGG', 'DDDDDDDDDDD', '~DDSSSSSDD~', '~~DSSSSSD~~', '~~~~SSS~~~~'],
      {'L': {'back': 'oak_leaves'}, 'T': {'back': 'oak_log'}, 'G': F('grass'), 'D': F('dirt'), 'S': F('stone')})
built('sky_sand_isle', ['AAAAAAA', 'ABBBBBA', '~BBBBB~', '~~BBB~~'], {'A': F('sand'), 'B': F('sandstone')})
built('sky_ore_isle', ['SSSSSSSSS', 'SCSSTSSKS', '~SSISSCS~', '~~STSKS~~', '~~~SSS~~~'], {'S': F('stone'), 'C': F('copper_ore'), 'T': F('tin_ore'), 'I': F('iron_ore'), 'K': F('coal_ore')})
built('sky_clay_isle', ['~~~X~~~', 'GGGGGGG', 'DCCDCCD', '~DCCCD~', '~~DDD~~'], {'G': F('grass'), 'D': F('dirt'), 'C': F('clay'), 'X': {'back': 'golden_chest'}})
built('sky_ice_isle', ['IIIIIII', 'IIIIIII', '~IIIII~', '~~III~~'], {'I': F('ice')})
built('sky_marble_isle', ['~~A~A~~', 'MMMMMMM', 'MLMMMLM', '~MMLMM~', '~~MMM~~'], {'M': F('marble'), 'L': F('limestone'), 'A': {'back': 'amethyst'}})
built('sky_gold_isle', ['~~~X~~~', 'SSSSSSS', 'SGSFSGS', '~SSGSS~', '~~SSS~~'], {'S': F('stone'), 'G': F('gold_ore'), 'F': F('flint'), 'X': {'back': 'golden_chest'}})
worldgen = {'skyblock': {'island': 'sky_home', 'floor': 'void_floor', 'gap': [45, 90], 'islets': ['sky_sand_isle', 'sky_ore_isle', 'sky_clay_isle', 'sky_marble_isle', 'sky_ice_isle', 'sky_gold_isle', 'sky_ore_isle', 'sky_sand_isle', 'sky_clay_isle', 'sky_gold_isle']}, 'structures': structures}

def out(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v)) for k, v in obj.items()) + '\n}\n')
json.dump({'label': 'Skyblock', 'icon': 'icon.png', 'own_quests': True, 'description': 'A new world is empty sky and one small island. A condenser, a sieve, a lodestone table and a hatchery give everything the ground, the sea and their creatures would have - the game\'s, and every mod\'s.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
# ---- a book of quests: the mod's own (tools/quests.py leaves it alone), a tree as every tab is ---------------------------------
HAVE = {i['name'] for i in base_items} | {i['name'] for i in items}
BOOK = [('The condenser', 'Made at the workbench. It makes dust of nothing.', {'near': 'cloud_condenser'}, 'cloud_condenser', [('sky_dust', 10)], [('sky_dust', 16), ('star_grit', 4), ('void_essence', 1), ('dirt', 8)]),
        ('The sieve', 'Dust in, something of the ground out.', {'near': 'sky_sieve'}, 'sky_sieve', [('sky_dust', 12)], [('stone', 8), ('sand', 4), ('clay', 4), ('coal', 4), ('copper_ore', 2), ('tin_ore', 2), ('iron_ore', 2), ('gold_ore', 1), ('ice', 2)]),
        ('What grows', 'Seeds and fruit come through the sieve too.', {'have': 'apple'}, 'apple', [('star_grit', 2)], [('wheat', 3), ('carrot', 3), ('corn', 3), ('tomato', 3), ('flax', 3), ('pine_cone', 1), ('coconut', 1), ('cherry', 2), ('orange', 2), ('mango', 1)]),
        ('The lodestone table', 'What you want, for certain, at four times the price.', {'near': 'lodestone_table'}, 'lodestone_table', [('star_grit', 4)], [('amethyst_shard', 1), ('sapphire_shard', 1), ('emerald_shard', 1), ('ruby_shard', 1), ('diamond_shard', 1), ('basalt', 2)]),
        ('The hatchery', 'A caged creature, to let out.', {'near': 'sky_hatchery'}, 'sky_hatchery', [('star_grit', 4)], [('caged_dodo', 1), ('caged_fish', 1), ('caged_yak', 1), ('caged_scorpion', 1), ('caged_shark', 1), ('feather', 4), ('leather', 2), ('milk', 1)]),
        ('Water', 'A well shut in by a block either side fills by itself.', {'near': 'sky_well'}, 'sky_well', [('sky_dust', 10)], [('fish', 2), ('kelp', 2), ('golden_chest', 1), ('marble', 4), ('sandstone', 4)])]
tabs = [{'name': 'skyblock', 'label': 'Skyblock', 'order': 30, 'icon': 'cloud_condenser'}]
quests = [{'name': 'skyblock.start', 'tab': 'skyblock', 'label': 'An island', 'text': 'A tree, some dirt, and a long way down.', 'task': {'check': True}, 'rewards': [{'item': 'acorn', 'count': 3}, {'item': 'dirt', 'count': 6}], 'icon': 'cloud_condenser', 'at': [0, 0], 'hub': True}]
for k, (lab, text, task, icon, rew, kids) in enumerate(BOOK):
    head = 'skyblock.%d' % k
    quests.append({'name': head, 'tab': 'skyblock', 'label': lab, 'text': text, 'needs': ['skyblock.start'], 'task': task, 'rewards': [{'item': i, 'count': n} for i, n in rew] + [{'item': 'acorn', 'count': 1}], 'at': [2 + 4 * k, 0], 'hub': True, 'pick': True, 'crystals': 5, 'icon': icon})
    for j, (n, cnt) in enumerate([kd for kd in kids if kd[0] in HAVE]):
        lab_ = next(i['label'] for i in base_items + items if i['name'] == n)
        quests.append({'name': '%s.%s' % (head, n), 'tab': 'skyblock', 'label': lab_, 'text': 'Come by it in the sky.', 'needs': [head], 'task': {'have': n, 'count': cnt} if cnt > 1 else {'have': n}, 'rewards': [{'item': 'star_grit' if k > 2 else 'sky_dust', 'count': 4}, {'item': n, 'count': 1}], 'at': [2 + 4 * k + j % 3, 1 + j // 3], 'icon': n})
out('quests.json', {'tabs': tabs, 'quests': quests}); print('%d quests' % len(quests))
out('tiles.json', {'tiles': tiles}); out('items.json', {'items': items}); out('recipes.json', {'recipes': recipes}); out('worldgen.json', worldgen)
ic = Image.new('RGBA', (64, 64), c('87bee8')); px = ic.load()
for x in range(14, 50):
    for y in range(30, 34): px[x, y] = c('5a9a48')
    for y in range(34, 40 + min(x - 14, 49 - x) // 2): px[x, y] = c('6a4e2c') if y < 38 else c('7c7e84')
for x in range(34, 46):
    for y in range(12, 22): px[x, y] = c('3a7a3a')
for y in range(20, 30): px[39, y] = px[40, y] = c('6a4e2c')
ic.save(os.path.join(mod, 'icon.png'))
print('%d blocks, %d items, %d recipes, %d structures' % (len(tiles), len(items), len(recipes), len(structures)))
