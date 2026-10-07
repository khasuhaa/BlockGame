"""Works out, from the game's data files, what can be got and what is for anything; writes obtainment.txt."""
import json, os, sys, re
REPO = sys.argv[1]; D = os.path.join(REPO, 'data')
L = lambda n: json.load(open(os.path.join(D, n)))
tiles = L('tiles.json')['tiles']; items = L('items.json')['items']; recipes = L('recipes.json')['recipes']
trees = L('trees.json')['trees']; wg = L('worldgen.json'); mobs = L('mobs.json')['mobs']; rules = L('rules.json'); trade = L('trade.json')
T = {t['name']: t for t in tiles}; I = {i['name']: i for i in items}
recipe_total = len(recipes)
# (A recipe that makes one of several things by chance is, for these purposes, a recipe for each of them.)
recipes = [dict(r, output={'item': c['item'], 'count': r['output'].get('count', 1)}) for r in recipes if 'one_of' in r['output'] for c in r['output']['one_of']] + \
          [r for r in recipes if 'one_of' not in r['output']]
label = lambda n: I[n].get('label', n) if n in I else T.get(n, {}).get('label', n)

# ---- what the world is made of ----------------------------------------------------
world_tiles = set()
def walk(o):
    if isinstance(o, str):
        if o in T: world_tiles.add(o)
    elif isinstance(o, list):
        for x in o: walk(x)
    elif isinstance(o, dict):
        for k, v in o.items():
            if k == 'trees': continue
            walk(v)
walk(wg)
if rules.get('weather', {}).get('ice') in T: world_tiles.add(rules['weather']['ice'])  # water freezes into it in winter
tree_by = {t['name']: t for t in trees}
world_trees = set()
for b in wg['biomes']:
    for t in b.get('trees', []): world_trees.add(t['tree'])
for t in wg.get('islands', {}).get('trees', []): world_trees.add(t['tree'])   # the gem trees of the islands in the sky
changed = True
while changed:  # what grows, rots, or is put down by something in the world
    changed = False
    def add(t):
        global changed
        if t and t in T and t not in world_tiles: world_tiles.add(t); changed = True
    for n in list(world_tiles):
        add(T[n].get('grows_into')); add(T[n].get('decays_into')); add(T[n].get('grows_top'))
    for tr in world_trees:
        add(tree_by[tr]['log']); add(tree_by[tr].get('leaves'))

# creatures that turn up, and the ones bred from them
M = {m['name']: m for m in mobs}
live = {m['name'] for m in mobs if 'spawn' in m}
changed = True
while changed:
    changed = False
    for n in list(live):
        for nx in M[n].get('lays', {}).get('next', []):
            if nx not in live: live.add(nx); changed = True
for n in live:
    egg = M[n].get('lays', {}).get('tile')
    if egg: world_tiles.add(egg)

have = set()
why = {}
def got(item, reason):
    if item in I and item not in have: have.add(item); why[item] = reason
for n in world_tiles:
    t = T[n]
    if not t.get('removable') and not t.get('vanishes'): continue
    got(t.get('drop'), 'from the block "%s" in the world' % t.get('label', n))
    for d in t.get('drops', []): got(d.get('item'), 'from the block "%s" in the world' % t.get('label', n))
    for l in t.get('loot', []):
        got(l.get('item'), 'found in a %s' % t.get('label', n))
        for c in l.get('one_of', []): got(c['item'], 'found in a %s' % t.get('label', n))
for tr in world_trees:
    got(tree_by[tr].get('fruit'), 'fruit of the %s tree' % tr)
for n in live:
    for d in M[n].get('drops', []): got(d['item'], 'left by a %s' % M[n].get('label', n))
    if M[n].get('shear'): got(M[n]['shear']['item'], 'shorn from a %s' % M[n].get('label', n))
    if M[n].get('milk'): got(M[n]['milk']['item'], 'milked from a %s' % M[n].get('label', n))
for l in rules.get('meditation', {}).get('loot', []): got(l['item'], 'from meditating')
for s in rules['player'].get('start_items', []): got(s['item'], 'a new world starts the first character with it')

