#!/usr/bin/env python3
"""Writes a quests.json for the game and for every mod: a tab each, and a line of quests through what it has.

    python3 tools/quests.py .

The quests are worked out from the data - the benches a mod has, what is made at them and in what order, the creatures
met, the places reached - so run this again after anything that changes a mod's content (its generator, for one).
A quest's name is made from what it is about, so that a save knows it again after the list has changed.
Also writes mods/questbook, the mod that switches the book on: with it off the quests are so much unread data.
"""
import json, os, re, sys, zlib

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
MAJOR = {'dimensions', 'seeker_of_the_dao', 'beyond_the_sky', 'plunder_and_thunder', 'dungeoneering'}
SHORT = {'data': 'world', 'dimensions': 'dim', 'seeker_of_the_dao': 'dao', 'beyond_the_sky': 'sky', 'plunder_and_thunder': 'sea', 'dungeoneering': 'dg', 'magic_mayhem': 'magic', 'industry': 'ind', 'digital_storage': 'digi', 'culinary_delights': 'cook',
         'explorers_delight': 'expl', 'discovery_road': 'road', 'alchemie': 'alch', 'rags_to_riches': 'rags', 'machinations': 'mach', 'potioncraft': 'pot'}
def load(d, f, key):
    try: out = json.load(open(os.path.join(d, f))).get(key, [])
    except (OSError, ValueError): return []
    for r in out if key == 'recipes' else []: r.setdefault('inputs', []); r.setdefault('output', {})
    return out
def slug(s): return re.sub(r'[^a-z0-9]+', '_', s.lower()).strip('_')
def a_list(names):
    names = list(names); return names[0] if len(names) == 1 else ', '.join(names[:-1]) + ' and ' + names[-1]

base = os.path.join(repo, 'data')
B_ITEMS, B_TILES, B_REC, B_MOBS = load(base, 'items.json', 'items'), load(base, 'tiles.json', 'tiles'), load(base, 'recipes.json', 'recipes'), load(base, 'mobs.json', 'mobs')
COINS = [('platinum_coin', 10000), ('gold_coin', 100), ('copper_coin', 1)]
TIER_WORTH = [0, 20, 60, 150, 400, 1000, 2500, 6000, 15000, 35000, 80000, 150000]

