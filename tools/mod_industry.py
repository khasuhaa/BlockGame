# Writes the Industry mod: its data files and its textures, from the plan in ModDescriptions/Industry.txt.
import sys, os, json, copy
from PIL import Image
repo = sys.argv[1]; mod = os.path.join(repo, 'mods/industry'); tex = os.path.join(mod, 'textures')
for d in ('tiles', 'items'): os.makedirs(os.path.join(tex, d), exist_ok=True)
base = os.path.join(repo, 'data')
def load(n): return json.load(open(os.path.join(base, n)))
T = {t['name']: t for t in load('tiles.json')['tiles']}
I = {t['name']: t for t in load('items.json')['items']}
R = load('recipes.json')['recipes']
V = {v['name']: v for v in load('worldgen.json')['veins']}

def dk(c, f=0.72): return tuple(int(v * f) for v in c[:3]) + (255,)
def lt(c, f=1.25): return tuple(min(255, int(v * f)) for v in c[:3]) + (255,)
class P:
    def __init__(s): s.im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); s.px = s.im.load()
    def r(s, x0, y0, x1, y1, c):
        for y in range(max(0, y0), min(15, y1) + 1):
            for x in range(max(0, x0), min(15, x1) + 1): s.px[x, y] = c
    def p(s, x, y, c): s.r(x, y, x, y, c)
    def save(s, kind, name):
        k = 3 if kind == 'items' else 2
        s.im.resize((16 * k, 16 * k), Image.NEAREST).save(os.path.join(tex, kind, name + '.png')); return s
def block_icon(tile_file, name):
    im = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); im.paste(Image.open(os.path.join(tex, 'tiles', tile_file + '.png')).convert('RGBA').crop((0, 0, 32, 32)), (8, 8))
    im.save(os.path.join(tex, 'items', name + '.png'))
def tint(src, kind, name, colour, keep=None):
    """A base texture in another metal: its greys take the colour, by how light they are."""
    im = Image.open(os.path.join(base, 'textures', src)).convert('RGBA'); px = im.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, b, a = px[x, y]
            if not a or (keep and keep(r, g, b)): continue
            v = (r + g + b) / 3 / 255.0
            v = 0.25 + v * 0.95
            px[x, y] = tuple(min(255, int(c * v)) for c in colour[:3]) + (a,)
    im.save(os.path.join(tex, kind, name + '.png'))
def is_wood(r, g, b): return r > g + 12 and g > b + 12 and r < 215   # handles, sticks
STONE = set(Image.open(os.path.join(base, 'textures/tiles/stone_0.png')).convert('RGBA').getdata())
def is_stone(r, g, b): return (r, g, b, 255) in STONE

LEAD=(118,124,150,255); HBRONZE=(150,104,70,255); RBRONZE=(226,170,70,255); DREAD=(88,60,100,255); IMPERIAL=(210,214,255,255)
METALS=[('lead','Lead',LEAD,180),('heavy_bronze','Heavy bronze',HBRONZE,260),('royal_bronze','Royal bronze',RBRONZE,520),('dreadsteel','Dreadsteel',DREAD,2600),('imperial','Imperial',IMPERIAL,3400)]
SL=(180,188,200,255); S=(120,134,156,255); SD=(93,104,121,255); SK=(66,73,85,255); K=(22,22,30,255); KD=(58,58,69,255)
GOLD=(215,159,29,255); FIRE=(240,140,40,255); HOT=(250,210,90,255); WHITE=(232,232,226,255); RED=(190,60,60,255); GREEN=(80,170,90,255); BLUE=(70,120,200,255)
WL=(201,159,99,255); W=(174,130,73,255); WM=(140,101,55,255); WD=(106,72,39,255)