# ---- benches, and how far each can be raised ----------------------------------------
kinds = {}  # kind of bench -> tier -> a tile of that kind and tier (for its label and what raising it costs)
for t in tiles:
    if t.get('bench'): kinds.setdefault(t['bench_kind'], {}).setdefault(t.get('bench_level', 1), t)
def reachable_tiles():
    """Every block that can be had standing in the world: there already, put down, or raised from one that can."""
    r = set(world_tiles)
    for i in items:
        if i['name'] not in have or not i.get('places'): continue
        for p in ([i['places']] if isinstance(i['places'], str) else i['places']): r.add(p)
    grew = True
    while grew:
        grew = False
        for n in list(r):
            up = T[n].get('upgrade')
            if up and up['to'] not in r and all(c['item'] in have for c in up.get('cost', [])):
                r.add(up['to']); grew = True
    return r
def bench_level(kind):
    r = reachable_tiles()
    return max([t.get('bench_level', 1) for t in tiles if t.get('bench') and t['bench_kind'] == kind and t['name'] in r] or [0])
tier_of = {}  # the item of a bench above its first tier: got by raising the bench and taking it up
for i in items:
    t = T.get(i['places']) if isinstance(i.get('places'), str) else None
    if t and t.get('bench') and t.get('bench_level', 1) > 1: tier_of[i['name']] = (t['bench_kind'], t['bench_level'], t['name'])
def close():
    changed = True
    while changed:
        changed = False
        for r in recipes:
            out = r['output']['item']
            if out in have: continue
            if bench_level(r['bench']) < r.get('level', 1): continue
            if all(i['item'] in have for i in r.get('inputs', [])):
                got(out, 'made at the %s' % kinds[r['bench']][1].get('label', r['bench'])); changed = True
        # a creature that can be caged, once there is a cage to cage it with
        if any(i.get('cages') and i['name'] in have for i in items):
            for n in live:
                c = M[n].get('caged')
                if c and c not in have: got(c, 'a %s taken up in a cage' % M[n].get('label', n)); changed = True
        r = reachable_tiles()
        for n, (kind, lvl, tile) in tier_of.items():
            if n not in have and tile in r and T[tile].get('removable'): got(n, 'a bench raised to that tier'); changed = True
close()
really = set(have)
# ... and again supposing there were a Workbench, to see what else stands in the way
got('workbench', 'supposed')
close()
supposed = set(have)
# ... and supposing clay had a source as well, which is what the kiln is waiting on
got('clay', 'supposed')
close()
with_clay = set(have)
have.clear(); have.update(really)
def tiered(i):
    n = i['name']
    return i.get('label', n) + (' (tier %d)' % tier_of[n][1] if n in tier_of else '')

# ---- what things are for ----------------------------------------------------------------
used_in = {}
for r in recipes:
    for i in r.get('inputs', []): used_in.setdefault(i['item'], []).append(r['output']['item'])
upgrade_cost = set()
for t in tiles:
    for c in t.get('upgrade', {}).get('cost', []): upgrade_cost.add(c['item'])
tool_classes_used = {t.get('tool') for t in tiles if t.get('tool')}
currencies = {c['item'] for c in trade.get('currencies', [])}
def purposes(i):
    p = []
    n = i['name']
    if n in used_in: p.append('an ingredient')
    if n in upgrade_cost: p.append('pays for a bench upgrade')
    if i.get('food'): p.append('food')
    if i.get('fuel'): p.append('fuel')
    if i.get('tool') and i['tool'].get('class') in tool_classes_used: p.append('a tool')
    if i.get('damage'): p.append('a weapon')
    if i.get('armor'): p.append('armour')
    if i.get('shears'): p.append('shears')
    if i.get('cages'): p.append('cages creatures')
    if i.get('ignites'): p.append('sets fire to things')
    if any(m.get('caged') == n for m in mobs): p.append('a creature to let out')
    if i.get('paints') or i.get('strips'): p.append('paints or strips')
    if i.get('milks'): p.append('milks')
    if i.get('places'): p.append('can be put down')
    if i.get('plants') or i.get('sows'): p.append('can be planted')
    if n in currencies: p.append('money')
    if i.get('holds_water'): p.append('carries water')
    if i.get('container'): p.append('holds things')
    if i.get('crystals') or i.get('becomes_player'): p.append('never held')
    return p

