#!/usr/bin/env python3
"""Writes the Dimensions mod: its data files and its textures.

    python3 tools/mod_dimensions.py .

Two more places for a world - the Aether, islands in open sky, and the Deep, great caves - each with its own
stone, ores, gems, trees, crops, creatures, benches and things to make; the portals that lead to them; four more
tiers of portal and trade portal and more levels of most benches; and at the end of it all the World Seed and a
third character. The plan is ModDescriptions/Dimensions.txt.

Nearly everything is made from something the game already has: its data copied under a new name, its picture
recoloured. So the pictures are in the game's own manner, and a field the game's own things have is never
forgotten. Run tools/worn_art.py afterwards for what is worn.

Either edit this and run it again, or edit the mod's files by hand and stop using this: not both.
"""
import colorsys, copy, json, os, sys
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
DATA = os.path.join(repo, 'data')
mod = os.path.join(repo, 'mods/dimensions'); TEX = os.path.join(mod, 'textures')
BT = {t['name']: t for t in json.load(open(os.path.join(DATA, 'tiles.json')))['tiles']}
BI = {t['name']: t for t in json.load(open(os.path.join(DATA, 'items.json')))['items']}
BM = {t['name']: t for t in json.load(open(os.path.join(DATA, 'mobs.json')))['mobs']}
BTR = {t['name']: t for t in json.load(open(os.path.join(DATA, 'trees.json')))['trees']}
MAGIC = os.path.join(repo, 'mods/magic_mayhem')
BM.update({t['name']: dict(t, _root=MAGIC) for t in json.load(open(os.path.join(MAGIC, 'mobs.json')))['mobs'] if t.get('flies')})

tiles, items, recipes, trees, mobs, biomes, veins, decor = [], [], [], [], [], [], [], []
PRICE = {}

# ---- pictures --------------------------------------------------------------------------------------------------
def hx(s): return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4))
def ramp(*cols):
    cols = [hx(c) for c in cols]
    def at(t):
        t = max(0.0, min(1.0, t)) * (len(cols) - 1); i = min(int(t), len(cols) - 2); f = t - i
        return tuple(int(cols[i][k] + (cols[i + 1][k] - cols[i][k]) * f) for k in range(3))
    return at

def recolor(src, dst, r, sel='all', r2=None):
    """sel: 'all' every pixel by how light it is; 'gray' only the grey ones (a tool's head, not its handle);
    'ore' the coloured ones by r and the grey ones by r2; 'tint' everything, half way."""
    im = Image.open(src).convert('RGBA'); px = im.load(); w, h = im.size
    lum = lambda p: (0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2]) / 255.0
    sat = lambda p: colorsys.rgb_to_hsv(p[0] / 255, p[1] / 255, p[2] / 255)[1]
    grey = lambda p: sat(p) < 0.2
    pick = lambda p: p[3] > 0 and lum(p) > 0.1 and (sel in ('all', 'tint') or (sel == 'gray' and grey(p)) or sel == 'ore')
    ls = [lum(px[x, y]) for y in range(h) for x in range(w) if pick(px[x, y])]
    lo, hi = (min(ls), max(ls)) if ls else (0, 1)
    for y in range(h):
        for x in range(w):
            p = px[x, y]
            if not pick(p): continue
            t = (lum(p) - lo) / (hi - lo) if hi > lo else 0.5
            c = (r2 if sel == 'ore' and grey(p) and r2 else r)(t)
            if sel == 'tint': c = tuple((p[k] * 2 + c[k] * 3) // 5 for k in range(3))
            px[x, y] = c + (p[3],)
    os.makedirs(os.path.dirname(dst), exist_ok=True); im.save(dst)

def retex(obj, base, new, r, sel, r2, root):
    """A copy of a definition with every picture it names recoloured under the new name."""
    if isinstance(obj, dict): return {k: retex(v, base, new, r, sel, r2, root) for k, v in obj.items() if k != '_root'}
    if isinstance(obj, list): return [retex(v, base, new, r, sel, r2, root) for v in obj]
    if isinstance(obj, str) and obj.endswith('.png'):
        d, f = os.path.split(obj)
        out = d + '/' + (f.replace(base, new) if base in f else new + '_' + f)
        recolor(os.path.join(root, 'textures', obj), os.path.join(TEX, out), r, sel, r2)
        return out
    return obj

def clean(d): return {k: v for k, v in d.items() if v is not None}

def tile(base, name, label, r, sel='all', r2=None, **over):
    d = retex(copy.deepcopy(BT[base]), base, name, r, sel, r2, DATA)
    for k in ('aliases', 'texture_seasons', 'upgrade'): d.pop(k, None)
    if d.get('drop') == base: d['drop'] = name
    d.update(name=name, label=label); d.update(over); d = clean(d); tiles.append(d); return d

def item(base, name, label, r, sel='all', r2=None, tier=7, price=None, cat='materials', **over):
    d = retex(copy.deepcopy(BI[base]), base, name, r, sel, r2, DATA)
    # (Only what a thing is to look at and to hold comes over: what the game's one does is not this one's doing.)
    d = {k: v for k, v in d.items() if k in ('texture', 'max_stack', 'equip', 'tool', 'damage', 'range', 'fuel', 'two_tone')}
    d.update(name=name, label=label, category=cat, price=price or TIER_PRICE[tier])
    if cat not in ('benches', 'portals', 'creatures'): d['tier'] = tier
    d.update(over)
    d = clean(d); items.append(d); PRICE[name] = d['price']; return d

TIER_PRICE = {6: 4000, 7: 12000, 8: 40000, 9: 120000, 10: 400000}

def R(bench, out, ins, count=1, level=None, at=None, time=120, crystals=0, **more):
    r = {'bench': bench, 'output': {'item': out, 'count': count}, 'inputs': [{'item': i, 'count': n} for i, n in ins], 'time': time}
    if level: r['level'] = level
    if at: r['at'] = at
    if crystals: r['crystals'] = crystals
    r.update(more); recipes.append(r)

def block(base, name, label, r, tier, price, sel='all', r2=None, iover=None, **over):
    """A block and the item that is it."""
    t = tile(base, name, label, r, sel, r2, **over)
    if base in BI: item(base, name, label, r, sel, r2, tier=tier, price=price, places=name, **(iover or {})); return
    # (No item of the game's to copy: its icon is the block, as a block's is.)
    first = t['texture'] if isinstance(t['texture'], str) else t['texture'][0]
    icon = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); icon.paste(Image.open(os.path.join(TEX, first)).convert('RGBA').resize((32, 32), Image.NEAREST), (8, 8))
    os.makedirs(os.path.join(TEX, 'items'), exist_ok=True); icon.save(os.path.join(TEX, 'items', name + '.png'))
    items.append({'name': name, 'label': label, 'places': name, 'texture': 'items/%s.png' % name, 'category': 'materials', 'price': price, 'tier': tier}); PRICE[name] = price

def furnish(base, name, label, r, tier, price):
    """Something of the game's to put down, in another material: the block its item places, and the item."""
    tb = BI[base]['places']
    tile(tb, name, label, r, 'tint', drop=name)
    item(base, name, label, r, 'tint', tier=tier, price=price, places=name, **({'container': BI[base]['container']} if 'container' in BI[base] else {}))

# ---- the benches ------------------------------------------------------------------------------------------------
# Where things are made: (kind, the tile that must be had or None, level).
BENCH = {}
def ext(base, name, label, r, cost, crystals=0, numeral='', **over):
    """One more level of a bench the game has, hung on the end of whatever is highest."""
    kind = BT[base].get('bench_kind')
    e = {'cost': [{'item': i, 'count': n} for i, n in cost]}
    if crystals: e['crystals'] = crystals
    tile(base, name, label, r, 'tint', bench_level=None, trade_level=None, extends=e, **over)
    item(base, name, label + (' ' + numeral if numeral else ''), r, 'tint', cat='portals' if 'portal' in name else 'benches', price=TIER_PRICE[7], places=name)
    BENCH[name] = (kind, name)

def bench(base, name, label, r, kind=None, **over):
    """A bench of a kind of its own."""
    tile(base, name, label, r, 'tint', bench_kind=kind or name, bench_level=1, **over)
    item(base, name, label, r, 'tint', cat='benches', price=TIER_PRICE[7], places=name)
    BENCH[name] = (kind or name, None)

def make(where, out, ins, count=1, time=120, **more):
    """A recipe at one of this mod's benches, or at one of the game's by ('kind', level)."""
    if isinstance(where, tuple): R(where[0], out, ins, count, level=where[1], time=time, **more)
    else: R(BENCH[where][0], out, ins, count, at=BENCH[where][1], time=time, **more)

# ---- colours ----------------------------------------------------------------------------------------------------
SKY = dict(aetherstone=ramp('6f7fa8', '9fb0d4', 'd6e2f6'), cloudstone=ramp('b9c3d6', 'e0e7f2', 'ffffff'), skymarble=ramp('a9a2c8', 'd9d2ee', 'fbf8ff'),
           moonrock=ramp('3d4466', '6d76a0', 'b4bce0'), storm_slate=ramp('2a2c3a', '494c62', '7c809c'), ruin_brick=ramp('7d7460', 'b3a888', 'e0d6b6'))
DEEP = dict(deepslate=ramp('1e1f29', '383a49', '5d6074'), obsidian_rock=ramp('120d1c', '2a2038', '57466e'), magmarock=ramp('2a1410', '5a2a1c', 'c05a28'),
            lumerock=ramp('1d4a52', '3a8f96', '9ff0e6'), bonestone=ramp('8a8068', 'c8c0a4', 'f4eed8'), ashstone=ramp('3a3836', '63605c', '9a968e'))
METAL = dict(skysteel=ramp('4a6fa8', '8fb8e8', 'e4f4ff'), galvanite=ramp('8a6a10', 'e8c020', 'fff8a0'), aerite=ramp('3a9a8a', '7ee0c8', 'e0fff6'),
             moonsilver=ramp('6a6a90', 'b8b8e0', 'ffffff'), deepiron=ramp('2c2f3c', '5a6074', '9aa2b8'), cinderite=ramp('7a1c08', 'e85a18', 'ffd070'),
             umbral=ramp('1a1026', '46306a', '9a78d0'), adamant=ramp('0f4a3a', '20a078', '90ffd8'))
GEM = dict(zephyrite=ramp('1f8f9a', '5fe0e0', 'e8ffff'), heliodor=ramp('a87a08', 'ffd838', 'fffbd0'), lumite=ramp('2a7a9a', '70e0f0', 'f0ffff'),
           umbrite=ramp('2a1050', '7038c0', 'd0a8ff'), emberheart=ramp('8a1008', 'ff5020', 'ffe090'))
