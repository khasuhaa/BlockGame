#!/usr/bin/env python3
"""Writes tools/dungeoneering_loot.csv - the table tools/mod_dungeoneering.py makes its loot from - out of the owner's
two documents, which are kept in the repository root and are not committed:

    "Copy of Personal Min_Max List 2025 - Sheet1.csv"   the first fifteen dungeons: a base and a count of upgrades
    itemValues.json                                      all of them, by code: least and most "potential", health

    python3 tools/dungeoneering_table.py .

Three things are worked out here, and are not in either document:
  - The thirteen dungeons after Gilded Skies have only a potential. Their base and their count of upgrades are taken
    in the proportions the fifteenth dungeon has in the first document: an upgrade is worth 10, a weapon's base is
    15/16 of its potential, an ordinary armour's power 0.806 of it, a guardian's health all but a two-hundredth.
  - Between Steampunk Sewers (levels 119 to 126) and Orbital Outpost (140 to 146) the documents have nothing, and the
    numbers go up nine times over where everywhere else they double. The Aether Shipyard is put there: not in either
    document - its things are named here - and each of its numbers is the mean, in proportion, of the two beside it.
  - What an upgrade adds, for the first fifteen: from the document's potential, its base and its count.
"""
import csv, json, math, os, re, sys

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
SHEET, VALUES = os.path.join(repo, 'Copy of Personal Min_Max List 2025 - Sheet1.csv'), os.path.join(repo, 'itemValues.json')
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'dungeoneering_loot.csv')
if not os.path.exists(SHEET) or not os.path.exists(VALUES): sys.exit('the two documents are not here: tools/dungeoneering_loot.csv is left as it is')
FIRST = ['DT', 'WO', 'PI', 'KC', 'UW', 'SP', 'TC', 'GH', 'SS', 'OO', 'VC', 'AT', 'EF', 'NL', 'GS']
LATER = ['OM', 'WT', 'EC', 'CL', 'MK', 'TD', 'CF', 'BC', 'FE', 'AD', 'PW', 'WS', 'SD']
GRADE = {'Gray': 1, 'Green': 2, 'Blue': 3, 'Purple': 4, 'Legendary': 5, 'Ultimate': 6}
CLASS = {'War': 'warrior', 'Mage': 'mage', 'Legendary': 'warrior', 'War Legend': 'warrior', 'Mage Legend': 'mage', 'War Ultimate': 'warrior', 'Mage Ultimate': 'mage', 'DPS Armor': 'armor', 'Guardian': 'guardian'}
def num(x):
    try: return float(str(x).replace(',', '') or 0)
    except ValueError: return None
def mid(lo, hi):
    # (One figure in the document is not a number - the most a purple Cultists' armour can have: the least, and the
    # tenth by which the two always differ, stand in for it.)
    lo, hi = num(lo), num(hi)
    return (lo + hi) / 2.0 if lo is not None and hi is not None else (lo * 1.0526 if lo is not None else hi / 1.0526 if hi is not None else 0.0)
rows = {}
for r in list(csv.reader(open(SHEET)))[1:]:
    d = r[0].strip().upper()
    if d not in FIRST or r[2] not in GRADE or '(Glitched)' in r[1] or 'Placeholder' in r[1]: continue
    base, ups = num(r[16]), num(r[15])
    gain = ((num(r[9]) - num(r[6])) + (num(r[10]) - num(r[5]))) / 2.0 / max(ups, 1.0)
    rows.setdefault(d, []).append([d, re.sub(r'\s+', ' ', r[1]).strip(), GRADE[r[2]], CLASS[r[7]], int(base), int(ups), int(num(r[8])), round(max(gain, 0.0), 3)])
values = json.load(open(VALUES))
for d in LATER:
    for name, it in values[d.lower()].items():
        cls = CLASS[it['class']]
        for g, n in GRADE.items():
            if g not in it: continue
            pot = mid(it[g]['minpot'], it[g]['maxpot']); hp = mid(it[g]['minhp'], it[g]['maxhp'])
            base = pot * (0.806 if cls == 'armor' else 0.9945 if cls == 'guardian' else 0.9375)
            rows.setdefault(d, []).append([d, re.sub(r'\s+', ' ', name).strip(), n, cls, int(base), int((pot - base) / 10.0), int(hp), 10.0])