is_block = lambda i: bool(i.get('places')) and not T[i['places'] if isinstance(i['places'], str) else i['places'][0]].get('bench') \
                     and not T[i['places'] if isinstance(i['places'], str) else i['places'][0]].get('trade')
is_bench = lambda i: bool(i.get('places')) and isinstance(i['places'], str) and (T[i['places']].get('bench') or T[i['places']].get('trade'))

out = []
P = out.append
def section(title, note=None):
    P(''); P('=' * 78); P(title); P('=' * 78)
    if note:
        for line in note: P(line)
        P('')

P('OBTAINMENT')
P('Worked out from the data files (data/*.json) by a script, not by hand.')
P('')
P('"Obtainable" means it can be got by playing, without buying it: dug or picked in a')
P('generated world, left by a creature, found in a golden chest, brought by meditating,')
P('or made from such things at a bench that can itself be got and raised to the tier')
P('needed. A trade portal only deals in what has already been found in the world, so it')
P('is no source of anything new; the dev portal a character starts with gives everything')
P('away and is not counted either.')
P('')
P('Everything is treated as an item unless it can actually be put down in the world today')
P('(the "X block" things, benches, and the blocks the game already had). Beds, doors,')
P('columns, stairs, ladders and the like are items for now.')
P('')
P('Totals: %d items, %d recipes, %d kinds of bench.' % (len(items), recipe_total, len(kinds)))

missing = [i for i in items if i['name'] not in have and not i.get('crystals') and not i.get('becomes_player')]
no_recipe = [i for i in missing if not any(r['output']['item'] == i['name'] for r in recipes) and i['name'] not in tier_of]
only_wb = [i for i in missing if i not in no_recipe and i['name'] in supposed]
blocked = [i for i in missing if i not in no_recipe and i['name'] not in supposed]
wb_missing = 'workbench' not in really
clay_missing = 'clay' not in really
note = []
if wb_missing:
    note += ['*** THE WORKBENCH ITSELF CANNOT BE MADE. ***',
             'Every bench is made at a Workbench, so as the data stands almost nothing can be made.',
             'The list is split: what has no source (1a), what is blocked even supposing a Workbench',
             '(1b), and what is held up by nothing but the missing Workbench (1c).', '']
else:
    note += ['The Workbench is made at any portal (1 dirt), so the benches can all be started.',
             'The list is split in two: what has no source at all (1a), and what has a recipe that',
             'cannot be completed because of something in 1a (1b).', '']
if clay_missing:
    note += ['*** THE KILN, THE FURNACE AND THE PRESS WAIT ON EACH OTHER: nothing gives clay. ***', '']
else:
    note += ['Clay is now dug from veins in the dirt, which opens the kiln, and with it charcoal, the',
             'furnace, bronze and the press.', '']
note += ['%d of the %d items can be got as things stand.' % (len(really), len(items))]
section('1. UNOBTAINABLE ITEMS  (%d of %d)' % (len(missing), len(items)), note)
P('1a. NOTHING MAKES OR GIVES THESE  (%d) - the roots: settle these and most of 1b follows' % len(no_recipe))
P('    (after each: what it is wanted for)')
P('')
def wanted(i):
    uses = sorted({label(u) for u in used_in.get(i['name'], [])})
    extra = (['a bench upgrade'] if i['name'] in upgrade_cost else [])
    return extra + uses
first = [i for i in no_recipe if i['name'] == 'workbench']
rest = [i for i in no_recipe if i['name'] != 'workbench']
for i in first + sorted([i for i in rest if wanted(i)], key=lambda i: (-len(wanted(i)), i.get('label', ''))):
    w = wanted(i)
    if i['name'] == 'workbench': w = ['EVERYTHING: every other bench is made at it']
    if not w: continue
    P('  - %-22s -> %s%s' % (i.get('label', i['name']), ', '.join(w[:5]), ' (+%d more)' % (len(w) - 5) if len(w) > 5 else ''))
