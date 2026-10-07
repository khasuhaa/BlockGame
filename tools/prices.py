"""Sets a buying price on every item: raw things by how hard they are to come by, made things by what goes into them."""
import json, sys, os, math
REPO = sys.argv[1]; D = os.path.join(REPO, 'data')
items = json.load(open(D + '/items.json'))['items']; I = {i['name']: i for i in items}
recipes = json.load(open(D + '/recipes.json'))['recipes']
tiles = json.load(open(D + '/tiles.json'))['tiles']; T = {t['name']: t for t in tiles}
trade = json.load(open(D + '/trade.json')); coins = {c['item'] for c in trade['currencies']}
RAW = dict(dirt=10, sand=10, black_sand=25, stone=10, flint=15, clay=15, wood=20, stick=10, coal=30,
           copper_ore=40, tin_ore=40, iron_ore=80, gold_ore=200, platinum_ore=600, titanium_ore=500,
           limestone=20, marble=40, lapis_lazuli=120, red_marble=100, sandstone=15, ice=20, basalt=30,
           amethyst=300, sapphire=600, emerald=1200, ruby=2400, diamond=5000,
           acorn=10, cherry=15, apple=15, orange=15, prickly_pear=15, coffee_bean=20, mango=20, coconut=20,
           pine_cone=10, maple_seed=10, sunflower_seed=10, carrot=15, wheat=15, tomato=15, chilli=15,
           flax=15, flax_seeds=10, kelp=10, coral=20, cactus=15, tulip_bulb=30, tulip_seed=20,
           dodo_meat=30, feather=15, fish=30, fur=60, leather=60, yak_horn=80, yak_shavings=30, milk=30,
           worms=20, vine=15, corn=15, poison=60, shark_head=500, golden_chest=1500, dodo_egg=50,
           carbon=40, fuel=60, pig_iron=120, silicon_crystal=200, carbon_fiber_sheet=400, platinum_ingot=2400, titanium_ingot=2000)
# a dodo's egg is worth about what the bird will lay down
for kind, base in dict(limestone=200, sandstone=250, marble=400, lapis=900, red_marble=1200, stone=100, dirt=100, wood=150, flint=150, copper=300, tin=300, iron=500, gold=1000, titanium=2000,
                       platinum=2500, amethyst=1500, sapphire=2500, emerald=4000, ruby=7000, diamond=12000).items():
    RAW[kind + '_dodo_egg'] = base
for g in ('amethyst', 'sapphire', 'emerald', 'ruby', 'diamond'): RAW[g + '_shard'] = RAW[g] // 3 - RAW[g] // 30
made = {}
for r in recipes:
    if 'item' in r['output']: made.setdefault(r['output']['item'], []).append(r)   # (not what only comes out by chance)
memo = {}
def nice(v):
    v = max(10.0, v)
    step = 5 if v < 100 else 10 if v < 1000 else 50 if v < 10000 else 500
    return int(round(v / step) * step)
def price(n, depth=0):
    if n in memo: return memo[n]
    if n in RAW: memo[n] = RAW[n]; return RAW[n]
    if depth > 30: return 50
    memo[n] = 50   # (while it is being worked out: what a loop in the recipes comes back to)
    best = None
    for r in made.get(n, []):
        if any(i['item'] in coins for i in r.get('inputs', [])): continue   # (melting coins down is not what a thing is worth)
        cost = sum(price(i['item'], depth + 1) * i['count'] for i in r.get('inputs', []))
        if 'random_inputs' in r: cost += sum(price(x, depth + 1) for x in r['random_inputs']['of']) / len(r['random_inputs']['of']) * r['random_inputs']['count']
        each = (cost * 1.15 + r.get('time', 0) * 0.5) / r['output'].get('count', 1)
        if best is None or each < best: best = each
    if best is None:
        # a bench raised a tier: the bench below it, and what the raising takes
        p = I.get(n, {}).get('places')
        below = next((t for t in tiles if isinstance(p, str) and t.get('upgrade', {}).get('to') == p), None)
        if below:
            lower = next((i['name'] for i in items if i.get('places') == below['name']), None)
            best = (price(lower, depth + 1) if lower else 50) + sum(price(c['item'], depth + 1) * c['count'] for c in below['upgrade'].get('cost', []))
        else:
            best = I.get(n, {}).get('price', 50)   # (nothing says: what it was given by hand, if anything)
    memo[n] = nice(best)
    return memo[n]
out = []
for i in items:
    n = i['name']
    if n in coins or i.get('crystals') or i.get('becomes_player'):
        i.pop('price', None)
    else:
        i['price'] = price(n)
    out.append(i)
open(D + '/items.json', 'w').write('{\n  "items": [\n' + ',\n'.join('    ' + json.dumps(r) for r in out) + '\n  ]\n}\n')
ps = sorted((i['price'], i.get('label', i['name'])) for i in out if 'price' in i)
print(len(ps), 'priced; cheapest', ps[:3], 'dearest', ps[-4:])
for n in ('copper_ingot', 'bronze_ingot', 'iron_ingot', 'steel_ingot', 'bronze_sword', 'titanium_pickaxe', 'torch', 'pizza', 'wood_stairs', 'workbench', 'furnace', 'paint', 'copper_wire', 'flywheel', 'iron_chestplate', 'bucket'):
    if n in I: print(' ', n, I[n].get('price'))
