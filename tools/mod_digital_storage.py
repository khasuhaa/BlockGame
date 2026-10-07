# Writes the Digital Storage mod: its data files and its textures.
import sys, os, json, copy
from PIL import Image
repo = sys.argv[1]; mod = os.path.join(repo, 'mods/digital_storage'); tex = os.path.join(mod, 'textures')
for d in ('tiles', 'items'): os.makedirs(os.path.join(tex, d), exist_ok=True)
base = os.path.join(repo, 'data')
def load(n): return json.load(open(os.path.join(base, n)))
T = {t['name']: t for t in load('tiles.json')['tiles']}
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
STONE = set(Image.open(os.path.join(base, 'textures/tiles/stone_0.png')).convert('RGBA').getdata())
QUARTZ=(190,232,240,255); FLUIX=(150,90,210,255); FLUIXL=(200,160,245,255); SL=(180,188,200,255); S=(120,134,156,255); SD=(93,104,121,255); SK=(66,73,85,255)
K=(22,22,30,255); KD=(58,58,69,255); GOLD=(215,159,29,255); WHITE=(236,236,230,255); GREEN=(90,210,120,255); BLUE=(80,150,230,255); RED=(210,70,70,255); CYAN=(110,220,230,255)
tiles=[]; items=[]; recipes=[]
def item(name, label, cat, price, **kw):
    d={'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'category': cat, 'price': price}; d.update(kw); items.append(d)
def rec(bench, out, n, ins, time, level=1):
    d={'bench': bench, 'output': {'item': out, 'count': n}, 'inputs': [{'item': a, 'count': b} for a, b in ins], 'time': time}
    if level > 1: d['level'] = level
    recipes.append(d)
def body(c=SD, cl=S, cd=SK):
    t=P(); t.r(1,1,14,14,c); t.r(1,1,14,1,cl); t.r(1,14,14,14,cd); t.r(14,2,14,14,cd); t.r(1,2,1,13,cl); return t
def part(name, label, price, t, cat='building', **fields):
    t.save('tiles', name); block_icon(name, name)
    d={'name': name, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.3, 'drop': name,
       'texture': 'tiles/%s.png' % name, 'tool': 'pickaxe', 'floats': True, 'popup_name': label.lower()}
    d.update(fields); tiles.append(d)
    kw={'places': name}
    if 'storage' in fields: kw['container']={'slots': fields['storage']['columns'] * fields['storage']['rows']}
    item(name, label, cat, price, **kw)

# ---- certus quartz, in the rock
ore=copy.deepcopy(T['iron_ore']); ore.update({'name': 'quartz_ore', 'label': 'Certus quartz ore', 'drop': 'certus_quartz', 'texture': ['tiles/quartz_ore_0.png', 'tiles/quartz_ore_1.png'], 'light': 2}); tiles.append(ore)
for k in (0, 1):
    im=Image.open(os.path.join(base, 'textures/tiles/iron_ore_%d.png' % k)).convert('RGBA'); px=im.load()
    for y in range(32):
        for x in range(32):
            if px[x, y] not in STONE:
                v=sum(px[x, y][:3]) / 765.0; px[x, y]=tuple(min(255, int(c * (0.45 + v * 0.8))) for c in QUARTZ[:3]) + (255,)
    im.save(os.path.join(tex, 'tiles', 'quartz_ore_%d.png' % k))
vein=copy.deepcopy(V['iron']); vein.update({'name': 'quartz', 'tile': 'quartz_ore'})
def crystal(c):
    t=P(); t.r(6,2,9,13,c); t.r(5,5,10,11,c); t.r(7,1,8,1,lt(c)); t.r(6,2,6,12,lt(c)); t.r(10,6,10,11,dk(c)); t.r(7,13,8,14,dk(c)); t.p(7,4,WHITE); return t
crystal(QUARTZ).save('items', 'certus_quartz'); item('certus_quartz', 'Certus quartz', 'gems', 90)
crystal(FLUIX).save('items', 'fluix_crystal'); item('fluix_crystal', 'Fluix crystal', 'gems', 140)
def chip(c):
    t=P(); t.r(3,3,12,12,(40,60,56,255)); t.r(5,5,10,10,c); t.r(7,7,8,8,lt(c,1.5))
    for i in (4,7,10): t.p(2,i,GOLD); t.p(13,i,GOLD); t.p(i,2,GOLD); t.p(i,13,GOLD)
    return t
for n, label, c, price in (('logic_processor', 'Logic processor', GOLD, 900), ('calculation_processor', 'Calculation processor', QUARTZ, 500), ('engineering_processor', 'Engineering processor', CYAN, 5600)):
    chip(c).save('items', n); item(n, label, 'materials', price)

# ---- the parts of a network
def cable():
    t=P(); t.r(0,6,15,9,FLUIX); t.r(0,6,15,6,FLUIXL); t.r(0,9,15,9,dk(FLUIX)); t.r(6,0,9,15,FLUIX); t.r(6,0,6,15,FLUIXL); t.r(9,0,9,15,dk(FLUIX)); t.r(6,6,9,9,FLUIXL); t.r(7,7,8,8,WHITE); return t
def controller():
    t=body(); t.r(4,4,11,11,K); t.r(5,5,10,10,FLUIX); t.r(6,6,9,9,FLUIXL); t.r(7,7,8,8,WHITE)
    for x,y in ((2,2),(13,2),(2,13),(13,13)): t.p(x,y,CYAN)
    return t
def drive():
    t=body()
    for i,(x,y) in enumerate(((3,3),(9,3),(3,9),(9,9))): t.r(x,y,x+3,y+3,K); t.r(x,y+1,x+3,y+1,(BLUE,GREEN,CYAN,GOLD)[i]); t.p(x+3,y+3,GREEN)
    return t
def terminal():
    t=body(); t.r(3,3,12,10,K); t.r(4,4,11,9,(40,90,120,255))
    for x,y,c in ((5,5,CYAN),(7,5,GOLD),(9,5,GREEN),(5,7,WHITE),(7,7,RED),(9,7,CYAN)): t.r(x,y,x+1,y+1,c)
    t.r(4,12,11,12,SL); return t
def bus(c, arrow):
    t=P(); t.r(4,4,11,11,SD); t.r(4,4,11,4,S); t.r(4,11,11,11,SK); t.r(0,6,3,9,FLUIX); t.r(12,5,15,10,c)
    for x,y in arrow: t.p(x,y,WHITE)
    return t
def access():
    t=P(); t.r(7,6,8,14,SL); t.r(4,14,11,15,SD); t.r(6,3,9,5,FLUIX); t.r(7,4,8,4,WHITE)
    for x,y in ((3,2),(2,4),(3,6),(12,2),(13,4),(12,6)): t.p(x,y,CYAN)
    return t
def cpu(extra):
    t=body((70,60,96,255),(110,96,146,255),(44,38,62,255)); t.r(4,4,11,11,K); t.r(5,5,10,10,(60,110,90,255)); t.r(7,7,8,8,GOLD if not extra else CYAN)
    if extra: t.r(5,5,6,6,CYAN); t.r(9,9,10,10,CYAN)
    return t
def assembler():
    t=body(); t.r(3,3,12,12,(170,214,230,150)); t.r(3,3,12,3,SL); t.r(3,12,12,12,SL); t.r(3,3,3,12,SL); t.r(12,3,12,12,SL); t.r(6,6,9,9,FLUIXL); t.r(7,5,8,10,FLUIX); t.r(5,7,10,8,FLUIX); return t
def bridge():
    t=P(); t.r(1,1,14,14,K); t.r(3,3,12,12,(30,20,60,255)); t.r(5,5,10,10,FLUIX); t.r(6,6,9,9,WHITE)
    for i in range(2,14,3): t.p(i,1,CYAN); t.p(i,14,CYAN); t.p(1,i,CYAN); t.p(14,i,CYAN)
    return t
def plane(c):
    t=P(); t.r(2,5,13,10,SD); t.r(2,5,13,5,S); t.r(2,10,13,10,SK); t.r(4,7,11,8,c); t.r(0,6,1,9,FLUIX); t.r(14,6,15,9,FLUIX); return t
def inscriber():
    t=body(); t.r(3,3,12,6,SL); t.r(7,7,8,9,SL); t.r(4,10,11,10,QUARTZ); t.r(3,11,12,12,SK); t.p(12,4,GREEN); return t
IN=[(13,7),(13,8),(14,7),(14,8),(12,6),(12,9)]; OUT=[(13,7),(13,8),(12,7),(12,8),(14,6),(14,9)]; BOTH=[(12,7),(13,7),(14,7),(12,8),(13,8),(14,8)]
part('network_cable', 'Network cable', 110, cable(), network='cable', mine_time=0.1)
part('network_controller', 'Network controller', 8600, controller(), network='controller', draw=10, light=6)
part('storage_drive', 'Storage drive', 2600, drive(), network='drive', storage={'columns': 2, 'rows': 2})
part('storage_terminal', 'Storage terminal', 2400, terminal(), network='terminal', light=4)
part('import_bus', 'Import bus', 1500, bus(BLUE, IN), network='import', storage={'columns': 1, 'rows': 1})
part('export_bus', 'Export bus', 1200, bus(GOLD, OUT), network='export', storage={'columns': 1, 'rows': 1})
part('storage_bus', 'Storage bus', 1300, bus(GREEN, BOTH), network='storage_bus')
part('wireless_access_point', 'Wireless access point', 1100, access(), network='access', network_range=32)
part('crafting_cpu', 'Crafting CPU', 15500, cpu(False), network='cpu')
part('crafting_coprocessor', 'Crafting co-processor', 8500, cpu(True), network='coprocessor')
part('assembler', 'Assembler', 3900, assembler(), network='assembler')
part('quantum_bridge', 'Quantum bridge', 31000, bridge(), network='bridge', light=6)
part('annihilation_plane', 'Annihilation plane', 2100, plane(RED), network='break', storage={'columns': 1, 'rows': 1})
part('formation_plane', 'Formation plane', 1700, plane(GREEN), network='place', storage={'columns': 1, 'rows': 1})
part('inscriber', 'Inscriber', 2700, inscriber(), cat='benches', bench=True, bench_kind='inscriber', bench_level=1, electric={'per_second': 100}, floats=False)

# ---- cells, and the terminal that is carried
def cell(c):
    t=P(); t.r(3,2,12,13,SD); t.r(3,2,12,2,SL); t.r(3,13,12,13,SK); t.r(5,4,10,8,K); t.r(6,5,9,7,c); t.r(5,10,10,11,c); t.p(11,12,GREEN); return t
def component(c):
    t=P(); t.r(4,4,11,11,(40,60,56,255)); t.r(6,6,9,9,c); t.r(4,4,11,4,lt(c)); t.p(3,6,GOLD); t.p(3,9,GOLD); t.p(12,6,GOLD); t.p(12,9,GOLD); return t
prev=None
for k, c, price in ((1, BLUE, 1600), (4, GREEN, 5600), (16, GOLD, 17500), (64, RED, 59000)):
    comp='cell_component_%dk' % k; n='storage_cell_%dk' % k
    component(c).save('items', comp); item(comp, '%dk storage component' % k, 'materials', price)
    cell(c).save('items', n); item(n, '%dk storage cell' % k, 'tools', price + 240, cell=k * 1024, max_stack=1)
    if prev is None: rec('inscriber', comp, 1, [('certus_quartz', 4), ('logic_processor', 1)], 60)
    else: rec('inscriber', comp, 1, [(prev, 3), ('engineering_processor' if k == 64 else 'calculation_processor', 1)], 120)
    rec('inscriber', n, 1, [(comp, 1), ('glass', 2), ('iron_ingot', 1)], 30)
    prev=comp
t=P(); t.r(3,1,12,14,SD); t.r(3,1,12,1,SL); t.r(4,3,11,9,K); t.r(5,4,10,8,(40,90,120,255)); t.p(6,5,CYAN); t.p(8,6,GOLD); t.p(9,5,GREEN); t.r(6,11,9,12,SL); t.r(7,0,8,0,FLUIXL); t.save('items', 'wireless_terminal')
item('wireless_terminal', 'Wireless terminal', 'tools', 9800, remote=True, max_stack=1)

rec('workbench', 'inscriber', 1, [('steel_block', 1), ('copper_wire', 10), ('certus_quartz', 4)], 120, level=4)
rec('inscriber', 'fluix_crystal', 2, [('certus_quartz', 1), ('copper_ingot', 1), ('coal', 1)], 20)
rec('inscriber', 'logic_processor', 1, [('gold_ingot', 1), ('silicon_wafer', 1), ('fluix_crystal', 1)], 40)
rec('inscriber', 'calculation_processor', 1, [('certus_quartz', 1), ('silicon_wafer', 1), ('fluix_crystal', 1)], 40)
rec('inscriber', 'engineering_processor', 1, [('diamond', 1), ('silicon_wafer', 1), ('fluix_crystal', 1)], 40)
rec('inscriber', 'network_cable', 8, [('fluix_crystal', 2), ('glass', 2)], 20)
rec('inscriber', 'network_controller', 1, [('steel_block', 1), ('fluix_crystal', 4), ('engineering_processor', 1)], 120)
rec('inscriber', 'storage_drive', 1, [('steel_block', 1), ('calculation_processor', 2), ('network_cable', 2)], 60)
rec('inscriber', 'storage_terminal', 1, [('glass', 2), ('logic_processor', 2), ('network_cable', 1)], 60)
rec('inscriber', 'import_bus', 1, [('iron_ingot', 2), ('logic_processor', 1), ('network_cable', 1)], 40)
rec('inscriber', 'export_bus', 1, [('iron_ingot', 2), ('calculation_processor', 1), ('network_cable', 1)], 40)
rec('inscriber', 'storage_bus', 1, [('chest', 1), ('logic_processor', 1), ('network_cable', 1)], 40)
rec('inscriber', 'wireless_access_point', 1, [('fluix_crystal', 2), ('calculation_processor', 1), ('network_cable', 1)], 60)
rec('inscriber', 'wireless_terminal', 1, [('storage_terminal', 1), ('wireless_access_point', 1), ('engineering_processor', 1)], 120)
rec('inscriber', 'crafting_cpu', 1, [('steel_block', 1), ('engineering_processor', 2), ('logic_processor', 2), ('network_cable', 1)], 120)
rec('inscriber', 'crafting_coprocessor', 1, [('steel_block', 1), ('engineering_processor', 1), ('calculation_processor', 2)], 120)
rec('inscriber', 'assembler', 1, [('steel_block', 1), ('glass', 4), ('logic_processor', 2), ('network_cable', 1)], 60)
rec('inscriber', 'quantum_bridge', 1, [('steel_block', 1), ('fluix_crystal', 8), ('engineering_processor', 4), ('diamond', 1)], 300)
rec('inscriber', 'annihilation_plane', 1, [('iron_ingot', 3), ('fluix_crystal', 4), ('logic_processor', 1)], 60)
rec('inscriber', 'formation_plane', 1, [('iron_ingot', 3), ('fluix_crystal', 4), ('calculation_processor', 1)], 60)

def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        if isinstance(obj, dict) and len(obj) == 1 and isinstance(list(obj.values())[0], list):
            k, v = list(obj.items())[0]
            f.write('{\n  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]\n}\n')
        else: json.dump(obj, f, indent=2); f.write('\n')
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes}); dump('worldgen.json', {'veins': [vein]})
dump('mod.json', {'label': 'Digital Storage', 'icon': 'icon.png'})
t=controller(); t.im.resize((64, 64), Image.NEAREST).save(os.path.join(mod, 'icon.png'))
print(len(tiles), 'tiles', len(items), 'items', len(recipes), 'recipes')