P('')
P('    Left over from before, wanted for nothing, and now with no recipe:')
line = '      '
for n in sorted(tiered(i) for i in rest if not wanted(i)):
    if len(line) + len(n) + 2 > 86: P(line.rstrip(', ')); line = '      '
    line += n + ', '
P(line.rstrip(', '))
P('')
P('1b. HAVE A RECIPE, BUT IT CANNOT BE COMPLETED  (%d)' % len(blocked))
P('    (after each: the bench, and what is missing)')
P('')
have.clear(); have.update(supposed)
for i in sorted(blocked, key=lambda i: tiered(i)):
    if i['name'] in tier_of:
        kind, lvl, tile = tier_of[i['name']]
        below = next((t for t in tiles if t.get('upgrade', {}).get('to') == tile), {})
        cost = below.get('upgrade', {}).get('cost', [])
        lack = [label(c['item']) for c in cost if c['item'] not in have] or ['the tier below']
        P('  - %-30s %-24s needs: %s' % (tiered(i), 'raise the bench', ', '.join(lack)))
        continue
    best = None
    for r in recipes:
        if r['output']['item'] != i['name']: continue
        lack = [label(x['item']) for x in r.get('inputs', []) if x['item'] not in have]
        bl = bench_level(r['bench'])
        bench_txt = kinds[r['bench']][1].get('label', r['bench']) + (' tier %d' % r.get('level', 1) if r.get('level', 1) > 1 else '')
        if bl < r.get('level', 1): lack = ['the bench itself' if bl == 0 else 'tier %d of the bench' % r.get('level', 1)] + lack
        cand = (len(lack), bench_txt, lack)
        if best is None or cand < best: best = cand
    P('  - %-30s %-24s needs: %s' % (tiered(i), best[1], ', '.join(best[2])))
have.clear(); have.update(really)
P('')
if only_wb:
    P('1c. HELD UP ONLY BY THE MISSING WORKBENCH  (%d) - all of these work once there is one' % len(only_wb))
    line = '    '
    for n in sorted(tiered(i) for i in only_wb):
        if len(line) + len(n) + 2 > 86: P(line.rstrip(', ')); line = '    '
        line += n + ', '
    P(line.rstrip(', '))
    P('')
P('For reference, what CAN be got as things stand (%d): ' % len(really))
line = '    '
for n in sorted(label(x) for x in really):
    if len(line) + len(n) + 2 > 86: P(line.rstrip(', ')); line = '    '
    line += n + ', '
P(line.rstrip(', '))

# benches
section('2. BENCHES WITH NO PURPOSE YET',
        ['A bench has a purpose if anything is made at it, or it does something else (a portal',
         'teleports; a trade portal trades; a campfire, kiln and furnace also burn fuel).'])
made_at = {}
for r in recipes: made_at.setdefault(r['bench'], 0); made_at[r['bench']] += 1
for kind in sorted(kinds, key=lambda k: kinds[k][1].get('label', k)):
    if made_at.get(kind): continue
    if any(t.get('generator') or t.get('battery') for t in tiles if t.get('bench') and t.get('bench_kind', t['name']) == kind): continue  # it makes or stores electricity
    lab = kinds[kind][1].get('label', kind)
    if kind == rules.get('teleport', {}).get('bench'):
        P('  - %-28s nothing is made at it any more; it still teleports, and its tiers still cost gems' % lab); continue
    it = next((i['name'] for i in items if i.get('places') == kinds[kind][1]['name']), None)
    P('  - %-28s %s' % (lab, 'can be made' if it in really else 'could be made, given a Workbench' if it in supposed else 'cannot be made yet either (see 1b)'))
P('')
P('  Benches whose higher tiers unlock nothing:')
for kind in sorted(kinds):
    top = max(kinds[kind]); need = max([r.get('level', 1) for r in recipes if r['bench'] == kind] or [0])
    if top > 1 and need < top and kind != rules.get('teleport', {}).get('bench'):
        P('  - %s: tiers %d-%d have no recipes of their own' % (kinds[kind][1].get('label', kind), need + 1, top))
if all(not (max(kinds[k]) > 1 and max([r.get('level', 1) for r in recipes if r['bench'] == k] or [0]) < max(kinds[k])) for k in kinds if k != rules.get('teleport', {}).get('bench')):
    P('  (none)')

