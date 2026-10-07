# Writes the Magic and Mayhem mod: its data files and its textures, from the plan in MagicMayhem.txt.
import sys, os, json, copy
from PIL import Image
repo = sys.argv[1]; mod = os.path.join(repo, 'mods/magic_mayhem'); tex = os.path.join(mod, 'textures')
for d in ('tiles', 'items', 'mobs'): os.makedirs(os.path.join(tex, d), exist_ok=True)
base = os.path.join(repo, 'data')
def load(n): return json.load(open(os.path.join(base, n)))
T = {t['name']: t for t in load('tiles.json')['tiles']}
I = {t['name']: t for t in load('items.json')['items']}
TR = {t['name']: t for t in load('trees.json')['trees']}
M = {t['name']: t for t in load('mobs.json')['mobs']}

# ---- drawing: sixteen pixels square, drawn double for blocks and creatures, treble for items
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
        s.im.resize((16 * k, 16 * k), Image.NEAREST).save(os.path.join(tex, kind, name + '.png'))
        return s
def block_icon(tile_file, name):
    im = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); im.paste(Image.open(os.path.join(tex, 'tiles', tile_file + '.png')).convert('RGBA'), (8, 8))
    im.save(os.path.join(tex, 'items', name + '.png'))
def recolour(src, kind, name, f):
    im = Image.open(os.path.join(base, 'textures', src)).convert('RGBA'); px = im.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, b, a = px[x, y]
            if a: px[x, y] = f(r, g, b) + (a,)
    im.save(os.path.join(tex, kind, name + '.png'))
def to_magic(r, g, b):
    # green to a violet-pink; what is not green (wood, stone) is left as it is
    if g > r + 18 and g > b + 8: return (min(255, int(g * 0.95)), int(r * 0.75), min(255, int(g * 1.05)))
    return (r, g, b)

WL=(201,159,99,255); W=(174,130,73,255); WM=(140,101,55,255); WD=(106,72,39,255)
ML=(192,194,205,255); MM=(149,151,163,255); MD=(107,109,120,255); K=(22,22,30,255); KD=(58,58,69,255)
MAG=(200,90,220,255); MAGL=(236,170,246,255); MAGD=(120,50,150,255); OBS=(40,32,58,255); OBSL=(78,62,108,255); OBSD=(22,18,34,255)
GOLD=(215,159,29,255); FIRE=(240,140,40,255); HOT=(250,210,90,255); WATER=(70,130,220,255); WATL=(150,200,250,255)
AIR=(200,230,240,255); AIRD=(140,180,200,255); EARTH=(130,96,60,255); EARL=(176,140,96,255); WHITE=(236,236,230,255)
PAPER=(232,222,196,255); PAPD=(190,176,146,255); LEAF=(90,170,80,255); LEAFL=(140,205,110,255); RED=(190,60,60,255); LIFE=(110,210,130,255)
SUN=(250,200,70,255); MOON=(170,180,230,255); GLASS=(190,225,235,150); GLASSE=(150,190,205,255)