# The Aether Shipyard: between the ninth and what was the tenth.
def spread(d, kind):
    """The things of a kind in a dungeon, least first: for each, its rows by grade."""
    by = {}
    for r in rows[d]:
        if (r[3] in ('warrior', 'mage') and kind == 'weapon' and r[2] < 5) or (r[3] == kind and r[2] < 5) or (kind == 'best' and r[2] == 5): by.setdefault(r[1], {})[r[2]] = r
    return sorted(by.values(), key=lambda g: g[min(g)][4])
def at(lot, share, grade, col):
    x = share * (len(lot) - 1); i = min(int(x), len(lot) - 2) if len(lot) > 1 else 0; f = x - i
    a = max(1.0, lot[i][grade][col]); b = max(1.0, lot[min(i + 1, len(lot) - 1)][grade][col])
    return math.exp(math.log(a) * (1 - f) + math.log(b) * f)
def between(kind, share, grade, col):
    return math.sqrt(at(spread('SS', kind), share, grade, col) * at(spread('OO', kind), share, grade, col))
YARD = [('Rivet Cleaver', 'warrior'), ('Spark Wand', 'mage'), ('Dual Propeller Blades', 'warrior'), ('Barometer Staff', 'mage'), ('Brass Boarding Axe', 'warrior'), ('Aether Spelldagger', 'mage'), ('Cloudsteel Katana', 'warrior'), ('Stormglass Orb', 'mage'),
        ('Piston Greathammer', 'warrior'), ('Cloudsteel Spellblade', 'mage'), ('Aether Lance', 'warrior'), ('Lightning Rod Staff', 'mage'), ('Skyforged Greatsword', 'warrior'), ('Skyforged Greatstaff', 'mage')]
yard = []
for k, (name, cls) in enumerate(YARD):
    share = (k // 2) / (len(YARD) / 2 - 1)
    for g in (1, 2, 3, 4):
        power, ups = between('weapon', share, g, 4), between('weapon', share, g, 5)
        yard.append(['AS', name, g, cls, int(power), int(ups), 0, round(between('weapon', share, g, 7), 3)])
for k, name in enumerate(('Deckhand', 'Aeronaut', "Skycaptain's")):
    for g in (1, 2, 3, 4):
        yard.append(['AS', name, g, 'armor', int(between('armor', k / 2, g, 4)), int(between('armor', k / 2, g, 5)), int(between('armor', k / 2, g, 6)), round(between('armor', k / 2, g, 7), 3)])
        yard.append(['AS', name + ' Guardian', g, 'guardian', int(between('guardian', k / 2, g, 4)), int(between('guardian', k / 2, g, 5)), 0, round(between('guardian', k / 2, g, 7), 3)])
for name, cls in (('Stormcaller Greatsword', 'warrior'), ('Zephyr Greatstaff', 'mage')):
    yard.append(['AS', name, 5, cls, int(between('best', 0, 5, 4)), int(between('best', 0, 5, 5)), 0, round(between('best', 0, 5, 7), 3)])
rows['AS'] = yard
order = FIRST[:9] + ['AS'] + FIRST[9:] + LATER
with open(OUT, 'w', newline='\n') as f:
    w = csv.writer(f, lineterminator='\n'); w.writerow(['dungeon', 'name', 'grade', 'class', 'power', 'upgrades', 'health', 'gain'])
    for d in order: w.writerows(rows[d])
print('%d rows, %d dungeons' % (sum(len(rows[d]) for d in order), len(order)))
for d in ('SS', 'AS', 'OO', 'GS', 'OM', 'SD'): print(d, min(r[4] for r in rows[d] if r[3] in ('warrior', 'mage')), max(r[4] for r in rows[d] if r[3] in ('warrior', 'mage')), max(r[6] for r in rows[d]), max(r[5] for r in rows[d]))