tiles=[]; items=[]; recipes=[]
def item(name, label, cat, price, **kw):
    d={'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'category': cat, 'price': price}; d.update(kw); items.append(d)
def rec(bench, out, n, ins, time, level=1):
    d={'bench': bench, 'output': {'item': out, 'count': n}, 'inputs': [{'item': a, 'count': b} for a, b in ins], 'time': time}
    if level > 1: d['level'] = level
    recipes.append(d)

# ---- lead ore, and the ingots, blocks, columns and stairs of five metals
ore=copy.deepcopy(T['iron_ore']); ore.update({'name': 'lead_ore', 'label': 'Lead ore', 'drop': 'lead_ore', 'texture': ['tiles/lead_ore_0.png', 'tiles/lead_ore_1.png']}); tiles.append(ore)
for k in (0, 1): tint('tiles/iron_ore_%d.png' % k, 'tiles', 'lead_ore_%d' % k, LEAD, keep=is_stone)
tint('items/iron_ore.png', 'items', 'lead_ore', LEAD); item('lead_ore', 'Lead ore', 'materials', 80)
for name, label, colour, price in METALS:
    tint('items/iron_ingot.png', 'items', name + '_ingot', colour); item(name + '_ingot', label + ' ingot', 'materials', price)
    for part, plabel, cat, mult, count in (('block', 'block', 'blocks', 5.2, 1), ('column', 'column', 'building', 2.8, 2), ('stairs', 'stairs', 'building', 2.8, 2)):
        d=copy.deepcopy(T['platinum_' + part]); n='%s_%s' % (name, part)
        d.update({'name': n, 'label': '%s %s' % (label, plabel), 'drop': n, 'texture': 'tiles/%s.png' % n}); tiles.append(d)
        tint('tiles/platinum_%s.png' % part, 'tiles', n, colour); block_icon(n, n)
        item(n, '%s %s' % (label, plabel), cat, int(price * mult / 10) * 10, places=n)
    for bench, time in (('metalwork_bench', 80), ('electric_metalwork_bench', 60)):
        rec(bench, name + '_block', 1, [(name + '_ingot', 5)], time, level=2 if bench == 'metalwork_bench' else 1)
    rec('builder_bench', name + '_column', 2, [(name + '_block', 1)], 80); rec('builder_bench', name + '_stairs', 2, [(name + '_block', 1)], 80)
rec('furnace', 'lead_ingot', 1, [('lead_ore', 3)], 80, level=2); rec('electric_furnace', 'lead_ingot', 1, [('lead_ore', 2)], 40)

# ---- source ores: an ore that is never mined out
SOURCES=[('copper','copper_ore',100),('tin','tin_ore',100),('coal','coal_ore',100),('iron','iron_ore',200),('lead','lead_ore',200),('gold','gold_ore',200),('titanium','titanium_ore',300),('platinum','platinum_ore',300)]
rares=[]
for metal, ore_name, one_in in SOURCES:
    src=tiles[0] if ore_name == 'lead_ore' else T[ore_name]
    d=copy.deepcopy(src); n=metal + '_source_ore'
    d.update({'name': n, 'label': '%s source ore' % metal.capitalize(), 'texture': ['tiles/%s_0.png' % n, 'tiles/%s_1.png' % n], 'endless': True, 'mine_time': round(src['mine_time'] * 1.5, 2)}); tiles.append(d)
    for k in (0, 1):
        path=os.path.join(tex if ore_name == 'lead_ore' else os.path.join(base, 'textures'), 'tiles/%s_%d.png' % (ore_name, k))
        im=Image.open(path).convert('RGBA'); px=im.load()
        for a in range(0, 32, 2):      # a bright frame, broken at the middle of each side: something that will not run out
            if 10 <= a < 22: continue
            for (x, y) in ((a, 0), (a, 30), (0, a), (30, a)):
                for dx in (0, 1):
                    for dy in (0, 1): px[x + dx, y + dy] = GOLD
        im.save(os.path.join(tex, 'tiles', '%s_%d.png' % (n, k)))
    rares.append({'vein': metal, 'tile': n, 'one_in': one_in})
lead_vein=copy.deepcopy(V['iron']); lead_vein.update({'name': 'lead', 'tile': 'lead_ore'})

# ---- machines
def body(c=S, cl=SL, cd=SK):
    t=P(); t.r(1,2,14,15,c); t.r(1,2,14,2,cl); t.r(1,15,14,15,cd); t.r(14,3,14,15,cd); return t
def machine(name, label, price, draw_fn, **fields):
    draw_fn().save('tiles', name); block_icon(name, name)
    d={'name': name, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.5, 'drop': name,
       'texture': 'tiles/%s.png' % name, 'tool': 'pickaxe', 'popup_name': label.lower()}
    d.update(fields); tiles.append(d)
    kw={'places': name}
    if 'storage' in fields: kw['container']={'slots': fields['storage']['columns'] * fields['storage']['rows']}
    item(name, label + fields.pop('_roman', ''), 'benches', price, **kw)
def bench(kind, level, power): return {'bench': True, 'bench_kind': kind, 'bench_level': level, 'electric': {'per_second': power}}
def pips(t, n):
    for i in range(n): t.p(7 - n + 2 * i + 1, 14, GOLD)
    return t
def miner():
    t=body(); t.r(3,4,12,8,K); t.r(4,5,11,7,KD); t.r(7,9,8,13,SL); t.r(6,13,9,14,(200,200,210,255)); t.p(7,15,WHITE); t.p(8,15,WHITE); t.p(5,6,GREEN); t.p(10,6,FIRE); return t
def alloy(tier):
    def f():
        t=body((104,82,70,255),(150,120,100,255),(60,46,40,255)); t.r(3,4,6,9,K); t.r(9,4,12,9,K); t.r(4,6,5,9,FIRE); t.r(10,6,11,9,HOT); t.r(3,11,12,12,SD); return pips(t, tier - 1)
    return f
def ibench(tier):
    def f():
        t=P(); t.r(0,6,15,7,SL); t.r(0,8,15,8,SD); t.r(1,9,2,15,SD); t.r(13,9,14,15,SD); t.r(3,11,12,11,SK)
        t.r(2,3,6,5,KD); t.r(9,2,13,5,HBRONZE); t.r(9,2,13,2,lt(HBRONZE)); t.p(11,4,K); return pips(t, tier - 1)
    return f
def ifurnace():
    t=body(SD,S,SK); t.r(3,4,12,10,K); t.r(4,5,11,9,FIRE); t.r(6,6,9,8,HOT); t.r(1,12,14,13,HBRONZE); t.r(0,12,0,13,SK); t.r(15,12,15,13,SK); return t
def armory():
    t=body(); t.r(4,4,11,10,HBRONZE); t.r(4,4,11,4,lt(HBRONZE)); t.r(3,4,3,6,dk(HBRONZE)); t.r(12,4,12,6,dk(HBRONZE)); t.r(7,5,8,9,lt(HBRONZE)); t.r(1,12,14,13,HBRONZE); return t
def smithery():
    t=body(); t.r(3,8,12,9,KD); t.r(2,9,2,9,KD); t.r(5,10,10,11,KD); t.r(9,3,11,5,SL); t.r(6,4,8,4,WM); t.r(1,12,14,13,HBRONZE); return t
def factory(tier):
    def f():
        t=P(); t.r(1,6,14,15,S); t.r(1,6,14,6,SL); t.r(1,15,14,15,SK); t.r(2,1,4,5,KD); t.r(6,3,8,5,KD); t.r(2,0,4,0,WHITE)
        t.r(3,8,5,10,HOT); t.r(7,8,9,10,HOT); t.r(11,8,12,10,HOT); t.r(1,12,14,13,HBRONZE); return pips(t, tier - 1)
    return f
def processor():
    t=body(DREAD, lt(DREAD,1.5), dk(DREAD)); t.r(4,4,11,11,K); t.r(5,5,10,10,(40,120,90,255)); t.r(7,7,8,8,GOLD)
    for i in (5,7,9): t.p(3,i,GOLD); t.p(12,i,GOLD); t.p(i+1,3,GOLD); t.p(i+1,12,GOLD)
    return t
def plant():
    t=P(); t.r(1,7,14,15,DREAD); t.r(1,7,14,7,lt(DREAD,1.5)); t.r(1,15,14,15,dk(DREAD)); t.r(3,1,6,6,SD); t.r(9,3,12,6,SD); t.r(3,0,6,0,WHITE); t.r(9,2,12,2,WHITE)
    t.r(5,9,10,13,K); t.r(6,10,9,13,(150,200,255,255)); t.r(7,11,8,12,WHITE); return t
def crate():
    t=P(); t.r(1,1,14,14,DREAD); t.r(1,1,14,1,lt(DREAD,1.5)); t.r(1,14,14,14,dk(DREAD)); t.r(1,1,1,14,lt(DREAD,1.3)); t.r(14,1,14,14,dk(DREAD))
    t.r(3,3,12,3,IMPERIAL); t.r(3,12,12,12,IMPERIAL); t.r(3,3,3,12,IMPERIAL); t.r(12,3,12,12,IMPERIAL); t.r(7,6,8,9,GOLD); return t
def belt(c):
    def f():
        t=P(); t.r(0,6,15,9,KD); t.r(0,5,15,5,c); t.r(0,10,15,10,c)
        for x in (2,7,12): t.r(x,7,x+1,8,c); t.p(x+2,7,dk(c)); t.p(x+2,8,dk(c))
        return t
    return f
def extractor():
    t=P(); t.r(3,3,12,12,S); t.r(3,3,12,3,SL); t.r(3,12,12,12,SK); t.r(5,5,10,10,K); t.r(6,7,9,8,RBRONZE); t.p(10,7,RBRONZE); t.p(10,8,RBRONZE); t.r(0,6,2,9,RBRONZE); t.r(13,6,15,9,KD); return t

machine('electric_miner', 'Electric miner', 15500, miner, storage={'columns': 2, 'rows': 2}, draw=10, miner=20)
for i in (1, 2, 3):
    n='alloy_furnace' + ('' if i == 1 else '_%d' % i); f=bench('alloy_furnace', i, 100)
    if i < 3: f['upgrade']={'to': 'alloy_furnace_%d' % (i + 1), 'cost': [{'item': ['lead_block', 'royal_bronze_block'][i - 1], 'count': 1}]}
    machine(n, 'Alloy furnace', [4200, 5400, 8600][i - 1], alloy(i), _roman=['', ' II', ' III'][i - 1], **f)
for i in (1, 2):
    n='industrial_bench' + ('' if i == 1 else '_2'); f=bench('industrial_bench', i, 100)
    if i == 1: f['upgrade']={'to': 'industrial_bench_2', 'cost': [{'item': 'imperial_ingot', 'count': 2}]}
    machine(n, 'Industrial bench', [2400, 9600][i - 1], ibench(i), _roman=['', ' II'][i - 1], **f)
machine('industrial_furnace', 'Industrial furnace', 6800, ifurnace, storage={'columns': 2, 'rows': 1}, draw=50, automat={'bench': 'electric_furnace', 'inputs': 1, 'speed': 2.0})
machine('industrial_armory', 'Industrial armory', 15500, armory, **bench('industrial_armory', 1, 500))
machine('industrial_smithery', 'Industrial smithery', 15500, smithery, **bench('industrial_smithery', 1, 500))
machine('factory', 'Factory', 9400, factory(1), storage={'columns': 5, 'rows': 1}, draw=50, automat={'inputs': 4, 'speed': 1.0},
        upgrade={'to': 'factory_2', 'cost': [{'item': 'processing_chip', 'count': 1}]})
machine('factory_2', 'Factory', 22000, factory(2), _roman=' II', storage={'columns': 7, 'rows': 1}, draw=50, automat={'inputs': 6, 'speed': 2.0})
machine('industrial_processor', 'Industrial processor', 21000, processor, **bench('industrial_processor', 1, 1000))
machine('power_plant', 'Power plant', 19000, plant, bench=True, bench_kind='power_plant', bench_level=1, generator={'power': 100, 'needs': 'fuel'}, fuel_capacity=18000,
        fuels=[{'item': 'dreadsteel_ingot', 'ticks': 1000}, {'item': 'imperial_ingot', 'ticks': 1800}])
machine('industrial_crate', 'Industrial crate', 13000, crate, storage={'columns': 4, 'rows': 4}, sorts=4)
items[-1]['category']='building'
for n, label, c, rate, price in (('conveyor', 'Conveyor', HBRONZE, 1, 160), ('gilded_conveyor', 'Gilded conveyor', RBRONZE, 2, 340), ('imperial_conveyor', 'Imperial conveyor', IMPERIAL, 5, 1500)):
    belt(c)().save('tiles', n); block_icon(n, n)
    tiles.append({'name': n, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.1, 'drop': n, 'texture': 'tiles/%s.png' % n, 'tool': 'pickaxe', 'floats': True, 'conveyor': rate})
    item(n, label, 'building', price, places=n)
extractor().save('tiles', 'conveyor_extractor'); block_icon('conveyor_extractor', 'conveyor_extractor')
tiles.append({'name': 'conveyor_extractor', 'label': 'Conveyor extractor', 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.2, 'drop': 'conveyor_extractor',
              'texture': 'tiles/conveyor_extractor.png', 'tool': 'pickaxe', 'floats': True, 'extractor': True, 'storage': {'columns': 1, 'rows': 1}})
item('conveyor_extractor', 'Conveyor extractor', 'building', 1600, places='conveyor_extractor', container={'slots': 1})
t=P(); t.r(3,3,12,12,(40,120,90,255)); t.r(5,5,10,10,K); t.r(7,7,8,8,GOLD)
for i in (4,7,10): t.p(2,i,GOLD); t.p(13,i,GOLD); t.p(i,2,GOLD); t.p(i,13,GOLD)
t.save('items', 'processing_chip'); item('processing_chip', 'Processing chip', 'materials', 11500)
# the workbench's fifth and sixth tiers
w4=copy.deepcopy(T['workbench_4']); w4['upgrade']={'to': 'workbench_5', 'cost': [{'item': 'heavy_bronze_ingot', 'count': 5}]}; tiles.append(w4)
for lvl, colour, cost in ((5, HBRONZE, ('dreadsteel_ingot', 5)), (6, DREAD, None)):
    d=copy.deepcopy(T['workbench_4']); n='workbench_%d' % lvl; d.update({'name': n, 'drop': n, 'texture': 'tiles/%s.png' % n, 'bench_level': lvl})
    if cost: d['upgrade']={'to': 'workbench_%d' % (lvl + 1), 'cost': [{'item': cost[0], 'count': cost[1]}]}
    tiles.append(d)
    im=Image.open(os.path.join(base, 'textures/tiles/workbench_4.png')).convert('RGBA'); px=im.load()
    for x in range(32):
        for y in range(8, 12):
            if px[x, y][3]: px[x, y]=colour if y > 9 else lt(colour)
    im.save(os.path.join(tex, 'tiles', n + '.png')); block_icon(n, n)
    item(n, 'Workbench ' + ['V', 'VI'][lvl - 5], 'benches', [3300, 16000][lvl - 5], places=n)

# ---- tools and armour of dreadsteel and of imperial
for metal, label, colour, speed, armor, price in (('dreadsteel', 'Dreadsteel', DREAD, 10.0, 0.19, 2600), ('imperial', 'Imperial', IMPERIAL, 12.0, 0.21, 3400)):
    for tool, src, n_ingots in (('pickaxe', 'titanium_pickaxe', 5), ('axe', 'titanium_axe', 3), ('machete', 'iron_machete', 2)):
        d=copy.deepcopy(I[src]); n='%s_%s' % (metal, tool)
        d.update({'name': n, 'label': '%s %s' % (label, tool), 'texture': 'items/%s.png' % n, 'price': price * n_ingots + 200}); d['tool']=dict(d['tool'], speed=speed); items.append(d)
        tint('items/%s.png' % src, 'items', n, colour, keep=is_wood)
        rec('industrial_smithery', n, 1, [('stick', 1), (metal + '_ingot', n_ingots)], 180)
    for part, n_ingots in (('helmet', 2), ('chestplate', 5), ('leggings', 3), ('boots', 2)):
        d=copy.deepcopy(I['titanium_' + part]); n='%s_%s' % (metal, part)
        d.update({'name': n, 'label': d['label'].replace('Titanium', label), 'texture': 'items/%s.png' % n, 'armor': armor, 'price': price * n_ingots + 200}); items.append(d)
        tint('items/titanium_%s.png' % part, 'items', n, colour)
        rec('industrial_armory', n, 1, [(metal + '_ingot', n_ingots)], 180)
d=copy.deepcopy(I['titanium_sword']); d.update({'name': 'dreadsteel_sword', 'label': 'Dreadsteel sword', 'texture': 'items/dreadsteel_sword.png', 'damage': 50, 'price': 8200}); items.append(d)
tint('items/titanium_sword.png', 'items', 'dreadsteel_sword', DREAD, keep=is_wood); rec('industrial_smithery', 'dreadsteel_sword', 1, [('stick', 1), ('dreadsteel_ingot', 3)], 180)
d={'name': 'imperial_longbow', 'label': 'Imperial longbow', 'texture': 'items/imperial_longbow.png', 'category': 'tools', 'damage': 20, 'range': 14, 'price': 18000}; items.append(d)
tint('items/gold_bow.png', 'items', 'imperial_longbow', IMPERIAL, keep=is_wood); rec('industrial_smithery', 'imperial_longbow', 1, [('stick', 50), ('imperial_ingot', 5), ('feather', 5)], 180)

# ---- the recipes of the plan
rec('workbench', 'electric_miner', 1, [('steel_block', 1), ('copper_wire', 10), ('titanium_pickaxe', 1)], 120, level=4)
rec('workbench', 'alloy_furnace', 1, [('steel_block', 1), ('copper_wire', 10), ('titanium_ingot', 1)], 120, level=4)
rec('workbench', 'industrial_bench', 1, [('steel_block', 1), ('copper_wire', 10), ('heavy_bronze_ingot', 1)], 120, level=4)
rec('workbench', 'industrial_furnace', 1, [('heavy_bronze_block', 1), ('conveyor', 5), ('chest', 1), ('electric_furnace', 1)], 180, level=5)
rec('workbench', 'industrial_armory', 1, [('heavy_bronze_block', 1), ('conveyor', 5), ('titanium_chestplate', 1)], 180, level=5)
rec('workbench', 'industrial_smithery', 1, [('heavy_bronze_block', 1), ('conveyor', 5), ('titanium_pickaxe', 1)], 180, level=5)
rec('workbench', 'factory', 1, [('heavy_bronze_block', 1), ('refinery', 1), ('conveyor', 5), ('chest', 1)], 180, level=5)
rec('workbench', 'industrial_processor', 1, [('dreadsteel_block', 1), ('imperial_conveyor', 5), ('chest', 1)], 300, level=6)
rec('workbench', 'power_plant', 1, [('dreadsteel_block', 1), ('imperial_conveyor', 5)], 300, level=6)
rec('industrial_bench', 'conveyor', 5, [('heavy_bronze_ingot', 3)], 20)
rec('industrial_bench', 'gilded_conveyor', 3, [('conveyor', 3), ('royal_bronze_ingot', 1)], 20)
rec('industrial_bench', 'conveyor_extractor', 1, [('royal_bronze_ingot', 3)], 20)
rec('industrial_bench', 'imperial_conveyor', 3, [('gilded_conveyor', 3), ('imperial_ingot', 1)], 60, level=2)
rec('industrial_processor', 'processing_chip', 1, [('dreadsteel_ingot', 3), ('imperial_ingot', 1)], 300)
rec('industrial_processor', 'industrial_crate', 1, [('dreadsteel_ingot', 3), ('safe', 1), ('imperial_ingot', 1)], 300)
rec('alloy_furnace', 'bronze_ingot', 3, [('copper_ingot', 1), ('tin_ingot', 1)], 10)
rec('alloy_furnace', 'steel_ingot', 3, [('iron_ingot', 1), ('coal', 1)], 10)
rec('alloy_furnace', 'heavy_bronze_ingot', 2, [('lead_ingot', 1), ('bronze_ingot', 1)], 30, level=2)
rec('alloy_furnace', 'royal_bronze_ingot', 2, [('gold_ingot', 1), ('bronze_ingot', 1)], 30, level=2)
rec('alloy_furnace', 'dreadsteel_ingot', 1, [('titanium_ingot', 1), ('steel_ingot', 1)], 30, level=3)
rec('alloy_furnace', 'imperial_ingot', 1, [('platinum_ingot', 1), ('lead_ingot', 1), ('gold_ingot', 1)], 30, level=3)

worldgen={'veins': [lead_vein], 'vein_rares': rares}
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        if isinstance(obj, dict) and len(obj) == 1 and isinstance(list(obj.values())[0], list):
            k, v = list(obj.items())[0]
            f.write('{\n  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]\n}\n')
        else: json.dump(obj, f, indent=2); f.write('\n')
for d in tiles: d.pop('_roman', None)
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes}); dump('worldgen.json', worldgen)
dump('mod.json', {'label': 'Industry', 'icon': 'icon.png'})
t=P(); t.r(0,0,15,15,SK); t.r(2,7,13,13,S); t.r(2,7,13,7,SL); t.r(3,2,5,6,KD); t.r(8,4,10,6,KD); t.r(3,1,5,1,WHITE); t.r(4,9,6,11,HOT); t.r(9,9,11,11,HOT); t.r(0,14,15,15,HBRONZE)
t.im.resize((64, 64), Image.NEAREST).save(os.path.join(mod, 'icon.png'))
print(len(tiles), 'tiles', len(items), 'items', len(recipes), 'recipes')