def coins(worth):
    out = []
    for name, value in COINS:
        n = int(worth // value)
        if n > 0 and len(out) < 2:
            n = min(n, 90); out.append({'item': name, 'count': n}); worth -= n * value
    return out or [{'item': 'copper_coin', 'count': 5}]

def write(d, mod_id, order):
    own = d != base
    items, tiles, recipes, mobs = load(d, 'items.json', 'items'), load(d, 'tiles.json', 'tiles'), load(d, 'recipes.json', 'recipes'), load(d, 'mobs.json', 'mobs')
    dims, biomes, dungeons = load(d, 'worldgen.json', 'dimensions'), load(d, 'worldgen.json', 'biomes'), load(d, 'worldgen.json', 'dungeons')
    if not items and not recipes: return 0
    I = {i['name']: i for i in B_ITEMS}; I.update({i['name']: i for i in items})
    T = {t['name']: t for t in B_TILES}; T.update({t['name']: t for t in tiles})
    M = {m['name']: m for m in B_MOBS}; M.update({m['name']: m for m in mobs})
    all_rec = (B_REC + recipes) if own else recipes
    by = {}
    for r in all_rec:
        o = r['output'].get('item')
        if o and all(i['item'] != o for i in r['inputs']): by.setdefault(o, []).append(r)
    # How many makings deep each thing is: worked out round and round until nothing changes (a thing got some other way is 0).
    depth = {o: 1 for o in by}
    for _ in range(40):
        changed = False
        for o, rs in by.items():
            v = min(1 + max((depth.get(x['item'], 0) for x in r['inputs']), default=0) for r in rs)
            v = min(v, 40)
            if v != depth[o]: depth[o] = v; changed = True
        if not changed: break
    def steps(i): return depth.get(i, 0)
    def tier(i):
        it = I.get(i, {})
        if it.get('tier'): return it['tier']
        p = it.get('price', 10); return 1 + sum(p >= t for t in (50, 150, 500, 1500, 5000))
    def bench_label(kind):
        for t in T.values():
            if t.get('bench') and t.get('bench_kind', t['name']) == kind: return t['label']
        return kind.replace('_', ' ')
    places = {}
    for i in I.values():
        for t_ in (i['places'] if isinstance(i.get('places'), list) else [i['places']] if i.get('places') else []): places.setdefault(t_, i['name'])
    for group in (I, T, M):
        for k_, v_ in group.items(): v_.setdefault('label', k_.replace('_', ' ').capitalize())
    SRC = {}
    for dg in dungeons:
        for l in dg.get('loot', []): SRC.setdefault(l['item'], 'Found in the chest at the end of %s.' % dg['label'])
        if dg.get('stone'): SRC.setdefault(dg['stone'], 'Found in the chest at the end of %s.' % dg['label'])
    for t in T.values():
        for dr in t.get('drops', []):
            if dr.get('item'): SRC.setdefault(dr['item'], 'Got from %s.' % t['label'].lower())
        if isinstance(t.get('drop'), str) and places.get(t['name']) != t['drop']: SRC.setdefault(t['drop'], 'Got from %s.' % t['label'].lower())
    for m in M.values():
        for dr in m.get('drops', []):
            if dr.get('item'): SRC.setdefault(dr['item'], 'Left by the %s.' % m['label'].lower())
    def how(i):
        if i in by:
            r = min(by[i], key=lambda r: len(r['inputs'])); ins = a_list('%s%s' % ('%d ' % x['count'] if x['count'] > 1 else '', I.get(x['item'], {}).get('label', x['item']).lower()) for x in r['inputs'][:4]) if r['inputs'] else 'nothing at all'
            s = 'Made at the %s%s from %s.' % (bench_label(r['bench']).lower(), ' (grade %d)' % r['level'] if r.get('level', 1) > 1 else '', ins)
            if r.get('formula'): s += ' The plan for it must be known first.'
            return s
        if i in SRC: return SRC[i]
        return 'Found out in the world.'
    quota = 60 if not own else 112 if mod_id in MAJOR else 33
    cand = []   # (rank, label, text, task, icon, milestone)
    mine = [i for i in items]
    top = max([tier(i['name']) for i in mine] + [1])
    def rank(i): return (tier(i) - 1 + min(steps(i), 20) / 21.0) / top
    used_items = set()
    # Benches and the like: set down, and raised.
    kinds = {}
    for t in tiles:
        if t.get('bench') or t.get('trade') or t.get('loot_forge') or t.get('dungeon_gate'): kinds.setdefault(t.get('bench_kind', t['name']) if t.get('bench') else t['name'], []).append(t)
    for kind, group in kinds.items():
        group.sort(key=lambda t: t.get('bench_level', t.get('trade_level', 1)) or 1)
        picks = [group[0]] + ([group[-1]] if len(group) > 1 else []) if len(kinds) > 14 else group
        for t in picks:
            it = places.get(t['name']); lvl = t.get('bench_level') or t.get('trade_level') or (group.index(t) + 1)
            if t is group[0]: text = (how(it) if it else 'Find one.') + ' Set it down and stand by it.'
            else: text = 'Raise the %s, a grade at a time, until it is this.' % group[0]['label'].lower()
            lab = t['label'] + ('' if t is group[0] or re.search(r' [IVX]+$', t['label']) else ' %s' % ['', 'I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X', 'XI', 'XII', 'XIII', 'XIV'][min(lvl, 14)])
            cand.append(((rank(it) if it else 0.3) + (0.0 if t is group[0] else 0.08 * group.index(t)), lab, text, {'near': t['name']}, it, t is not group[0]))
            if it: used_items.add(it)
    # (Not more than a third of the line: the first of each before the last of any.)
    cand.sort(key=lambda q: (q[5], q[0])); cand = cand[:max(4, quota // 3)]
    # Dungeons: who is in them, who they belong to, and what the chest gives.
    for k, dg in enumerate(dungeons):
        r0 = (dg.get('tier', k + 1) - 1) / max(1, len(dungeons))
        for n, mob in enumerate(dg['mobs']):
            boss = n == len(dg['mobs']) - 1
            cand.append((r0 + (0.07 if boss else 0.01 * n) / len(dungeons) * 10 / 10, ('%s' % M[mob]['label']) if boss else M[mob]['label'], ('The one %s belongs to: past the last door.' if boss else 'Met in %s.') % dg['label'], {'kill': mob, 'count': 1 if boss else 10}, M[mob].get('caged'), boss))
        loot = [l['item'] for l in dg.get('loot', [])]
        best = [i for i in loot if I[i].get('grade') == 5]; plain = [i for i in loot if I[i].get('grade') == 1 and not I[i].get('equip')]; fine = [i for i in loot if I[i].get('grade') == 4 and not I[i].get('equip')]; worn = [i for i in loot if I[i].get('grade') == 3 and I[i].get('equip')]
        for i, extra in ([(plain[0], 0.02)] if plain else []) + ([(worn[0], 0.04)] if worn else []) + ([(fine[-1], 0.06)] if fine else []) + [(x, 0.09) for x in best]:
            cand.append((r0 + extra / len(dungeons) * 10 / 10, I[i]['label'], how(i), {'have': i}, i, False)); used_items.add(i)
        if dg.get('stone'): cand.append((r0 + 0.08 / len(dungeons) * 10 / 10, I[dg['stone']]['label'], how(dg['stone']) + ' It raises the portal.', {'have': dg['stone']}, dg['stone'], False)); used_items.add(dg['stone'])
    # Places.
    for dm in dims:
        if dm.get('generator') == 'dungeon': continue
        cand.append((0.35 + 0.1 * dims.index(dm), dm.get('label', dm['name']), 'Another place. Find the way there, and go.', {'in': dm['name']}, None, True))
    lim = 14 if mod_id in MAJOR else 6
    for n, b in enumerate(biomes[:lim]):
        cand.append((0.1 + 0.8 * n / max(1, min(len(biomes), lim)), b.get('label', b['name']), 'A kind of country. Walk until you are standing in it.', {'biome': b['name']}, None, False))
    # Creatures.
    wild = [m for m in mobs if m.get('spawn') and not m.get('mount')]
    wild.sort(key=lambda m: m.get('health', 1))
    lim = 22 if mod_id in MAJOR else 7
    for n, m in enumerate(wild[::max(1, len(wild) // lim)][:lim] if wild else []):
        hp = m.get('health', 1); many = 8 if hp <= 20 else 5 if hp <= 100 else 3 if hp <= 600 else 1
        cand.append((0.08 + 0.9 * n / max(1, min(len(wild), lim)), m['label'], 'Out in the world, where its kind lives.' if not m.get('boarding') else 'A ship. Sink her, or board her and take her captain.', {'kill': m['name'], 'count': many}, m.get('caged'), many == 1))
    # Things made, from the first to the last: evenly along the way, what is used or worn before what is only material.
    def wanted(i):
        n = i['name']
        if n in used_items or n.startswith('caged_') or n.endswith(('_column', '_stairs')) or i.get('category') == 'creatures' or i.get('rolled') or (isinstance(i.get('places'), str) and T.get(i['places'], {}).get('bench')): return False
        return n in by or how(n) != 'Found out in the world.'
    pool = sorted((i for i in mine if wanted(i)), key=lambda i: (rank(i['name']), 0 if i.get('equip') or i.get('relic') or i.get('damage') or i.get('category') in ('tools', 'special', 'clothing') else 1, i['name']))
    room = max(0, quota - 1 - len(cand))
    pick = pool if len(pool) <= room else [pool[int(k * len(pool) / room)] for k in range(room)]
    for i in pick:
        n = i['name']; made = n in by; many = 1 if made or i.get('max_stack') == 1 else 8
        cand.append((rank(n), i['label'], how(n), {'have': n, 'count': many} if many > 1 else {'have': n}, n, bool(i.get('relic') or i.get('equip')) and tier(n) >= top - 1))
    # Whatever else it has that is only ever found.
    if len(cand) < quota - 1:
        rest = [i for i in mine if i not in pick and i['name'] not in used_items and not i['name'].startswith('caged_') and not i['name'].endswith(('_column', '_stairs')) and not i.get('rolled') and not (isinstance(i.get('places'), str) and T.get(i['places'], {}).get('bench'))]
        rest.sort(key=lambda i: (rank(i['name']), i['name']))
        for i in rest[:quota - 1 - len(cand)]:
            cand.append((rank(i['name']), i['label'], 'Not made: found, in chests and in the places it belongs to.', {'have': i['name']}, i['name'], bool(i.get('relic'))))
    # A small mod: what its makings want of the world rounds the line out.
    if len(cand) < quota - 1:
        wants = []
        for r in recipes:
            for x in r['inputs']:
                if x['item'] not in {i['name'] for i in mine} and x['item'] not in wants and 'coin' not in x['item']: wants.append(x['item'])
        for n, w_ in enumerate(wants[:quota - 1 - len(cand)]):
            cand.append((0.02 + 0.5 * n / max(1, len(wants)), I[w_]['label'], 'Wanted for what is made here. ' + how(w_), {'have': w_, 'count': 8 if I[w_].get('max_stack', 99) != 1 else 1}, w_, False))
    cand.sort(key=lambda q: q[0])
    # Written out: a welcome first, then chapters of eight - a chapter's first quest waits on the last chapter's first.
    short = SHORT.get(mod_id, mod_id[:6]); label = 'The world' if not own else json.load(open(os.path.join(d, 'mod.json'))).get('label', mod_id)
    about = 'The game as it comes: a portal, a workbench, and the ground under you.' if not own else json.load(open(os.path.join(d, 'mod.json'))).get('description', label)
    names, quests = set(), []
    def name_of(task):
        what = next(iter(v for k, v in task.items() if k != 'count'))
        n = ('%s.%s' % (short, slug(str(what))))[:31]
        while n in names: n = n[:27] + '_%03d' % (zlib.crc32((n + str(len(names))).encode()) % 1000)
        names.add(n); return n
    intro = short + '.welcome'; names.add(intro)
    tab_icon = next((c_[4] for c_ in cand if c_[4]), None)
    quests.append({'name': intro, 'tab': mod_id, 'label': label, 'text': about[:159], 'task': {'check': True}, 'rewards': coins(10), 'icon': tab_icon})
    of_item, head, due = {}, intro, False
    for n, (r_, lab, text, task, icon, big) in enumerate(cand):
        q = {'name': name_of(task), 'tab': mod_id, 'label': lab[:47], 'text': text[:159], 'task': task}
        due = due or n % 8 == 0
        # (A chapter does not wait on a thing that is a matter of luck.)
        soft = 'have' in task and (I.get(task['have'], {}).get('grade') == 5 or I.get(task['have'], {}).get('relic')) or task.get('kill') and big and not dungeons
        first = due and not soft
        needs = [head]
        if 'have' in task and task['have'] in by:
            for x in min(by[task['have']], key=lambda r: len(r['inputs']))['inputs']:
                if x['item'] in of_item and of_item[x['item']] not in needs and len(needs) < 3: needs.append(of_item[x['item']])
        q['needs'] = needs
        worth = TIER_WORTH[min(11, max(1, int(r_ * top) + 1))] * (3 if first or big else 1) * (0.5 if task.get('have') and not big else 1)
        q['rewards'] = coins(max(5, worth))
        if icon: q['icon'] = icon
        if not q.get('icon') and 'have' not in task: q['icon'] = q['rewards'][0]['item']
        quests.append(q)
        if 'have' in task: of_item[task['have']] = q['name']
        if first: head = q['name']; due = False
    out = {'tabs': [{'name': mod_id, 'label': label, 'order': order, 'icon': tab_icon}], 'quests': quests}
    if not tab_icon: out['tabs'][0].pop('icon'); quests[0].pop('icon')
    with open(os.path.join(d, 'quests.json'), 'w') as f:
        f.write('{\n  "tabs": %s,\n  "quests": [\n%s\n  ]\n}\n' % (json.dumps(out['tabs']), ',\n'.join('    ' + json.dumps(q) for q in quests)))
    return len(quests)

total = write(base, 'data', 0); print('the world: %d' % total)
mods = os.path.join(repo, 'mods')
for n, mod_id in enumerate(sorted(os.listdir(mods))):
    d = os.path.join(mods, mod_id)
    if mod_id == 'questbook' or not os.path.isdir(d): continue
    k = write(d, mod_id, 10 + n); total += k; print('%s: %d' % (mod_id, k))
book = os.path.join(mods, 'questbook'); os.makedirs(book, exist_ok=True)
json.dump({'questbook': True}, open(os.path.join(book, 'rules.json'), 'w'), indent=1)
json.dump({'label': 'Questbook', 'icon': 'icon.png', 'description': 'A book of quests: a tab for the game and one for each mod that is on, tasks it sees done, and rewards to claim.'}, open(os.path.join(book, 'mod.json'), 'w'), indent=1)
print('%d quests' % total)