# items with no purpose
section('3. ITEMS WITH NO PURPOSE YET  (items only - blocks and benches are in other sections)',
        ['An item has a purpose if it is an ingredient, pays for an upgrade, is food, fuel, a',
         'working tool, a weapon, money, can be planted, or carries something.'])
plain = [i for i in items if not is_block(i) and not is_bench(i) and not i.get('crystals') and not i.get('becomes_player')]
none = [i for i in plain if not purposes(i) and not i.get('equip')]
worn = [i for i in plain if not purposes(i) and i.get('equip')]
groups = {}
for i in none: groups.setdefault(i.get('category', '?'), []).append(i.get('label', i['name']))
cat_label = {c['name']: c['label'] for c in trade['categories']}
P('3a. DO NOTHING AT ALL  (%d)' % len(none))
for c in sorted(groups):
    P(''); P('  %s (%d):' % (cat_label.get(c, c), len(groups[c])))
    names = sorted(groups[c])
    line = '    '
    for n in names:
        if len(line) + len(n) + 2 > 86: P(line.rstrip(', ')); line = '    '
        line += n + ', '
    P(line.rstrip(', '))
P('')
P('3b. CAN BE WORN, BUT WEARING THEM DOES NOTHING  (%d)' % len(worn))
P('    (Everything else that is worn - linen, fur, leather, tin, iron, titanium and carbon')
P('    fiber - stops a share of every attack. These go in a slot and stop none of it.)')
line = '    '
for n in sorted(i.get('label', i['name']) for i in worn):
    if len(line) + len(n) + 2 > 86: P(line.rstrip(', ')); line = '    '
    line += n + ', '
P(line.rstrip(', '))
P('')
P('3c. TOOLS THAT ARE TOOLS IN NAME ONLY')
P('    (no kind of block is dug faster by them and they do no extra damage)')
for i in sorted(none, key=lambda i: i.get('label', '')):
    if i.get('category') == 'tools': P('    - %s' % i.get('label'))

# blocks not in world gen
section('4. BLOCKS THAT ARE NOT IN WORLD GENERATION',
        ['Things that can be put down in the world (benches aside) which a new world never',
         'has in it: they only exist once a player makes and places them.'])
blocks = [i for i in items if is_block(i)]
rows = []
for i in blocks:
    ts = [i['places']] if isinstance(i['places'], str) else i['places']
    if any(t in world_tiles for t in ts): continue
    if any(T[t].get('hatches') for t in ts): continue  # (an egg put down is still an egg)
    rows.append((i.get('label', i['name']), 'obtainable' if i['name'] in really else 'obtainable given a Workbench' if i['name'] in supposed else 'NOT obtainable (see 1)'))
for lab, st in sorted(rows): P('  - %-28s %s' % (lab, st))
P('')
P('  Blocks that ARE generated, for reference:')
gen = sorted({T[n].get('label', n.replace('_', ' ').capitalize()) for n in world_tiles if not T[n].get('bench')})
line = '    '
for n in gen:
    if len(line) + len(n) + 2 > 86: P(line.rstrip(', ')); line = '    '
    line += n + ', '
P(line.rstrip(', '))

section('5. NAMED IN recipes.txt BUT WITH NO SOURCE IN THE WORLD',
        ['The raw materials the recipes assume exist somewhere. None of these is dug, grown,',
         'dropped or found yet; this is the short list to decide on first.'])
raws = [i for i in no_recipe if (i['name'] in used_in or i['name'] in upgrade_cost) and i['name'] != 'workbench']
line = '    '
for n in sorted(i.get('label', i['name']) for i in raws):
    if len(line) + len(n) + 2 > 86: P(line.rstrip(', ')); line = '    '
    line += n + ', '
P(line.rstrip(', '))