WOOD = dict(windwood=(ramp('8a7a5a', 'cbb98e', 'f0e6c8'), ramp('3f9a7a', '7fd8b0', 'd8fff0')), sky_willow=(ramp('6a6a80', 'a0a0bc', 'd8d8ec'), ramp('5a7ad8', '9ab8ff', 'e0ecff')),
            sunpetal=(ramp('4a8a3a', '7ac060', 'c0f0a0'), ramp('d88a10', 'ffd040', 'fff8c0')), frostpine=(ramp('4a5a6a', '7a90a8', 'c0d4e8'), ramp('8ac8e8', 'd0f0ff', 'ffffff')),
            cloud_oak=(ramp('9a9aa8', 'd0d0dc', 'ffffff'), ramp('c8d0e0', 'eef2fa', 'ffffff')), amberleaf=(ramp('6a4a2a', 'a87840', 'e0b878'), ramp('c05a10', 'f09030', 'ffd890')),
            storm_birch=(ramp('2a2a36', '55556a', '9090a8'), ramp('5a4a9a', '9080e0', 'd8d0ff')), star_apple=(ramp('5a3a5a', '8a5a8a', 'c8a0c8'), ramp('3a2a7a', '6a5ad0', 'c0b8ff')),
            bluecap=(ramp('b8b8a8', 'e0e0d0', 'ffffff'), ramp('1a4a9a', '3a8ae0', 'a0d8ff')), emberstool=(ramp('5a3a30', '8a5a48', 'c89880'), ramp('9a2008', 'f06020', 'ffc060')),
            pale_giant=(ramp('a8a090', 'd8d0c0', 'ffffff'), ramp('b0a890', 'e8e0c8', 'ffffff')), ink_morel=(ramp('3a3a4a', '6a6a80', 'a8a8c0'), ramp('10101a', '30304a', '6a6a9a')),
            lantern_fungus=(ramp('6a6a40', 'a0a060', 'd8d890'), ramp('a8a010', 'f0e840', 'ffffc0')), shelf_wood=(ramp('5a4028', '8a6840', 'c0a070'), ramp('7a5a30', 'b89058', 'e8c890')),
            rootwood=(ramp('3a2a1a', '6a4a2a', 'a07a48'), ramp('2a5a2a', '4a8a40', '90c878')), bonewood=(ramp('8a8470', 'c0b8a0', 'f0ead8'), ramp('9a9484', 'd0c8b4', 'faf4e4')))
TIER_COL = {7: GEM['zephyrite'], 8: GEM['heliodor'], 9: GEM['umbrite'], 10: GEM['emberheart']}
NUM = {5: 'V', 6: 'VI', 7: 'VII', 8: 'VIII', 9: 'IX', 10: 'X', 11: 'XI', 12: 'XII'}

# ---- what bounds the places, and what they are made of ---------------------------------------------------------
tile('stone', 'void_wall', 'Edge of the world', ramp('05050a', '0c0c14', '181824'), removable=False, drop=None, hardness=99, texture=None)
tiles[-1]['texture'] = retex('tiles/stone_0.png', 'stone', 'void_wall', ramp('05050a', '0c0c14', '181824'), 'all', None, DATA)

def stone(name, label, r, tier, **over):
    price = TIER_PRICE[tier] // 100
    block('stone', name, label, r, tier, price, **over)
    block('stone_column', name + '_column', label + ' column', r, tier, price)
    block('stone_stairs', name + '_stairs', label + ' stairs', r, tier, price)
    block('marble', name + '_tile', 'Polished ' + label.lower(), r, tier, price * 2)
    R('builder_bench', name + '_column', [(name, 1)], 2, time=80); R('builder_bench', name + '_stairs', [(name, 1)], 2, time=80)
    R('builder_bench', name + '_tile', [(name, 2)], 2, time=80)

for n, lab in [('aetherstone', 'Aetherstone'), ('cloudstone', 'Cloudstone'), ('skymarble', 'Skymarble'), ('moonrock', 'Moonrock'), ('storm_slate', 'Storm slate'), ('ruin_brick', 'Ruin brick')]:
    stone(n, lab, SKY[n], 7)
for n, lab in [('deepslate', 'Deepslate'), ('obsidian_rock', 'Obsidian'), ('magmarock', 'Magmarock'), ('lumerock', 'Lumerock'), ('bonestone', 'Bonestone'), ('ashstone', 'Ashstone')]:
    stone(n, lab, DEEP[n], 9, **({'light': 9, 'pulse': 0.08} if n == 'lumerock' else {'light': 3} if n == 'magmarock' else {}))