tiles=[]; items=[]; recipes=[]
def item(name, label, cat, price, **kw):
    d={'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'category': cat, 'price': price}; d.update(kw); items.append(d)
def rec(bench, out, n, ins, time, level=1, **kw):
    d={'bench': bench, 'output': {'item': out, 'count': n}, 'inputs': [dict({'item': a, 'count': b}, **({'full': True} if a == 'bucket' else {})) for a, b in ins], 'time': time}
    if level > 1: d['level'] = level
    d.update(kw); recipes.append(d)

# ---- benches
def lectern(tier):
    t=P(); cloth=[RED,(70,120,200,255),MAG][tier-1]
    t.r(7,8,8,14,WM); t.r(4,14,11,15,WD); t.r(5,13,10,13,WM)
    t.r(2,5,13,7,W); t.r(2,5,13,5,WL); t.r(2,8,13,8,WD)
    t.r(4,2,11,4,PAPER); t.r(7,2,8,4,PAPD); t.r(4,4,11,4,cloth)
    for i in range(tier-1): t.p(6+2*i,10,GOLD)
    return t
def orb(tier):
    t=P(); c=[WATER,(250,170,70,255),MAG][tier-1]
    t.r(5,12,10,13,MD); t.r(4,14,11,15,KD); t.r(6,11,9,11,MM)
    t.r(5,2,10,10,c); t.r(4,4,11,8,c); t.r(6,1,9,1,c); t.r(6,11,9,11,dk(c))
    t.r(6,3,7,4,lt(c,1.5)); t.p(9,8,dk(c)); t.p(10,7,dk(c))
    for i in range(tier-1): t.p(6+2*i,14,GOLD)
    return t
def core():
    t=P()
    t.r(1,1,14,14,OBS); t.r(1,1,14,1,OBSL); t.r(1,14,14,14,OBSD); t.r(1,1,1,14,OBSL); t.r(14,1,14,14,OBSD)
    t.r(5,5,10,10,MAG); t.r(6,4,9,11,MAG); t.r(4,6,11,9,MAG); t.r(6,6,7,7,MAGL); t.r(7,7,8,8,WHITE)
    for x,y in ((3,3),(12,3),(3,12),(12,12)): t.p(x,y,MAGL)
    return t
def array_():
    t=P()
    for x,y in ((7,1),(8,1),(4,2),(11,2),(2,4),(13,4),(1,7),(14,7),(1,8),(14,8),(2,11),(13,11),(4,13),(11,13),(7,14),(8,14)): t.p(x,y,MAG)
    for i in range(3,13): t.p(i,i,MAGD); t.p(15-i,i,MAGD)
    t.r(6,6,9,9,MAGL); t.r(7,7,8,8,WHITE); t.r(7,3,8,4,GOLD); t.r(7,11,8,12,GOLD); t.r(3,7,4,8,GOLD); t.r(11,7,12,8,GOLD)
    return t
def bench_tile(name, label, kind, level, upgrade=None):
    d={'name': name, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.4, 'drop': name,
       'texture': 'tiles/%s.png' % name, 'bench': True, 'bench_kind': kind, 'bench_level': level, 'tool': 'axe', 'popup_name': label.lower()}
    if upgrade: d['upgrade'] = {'to': upgrade[0], 'cost': [{'item': upgrade[1], 'count': upgrade[2]}]}
    tiles.append(d)
roman=['', ' II', ' III']
for i in (1,2,3):
    n='lectern' + ('' if i==1 else '_%d' % i); lectern(i).save('tiles', n); block_icon(n, n)
    bench_tile(n, 'Lectern', 'lectern', i, None if i==3 else ('lectern_%d' % (i+1), ['elemental_essence','sundial_essence'][i-1], 1))
    item(n, 'Lectern' + roman[i-1], 'benches', [80, 2600, 9000][i-1], places=n)
    n='magic_orb' + ('' if i==1 else '_%d' % i); orb(i).save('tiles', n); block_icon(n, n)
    bench_tile(n, 'Magic orb', 'magic_orb', i, None if i==3 else ('magic_orb_%d' % (i+1), ['elemental_essence','sundial_essence'][i-1], 1))
    item(n, 'Magic orb' + roman[i-1], 'benches', [1500, 4000, 11000][i-1], places=n)
core().save('tiles', 'magic_core'); block_icon('magic_core', 'magic_core'); bench_tile('magic_core', 'Magic core', 'magic_core', 1)
tiles[-1]['tool'] = 'pickaxe'; tiles[-1]['light'] = 8
item('magic_core', 'Magic core', 'benches', 8000, places='magic_core')
array_().save('tiles', 'magic_array'); block_icon('magic_array', 'magic_array')
tiles.append({'name': 'magic_array', 'label': 'Magic array', 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.4, 'drop': 'magic_array',
              'texture': 'tiles/magic_array.png', 'tool': 'pickaxe', 'light': 8, 'floats': True, 'quickens': {'speed': 2.0, 'radius': 5}})
item('magic_array', 'Magic array', 'building', 30000, places='magic_array')
# the workbench's fifth tier
w4=copy.deepcopy(T['workbench_4']); w4['upgrade']={'to': 'workbench_5', 'cost': [{'item': 'obsidian', 'count': 5}]}; tiles.append(w4)
w5=copy.deepcopy(T['workbench_4']); w5.update({'name': 'workbench_5', 'drop': 'workbench_5', 'texture': 'tiles/workbench_5.png', 'bench_level': 5}); tiles.append(w5)
im=Image.open(os.path.join(base,'textures/tiles/workbench_4.png')).convert('RGBA'); px=im.load()
for x in range(0,32):
    for y in range(8,12):
        if px[x,y][3]: px[x,y]=OBS if y>9 else OBSL
for x in (12,13,18,19): px[x,10]=MAGL; px[x,11]=MAGL
im.save(os.path.join(tex,'tiles/workbench_5.png')); block_icon('workbench_5','workbench_5')
item('workbench_5', 'Workbench V', 'benches', 3500, places='workbench_5')

# ---- obsidian, the magic gem and its tree, sugarcane
for k in (0,1):
    t=P(); t.r(0,0,15,15,OBS)
    for x,y in ((2,3),(9,1),(12,6),(5,9),(10,12),(1,13),(14,11),(7,5)): t.p((x+5*k)%16,(y+3*k)%16,OBSL)
    for x,y in ((4,6),(11,3),(8,10),(13,14),(2,10),(6,13)): t.p((x+7*k)%16,(y+5*k)%16,OBSD)
    t.r(3+6*k,7,5+6*k,7,(96,70,140,255)); t.save('tiles','obsidian_%d' % k)
ob=copy.deepcopy(T['basalt']); ob.update({'name': 'obsidian', 'label': 'Obsidian', 'drop': 'obsidian', 'texture': ['tiles/obsidian_0.png','tiles/obsidian_1.png'], 'mine_time': 1.6}); tiles.append(ob)
block_icon('obsidian_0','obsidian'); item('obsidian', 'Obsidian', 'blocks', 120, places='obsidian')
g=copy.deepcopy(T['emerald']); g.update({'name': 'magic_gem', 'label': 'Magic gem', 'drop': 'magic_gem', 'texture': 'tiles/magic_gem.png',
    'texture_mount': {'floor': 'tiles/magic_gem_on_floor.png', 'ceiling': 'tiles/magic_gem_on_ceiling.png', 'side': 'tiles/magic_gem_on_side.png'}}); tiles.append(g)
for s in ('', '_on_floor', '_on_ceiling', '_on_side'): recolour('tiles/emerald%s.png' % s, 'tiles', 'magic_gem%s' % s, to_magic)
recolour('items/emerald.png', 'items', 'magic_gem', to_magic); recolour('items/emerald_shard.png', 'items', 'magic_gem_shard', to_magic)
item('magic_gem', 'Magic gem', 'gems', 1400, places='magic_gem'); item('magic_gem_shard', 'Magic gem shard', 'gems', 420)
for part in ('log', 'leaves'):
    d=copy.deepcopy(T['emerald_tree_%s' % part]); n=len(d['texture'])
    d.update({'name': 'magic_tree_%s' % part, 'label': 'Magic tree ' + ('trunk' if part=='log' else 'leaves'), 'texture': ['tiles/magic_tree_%s_%d.png' % (part, i) for i in range(n)]}); tiles.append(d)
    for i in range(n): recolour('tiles/emerald_tree_%s_%d.png' % (part, i), 'tiles', 'magic_tree_%s_%d' % (part, i), to_magic)
tree=copy.deepcopy(TR['emerald_tree']); tree.update({'name': 'magic_tree', 'log': 'magic_tree_log', 'leaves': 'magic_tree_leaves', 'fruit': 'magic_gem_shard'})
def cane(stage):
    t=P(); h=[4,8,13,16,16][stage]
    for x in (5,8,11):
        top=16-h+(0 if x==8 else 1)
        t.r(x,top,x,15,LEAF)
        for y in range(top+2,16,4): t.p(x,y,dk(LEAF))
        if stage>=1: t.p(x-1,top+1,LEAFL); t.p(x+1,top+2,LEAFL)
    if stage==4:
        for x in (5,8,11): t.r(x-1,2,x+1,3,LEAFL); t.p(x-2,4,LEAF); t.p(x+2,5,LEAF); t.r(x,0,x,1,(226,214,170,255))
    return t
for i in range(4):
    cane(i).save('tiles','sugarcane_%d' % i)
    d=copy.deepcopy(T['wheat_%d' % i]); d.update({'name': 'sugarcane_%d' % i, 'label': 'Sugarcane', 'texture': 'tiles/sugarcane_%d.png' % i,
        'drops': [{'item': 'sugarcane', 'count': 1}], 'soil': ['sand', 'dirt', 'grass', 'compost']})
    for k in ('grows_into','grows_top','topped_by'):
        if k in d: d[k]=d[k].replace('wheat','sugarcane')
    tiles.append(d)
cane(4).save('tiles','sugarcane_top'); d=copy.deepcopy(T['wheat_top']); d.update({'name': 'sugarcane_top', 'label': 'Sugarcane', 'texture': 'tiles/sugarcane_top.png', 'drops': [{'item': 'sugarcane', 'count': 3}], 'cut_to': 'sugarcane_2'}); tiles.append(d)
t=P(); t.r(4,2,5,14,LEAF); t.r(8,1,9,14,LEAFL); t.r(11,3,12,14,LEAF); [t.p(x,y,dk(LEAF)) for x in (4,5,8,9,11,12) for y in (5,9,12)]; t.save('items','sugarcane')
item('sugarcane', 'Sugarcane', 'materials', 15, sows='sugarcane_0')

# ---- things made
def book(c, mark):
    t=P(); t.r(3,2,12,13,c); t.r(3,2,3,13,dk(c)); t.r(4,13,12,14,PAPER); t.r(12,3,12,13,PAPD); t.r(6,5,9,8,mark); t.r(7,6,8,7,lt(mark,1.4)); t.r(3,2,12,2,lt(c)); return t
def scroll(c, glyph):
    t=P(); t.r(3,3,12,12,PAPER); t.r(2,2,13,3,PAPD); t.r(2,12,13,13,PAPD); t.r(1,2,1,3,WM); t.r(14,2,14,3,WM); t.r(1,12,1,13,WM); t.r(14,12,14,13,WM)
    for x,y in glyph: t.p(x,y,c)
    return t
def flask(c):
    t=P(); t.r(6,1,9,2,WM); t.r(7,3,8,4,GLASSE); t.r(4,6,11,13,c); t.r(5,5,10,14,c); t.r(5,5,10,5,GLASSE); t.r(4,6,4,13,GLASSE); t.r(11,6,11,13,GLASSE); t.r(5,14,10,14,GLASSE); t.r(6,7,7,8,lt(c,1.5)); return t
def wisp(c):
    t=P(); t.r(6,3,9,12,c); t.r(4,6,11,10,c); t.r(5,4,10,11,c); t.r(7,1,8,2,c); t.r(6,5,7,7,lt(c,1.5)); t.p(8,13,dk(c)); t.p(7,14,dk(c)); return t
FLAME=[(7,5),(8,5),(6,6),(9,6),(6,7),(7,7),(8,7),(9,7),(7,8),(8,8),(7,9),(8,10),(7,4)]
DROP=[(7,5),(8,5),(7,6),(8,6),(6,7),(9,7),(6,8),(9,8),(6,9),(9,9),(7,10),(8,10),(7,8)]
SHIELD=[(5,5),(6,5),(7,5),(8,5),(9,5),(10,5),(5,6),(10,6),(5,7),(10,7),(6,8),(9,8),(6,9),(9,9),(7,10),(8,10)]
PICK=[(5,5),(6,5),(7,5),(8,5),(9,5),(10,6),(8,6),(7,7),(6,8),(5,9),(5,10),(10,7)]
SUNG=[(7,6),(8,6),(6,7),(9,7),(6,8),(9,8),(7,9),(8,9),(7,7),(8,8),(7,4),(8,4),(7,11),(8,11),(4,7),(4,8),(11,7),(11,8)]
MOONG=[(7,5),(8,5),(6,6),(5,7),(5,8),(6,9),(7,10),(8,10),(9,9),(7,7),(7,8),(8,9)]
HEART=[(5,6),(6,5),(7,6),(8,6),(9,5),(10,6),(5,7),(10,7),(6,8),(9,8),(7,9),(8,9),(6,6),(9,6),(7,7),(8,7),(6,7),(9,7),(7,8),(8,8),(7,10),(8,10)]
t=P(); t.r(3,2,12,13,PAPER); t.r(3,2,12,2,WHITE); t.r(12,3,12,13,PAPD); [t.r(5,y,10,y,PAPD) for y in (5,7,9,11)]; t.save('items','paper')
item('paper', 'Paper', 'materials', 10)
book((120,70,40,255), GOLD).save('items','spellbook'); item('spellbook', 'Spellbook', 'materials', 150)
book((60,70,150,255), MAG).save('items','masters_spellbook'); item('masters_spellbook', "Master's spellbook", 'materials', 3200)
book(OBS, MAGL).save('items','grandmasters_spellbook'); item('grandmasters_spellbook', "Grandmaster's spellbook", 'materials', 9000)
scroll(FIRE, FLAME).save('items','fireball_scroll')
item('fireball_scroll', 'Fireball scroll', 'tools', 420, uses=10, range=10, cast={'element': 'flame', 'damage': 10, 'splash': 1, 'cooks': True})
scroll(WATER, DROP).save('items','water_scroll')
item('water_scroll', 'Water scroll', 'tools', 420, uses=50, range=8, cast={'element': 'water', 'water': 8, 'bane': 10, 'splash': 1})
scroll(AIRD, SHIELD).save('items','shield_scroll')
item('shield_scroll', 'Shield scroll', 'tools', 420, uses=10, range=8, cast={'element': 'air', 'shield': 1.0, 'bane': 10, 'splash': 1})
scroll(EARTH, PICK).save('items','miners_scroll')
item('miners_scroll', "Miner's scroll", 'tools', 480, uses=1, range=8, cast={'element': 'earth', 'dig': 5, 'bane': 10, 'splash': 5})
scroll((220,150,30,255), SUNG).save('items','sunrise_scroll'); item('sunrise_scroll', 'Sunrise scroll', 'tools', 6500, uses=1, cast={'time': 'sunrise'})
scroll((90,100,190,255), MOONG).save('items','moonrise_scroll'); item('moonrise_scroll', 'Moonrise scroll', 'tools', 6500, uses=1, cast={'time': 'moonrise'})
scroll((60,170,90,255), HEART).save('items','life_scroll'); item('life_scroll', 'Life scroll', 'materials', 40000)
t=P(); t.r(6,1,9,2,WM); t.r(5,3,10,3,GLASSE); t.r(4,4,11,14,GLASS); t.r(4,4,4,14,GLASSE); t.r(11,4,11,14,GLASSE); t.r(5,14,10,14,GLASSE); t.r(6,6,6,9,WHITE); t.save('items','glass_jar')
item('glass_jar', 'Glass jar', 'materials', 50)
for n,label,c,price in (('flame','Flame',FIRE,300),('water','Water',WATER,300),('air','Air',AIR,300),('earth','Earth',EARL,300)):
    wisp(c).save('items', n+'_essence'); item(n+'_essence', label+' essence', 'materials', price)
    flask(c).save('items', n+'_catalyst'); item(n+'_catalyst', label+' catalyst', 'materials', price+90)
t=flask(MAG); [t.p(x,y,c) for (x,y,c) in ((6,10,FIRE),(9,10,WATER),(6,12,EARL),(9,12,AIR))]; t.save('items','elemental_essence'); item('elemental_essence', 'Elemental essence', 'materials', 3400)
flask(SUN).save('items','sun_catalyst'); item('sun_catalyst', 'Sun catalyst', 'materials', 1400)
flask(MOON).save('items','moon_catalyst'); item('moon_catalyst', 'Moon catalyst', 'materials', 3200)
t=flask((250,225,150,255)); t.r(6,9,7,12,SUN); t.r(8,9,9,12,MOON); t.save('items','sundial_essence'); item('sundial_essence', 'Sundial essence', 'materials', 9800)
flask(LIFE).save('items','life_catalyst'); item('life_catalyst', 'Life catalyst', 'materials', 21000)
t=P(); t.r(2,7,13,8,MM); t.r(2,7,13,7,ML); t.r(1,6,2,9,MD); t.r(13,6,14,9,MD); t.r(6,5,9,10,MAG); t.r(7,6,8,7,MAGL); t.save('items','magic_conduit'); item('magic_conduit', 'Magic conduit', 'materials', 5200)
t=P(); t.r(5,2,10,13,OBS); t.r(4,5,11,10,OBS); t.r(6,1,9,1,OBSL); t.r(5,2,5,12,OBSL); t.r(7,5,8,9,MAGL); t.r(6,6,9,8,MAG); t.r(7,7,8,7,WHITE); t.save('items','soulstone'); item('soulstone', 'Soulstone', 'materials', 5000)

# ---- recipes, as the plan has them
rec('workbench', 'lectern', 1, [('wood', 2), ('feather', 1)], 20)
rec('workbench', 'magic_orb', 1, [('glass', 3), ('magic_gem', 1)], 120, level=3)
rec('workbench', 'magic_core', 1, [('magic_gem', 5), ('obsidian', 5)], 300, level=5)
rec('magic_core', 'magic_conduit', 1, [('magic_gem', 1), ('elemental_essence', 1)], 120)
rec('magic_core', 'soulstone', 2, [('magic_gem', 3), ('obsidian', 3), ('diamond', 1)], 180)
rec('magic_core', 'magic_array', 1, [('magic_conduit', 5), ('soulstone', 5), ('sundial_essence', 1)], 300)
rec('lectern', 'paper', 3, [('sugarcane', 1)], 15)
rec('lectern', 'spellbook', 1, [('leather', 2), ('paper', 2)], 30)
rec('lectern', 'fireball_scroll', 1, [('spellbook', 1), ('flame_catalyst', 1)], 30)
rec('lectern', 'water_scroll', 1, [('spellbook', 1), ('water_catalyst', 1)], 30)
rec('lectern', 'shield_scroll', 1, [('spellbook', 1), ('air_catalyst', 1)], 30)
rec('lectern', 'miners_scroll', 1, [('spellbook', 1), ('earth_catalyst', 1)], 30)
rec('lectern', 'masters_spellbook', 1, [('spellbook', 1), ('magic_gem', 2)], 120, level=2)
rec('lectern', 'sunrise_scroll', 1, [('masters_spellbook', 1), ('sun_catalyst', 1)], 180, level=2)
rec('lectern', 'moonrise_scroll', 1, [('masters_spellbook', 1), ('moon_catalyst', 1)], 180, level=2)
rec('lectern', 'grandmasters_spellbook', 1, [('masters_spellbook', 1), ('soulstone', 1)], 300, level=3)
rec('lectern', 'life_scroll', 1, [('grandmasters_spellbook', 1), ('life_catalyst', 1)], 300, level=3)
rec('magic_orb', 'glass_jar', 1, [('glass', 2)], 20)
rec('magic_orb', 'flame_catalyst', 1, [('flame_essence', 1), ('ember', 2)], 20)
rec('magic_orb', 'water_catalyst', 1, [('water_essence', 1), ('bucket', 1)], 20)
rec('magic_orb', 'air_catalyst', 1, [('air_essence', 1), ('feather', 5)], 20)
rec('magic_orb', 'earth_catalyst', 1, [('earth_essence', 1), ('iron_ore', 3)], 20)
rec('magic_orb', 'elemental_essence', 1, [('flame_catalyst', 2), ('water_catalyst', 2), ('air_catalyst', 2), ('earth_catalyst', 2), ('glass_jar', 1)], 120)
rec('magic_orb', 'sun_catalyst', 1, [('flame_catalyst', 1), ('fuel', 5), ('rainbow_essence', 1)], 300, level=2)
rec('magic_orb', 'moon_catalyst', 1, [('water_catalyst', 1), ('poison', 5), ('platinum_ingot', 1)], 300, level=2)
rec('magic_orb', 'sundial_essence', 1, [('sun_catalyst', 2), ('moon_catalyst', 2), ('glass_jar', 1)], 300, level=2)
rec('magic_orb', 'life_catalyst', 1, [('sundial_essence', 1), ('elemental_essence', 1), ('magic_gem', 5)], 300, level=3)
rec('press', 'magic_gem', 1, [('magic_gem_shard', 3)], 80, level=3)
# the third character: a life scroll and 100 crystals, once there are two
recipes.append({'bench': 'portal', 'output': {'item': 'new_player', 'count': 1}, 'inputs': [{'item': 'life_scroll', 'count': 1}], 'crystals': 100, 'characters': 2})

# ---- the spirits
def spirit(c, eyes=K):
    t=P(); t.r(5,2,10,11,c); t.r(4,4,11,9,c); t.r(6,1,9,1,lt(c)); t.r(5,2,6,4,lt(c,1.4)); t.p(6,6,eyes); t.p(9,6,eyes)
    t.p(5,12,c); t.p(8,13,c); t.p(10,12,c); t.p(6,13,dk(c)); t.p(9,14,dk(c)); return t
def bob(t):
    u=Image.new('RGBA',(16,16),(0,0,0,0)); u.paste(t.im,(0,1)); q=P(); q.im=u; q.px=u.load(); return q
mobs=[]
for n,label,c,weak,ess,extra in (
    ('flame_spirit','Flame spirit',FIRE,'water','flame_essence',{'flies': True, 'spawn': {'sky': True, 'time': 'day', 'chance': 0.004}}),
    ('air_spirit','Air spirit',AIR,'earth','air_essence',{'flies': True, 'spawn': {'sky': True, 'time': 'night', 'chance': 0.004}}),
    ('water_spirit','Water spirit',WATER,'flame','water_essence',{'swim': {}, 'spawn': {'water': True, 'biome': 'ocean', 'time': 'night', 'chance': 0.02}}),
    ('earth_spirit','Earth spirit',EARL,'air','earth_essence',{'hop': 0.1, 'spawn': {'above': ['stone'], 'cave': True, 'time': 'day', 'chance': 0.03}})):
    s=spirit(c); s.save('mobs', n); bob(s).save('mobs', n+'_drift')
    d={'name': n, 'label': label, 'texture': 'mobs/%s.png' % n, 'walk': ['mobs/%s_drift.png' % n, 'mobs/%s.png' % n], 'step_ticks': 4,
       'move_chance': 0.5, 'turn_chance': 0.2, 'health': 10, 'cap': 2, 'immune': True, 'weak_to': weak, 'drops': [{'item': ess, 'min': 0, 'max': 1}]}
    d.update(extra); mobs.append(d)

worldgen={'veins': [{'name': 'obsidian', 'tile': 'obsidian', 'in': ['stone'], 'size': [8, 24], 'per_100_columns': 3, 'depth': [165, 198]}],
          'cave_decor': [{'name': 'magic_gem', 'tile': 'magic_gem', 'per_100_columns': 2, 'depth': [45, 190]}],
          'island_trees': [{'tree': 'magic_tree', 'weight': 4, 'above': 110}],
          'biome_plants': [{'biome': 'beach', 'tile': 'sugarcane_0', 'one_in': 10}]}
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        if isinstance(obj, dict) and len(obj) == 1 and isinstance(list(obj.values())[0], list):
            k, v = list(obj.items())[0]
            f.write('{\n  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]\n}\n')
        else: json.dump(obj, f, indent=2); f.write('\n')
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes})
dump('trees.json', {'trees': [tree]}); dump('mobs.json', {'mobs': mobs}); dump('worldgen.json', worldgen); dump('rules.json', {'players': {'max': 3}})
dump('mod.json', {'label': 'Magic and Mayhem', 'icon': 'icon.png'})
t=P(); t.r(0,0,15,15,OBS); t.r(3,3,12,12,PAPER); t.r(2,2,13,3,PAPD); t.r(2,12,13,13,PAPD)
for x,y in FLAME: t.p(x,y+1,MAG)
t.im.resize((64,64),Image.NEAREST).save(os.path.join(mod,'icon.png'))
print(len(tiles),'tiles',len(items),'items',len(recipes),'recipes',len(mobs),'mobs')