section('6. NOTES ON HOW recipes.txt WAS READ')
for n in ['"Bucket of water" is a Tin bucket that is full (8/8). It is handed back empty, not used up.',
          'Recipes that ask for a "tin bucket" itself (mixing bench, feeder chest, paint) use the bucket up.',
          '"Bronze", "iron", "steel", "gold", "titanium" as amounts mean the ingots.',
          '"Gold nuggets" are what gold ore blocks drop (the item that was called Gold ore).',
          '"Coffee cherry" is what coffee trees bear (was Coffee bean). "Shop" in recipes.txt is the trade portal.',
          '"Flint/Stone/Gold spade" are the old shovels, renamed. "Basic torch" is the torch.',
          '"Raw dodo meat" and "Raw fish" are the old dodo meat and fish.',
          'Where a line gave no time, it takes the last time given above it in the same section.',
          '"2 platinum stairs - platinum stairs" was read as: from 1 platinum block, like the rest.',
          'Compost: each line is its own recipe, 5 of the thing for 1 compost.',
          'Paint: 92 recipes, one per mix of 1, 2 or 3 of the 8 pigments; the paint carries the mixed colour.',
          'The metalwork bench now has 2 tiers, as listed; its old third tier is gone.',
          'Old bench recipes with no line in recipes.txt are gone, as asked. Every portal recipe is kept:',
          '  the Workbench (1 dirt), the trade portal, the gold coin, the five gem pickaxes, the portal chest',
          '  and the new player.',
          'Clay is dug: veins of 4 to 10 in the dirt, 1 to 6 blocks down, as flint is.',
          'Machetes cut leaves and plants (flint 2x, bronze 4x, iron 5.5x as quick as by hand).',
          'Bows hit from up to 10 blocks off: copper bow 2 damage; gold bow 5, and 5 more by poison',
          '  (1 every 2 ticks).',
          'Limestone, marble, lapis lazuli and red marble are dug from scars through the rock; sandstone',
          '  from under deserts and beaches. Sunflowers, tomatoes, wheat, chilli and tulips grow wild;',
          '  pine, coconut and maple trees bear pine cones, coconuts and maple seeds.',
          'Scorpions leave poison; yaks leave a horn, and are shorn of shavings with a razor.',
          'Meditating and golden chests give gem shards, not gems; 3 shards make the gem at a tier 3 press.',
          'Columns and stairs are blocks of the background now; stairs can be walked up.',
          'Worms come from digging dirt (1 in 10); milk from a yak with an empty bucket; leather from',
          '  yaks and dropbears; vines hang in caves; mangoes grow on mango trees in the tropics.',
          'Ice is what open water freezes into in winter, and is dug. Doors, trapdoors, lanterns,',
          '  chandeliers, uplights and downlights are blocks now; worn armour stops part of an attack.',
          'Electricity: solar panels and steam generators make it, flywheels store it, copper wire carries',
          '  it; the electric stove, kiln, furnace, press and metalwork bench make what their fuelled',
          '  kinds make; the elevator motor and shaft make a lift.',
          'Safes hold 16 things like a chest; shelves and display cabinets hold 4 and show them. Beds',
          '  are two blocks wide and make sleep quicker; a soft bed, and coffee, take energy past full.',
          '  A cage takes up any creature of the land as a "Caged ..." item, to be let out elsewhere as',
          '  the player\'s own. Paint colours a block or a backwall and paint stripper takes it off:',
          '  100 uses each.',
          'The electric benches have lists of their own (the newer recipes.txt): furnace, stove, press,',
          '  sluice (an ore by chance from gravel or sand), egg extractor (what a dodo egg is an egg of),',
          '  refinery; the electric kiln and metalwork bench make what the fuelled ones make, quicker.',
          '  "Platinum nuggets" are platinum ore; an iron pot in a recipe is used up like any ingredient.',
          'Gates, signs, compost, luminous plaster and boats are put down; an ember burns wooden things.',
          'Gem shards grow on the gem trees of the islands in the sky, the rarer the higher.',
          'The recipe book (pause menu > Recipes) shows all of this in the game, from the same data.']:
    P('  - ' + n if not n.startswith('  ') else '  ' + n)
P('')
open(os.path.join(REPO, 'obtainment.txt'), 'w').write('\n'.join(out) + '\n')
print('items', len(items), 'obtainable', len(really), 'with workbench', len(supposed), 'roots', len(no_recipe), 'blocked', len(blocked), 'only wb', len(only_wb), 'no purpose', len(none), 'worn', len(worn))