# Ground: the surfaces things grow on.
def ground(base, name, label, r, tier, soil=None, **over):
    block(base, name, label, r, tier, TIER_PRICE[tier] // 200, drop=soil or name, **over)
ground('dirt', 'sky_soil', 'Sky soil', ramp('5a5a8a', '8a8ab8', 'c0c0e0'), 7, drops=None, fertile=1.5)
ground('grass', 'cloud_grass', 'Cloud grass', ramp('5a8ad0', 'b0e0f0', 'ffffff'), 7)
ground('grass', 'sun_grass', 'Sun grass', ramp('b08a20', 'f0d050', 'fff8c0'), 7)
ground('ice', 'frost_glass', 'Frost glass', ramp('7ab0d8', 'c8e8f8', 'ffffff'), 7)
ground('sand', 'dense_cloud', 'Dense cloud', ramp('c0c8dc', 'eef2fa', 'ffffff'), 7, wall=False)
ground('sand', 'storm_cloud', 'Storm cloud', ramp('3a3c50', '6a6c88', 'a8aac8'), 7, wall=False, harm=0.02, light=5)
ground('dirt', 'deep_mycelium', 'Mycelium', ramp('4a3a5a', '8a70a0', 'd0b8e0'), 9, drops=None, fertile=1.5)
ground('grass', 'glowmoss', 'Glowmoss', ramp('1a6a4a', '40d090', 'c0ffe0'), 9, light=8)
ground('dirt', 'rootsoil', 'Rootsoil', ramp('2a1c10', '54381e', '8a6238'), 9, drops=None, fertile=2.0)
ground('sand', 'bone_gravel', 'Bone gravel', ramp('8a8068', 'c8c0a4', 'f4eed8'), 9)
ground('ice', 'obsidian_glass', 'Obsidian glass', ramp('0a0612', '2a1c40', '6a50a0'), 9)
block('luminous_plaster', 'magma', 'Magma', ramp('a01808', 'ff6818', 'ffe060'), 9, 900, iover={'fuel': 12000}, light=14, pulse=0.2, harm=0.25, see_through=None, mine_time=1.5)
block('luminous_plaster', 'steam_vent', 'Steam vent', ramp('5a5a60', 'a0a0a8', 'f0f0f8'), 9, 600, light=4, pulse=0.3, harm=0.08, see_through=None)

# ---- metals: ore, ingot, block, tools, armour ------------------------------------------------------------------
# name, label, tier, the rock it lies in, hardness to dig, power its pick has, tool speed, armour, what its armour does besides
METALS = [('skysteel', 'Skysteel', 7, 'aetherstone', 7, 8, 9.5, 0.2, {'walk': 1.04}), ('galvanite', 'Galvanite', 7, 'storm_slate', 7, 0, 0, 0, None),
          ('aerite', 'Aerite', 7, 'cloudstone', 7, 0, 0, 0, None), ('moonsilver', 'Moonsilver', 8, 'moonrock', 8, 9, 11.0, 0.22, {'mine': 1.06}),
          ('deepiron', 'Deepiron', 9, 'deepslate', 9, 10, 12.5, 0.24, {'strength': 1}), ('cinderite', 'Cinderite', 9, 'magmarock', 9, 0, 0, 0, None),
          ('umbral', 'Umbral steel', 9, None, 0, 11, 14.0, 0.25, {'luck': 0.05}), ('adamant', 'Adamant', 10, 'obsidian_rock', 11, 12, 16.0, 0.27, {'regen': 1.1})]
HOST = dict(SKY, **DEEP)
for n, lab, tier, host, hard, power, speed, armor, boon in METALS:
    r, p = METAL[n], TIER_PRICE[tier]
    if host:
        tile('copper_ore', n + '_ore', lab + ' ore', r, 'ore', HOST[host], hardness=hard, mine_time=1.6)
        item('copper_ore', n + '_ore', lab + ' ore', r, 'ore', HOST[host], tier=tier, price=p // 8)
    item('iron_ingot', n + '_ingot', lab + ' ingot', r, tier=tier, price=p // 3)
    block('iron_block', n + '_block', lab + ' block', r, tier, p * 2, **({'floats': True} if n == 'aerite' else {'light': 10} if n == 'cinderite' else {}))
    block('iron_column', n + '_column', lab + ' column', r, tier, p)
    block('iron_stairs', n + '_stairs', lab + ' stairs', r, tier, p)
    item('tin_foil', n + '_plate', lab + ' plate', r, tier=tier, price=p)
    item('stick', n + '_rod', lab + ' rod', r, tier=tier, price=p // 2)
    item('train_wheel', n + '_gear', lab + ' gear', r, 'tint', tier=tier, price=p)
    furnish('iron_door', n + '_door', lab + ' door', r, tier, p * 2)
    furnish('steel_lantern', n + '_lantern', lab + ' lantern', r, tier, p)
    if not power: continue
    for tool, tlab, base, cls, k in [('pickaxe', 'pickaxe', 'iron_pickaxe', 'pickaxe', 5), ('axe', 'axe', 'iron_axe', 'axe', 4), ('shovel', 'shovel', 'gold_shovel', 'shovel', 3), ('machete', 'machete', 'iron_machete', 'machete', 3)]:
        item(base, n + '_' + tool, lab + ' ' + tlab, r, 'gray', tier=tier, price=p * k, cat='tools',
             tool={'class': cls, 'speed': speed}, power=power if tool == 'pickaxe' else None)
    item('iron_sword', n + '_sword', lab + ' sword', r, 'gray', tier=tier, price=p * 4, cat='tools', damage=int(speed * 2.2))
    for piece, plab, k in [('helmet', 'helmet', 2), ('chestplate', 'chestplate', 5), ('leggings', 'leggings', 3), ('boots', 'boots', 2)]:
        item('iron_' + piece, n + '_' + piece, lab + ' ' + plab, r, tier=tier, price=p * k, cat='clothing', armor=armor, warmth=0.25,
             cooling=0.15 if tier >= 9 else None, boons=boon)

# ---- gems --------------------------------------------------------------------------------------------------------
# name, label, tier, hardness, the power its pickaxe has
GEMS = [('zephyrite', 'Zephyrite', 7, 8, 8), ('heliodor', 'Heliodor', 8, 8, 9), ('lumite', 'Lumite', 9, 9, 0), ('umbrite', 'Umbrite', 9, 10, 11), ('emberheart', 'Emberheart', 10, 12, 12)]
for n, lab, tier, hard, power in GEMS:
    r, p = GEM[n], TIER_PRICE[tier]
    tile('diamond', n, lab, r, hardness=hard, light=9 if n in ('lumite', 'emberheart') else 6)
    item('diamond', n, lab, r, tier=tier, price=p)
    item('diamond_shard', n + '_shard', lab + ' shard', r, tier=tier, price=p // 6)
    block('diamond_block', n + '_block', lab + ' block', r, tier, p * 6)
    block('diamond_column', n + '_column', lab + ' column', r, tier, p * 3)
    block('diamond_stairs', n + '_stairs', lab + ' stairs', r, tier, p * 3)
    block('diamond_chandelier', n + '_chandelier', lab + ' chandelier', r, tier, p * 4)
    if not power: continue
    item('iron_pickaxe', n + '_pickaxe', lab + ' pickaxe', r, 'gray', cat='special', price=p * 4, tool={'class': 'pickaxe', 'speed': 14.0 + (tier - 6) * 3, 'ore_multiplier': tier}, power=power)
    # The dodo of it: the egg, the bird, and the bird in a cage.
    tile('diamond_dodo_egg_placed', n + '_dodo_egg_placed', lab + ' dodo egg', r, drop=n + '_dodo_egg', hatches=n + '_dodo')
    item('diamond_dodo_egg', n + '_dodo_egg', lab + ' dodo egg', r, cat='creatures', price=p * 3, places=n + '_dodo_egg_placed')
    item('caged_diamond_dodo', 'caged_' + n + '_dodo', 'Caged ' + lab.lower() + ' dodo', r, 'tint', cat='creatures', price=p * 6)

# ---- trees -------------------------------------------------------------------------------------------------------
# name, label, the tree whose shape it has, what it bears (and the picture that is drawn from), how much that feeds
SKY_SOIL, DEEP_SOIL = ['sky_soil', 'cloud_grass', 'sun_grass'], ['deep_mycelium', 'glowmoss', 'rootsoil']
WOODS = [('windwood', 'Windwood', 'oak', 'wind_nut', 'Wind nut', 'acorn', 0.08, 7), ('sky_willow', 'Sky-willow', 'cherry', 'sky_catkin', 'Willow catkin', 'pine_cone', 0, 7),
         ('sunpetal', 'Sunpetal', 'apple', 'sunpetal_bloom', 'Sunpetal bloom', 'orange', 0.15, 7), ('frostpine', 'Frostpine', 'pine', 'frost_cone', 'Frost cone', 'pine_cone', 0, 7),
         ('cloud_oak', 'Cloud-oak', 'maple', 'cloud_acorn', 'Cloud acorn', 'acorn', 0.08, 7), ('amberleaf', 'Amberleaf', 'mango', 'amber_fruit', 'Amber fruit', 'mango', 0.2, 7),
         ('storm_birch', 'Storm birch', 'cherry', 'storm_seed', 'Storm seed', 'maple_seed', 0, 7), ('star_apple', 'Star-apple', 'apple', 'star_apple_fruit', 'Star-apple', 'apple', 0.25, 7),
         ('bluecap', 'Bluecap', 'oak', 'bluecap_spore', 'Bluecap spore', 'acorn', 0.1, 9), ('emberstool', 'Emberstool', 'apple', 'ember_spore', 'Ember spore', 'chilli', 0.1, 9),
         ('pale_giant', 'Pale giant', 'pine', 'pale_spore', 'Pale spore', 'pine_cone', 0, 9), ('ink_morel', 'Ink morel', 'cherry', 'ink_cap', 'Ink cap', 'cherry', 0.15, 9),
         ('lantern_fungus', 'Lantern fungus', 'orange', 'lantern_bulb', 'Lantern bulb', 'orange', 0.15, 9), ('shelf_wood', 'Shelf-wood', 'maple', 'shelf_spore', 'Shelf spore', 'maple_seed', 0, 9),
         ('rootwood', 'Rootwood', 'mango', 'root_knot', 'Root knot', 'coconut', 0.2, 9), ('bonewood', 'Bonewood', 'coconut', 'bone_seed', 'Bone seed', 'coconut', 0, 9)]
for n, lab, shape, fruit, flab, ficon, food, tier in WOODS:
    trunk, leaf = WOOD[n]
    sky = tier == 7
    tile('oak_log', n + '_log', lab + (' trunk' if sky else ' stem'), trunk, drop=n + '_wood')
    tile('oak_leaves', n + '_leaves', lab + (' leaves' if sky else ' cap'), leaf, light=7 if n in ('lantern_fungus', 'bluecap') else None)
    block('wood', n + '_wood', lab + ' wood', trunk, tier, TIER_PRICE[tier] // 150)
    block('wood_column', n + '_wood_column', lab + ' post', trunk, tier, TIER_PRICE[tier] // 150)
    block('wood_stairs', n + '_wood_stairs', lab + ' steps', trunk, tier, TIER_PRICE[tier] // 150)
    R('woodwork_bench', n + '_wood_column', [(n + '_wood', 1)], 2, time=60); R('woodwork_bench', n + '_wood_stairs', [(n + '_wood', 1)], 2, time=60)
    # Furniture of it: a door, a ladder and a shelf.
    for base, what, wlab, need in [('wooden_door', 'door', 'door', 3), ('trapdoor', 'trapdoor', 'trapdoor', 2), ('shelf', 'shelf', 'shelf', 2)]:
        furnish(base, n + '_' + what, lab + ' ' + wlab, trunk, tier, TIER_PRICE[tier] // 100)
        R('woodwork_bench', n + '_' + what, [(n + '_wood', need)], 1, time=80)
    item(ficon, fruit, flab, leaf, 'tint', tier=tier, price=TIER_PRICE[tier] // 200, plants=n, sows=None, food=food or None)
    t = copy.deepcopy(BTR[shape]); t.update(name=n, log=n + '_log', leaves=n + '_leaves', fruit=fruit, soil=SKY_SOIL if sky else DEEP_SOIL); t.pop('needs_water', None)
    trees.append(t)

# ---- crops and wild plants -------------------------------------------------------------------------------------
def chain(start):
    seen, todo = [], [start]
    while todo:
        n = todo.pop()
        if n in seen or n not in BT: continue
        seen.append(n); todo += [BT[n].get(k) for k in ('grows_into', 'grows_top', 'topped_by', 'cut_to') if BT[n].get(k)]
    return seen
# name, label, the crop whose way of growing it has, colours, how much it feeds, tier
CROPS = [('skyberry', 'Skyberry', 'tomato', ramp('2a3a8a', '5a7ae0', 'c0d0ff'), 0.15, 7), ('cloud_reed', 'Cloud-reed', 'flax', ramp('8aa0c0', 'd0e0f0', 'ffffff'), 0, 7),
         ('windwheat', 'Windwheat', 'wheat', ramp('6a9a8a', 'a8e0c8', 'f0fff8'), 0, 7), ('frost_mint', 'Frost mint', 'carrot', ramp('3a9a9a', '80e0e0', 'e0ffff'), 0.1, 7),
         ('thunder_pepper', 'Thunder pepper', 'chilli', ramp('5a3a9a', 'a070f0', 'e8d0ff'), 0.1, 7), ('sun_corn', 'Sun corn', 'corn', ramp('c07a10', 'ffc830', 'fff4b0'), 0.15, 7),
         ('aether_cotton', 'Aether cotton', 'flax', ramp('b0a0d0', 'e8e0f8', 'ffffff'), 0, 7), ('moon_lily', 'Moon lily', 'sunflower', ramp('6a6aa0', 'c0c0f0', 'ffffff'), 0, 7),
         ('glowcap', 'Glowcap', 'carrot', ramp('1a7a6a', '40e0c0', 'd0fff0'), 0.15, 9), ('cave_rice', 'Cave rice', 'wheat', ramp('8a8a6a', 'd0d0a8', 'ffffe0'), 0, 9),
         ('blindroot', 'Blindroot', 'carrot', ramp('8a6a5a', 'd0a890', 'ffe8d8'), 0.2, 9), ('ember_pepper', 'Ember pepper', 'chilli', ramp('9a2008', 'f06020', 'ffc060'), 0.1, 9),
         ('moss_melon', 'Moss melon', 'tomato', ramp('2a6a2a', '5ac050', 'c0f0a0'), 0.25, 9), ('stoneberry', 'Stoneberry', 'tomato', ramp('4a4a5a', '8a8aa0', 'd0d0e0'), 0.15, 9),
         ('spore_pod', 'Spore pod', 'corn', ramp('6a4a7a', 'b080c8', 'f0d0ff'), 0.1, 9), ('deep_lichen', 'Deep lichen', 'flax', ramp('3a6a5a', '70b098', 'c8f0e0'), 0, 9)]
WILD = {7: [], 9: []}
for n, lab, base, r, food, tier in CROPS:
    for t in chain(base + '_0'):
        d = tile(t, t.replace(base, n), lab, r, 'tint', soil=SKY_SOIL if tier == 7 else DEEP_SOIL, light=6 if n in ('glowcap', 'moon_lily') else None)
        for k in ('grows_into', 'grows_top', 'topped_by', 'cut_to'):
            if k in d: d[k] = d[k].replace(base, n)
        for dr in d.get('drops', []): dr['item'] = n if dr['item'] in (base, 'flax_seeds', 'sunflower_seed') else dr['item']
        if d.get('drop') in (base, 'flax_seeds', 'sunflower_seed'): d['drop'] = n
        d['drops'] = [dr for dr in d.get('drops', []) if dr['item'] == n] or None
        if d['drops'] is None: d.pop('drops'); d['drop'] = n
    item(base if base in BI else 'wheat', n, lab, r, 'tint', tier=tier, price=TIER_PRICE[tier] // 300, sows=n + '_0', food=food or None)
    WILD[tier].append(n + '_0')

# ---- lights, beds, chests, vines, power -------------------------------------------------------------------------
block('oil_lantern', 'sky_lantern', 'Sky lantern', GEM['zephyrite'], 7, 400, 'tint', light=15)
block('oil_lantern', 'lumite_lamp', 'Lumite lamp', GEM['lumite'], 9, 4000, 'tint', light=15)
block('torch', 'cinder_torch', 'Cinder torch', METAL['cinderite'], 9, 900, 'tint', light=14)
tile('vine', 'sky_vine', 'Sky vine', ramp('3a7ab0', '80c8f0', 'e0f8ff')); item('vine', 'sky_vine', 'Sky vine', ramp('3a7ab0', '80c8f0', 'e0f8ff'), tier=7, price=40, places='sky_vine')
tile('vine', 'lumen_vine', 'Glow vine', ramp('1a7a5a', '40e0a0', 'd0fff0'), light=8); item('vine', 'lumen_vine', 'Glow vine', ramp('1a7a5a', '40e0a0', 'd0fff0'), tier=9, price=400, places='lumen_vine')
tile('soft_bed_foot', 'cloud_bed_foot', 'Cloud bed', SKY['cloudstone'], 'tint')
block('soft_bed', 'cloud_bed', 'Cloud bed', SKY['cloudstone'], 7, 9000, 'tint', pair='cloud_bed_foot', bed={'speed': 5.0, 'over': True})
block('chest', 'skysteel_chest', 'Skysteel chest', METAL['skysteel'], 7, 20000, 'tint', iover={'container': {'slots': 16}}, burns=None)
block('chest', 'deepiron_chest', 'Deepiron chest', METAL['deepiron'], 9, 200000, 'tint', iover={'container': {'slots': 16}}, burns=None)
block('copper_wire', 'galvanite_wire', 'Galvanite wire', METAL['galvanite'], 7, 600)
if 'copper_wire_floor' in BT:
    tile('copper_wire_floor', 'galvanite_wire_floor', 'Galvanite wire', METAL['galvanite']); tiles[-1]['drop'] = 'galvanite_wire'
    items[-1]['places_on_block'] = 'galvanite_wire_floor' if BI['copper_wire'].get('places_on_block') else None; items[-1] = clean(items[-1])
block('flywheel', 'storm_battery', 'Storm battery', METAL['galvanite'], 7, 60000, 'tint', bench_kind='storm_battery', battery=400000)
block('solar_panel', 'wind_turbine', 'Wind turbine', METAL['skysteel'], 7, 30000, 'tint', bench_kind='wind_turbine', generator={'power': 30, 'needs': 'sky'})
block('solar_panel', 'lightning_rod', 'Lightning rod', METAL['galvanite'], 8, 90000, 'tint', bench_kind='lightning_rod', generator={'power': 80, 'needs': 'sky'})
block('steam_generator', 'geothermal_generator', 'Geothermal generator', METAL['cinderite'], 9, 250000, 'tint', bench_kind='geothermal_generator', generator={'power': 150, 'needs': 'heat'}, fuel_capacity=None, texture_lit=None, lit_fps=None)

# ---- the benches -------------------------------------------------------------------------------------------------
zc, hc, uc, ec = GEM['zephyrite'], GEM['heliodor'], GEM['umbrite'], GEM['emberheart']
ext('workbench_4', 'aether_workbench', 'Aether workbench', METAL['skysteel'], [('skysteel_ingot', 5), ('zephyrite', 1)])
ext('workbench_4', 'deep_workbench', 'Deep workbench', METAL['deepiron'], [('deepiron_ingot', 5), ('golem_core', 1)])
for tier, gem in [(7, 'zephyrite'), (8, 'heliodor'), (9, 'umbrite'), (10, 'emberheart')]:
    ext('portal_6', 'dim_portal_%d' % tier, 'Portal', TIER_COL[tier], [(gem, 1)], numeral=NUM[tier])
    ext('trade_portal_6', 'dim_trade_portal_%d' % tier, 'Trade portal', TIER_COL[tier], [(gem, 1), ('platinum_coin', (tier - 6) * 2)], crystals=(tier - 6) * 400, numeral=NUM[tier])
for metal, lab, n in [('skysteel', 'Skysteel', 8), ('moonsilver', 'Moonsilver', 9), ('deepiron', 'Deepiron', 10), ('umbral', 'Umbral', 11), ('adamant', 'Adamant', 12)]:
    ext('tool_bench_7', metal + '_tool_bench', 'Tool bench', METAL[metal], [(metal + '_ingot', 3)], numeral=NUM[n])
for metal, n in [('skysteel', 4), ('moonsilver', 5), ('deepiron', 6), ('adamant', 7)]:
    ext('armor_bench_3', metal + '_armor_bench', 'Armor bench', METAL[metal], [(metal + '_plate', 3)], numeral=NUM.get(n, 'IV'))
ext('furnace_3', 'aether_furnace', 'Furnace', SKY['aetherstone'], [('aetherstone_tile', 10), ('skysteel_ingot', 2)], numeral='IV')
ext('furnace_3', 'cinder_furnace', 'Furnace', METAL['cinderite'], [('magmarock_tile', 10), ('cinderite_ingot', 2)], numeral='V')
for base, kind, lab in [('metalwork_bench_2', 'metalwork_bench', 'Metalwork bench'), ('craft_bench_4', 'craft_bench', 'Craft bench'), ('press_3', 'press', 'Press'),
                        ('tailor_bench_2', 'tailor_bench', 'Tailor\'s bench'), ('woodwork_bench', 'woodwork_bench', 'Woodwork bench'), ('builder_bench', 'builder_bench', 'Builder\'s bench'),
                        ('kiln', 'kiln', 'Kiln'), ('mixing_bench', 'mixing_bench', 'Mixing bench'), ('campfire', 'campfire', 'Hearth')]:
    ext(base, 'sky_' + kind, lab, METAL['skysteel'], [('skysteel_plate', 2), ('zephyrite_shard', 2)])
    ext(base, 'deep_' + kind, lab, METAL['deepiron'], [('deepiron_plate', 2), ('umbrite_shard', 2)])
bench('furnace_3', 'skyforge', 'Skyforge', GEM['zephyrite'])
bench('tailor_bench_2', 'cloud_loom', 'Cloud loom', SKY['cloudstone'])
bench('easel', 'stargazer_table', 'Stargazer\'s table', SKY['moonrock'])
bench('furnace_3', 'magma_forge', 'Magma forge', METAL['cinderite'])
bench('press_3', 'crystal_lathe', 'Crystal lathe', GEM['lumite'])
bench('compost_bin', 'fungal_vat', 'Fungal vat', ramp('4a3a5a', '8a70a0', 'd0b8e0'))
bench('woodwork_bench', 'bone_carver', 'Bone carver', DEEP['bonestone'])

# ---- the ways through -------------------------------------------------------------------------------------------
def gate(name, label, r, leads, **over):
    tile('portal_6', name, label, r, bench=None, bench_kind=None, bench_level=None, leads_to=leads, **over)
    item('portal_6', name, label, r, cat='portals', price=TIER_PRICE[7] * 10, places=name)
gate('aether_portal', 'Aether portal', ramp('5a8ad0', 'b0e0ff', 'ffffff'), 'aether', opens_above=80)
gate('deep_portal', 'Deep portal', ramp('5a0c08', 'd03818', 'ffb060'), 'deep', opens_below=20)
gate('world_seed', 'World seed', ramp('1a6a3a', '50d080', 'e0ffd0'), 'pocket')

# ---- creatures ---------------------------------------------------------------------------------------------------
def mob(base, name, label, r, sel='tint', **over):
    b = BM[base]
    d = retex(copy.deepcopy(b), base, name, r, sel, None, b.get('_root', DATA))
    for k in ('caged', 'lays', 'immune', 'weak_to', 'milk', 'shear'): d.pop(k, None)
    d.update(name=name, label=label); d.update(over); mobs.append(clean(d)); return mobs[-1]
def drop(i, a, b, chance=None): return clean({'item': i, 'min': a, 'max': b, 'chance': chance})
def loot(name, label, base, r, tier, **over): item(base, name, label, r, 'tint', tier=tier, price=over.pop('price', TIER_PRICE[tier] // 20), **over)

loot('cloud_wool', 'Cloud wool', 'yak_shavings', SKY['cloudstone'], 7); loot('moa_meat', 'Moa meat', 'dodo_meat', ramp('8a3a5a', 'e08aa0', 'ffd8e0'), 7, food=0.15)
loot('sky_feather', 'Sky feather', 'feather', GEM['zephyrite'], 7); loot('ray_hide', 'Ray hide', 'leather', ramp('4a6a9a', '90b8e0', 'e0f0ff'), 7)
loot('sky_jelly', 'Sky jelly', 'raw_resin', ramp('8a4a9a', 'd890e0', 'ffe0ff'), 7, food=0.1); loot('wind_essence', 'Wind essence', 'rainbow_essence', GEM['zephyrite'], 7)
loot('stardust', 'Stardust', 'salt', ramp('6a5ab0', 'c0b0ff', 'ffffff'), 8); loot('sentinel_shard', 'Sentinel shard', 'flint', SKY['ruin_brick'], 7)
loot('storm_feather', 'Storm feather', 'feather', METAL['galvanite'], 8, price=TIER_PRICE[8]); loot('hawk_talon', 'Hawk talon', 'yak_horn', SKY['storm_slate'], 7)
loot('chitin', 'Chitin', 'leather', ramp('1a2a2a', '3a6a6a', '80c0b0'), 9); loot('cave_silk', 'Cave silk', 'string', ramp('8a8a9a', 'd0d0e0', 'ffffff'), 9)
loot('bat_wing', 'Bat wing', 'leather', ramp('1a1020', '40304a', '80688a'), 9); loot('blind_fish', 'Blind cavefish', 'fish', ramp('9a8a8a', 'e0d0d0', 'ffffff'), 9, food=0.15)
loot('eel_lantern', 'Eel lantern', 'fish', GEM['lumite'], 9, food=0.1); loot('slug_cinder', 'Slug cinder', 'ember', METAL['cinderite'], 9, fuel=6000)
loot('spore_dust', 'Spore dust', 'flour', ramp('6a4a7a', 'b080c8', 'f0d0ff'), 9); loot('mole_fur', 'Mole fur', 'fur', ramp('2a2420', '5a4a40', '9a8878'), 9)
loot('worm_tooth', 'Worm tooth', 'yak_horn', DEEP['bonestone'], 9); loot('strider_hide', 'Strider hide', 'leather', ramp('4a3a5a', '8a70a0', 'd0b8e0'), 9)
loot('shadow_essence', 'Shadow essence', 'rainbow_essence', GEM['umbrite'], 9); loot('old_bone', 'Old bone', 'stick', DEEP['bonestone'], 9)
loot('golem_core', 'Golem core', 'time_crystal', GEM['lumite'], 9, price=TIER_PRICE[9] * 2); loot('amber', 'Amber', 'raw_resin', ramp('8a4a08', 'e09020', 'ffe090'), 9)
loot('fossil', 'Fossil', 'coral', DEEP['bonestone'], 9, places=None)

SKY_B = ['cloudmeadow', 'skywood', 'stormreach', 'crystal_spires', 'sunpetal_gardens', 'frostveil', 'sky_ruins', 'cloud_shoals']
DEEP_B = ['fungal_forest', 'glowmoss_grotto', 'crystal_cathedral', 'bone_hollows', 'sunken_lake', 'obsidian_flats', 'magma_fields', 'the_roots', 'the_abyss']
def above(tiles_, biome, chance): return {'above': tiles_, 'biome': biome, 'chance': chance}
mob('yak', 'fleeceling', 'Fleeceling', SKY['cloudstone'], spawn=above(['cloud_grass'], ['cloudmeadow', 'skywood'], 0.3), health=12,
    shear={'item': 'cloud_wool', 'min': 1, 'max': 3, 'regrow_ticks': 3000, 'texture': None}, drops=[drop('cloud_wool', 1, 2), drop('moa_meat', 1, 1)])
mobs[-1]['shear'] = clean(mobs[-1]['shear'])
mob('dodo', 'moa', 'Moa', ramp('8a5a3a', 'd0a070', 'fff0d0'), spawn=above(['cloud_grass', 'sun_grass'], ['cloudmeadow', 'sunpetal_gardens'], 0.3), health=14,
    drops=[drop('moa_meat', 1, 3), drop('sky_feather', 1, 2)])
mob('diamond_dodo', 'zephyrite_dodo', 'Zephyrite dodo', GEM['zephyrite'], 'all', spawn=above(['cloud_grass'], ['crystal_spires', 'cloudmeadow'], 0.03),
    lays={'tile': 'zephyrite_dodo_egg_placed', 'chance': 0.0004, 'next': ['heliodor_dodo'], 'next_chance': 0.2}, caged='caged_zephyrite_dodo',
    drops=[drop('dodo_meat', 1, 2), drop('sky_feather', 1, 2), drop('zephyrite', 1, 1, 0.1)])
mob('diamond_dodo', 'heliodor_dodo', 'Heliodor dodo', GEM['heliodor'], 'all', lays={'tile': 'heliodor_dodo_egg_placed', 'chance': 0.0004}, caged='caged_heliodor_dodo',
    drops=[drop('dodo_meat', 1, 2), drop('sky_feather', 1, 2), drop('heliodor', 1, 1, 0.1)])
mob('air_spirit', 'cloud_ray', 'Cloud ray', ramp('4a6a9a', '90b8e0', 'e0f0ff'), drops=[drop('ray_hide', 1, 2)], health=12, cap=3, spawn={'sky': True, 'biome': SKY_B, 'chance': 0.004})
mob('air_spirit', 'sky_jelly_mob', 'Sky jelly', ramp('8a4a9a', 'd890e0', 'ffe0ff'), drops=[drop('sky_jelly', 1, 2)], health=8, cap=3, spawn={'sky': True, 'biome': ['cloud_shoals', 'cloudmeadow'], 'chance': 0.004})
mob('flame_spirit', 'storm_wisp', 'Storm wisp', METAL['galvanite'], drops=[drop('wind_essence', 1, 2)], health=14, cap=2, spawn={'sky': True, 'biome': ['stormreach'], 'chance': 0.006})
mob('air_spirit', 'star_moth', 'Star moth', ramp('6a5ab0', 'c0b0ff', 'ffffff'), drops=[drop('stardust', 1, 2)], health=4, cap=4, spawn={'sky': True, 'biome': SKY_B, 'time': 'night', 'chance': 0.006})
mob('dropbear', 'gale_hawk', 'Gale hawk', SKY['storm_slate'], health=16, drops=[drop('sky_feather', 1, 3), drop('hawk_talon', 1, 1, 0.5)])
mobs[-1]['perch'] = dict(mobs[-1]['perch'], trees=['windwood', 'sky_willow', 'frostpine', 'cloud_oak'], bite=0.2)
mob('dropbear', 'thunderbird', 'Thunderbird', METAL['galvanite'], health=120, cap=1, spawn={'chance': 0.05}, drops=[drop('storm_feather', 1, 2), drop('galvanite_ingot', 2, 5)])
mobs[-1]['perch'] = dict(mobs[-1]['perch'], trees=['storm_birch'], bite=0.4, rest_ticks=20)
mob('scorpion', 'ruin_sentinel', 'Ruin sentinel', SKY['ruin_brick'], health=30, spawn=above(['ruin_brick'], ['sky_ruins'], 0.2), drops=[drop('sentinel_shard', 1, 3), drop('zephyrite_shard', 1, 1, 0.3)])
mob('scorpion', 'frost_mite', 'Frost mite', ramp('7ab0d8', 'c8e8f8', 'ffffff'), health=8, spawn=above(['frost_glass'], ['frostveil'], 0.2), drops=[drop('stardust', 1, 1, 0.3)])

mob('dodo', 'glow_beetle', 'Glow beetle', ramp('1a6a4a', '40d090', 'c0ffe0'), spawn=above(['deep_mycelium', 'glowmoss'], ['fungal_forest', 'glowmoss_grotto'], 0.3), health=6, drops=[drop('chitin', 1, 2)])
mob('scorpion', 'cave_crawler', 'Cave crawler', ramp('1a2a2a', '3a6a6a', '80c0b0'), health=18, spawn=above(['deepslate', 'deep_mycelium', 'bone_gravel'], ['fungal_forest', 'bone_hollows', 'the_roots'], 0.15),
    drops=[drop('chitin', 1, 3), drop('cave_silk', 1, 2)])
mob('dropbear', 'deep_bat', 'Deep bat', ramp('1a1020', '40304a', '80688a'), health=12, drops=[drop('bat_wing', 1, 2)])
mobs[-1]['perch'] = dict(mobs[-1]['perch'], trees=['bluecap', 'pale_giant', 'ink_morel', 'shelf_wood', 'rootwood'], bite=0.2)
mob('fish', 'blind_cavefish', 'Blind cavefish', ramp('9a8a8a', 'e0d0d0', 'ffffff'), drops=[drop('blind_fish', 1, 1)], spawn={'water': True, 'biome': DEEP_B, 'chance': 0.03})
mob('shark', 'lantern_eel', 'Lantern eel', GEM['lumite'], health=30, drops=[drop('eel_lantern', 1, 2), drop('blind_fish', 1, 2)], spawn={'water': True, 'biome': DEEP_B, 'chance': 0.004})
mob('scorpion', 'magma_slug', 'Magma slug', METAL['cinderite'], health=20, spawn=above(['magmarock'], ['magma_fields', 'the_abyss'], 0.2), drops=[drop('slug_cinder', 1, 3)])
mob('dodo', 'spore_puffer', 'Spore puffer', ramp('6a4a7a', 'b080c8', 'f0d0ff'), health=5, spawn=above(['deep_mycelium'], ['fungal_forest'], 0.2), drops=[drop('spore_dust', 1, 3)])
mob('yak', 'tunnel_mole', 'Tunnel mole', ramp('2a2420', '5a4a40', '9a8878'), health=14, spawn=above(['rootsoil', 'deepslate'], ['the_roots', 'fungal_forest'], 0.12),
    shear={'item': 'mole_fur', 'min': 1, 'max': 2, 'regrow_ticks': 3000}, drops=[drop('mole_fur', 1, 2)])
mob('scorpion', 'rock_worm', 'Rock worm', DEEP['ashstone'], health=24, spawn=above(['ashstone', 'obsidian_glass'], ['obsidian_flats', 'the_abyss'], 0.15), drops=[drop('worm_tooth', 1, 2)])
mob('yak', 'fungus_strider', 'Fungus strider', ramp('4a3a5a', '8a70a0', 'd0b8e0'), health=40, spawn=above(['glowmoss'], ['glowmoss_grotto'], 0.1), drops=[drop('strider_hide', 1, 3), drop('spore_dust', 1, 2)])
mob('scorpion', 'umbral_stalker', 'Umbral stalker', GEM['umbrite'], health=35, spawn=dict(above(['obsidian_glass', 'ashstone'], ['obsidian_flats', 'the_abyss', 'crystal_cathedral'], 0.1)),
    drops=[drop('shadow_essence', 1, 2), drop('umbrite_shard', 1, 1, 0.3)])
mob('scorpion', 'bone_hound', 'Bone hound', DEEP['bonestone'], health=22, step_ticks=2, spawn=above(['bone_gravel'], ['bone_hollows'], 0.25), drops=[drop('old_bone', 1, 3), drop('fossil', 1, 1, 0.2)])
mob('scorpion', 'crystal_golem', 'Crystal golem', GEM['lumite'], health=150, cap=1, step_ticks=5, spawn=above(['lumerock'], ['crystal_cathedral'], 0.03), drops=[drop('golem_core', 1, 1), drop('lumite', 2, 4)])
mobs[-1]['sting'] = dict(mobs[-1]['sting'], ticks=120, sight=10)
mob('diamond_dodo', 'umbrite_dodo', 'Umbrite dodo', GEM['umbrite'], 'all', spawn=above(['glowmoss', 'deep_mycelium'], ['glowmoss_grotto', 'crystal_cathedral'], 0.03),
    lays={'tile': 'umbrite_dodo_egg_placed', 'chance': 0.0004, 'next': ['emberheart_dodo'], 'next_chance': 0.2}, caged='caged_umbrite_dodo',
    drops=[drop('dodo_meat', 1, 2), drop('feather', 1, 2), drop('umbrite', 1, 1, 0.1)])
mob('diamond_dodo', 'emberheart_dodo', 'Emberheart dodo', GEM['emberheart'], 'all', lays={'tile': 'emberheart_dodo_egg_placed', 'chance': 0.0004}, caged='caged_emberheart_dodo',
    drops=[drop('dodo_meat', 1, 2), drop('feather', 1, 2), drop('emberheart', 1, 1, 0.1)])
for g in ('zephyrite', 'heliodor', 'umbrite', 'emberheart'): items[[i['name'] for i in items].index('caged_' + g + '_dodo')]['releases'] = None
items[:] = [clean(i) for i in items]

# ---- smelting, tools, armour, gems: the recipes every material has ---------------------------------------------
SMELT = {'skysteel': 'skyforge', 'galvanite': 'skyforge', 'aerite': 'skyforge', 'moonsilver': 'stargazer_table', 'deepiron': 'magma_forge', 'cinderite': 'magma_forge', 'adamant': 'magma_forge'}
TOOLB = {'skysteel': 'skysteel_tool_bench', 'moonsilver': 'moonsilver_tool_bench', 'deepiron': 'deepiron_tool_bench', 'umbral': 'umbral_tool_bench', 'adamant': 'adamant_tool_bench'}
ARMB = {'skysteel': 'skysteel_armor_bench', 'moonsilver': 'moonsilver_armor_bench', 'deepiron': 'deepiron_armor_bench', 'umbral': 'deepiron_armor_bench', 'adamant': 'adamant_armor_bench'}
for n, lab, tier, host, hard, power, speed, armor, boon in METALS:
    sky = tier < 9
    mw, pr = ('sky_metalwork_bench', 'sky_press') if sky else ('deep_metalwork_bench', 'deep_press')
    if host: make(SMELT[n], n + '_ingot', [(n + '_ore', 3)], time=160)
    make(mw, n + '_block', [(n + '_ingot', 5)]); make(mw, n + '_column', [(n + '_block', 1)], 2); make(mw, n + '_stairs', [(n + '_block', 1)], 2)
    make(pr, n + '_plate', [(n + '_ingot', 2)]); make(mw, n + '_rod', [(n + '_ingot', 1)], 2); make(mw, n + '_gear', [(n + '_plate', 1), (n + '_rod', 1)])
    make(mw, n + '_door', [(n + '_plate', 3)]); make(mw, n + '_lantern', [(n + '_plate', 1), ('skyglass' if sky else 'magma_glass', 1)], 2)
    make(pr, n + '_ingot', [(n + '_block', 1)], 5, time=60)
    if not power: continue
    wood = 'windwood_wood_column' if sky else 'bonewood_wood_column'
    for tool, k in [('pickaxe', 5), ('axe', 4), ('shovel', 3), ('machete', 3), ('sword', 4)]: make(TOOLB[n], n + '_' + tool, [(wood, 1), (n + '_ingot', k)], time=240)
    for piece, k in [('helmet', 2), ('chestplate', 5), ('leggings', 3), ('boots', 2)]: make(ARMB[n], n + '_' + piece, [(n + '_plate', k)], time=240)
make('magma_forge', 'umbral_ingot', [('deepiron_ingot', 2), ('umbrite_shard', 2), ('shadow_essence', 1)], time=300)
for n, lab, tier, hard, power in GEMS:
    pr, bb = ('sky_press', 'sky_builder_bench') if tier < 9 else ('deep_press', 'deep_builder_bench')
    make(pr, n + '_block', [(n, 5)]); make(bb, n + '_column', [(n + '_block', 1)], 2); make(bb, n + '_stairs', [(n + '_block', 1)], 2)
    make(pr, n, [(n + '_block', 1)], 5, time=60); make(pr, n, [(n + '_shard', 6)], time=60)
    make(pr, n + '_shard', [(n, 1)], 4); make('crystal_lathe' if tier >= 9 else 'stargazer_table', n + '_chandelier', [(n, 3), ('gold_ingot', 2)])
for tier, gem in [(7, 'zephyrite'), (8, 'heliodor'), (9, 'umbrite'), (10, 'emberheart')]:
    make('dim_portal_%d' % tier, gem + '_pickaxe', [(gem, 3), ('skysteel_rod' if tier < 9 else 'deepiron_rod', 1)])

# ---- parts and the long trees ------------------------------------------------------------------------------------
def part(name, label, icon, r, tier, where, ins, count=1, time=180, **over):
    item(icon, name, label, r, 'tint', tier=tier, price=over.pop('price', TIER_PRICE[tier]), **over); make(where, name, ins, count, time)
def steps(tier, first, links):
    """A tree: each thing made of the one before it and whatever else it takes. Returns the last."""
    last = first
    for name, label, icon, r, where, extra in links:
        part(name, label, icon, r, tier, where, ([(last, 1)] if last else []) + extra); last = name
    return last

# The Aether portal: twelve things, each made of the last, all from what the world as it began has.
sk, ga = METAL['skysteel'], METAL['galvanite']
k = steps(6, None, [
    ('featherweight_alloy', 'Featherweight alloy', 'titanium_ingot', ramp('7a8aa0', 'c0d0e8', 'ffffff'), ('electric_furnace', 1), [('titanium_ingot', 1), ('gold_ingot', 1), ('carbon_fiber_sheet', 1)]),
    ('featherweight_plate', 'Featherweight plate', 'tin_foil', ramp('7a8aa0', 'c0d0e8', 'ffffff'), ('press', 3), [('featherweight_alloy', 1)]),
    ('resonant_coil', 'Resonant coil', 'magnet', ga, ('craft_bench', 4), [('copper_wire', 20)]),
    ('storm_capacitor', 'Storm capacitor', 'flywheel', ga, ('craft_bench', 4), [('flywheel', 1), ('silicon_wafer', 2)]),
    ('charged_capacitor', 'Charged capacitor', 'flywheel', zc, ('electric_press', 1), [('gold_block', 1)]),
    ('sky_lens', 'Sky lens', 'mirror', zc, ('electric_kiln', 1), [('glass', 4), ('diamond_shard', 2)]),
    ('gem_prism', 'Gem prism', 'time_crystal', ramp('8a3ab0', '60c0f0', 'ffffff'), ('press', 3), [('amethyst', 1), ('sapphire', 1), ('emerald', 1), ('ruby', 1), ('diamond', 1)]),
    ('sky_compass', 'Sky compass', 'camera', zc, ('workbench', 4), [('magnet', 1), ('featherweight_plate', 1)]),
    ('keystone_core', 'Keystone core', 'silicon_crystal', zc, ('metalwork_bench', 2), [('diamond_block', 1), ('featherweight_plate', 2)]),
    ('aether_keystone', 'Aether keystone', 'time_crystal', zc, ('portal', 6), [('time_crystal', 10)]),
    ('aether_frame', 'Aether frame', 'jetpack_chassis', SKY['cloudstone'], ('workbench', 4), [('marble_column', 4), ('featherweight_plate', 4)])])
make(('portal', 6), 'aether_portal', [(k, 1), ('glass', 8)], time=600, crystals=200)
make(('tool_bench', 7), 'featherweight_pickaxe', [('stick', 1), ('featherweight_alloy', 5)], time=240)
item('iron_pickaxe', 'featherweight_pickaxe', 'Featherweight pickaxe', ramp('7a8aa0', 'c0d0e8', 'ffffff'), 'gray', tier=6, price=30000, cat='tools', tool={'class': 'pickaxe', 'speed': 8.5}, power=7)
part('glider', 'Glider', 'carbon_fiber_wing', SKY['cloudstone'], 6, ('workbench', 4), [('carbon_fiber_wing', 2), ('featherweight_plate', 2), ('linen', 6)], cat='clothing', equip='chest', max_stack=1, glides=True, warmth=0.1)

# The Deep portal: from the Aether.
k = steps(8, None, [
    ('aerite_lattice', 'Aerite lattice', 'carbon_fibers', METAL['aerite'], 'skyforge', [('aerite_ingot', 2), ('skysteel_rod', 2)]),
    ('lift_cell', 'Lift cell', 'epoxy', METAL['aerite'], 'cloud_loom', [('cloud_wool', 4), ('sky_jelly', 2)]),
    ('inverted_cell', 'Inverted cell', 'epoxy', METAL['moonsilver'], 'stargazer_table', [('stardust', 4), ('moonsilver_plate', 1)]),
    ('gravity_coil', 'Gravity coil', 'magnet', METAL['moonsilver'], 'sky_craft_bench', [('galvanite_wire', 20), ('moonsilver_rod', 2)]),
    ('charged_gravity_coil', 'Charged gravity coil', 'magnet', hc, 'sky_press', [('storm_battery', 1), ('wind_essence', 4)]),
    ('gravity_core', 'Gravity core', 'jet_engine', hc, 'skyforge', [('skysteel_block', 4), ('aerite_block', 2)]),
    ('storm_heart', 'Storm heart', 'time_crystal', ga, 'stargazer_table', [('storm_feather', 1), ('heliodor', 1)]),
    ('deep_compass', 'Deep compass', 'camera', METAL['cinderite'], 'aether_workbench', [('sky_compass', 1), ('moonsilver_gear', 2)]),
    ('deep_keystone_core', 'Deep keystone core', 'silicon_crystal', hc, 'sky_metalwork_bench', [('heliodor_block', 1), ('moonsilver_plate', 4)]),
    ('deep_keystone', 'Deep keystone', 'time_crystal', METAL['cinderite'], 'dim_portal_8', [('zephyrite', 2)]),
    ('deep_frame', 'Deep frame', 'jetpack_chassis', SKY['moonrock'], 'aether_workbench', [('moonrock_column', 8), ('moonsilver_plate', 4)])])
make('dim_portal_8', 'deep_portal', [(k, 1), ('moonrock_tile', 8)], time=900, crystals=400)

# The World Seed, at the ninth portal: a place of one's own.
k = steps(9, None, [
    ('rich_loam', 'Rich loam', 'compost', ramp('2a1c10', '54381e', '8a6238'), 'fungal_vat', [('rootsoil', 8), ('spore_dust', 4)]),
    ('living_loam', 'Living loam', 'compost', ramp('2a5a2a', '4a8a40', '90c878'), 'fungal_vat', [('glowcap', 4), ('moss_melon', 2)]),
    ('seed_husk', 'Seed husk', 'coconut', DEEP['bonestone'], 'bone_carver', [('old_bone', 6), ('amber', 2)]),
    ('lined_husk', 'Lined husk', 'coconut', ramp('8a8a9a', 'd0d0e0', 'ffffff'), 'deep_tailor_bench', [('cave_silk', 6), ('strider_hide', 2)]),
    ('lumite_germ', 'Lumite germ', 'silicon_crystal', GEM['lumite'], 'crystal_lathe', [('lumite', 4), ('golem_core', 1)]),
    ('quickened_germ', 'Quickened germ', 'silicon_crystal', uc, 'deep_press', [('shadow_essence', 4), ('umbrite', 1)]),
    ('sky_graft', 'Sky graft', 'acorn', zc, 'stargazer_table', [('wind_nut', 4), ('star_apple_fruit', 4), ('heliodor_shard', 4)]),
    ('diamond_graft', 'Diamond graft', 'acorn', ramp('5ac0d8', 'b0f0ff', 'ffffff'), 'deep_workbench', [('diamond_block', 2), ('diamond_shard', 20)]),
    ('void_shell', 'Void shell', 'time_crystal', uc, 'magma_forge', [('obsidian_glass', 16), ('umbral_plate', 4)]),
    ('sleeping_seed', 'Sleeping seed', 'coconut', ramp('1a6a3a', '50d080', 'e0ffd0'), 'deep_workbench', [('umbrite_block', 1), ('gravity_core', 1)])])
make('dim_portal_9', 'world_seed', [(k, 1), ('time_crystal', 50)], time=1200, crystals=800)

# And the third character, at the tenth: the end of it all.
k = steps(10, None, [
    ('adamant_frame', 'Adamant frame', 'jetpack_chassis', METAL['adamant'], 'magma_forge', [('adamant_plate', 4), ('adamant_rod', 4)]),
    ('jointed_frame', 'Jointed frame', 'jetpack_chassis', METAL['adamant'], 'deep_metalwork_bench', [('adamant_gear', 6), ('umbral_rod', 4)]),
    ('woven_sinew', 'Woven sinew', 'string', ramp('8a3a3a', 'd07070', 'ffd0d0'), 'deep_tailor_bench', [('cave_silk', 12), ('strider_hide', 4)]),
    ('strung_frame', 'Strung frame', 'jetpack_chassis', ramp('8a3a3a', 'd07070', 'ffd0d0'), 'deep_craft_bench', [('woven_sinew', 2)]),
    ('ember_heart_engine', 'Ember engine', 'jet_engine', ec, 'magma_forge', [('emberheart', 2), ('cinderite_block', 2)]),
    ('lumite_eye', 'Lumite eye', 'mirror', GEM['lumite'], 'crystal_lathe', [('lumite', 4), ('sky_lens', 2)]),
    ('thinking_lattice', 'Thinking lattice', 'silicon_wafer', hc, 'deep_craft_bench', [('silicon_wafer', 20), ('galvanite_wire', 40), ('heliodor', 2)]),
    ('waking_spark', 'Waking spark', 'time_crystal', ga, 'stargazer_table', [('storm_heart', 1), ('charged_gravity_coil', 1)]),
    ('quiet_body', 'Quiet body', 'new_player', METAL['adamant'], 'deep_workbench', [('adamant_helmet', 1), ('adamant_chestplate', 1), ('adamant_leggings', 1), ('adamant_boots', 1)], ),
    ('dreaming_body', 'Dreaming body', 'new_player', ec, 'dim_portal_10', [('emberheart_block', 1), ('time_crystal', 100)])])
for nm in ('quiet_body', 'dreaming_body'): items[[i['name'] for i in items].index(nm)].pop('becomes_player', None)
item('new_player', 'third_blockhead', 'Third blockhead', ec, 'tint', cat=None, price=None, becomes_player=True)
items[-1] = {kk: v for kk, v in items[-1].items() if kk in ('name', 'label', 'texture', 'becomes_player')}
make('dim_portal_10', 'third_blockhead', [(k, 1), ('golem_core', 2)], time=1800, crystals=1000, characters=2)

# ---- things made in each place -----------------------------------------------------------------------------------
make('sky_builder_bench', 'storm_cloud', [('dense_cloud', 2), ('wind_essence', 1)], 2)
part('cloud_cloth', 'Cloud cloth', 'linen', SKY['cloudstone'], 7, 'cloud_loom', [('cloud_wool', 3)])
part('aether_thread', 'Aether thread', 'string', ramp('b0a0d0', 'e8e0f8', 'ffffff'), 7, 'cloud_loom', [('aether_cotton', 3)], 2)
part('skyglass', 'Skyglass', 'glass', zc, 7, 'sky_kiln', [('glass', 2), ('zephyrite_shard', 1)], 2, places=None)
part('storm_glass', 'Storm glass', 'black_glass', ga, 7, 'sky_kiln', [('black_glass', 2), ('wind_essence', 1)], 2, places=None)
part('reed_paper', 'Reed paper', 'tin_foil', ramp('8aa0c0', 'd0e0f0', 'ffffff'), 7, 'sky_woodwork_bench', [('cloud_reed', 4)], 2)
part('star_chart', 'Star chart', 'sign', SKY['moonrock'], 8, 'stargazer_table', [('reed_paper', 2), ('stardust', 2)], places=None)
part('sky_pigment', 'Sky pigment', 'ultramarine_blue', zc, 7, 'sky_mixing_bench', [('skyberry', 4), ('moon_lily', 1)], 2)
part('sun_pigment', 'Sun pigment', 'indian_yellow', hc, 7, 'sky_mixing_bench', [('sunpetal_bloom', 4), ('sun_corn', 1)], 2)
part('talon_hook', 'Talon hook', 'yak_horn', SKY['storm_slate'], 7, 'sky_craft_bench', [('hawk_talon', 2), ('skysteel_rod', 1)])
part('jelly_glue', 'Jelly glue', 'epoxy', ramp('8a4a9a', 'd890e0', 'ffe0ff'), 7, 'sky_mixing_bench', [('sky_jelly', 3)], 2)
part('chitin_plate', 'Chitin plate', 'tin_foil', ramp('1a2a2a', '3a6a6a', '80c0b0'), 9, 'deep_press', [('chitin', 3)])
part('silk_cloth', 'Silk cloth', 'linen', ramp('8a8a9a', 'd0d0e0', 'ffffff'), 9, 'deep_tailor_bench', [('cave_silk', 3)])
part('magma_glass', 'Magma glass', 'glass', METAL['cinderite'], 9, 'deep_kiln', [('obsidian_glass', 2), ('slug_cinder', 1)], 2, places=None)
part('bone_meal', 'Bone meal', 'flour', DEEP['bonestone'], 9, 'bone_carver', [('old_bone', 2)], 3)
part('carved_bone', 'Carved bone', 'yak_horn', DEEP['bonestone'], 9, 'bone_carver', [('old_bone', 3), ('worm_tooth', 1)])
part('fossil_trophy', 'Fossil trophy', 'shark_head', DEEP['bonestone'], 9, 'crystal_lathe', [('fossil', 5), ('carved_bone', 2)], places=None, sized=None, places_sized=None, meter_max=None)
part('amber_lens', 'Amber lens', 'mirror', ramp('8a4a08', 'e09020', 'ffe090'), 9, 'crystal_lathe', [('amber', 3)], places=None)
part('shadow_dye', 'Shadow dye', 'tyrian_purple', uc, 9, 'deep_mixing_bench', [('shadow_essence', 1), ('ink_cap', 3)], 2)
part('ember_dye', 'Ember dye', 'red_ochre', ec, 9, 'deep_mixing_bench', [('ember_pepper', 3), ('slug_cinder', 1)], 2)
part('wing_leather', 'Wing leather', 'leather', ramp('1a1020', '40304a', '80688a'), 9, 'deep_tailor_bench', [('bat_wing', 3)])
# Clothes: against the thin air, and against the heat.
def wear(name, label, icon, slot_r, tier, where, ins, **over):
    item(icon, name, label, slot_r, 'tint', tier=tier, price=TIER_PRICE[tier] * 2, cat='clothing', **over); make(where, name, ins, time=240)
cw = SKY['cloudstone']
wear('cloudweave_hood', 'Cloudweave hood', 'linen_cap', cw, 7, 'cloud_loom', [('cloud_cloth', 2)], warmth=0.35, armor=0.04)
wear('cloudweave_coat', 'Cloudweave coat', 'fur_coat', cw, 7, 'cloud_loom', [('cloud_cloth', 5), ('aether_thread', 2)], warmth=0.45, armor=0.06)
wear('cloudweave_trousers', 'Cloudweave trousers', 'linen_pants', cw, 7, 'cloud_loom', [('cloud_cloth', 3)], warmth=0.35, armor=0.04)
wear('wind_boots', 'Wind boots', 'fur_boots', zc, 7, 'cloud_loom', [('cloud_cloth', 2), ('wind_essence', 2)], warmth=0.3, walk_speed=1.25)
hs = METAL['cinderite']
wear('heat_hood', 'Heat hood', 'linen_cap', hs, 9, 'deep_tailor_bench', [('silk_cloth', 2), ('chitin_plate', 1)], warmth=None, cooling=0.3, armor=0.08)
wear('heat_suit', 'Heat suit', 'leather_jacket', hs, 9, 'deep_tailor_bench', [('silk_cloth', 5), ('chitin_plate', 2)], warmth=None, cooling=0.4, armor=0.1)
wear('heat_trousers', 'Heat trousers', 'leather_pants', hs, 9, 'deep_tailor_bench', [('silk_cloth', 3), ('chitin_plate', 1)], warmth=None, cooling=0.3, armor=0.08)
wear('moleskin_boots', 'Moleskin boots', 'fur_boots', ramp('2a2420', '5a4a40', '9a8878'), 9, 'deep_tailor_bench', [('mole_fur', 4), ('wing_leather', 1)], warmth=0.3, cooling=0.2, walk_speed=1.15)
# The benches of each place, and the things set down in them.
make(('workbench', 4), 'skyforge', [('aetherstone', 20), ('featherweight_plate', 2)]); make('aether_workbench', 'cloud_loom', [('windwood_wood', 10), ('skysteel_rod', 2)])
make('aether_workbench', 'stargazer_table', [('moonrock_tile', 6), ('skyglass', 2), ('zephyrite', 1)]); make('aether_workbench', 'wind_turbine', [('skysteel_plate', 4), ('cloud_cloth', 4), ('electric_motor', 1)])
make('aether_workbench', 'lightning_rod', [('galvanite_rod', 6), ('galvanite_block', 1)]); make('aether_workbench', 'storm_battery', [('galvanite_block', 2), ('storm_glass', 4)])
make('aether_workbench', 'galvanite_wire', [('galvanite_ingot', 1)], 8); make('aether_workbench', 'sky_lantern', [('skyglass', 1), ('zephyrite_shard', 1)], 2)
make('aether_workbench', 'cloud_bed', [('cloud_cloth', 6), ('windwood_wood', 4)]); make('aether_workbench', 'skysteel_chest', [('skysteel_plate', 6)])
make('deep_workbench', 'magma_forge', [('magmarock_tile', 20), ('deepiron_plate', 4)]); make(('workbench', 4), 'magma_forge', [('magmarock', 30), ('moonsilver_plate', 4)])
make('aether_workbench', 'crystal_lathe', [('deepiron_gear', 4), ('lumite', 2)]); make('aether_workbench', 'fungal_vat', [('bluecap_wood', 10), ('deepiron_rod', 2)])
make('aether_workbench', 'bone_carver', [('bonestone', 10), ('old_bone', 6)]); make('deep_workbench', 'geothermal_generator', [('cinderite_block', 2), ('deepiron_gear', 4), ('electric_motor', 1)])
make('deep_workbench', 'lumite_lamp', [('lumite', 1), ('magma_glass', 1)], 2); make('deep_workbench', 'cinder_torch', [('slug_cinder', 1), ('bonewood_wood_column', 1)], 4)
make('deep_workbench', 'deepiron_chest', [('deepiron_plate', 8)])

# ---- food --------------------------------------------------------------------------------------------------------
def dish(name, label, icon, r, tier, where, ins, food, **over):
    item(icon, name, label, r, 'tint', tier=tier, price=TIER_PRICE[tier] // 40, food=food, **over); make(where, name, ins, time=160)
h, v = 'sky_campfire', 'fungal_vat'
dish('roast_moa', 'Roast moa', 'cooked_dodo_meat', ramp('8a5a3a', 'd0a070', 'fff0d0'), 7, h, [('moa_meat', 1)], 0.5)
dish('skyberry_tart', 'Skyberry tart', 'pizza', ramp('2a3a8a', '5a7ae0', 'c0d0ff'), 7, h, [('skyberry', 3), ('wind_flour', 1)], 0.6)
dish('wind_flour', 'Wind flour', 'flour', ramp('6a9a8a', 'a8e0c8', 'f0fff8'), 7, 'sky_mixing_bench', [('windwheat', 2)], None)
dish('cloud_bread', 'Cloud bread', 'bread', SKY['cloudstone'], 7, h, [('wind_flour', 2)], 0.55)
dish('cloud_candy', 'Cloud candy', 'rainbow_cake', ramp('d090c0', 'f8c8e8', 'ffffff'), 7, h, [('cloud_wool', 1), ('sunpetal_bloom', 2)], 0.3, food_energy=0.2)
dish('star_apple_pie', 'Star-apple pie', 'pizza', ramp('3a2a7a', '6a5ad0', 'c0b8ff'), 7, h, [('star_apple_fruit', 3), ('wind_flour', 1)], 0.7)
dish('thunder_stew', 'Thunder stew', 'dodo_stew', ramp('5a3a9a', 'a070f0', 'e8d0ff'), 7, h, [('thunder_pepper', 2), ('moa_meat', 1), ('sun_corn', 1)], 0.8)
dish('sky_tea', 'Sky tea', 'coffee', GEM['zephyrite'], 7, h, [('frost_mint', 3)], 0.1, food_energy=0.5)
dish('amber_jam', 'Amber jam', 'rainbow_essence', ramp('c05a10', 'f09030', 'ffd890'), 7, h, [('amber_fruit', 4)], 0.4)
dish('jelly_pudding', 'Jelly pudding', 'rainbow_cake', ramp('8a4a9a', 'd890e0', 'ffe0ff'), 7, h, [('sky_jelly', 2), ('skyberry', 2)], 0.45)
dish('sun_corn_cake', 'Sun corn cake', 'flatbread', ramp('c07a10', 'ffc830', 'fff4b0'), 7, h, [('sun_corn', 3)], 0.4)
dish('moonlit_feast', 'Moonlit feast', 'fish_curry', ramp('6a6aa0', 'c0c0f0', 'ffffff'), 8, 'stargazer_table', [('roast_moa', 1), ('star_apple_pie', 1), ('stardust', 1)], 1.0, food_health=0.5)
h = 'deep_campfire'
dish('seared_cavefish', 'Seared cavefish', 'cooked_fish', ramp('9a8a8a', 'e0d0d0', 'ffffff'), 9, h, [('blind_fish', 1)], 0.5)
dish('eel_broth', 'Eel broth', 'dodo_stew', GEM['lumite'], 9, h, [('eel_lantern', 1), ('glowcap', 2)], 0.7)
dish('glowcap_skewer', 'Glowcap skewer', 'cooked_dodo_meat', ramp('1a7a6a', '40e0c0', 'd0fff0'), 9, h, [('glowcap', 3), ('old_bone', 1)], 0.45)
dish('rice_cake', 'Cave rice cake', 'flatbread', ramp('8a8a6a', 'd0d0a8', 'ffffe0'), 9, h, [('cave_rice', 3)], 0.5)
dish('root_mash', 'Root mash', 'dough', ramp('8a6a5a', 'd0a890', 'ffe8d8'), 9, h, [('blindroot', 3)], 0.5)
dish('ember_curry', 'Ember curry', 'fish_curry', ramp('9a2008', 'f06020', 'ffc060'), 9, h, [('ember_pepper', 2), ('blind_fish', 1), ('cave_rice', 2)], 0.9)
dish('melon_slices', 'Melon slices', 'mozzarella', ramp('2a6a2a', '5ac050', 'c0f0a0'), 9, h, [('moss_melon', 2)], 0.4)
dish('stoneberry_wine', 'Stoneberry wine', 'coffee', ramp('4a2a5a', '8a4a9a', 'e0b0f0'), 9, v, [('stoneberry', 4)], 0.1, food_energy=0.6, energy_over=True)
dish('spore_ale', 'Spore ale', 'coffee', ramp('6a4a7a', 'b080c8', 'f0d0ff'), 9, v, [('spore_pod', 3), ('cave_rice', 2)], 0.2, food_energy=0.4)
dish('morel_broth', 'Ink morel broth', 'dodo_stew', ramp('10101a', '30304a', '6a6a9a'), 9, v, [('ink_cap', 3), ('lantern_bulb', 1)], 0.6, food_health=0.4)
dish('fermented_lichen', 'Fermented lichen', 'yeast', ramp('3a6a5a', '70b098', 'c8f0e0'), 9, v, [('deep_lichen', 4)], 0.3)
dish('abyssal_feast', 'Abyssal feast', 'pizza', GEM['emberheart'], 10, 'deep_campfire', [('ember_curry', 1), ('eel_broth', 1), ('stoneberry_wine', 1)], 1.0, food_health=0.6, food_energy=0.4)

# ---- the places themselves ---------------------------------------------------------------------------------------
def biome(name, label, dim, surface, soil, under, trees_=(), one_in=0, plants=(), **over):
    b = {'name': name, 'label': label, 'dimension': dim, 'surface': surface, 'soil': soil, 'under': {'tile': under, 'depth': 40}}
    if trees_: b.update(tree_one_in=one_in, trees=[{'tree': t, 'weight': w} for t, w in trees_])
    if plants: b['plants'] = [{'tile': t, 'one_in': n} for t, n in plants]
    b.update(over); biomes.append(b)
biome('cloudmeadow', 'Cloudmeadow', 'aether', 'cloud_grass', 'sky_soil', 'aetherstone', [('windwood', 3), ('cloud_oak', 1)], 14, [('skyberry_0', 14), ('windwheat_0', 12), ('moon_lily_0', 16), ('aether_cotton_0', 18)])
biome('skywood', 'Skywood', 'aether', 'cloud_grass', 'sky_soil', 'aetherstone', [('windwood', 3), ('sky_willow', 3), ('cloud_oak', 2), ('amberleaf', 1)], 4, [('cloud_reed_0', 14), ('skyberry_0', 20)])
biome('stormreach', 'Stormreach', 'aether', 'storm_slate', 'storm_slate', 'storm_slate', [('storm_birch', 1)], 10, [('thunder_pepper_0', 12)])
biome('crystal_spires', 'Crystal spires', 'aether', 'skymarble', 'skymarble', 'skymarble', [], 0, [])
biome('sunpetal_gardens', 'Sunpetal gardens', 'aether', 'sun_grass', 'sky_soil', 'cloudstone', [('sunpetal', 3), ('star_apple', 1), ('amberleaf', 1)], 6, [('sun_corn_0', 10), ('moon_lily_0', 12)])
biome('frostveil', 'Frostveil', 'aether', 'frost_glass', 'moonrock', 'moonrock', [], 0, [], cold=True)
biomes[-1].update(surface='cloud_grass'); biomes[-1].update(tree_one_in=9, trees=[{'tree': 'frostpine', 'weight': 1}], plants=[{'tile': 'frost_mint_0', 'one_in': 8}])
biome('sky_ruins', 'The ruins', 'aether', 'ruin_brick', 'ruin_brick', 'aetherstone', [], 0, [])
biome('cloud_shoals', 'Cloud shoals', 'aether', 'dense_cloud', 'cloudstone', 'cloudstone', [], 0, [])
biome('fungal_forest', 'Fungal forest', 'deep', 'deep_mycelium', 'deepslate', 'deepslate', [('bluecap', 3), ('pale_giant', 2), ('ink_morel', 1), ('lantern_fungus', 1)], 5, [('glowcap_0', 10), ('spore_pod_0', 12), ('blindroot_0', 14)])
biome('glowmoss_grotto', 'Glowmoss grotto', 'deep', 'glowmoss', 'deep_mycelium', 'deepslate', [('lantern_fungus', 2), ('bluecap', 1)], 9, [('cave_rice_0', 8), ('moss_melon_0', 10), ('glowcap_0', 10)])
biome('crystal_cathedral', 'Crystal cathedral', 'deep', 'lumerock', 'lumerock', 'deepslate', [], 0, [])
biome('bone_hollows', 'Bone hollows', 'deep', 'bone_gravel', 'bonestone', 'bonestone', [('bonewood', 1)], 8, [('stoneberry_0', 12)])
biome('sunken_lake', 'Sunken lake', 'deep', 'ashstone', 'ashstone', 'deepslate', [('shelf_wood', 1)], 12, [('deep_lichen_0', 8)])
biome('obsidian_flats', 'Obsidian flats', 'deep', 'obsidian_glass', 'obsidian_rock', 'obsidian_rock', [], 0, [])
biome('magma_fields', 'Magma fields', 'deep', 'magmarock', 'magmarock', 'magmarock', [('emberstool', 1)], 10, [('ember_pepper_0', 8)], hot=True)
biome('the_roots', 'The Roots', 'deep', 'rootsoil', 'rootsoil', 'deepslate', [('rootwood', 3), ('shelf_wood', 1)], 4, [('blindroot_0', 8), ('deep_lichen_0', 10)])
biome('the_abyss', 'The abyss', 'deep', 'ashstone', 'obsidian_rock', 'obsidian_rock', [], 0, [], hot=True)
biome('pocket_isle', 'A world of your own', 'pocket', 'grass', 'dirt', 'stone')

SKY_ROCK, DEEP_ROCK = ['aetherstone', 'cloudstone', 'skymarble', 'moonrock', 'storm_slate'], ['deepslate', 'obsidian_rock', 'magmarock', 'lumerock', 'bonestone', 'ashstone']
def vein(name, tile_, dim, hosts, size, per, depth): veins.append({'name': name, 'tile': tile_, 'dimension': dim, 'in': hosts, 'size': size, 'per_100_columns': per, 'depth': depth})
vein('skysteel', 'skysteel_ore', 'aether', SKY_ROCK, [3, 7], 26, [0, 300]); vein('galvanite', 'galvanite_ore', 'aether', SKY_ROCK, [2, 6], 16, [0, 300])
vein('aerite', 'aerite_ore', 'aether', SKY_ROCK, [2, 5], 14, [0, 300]); vein('moonsilver', 'moonsilver_ore', 'aether', SKY_ROCK, [2, 5], 10, [0, 160])
vein('sky_frost_seam', 'frost_glass', 'aether', ['aetherstone', 'moonrock'], [4, 10], 6, [0, 140]); vein('sky_marble_seam', 'skymarble', 'aether', ['aetherstone'], [6, 16], 8, [0, 300]); vein('sky_moonrock_seam', 'moonrock', 'aether', ['aetherstone', 'cloudstone'], [6, 16], 8, [0, 160])
vein('deepiron', 'deepiron_ore', 'deep', DEEP_ROCK, [3, 8], 70, [0, 240]); vein('cinderite', 'cinderite_ore', 'deep', DEEP_ROCK, [2, 6], 40, [140, 320])
vein('adamant', 'adamant_ore', 'deep', DEEP_ROCK, [2, 4], 14, [250, 320]); vein('deep_bone_seam', 'bonestone', 'deep', ['deepslate'], [8, 20], 20, [0, 200])
vein('deep_ash_seam', 'ashstone', 'deep', ['deepslate', 'obsidian_rock'], [8, 20], 20, [100, 320]); vein('deep_fossil', 'bone_gravel', 'deep', ['deepslate', 'bonestone'], [3, 8], 16, [0, 320])
def dec(name, tile_, dim, per, depth, **over): decor.append(dict({'name': name, 'tile': tile_, 'dimension': dim, 'per_100_columns': per, 'depth': depth}, **over))
dec('zephyrite', 'zephyrite', 'aether', 5, [0, 300]); dec('heliodor', 'heliodor', 'aether', 2.5, [0, 110]); dec('sky_vine', 'sky_vine', 'aether', 12, [0, 300], hang=[2, 7])
dec('sky_time_crystal', 'time_crystal', 'aether', 3, [0, 300])
dec('lumite', 'lumite', 'deep', 14, [0, 320]); dec('umbrite', 'umbrite', 'deep', 6, [120, 320]); dec('emberheart', 'emberheart', 'deep', 2.5, [260, 320])
dec('lumen_vine', 'lumen_vine', 'deep', 60, [0, 260], hang=[2, 9]); dec('deep_time_crystal', 'time_crystal', 'deep', 6, [0, 320]); dec('steam_vent', 'steam_vent', 'deep', 5, [200, 320], on_floor=True)
decor[:] = [d for d in decor if d['per_100_columns'] > 0]
# (Bones turn up where there is bone gravel: it leaves a fossil now and then.)
for t in tiles:
    if t['name'] == 'bone_gravel': t['drops'] = [{'item': 'bone_gravel', 'count': 1}, {'item': 'old_bone', 'count': 1, 'chance': 0.35}, {'item': 'fossil', 'count': 1, 'chance': 0.08}]; t.pop('drop', None)
    if t['name'] == 'rootsoil': t['drops'] = [{'item': 'rootsoil', 'count': 1}, {'item': 'amber', 'count': 1, 'chance': 0.06}]; t.pop('drop', None)
    if t['name'] in ('sky_soil', 'deep_mycelium'): t['drop'] = t['name']

dimensions = [
    {'name': 'aether', 'label': 'The Aether', 'generator': 'islands', 'height': 300, 'rock': 'aetherstone', 'edge': 'void_wall', 'platform': 'cloudstone', 'cloud': 'dense_cloud',
     'falls_home': True, 'climate_wavelength': 150, 'islands': {'per_100_columns': 7, 'length': [8, 76], 'depth': [4, 20]}},
    {'name': 'deep', 'label': 'The Deep', 'generator': 'caverns', 'height': 320, 'sky': False, 'rock': 'deepslate', 'edge': 'void_wall', 'platform': 'deepslate', 'magma': 'magma',
     'magma_rows': 7, 'climate_wavelength': 170, 'strata': [{'tile': 'obsidian_rock', 'from': 0.62}, {'tile': 'magmarock', 'from': 0.88}],
     'caverns': {'per_100_columns': 4.5, 'length': [50, 170], 'radius_min': 6, 'radius_max': 15, 'lakes': 0.22}},
    {'name': 'pocket', 'label': 'A world of your own', 'generator': 'void', 'width': 128, 'height': 128, 'rock': 'stone', 'edge': 'void_wall', 'platform': 'stone', 'tree': 'diamond_tree'}]

# ---- written out -------------------------------------------------------------------------------------------------
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v))
                                    for k, v in obj.items()) + '\n}\n')
names = [t['name'] for t in tiles]; assert len(names) == len(set(names)), [n for n in names if names.count(n) > 1]
# (What is worn names its picture, which tools/worn_art.py draws: in the place that tool puts it.)
for n, i in enumerate(items):
    if i.get('equip'):
        o = {}
        for k, v in i.items():
            o[k] = v
            if k == 'equip': o['worn'] = 'worn/%s.png' % i['name']
        items[n] = o
# Where each thing is listed for trade: in the game's own categories, by what it is.
SOLID = {t['name'] for t in tiles if t.get('solid') or t.get('steps') or t.get('supports')}
POWER = ('wire', 'battery', 'turbine', 'lightning_rod', 'generator', 'capacitor', 'coil')
for i in items:
    if i.get('category') != 'materials': continue
    n = i['name']
    i['category'] = ('nature' if 'food' in i or 'sows' in i or 'plants' in i else 'energy' if any(w in n for w in POWER) else
                     'blocks' if i.get('places') in SOLID else 'ores' if n.endswith(('_ore', '_ingot', '_shard')) or n in GEM else 'crafted')
names = [t['name'] for t in items]; assert len(names) == len(set(names)), [n for n in names if names.count(n) > 1]
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes}); dump('trees.json', {'trees': trees})
dump('mobs.json', {'mobs': mobs}); dump('worldgen.json', {'dimensions': dimensions, 'biomes': biomes, 'veins': veins, 'cave_decor': decor})
dump('rules.json', {'players': {'max': 3}})
json.dump({'label': 'Dimensions', 'icon': 'icon.png', 'description': 'The Aether and the Deep: two more places for a world, and all that is in them.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
recolor(os.path.join(DATA, 'textures/tiles/portal_6.png') if os.path.exists(os.path.join(DATA, 'textures/tiles/portal_6.png')) else os.path.join(DATA, 'textures/items/portal_6.png'),
        os.path.join(mod, 'icon.png'), ramp('5a8ad0', 'b0e0ff', 'ffffff'), 'tint')

# How deep each of the long trees goes: a thing made of nothing but what is found is 1.
made = {}
for r in recipes: made.setdefault(r['output']['item'], []).append([i['item'] for i in r['inputs']])
def depth(i, seen=()):
    if i not in made or i in seen: return 0
    return 1 + max((max((depth(x, seen + (i,)) for x in ins), default=0) for ins in made[i]), default=0)
print('%d tiles, %d items, %d recipes, %d trees, %d creatures, %d biomes' % (len(tiles), len(items), len(recipes), len(trees), len(mobs), len(biomes)))
print('stages: ' + ', '.join('%s %d' % (n, depth(n)) for n in ('aether_portal', 'deep_portal', 'world_seed', 'third_blockhead')))
