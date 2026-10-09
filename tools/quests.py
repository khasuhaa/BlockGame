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
    quota = 80 if not own else 160 if mod_id in MAJOR else 40
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
            it = places.get(t['name']); tail = re.search(r'_(\d+)$', t['name']); lvl = t.get('bench_level') or t.get('trade_level') or (int(tail.group(1)) if tail and t.get('extends') else group.index(t) + 1)
            if t is group[0]: text = (how(it) if it else 'Find one.') + ' Set it down and stand by it.'
            else: text = 'Raise the %s, a grade at a time, until it is this.' % group[0]['label'].lower()
            lab = t['label'] + ('' if (t is group[0] and not t.get('extends')) or re.search(r' [IVX]+$', t['label']) else ' %s' % ['', 'I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X', 'XI', 'XII', 'XIII', 'XIV'][min(lvl, 14)])
            at_ = (lvl - 1) / max(1, len(dungeons)) if dungeons and t.get('extends') else 0.0 if dungeons else (rank(it) if it else 0.3) + (0.0 if t is group[0] else 0.08 * group.index(t))
            cand.append((at_, lab, text if not t.get('extends') else 'Raise the portal with the stone the dungeon before gives.', {'near': t['name']}, it, t is not group[0]))
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
    # (So many from each kind of thing - never fewer than six where there are six - each lot spread from first to last.)
    kinds_ = {}
    for i in pool: kinds_.setdefault(i.get('category'), []).append(i)
    pick = []
    for c_, lot in sorted(kinds_.items(), key=lambda kv: str(kv[0])):
        share = min(len(lot), max(6, round(room * len(lot) / max(1, len(pool)))))
        pick += lot if len(lot) <= share else [lot[int(k * len(lot) / share)] for k in range(share)]
    if len(pick) > room: pick.sort(key=lambda i: rank(i['name'])); pick = [pick[int(k * len(pick) / room)] for k in range(room)]
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
    # Set out as a tree: a branch to each kind of thing, a column to each stage of the way. In a branch every stage
    # has a head, which waits on the head of the stage before; and the head opens every other quest of its stage.
    short = SHORT.get(mod_id, mod_id[:6]); label = 'The world' if not own else json.load(open(os.path.join(d, 'mod.json'))).get('label', mod_id)
    about = 'The game as it comes: a portal, a workbench, and the ground under you.' if not own else json.load(open(os.path.join(d, 'mod.json'))).get('description', label)
    CATL = {c_['name']: c_['label'] for c_ in load(base, 'trade.json', 'categories') + load(d, 'trade.json', 'categories')}
    stones = {dg.get('stone') for dg in dungeons}
    def branch_of(task):
        if 'near' in task: return 'Benches'
        if 'kill' in task: return 'Creatures'
        if 'in' in task or 'biome' in task: return 'Places'
        it = I.get(task['have'], {})
        if dungeons and (it.get('rolled') or task['have'] in stones): return 'Loot'
        c_ = it.get('category') or 'crafted'
        return CATL.get(c_, c_.replace('_', ' ').capitalize())
    S = len(dungeons) if dungeons else 6 if mod_id in MAJOR else 4
    tree = {}
    for q in cand: tree.setdefault(branch_of(q[3]), {}).setdefault(min(S - 1, int(q[0] * S)), []).append(q)
    small = [b for b in tree if sum(len(v) for v in tree[b].values()) < 3 and b != 'Benches']
    for b in small:
        for st, qs in tree.pop(b).items(): tree.setdefault('Sundries', {}).setdefault(st, []).extend(qs)
    order_ = sorted(tree, key=lambda b: (b != 'Benches', min(min(q[0] for q in qs) for qs in tree[b].values())))
    # What a quest gives: not coin alone - something toward what comes next, something to eat or to take, crystals for the heads.
    eats = sorted((i for i in I.values() if (i.get('food') or i.get('effect')) and i.get('max_stack', 64) != 1 and not i.get('equip') and not i.get('relic') and not i.get('rolled') and not i['name'].startswith('caged_') and (i in mine or not own or i.get('food'))), key=lambda i: (i not in mine, tier(i['name']), i['name']))
    own_eats = [i for i in eats if i in mine] or eats
    dungeon_of = {m_: dg for dg in dungeons for m_ in dg['mobs']}
    cap = [99]
    def give(item, n):
        # (Nothing from further along than the quest is.)
        if item not in I or ('coin' not in item and tier(item) > cap[0]): return None
        return {'item': item, 'count': int(max(1, min(n, I[item].get('max_stack', 64) or 64, 64)))}
    def snack(key, n):
        if not own_eats: return None
        pool_ = [i for i in own_eats if tier(i['name']) <= max(1, int(key[0] * top) + 1)] or own_eats[:6]
        return give(pool_[zlib.crc32(key[1].encode()) % len(pool_)]['name'], n)
    def toward(qs, n):
        """Something the makings of these quests want."""
        want = {}
        for q in qs:
            if 'have' in q[3] and q[3]['have'] in by:
                for x in min(by[q[3]['have']], key=lambda r: len(r['inputs']))['inputs']:
                    if 'coin' not in x['item'] and x['item'] not in by: want[x['item']] = want.get(x['item'], 0) + x['count']
        best = sorted(want, key=lambda k_: (-want[k_], k_))
        return [give(k_, n) for k_ in best[:2]]
    def rewards(q, stage, hub, nexts):
        r_, lab, text, task, icon, big = q
        worth = TIER_WORTH[min(11, max(1, int(r_ * top) + 1))]
        out, pick, crystals = [], False, 0
        cap[0] = int(r_ * top) + 2
        if hub:
            options = [coins(worth * 3)[0]] + [x for x in toward(nexts, 8)[:1] if x] + [x for x in [snack((r_, lab), 3)] if x]
            out, pick, crystals = options, len(options) > 1, 5 + 3 * stage
        elif 'kill' in task:
            dg = dungeon_of.get(task['kill'])
            if dg and big:
                loot = [l['item'] for l in dg.get('loot', []) if I[l['item']].get('grade') == 3 and not I[l['item']].get('equip')]
                war = [i for i in loot if I[i]['rolled'].get('class') == 'warrior']; mage = [i for i in loot if I[i]['rolled'].get('class') == 'mage']
                out = [give(x[len(x) // 2], 1) for x in (war, mage) if x] + [coins(worth * 3)[0]]; pick, crystals = True, 20
            else:
                out = [give(dr['item'], 3) for dr in M[task['kill']].get('drops', [])[:2] if 'coin' not in dr['item']] + [coins(worth)[0]]
                crystals = 15 if big else 0
        elif 'near' in task:
            kind = T[task['near']].get('bench_kind', task['near']); want = {}
            for r in recipes if own else all_rec:
                if r.get('bench') == kind:
                    for x in r['inputs']:
                        if 'coin' not in x['item'] and x['item'] not in by: want[x['item']] = want.get(x['item'], 0) + 1
            out = [give(k_, 6) for k_ in sorted(want, key=lambda k_: (-want[k_], k_))[:2]] + [coins(worth * 0.5)[0]]
            crystals = 10 if big else 0
        elif 'in' in task or 'biome' in task:
            out = [snack((r_, lab), 4), coins(worth)[0]]; crystals = 5 if 'in' in task else 0
        else:
            it = task['have']
            if it in by:
                ins = [x for x in min(by[it], key=lambda r: len(r['inputs']))['inputs'] if 'coin' not in x['item']]
                out = [give(x['item'], max(1, x['count'] // 2)) for x in ins[:2]] + [coins(worth * 0.4)[0]]
            else:
                out = [snack((r_, lab), 2), coins(worth * 0.6)[0]]
            if big: crystals = 10
        out = [x for x in out if x][:4] or coins(worth)
        seen, tidy = set(), []
        for x in out:
            if x['item'] not in seen: seen.add(x['item']); tidy.append(x)
        return tidy, pick and len(tidy) > 1, crystals
    names, quests = set(), []
    def name_of(task):
        what = next(iter(v for k, v in task.items() if k != 'count'))
        n = ('%s.%s' % (short, slug(str(what))))[:31]
        while n in names: n = n[:27] + '_%03d' % (zlib.crc32((n + str(len(names))).encode()) % 1000)
        names.add(n); return n
    intro = short + '.welcome'; names.add(intro)
    tab_icon = next((c_[4] for c_ in cand if c_[4]), None)
    first = {'name': intro, 'tab': mod_id, 'label': label, 'text': about[:250], 'task': {'check': True}, 'rewards': [x for x in [snack((0.0, label), 3)] if x] + coins(10), 'icon': tab_icon, 'at': [0, 0], 'hub': True}
    quests.append(first)
    ROMAN = ['', 'I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X', 'XI', 'XII', 'XIII', 'XIV', 'XV', 'XVI']
    y = 0
    for b in order_:
        stages = sorted(tree[b]); head = intro; rows = 0
        for n, st in enumerate(stages):
            qs = sorted(tree[b][st], key=lambda q: (q[5], 'have' in q[3] and bool(I.get(q[3]['have'], {}).get('grade', 0) >= 5 or I.get(q[3]['have'], {}).get('relic')), q[0]))
            nexts = tree[b][stages[n + 1]] if n + 1 < len(stages) else qs
            x0 = 2 + st * 4
            for k, q in enumerate(qs):
                hub = k == 0
                rw, pick, crystals = rewards(q, n, hub, nexts if hub else [])
                e = {'name': name_of(q[3]), 'tab': mod_id, 'label': ('%s %s' % (b, ROMAN[min(n + 1, 16)]))[:47] if hub else q[1][:47], 'text': (('%s. ' % q[1] if hub else '') + q[2])[:250], 'needs': [head], 'task': q[3], 'rewards': rw,
                     'at': [x0, y] if hub else [x0 + (k - 1) % 3, y + 1 + (k - 1) // 3]}
                if hub: e['hub'] = True
                if pick: e['pick'] = True
                if crystals: e['crystals'] = crystals
                if q[4]: e['icon'] = q[4]
                elif 'have' not in q[3]: e['icon'] = rw[0]['item']
                quests.append(e)
                if hub: head = e['name']
            rows = max(rows, (len(qs) - 1 + 2) // 3)
        y += rows + 2
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
    # (A mod that is a game of its own writes its own book: it is not the game with something added.)
    if os.path.exists(os.path.join(d, 'mod.json')) and json.load(open(os.path.join(d, 'mod.json'))).get('standalone'): continue
    k = write(d, mod_id, 10 + n); total += k; print('%s: %d' % (mod_id, k))
book = os.path.join(mods, 'questbook'); os.makedirs(book, exist_ok=True)
json.dump({'questbook': True}, open(os.path.join(book, 'rules.json'), 'w'), indent=1)
json.dump({'label': 'Questbook', 'icon': 'icon.png', 'description': 'A book of quests: a tab for the game and one for each mod that is on, tasks it sees done, and rewards to claim.'}, open(os.path.join(book, 'mod.json'), 'w'), indent=1)
print('%d quests' % total)
