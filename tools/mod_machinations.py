#!/usr/bin/env python3
"""Writes the Machinations of Madness mod: its data files and its textures.

    python3 tools/mod_machinations.py .

Signals and the machinery that goes by them - spark dust, torches, repeaters, levers, buttons, plates, gates,
clocks and latches; pistons and sticky pistons, resin blocks, bores that dig, deployers that build, spouts,
drains and blasting charges, hatches, lamps and moving walkways - and steam: boilers, pipes and tanks, belts,
arms, hoppers and machines that make things by themselves. One more level of the workbench, and two benches.

Anything with a direction is drawn once, facing right, and turned for the other three.

Either edit this and run it again, or edit the mod's files by hand and stop using this: not both.
"""
import json, os, random, sys
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
mod = os.path.join(repo, 'mods/machinations'); tex = os.path.join(mod, 'textures')
for d in ('tiles', 'items'): os.makedirs(os.path.join(tex, d), exist_ok=True)

INK = (22, 22, 30, 255)
def c(s): return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
def dk(col, f=0.75): return tuple(int(v * f) for v in col[:3]) + (255,)
def lt(col, f=1.25): return tuple(min(255, int(v * f)) for v in col[:3]) + (255,)
BRASS, COPPER, IRON, DARK, WOOD, SPARK, STEAM, RESIN = c('c8a040'), c('c06a38'), c('8a8e9a'), c('3a3c46'), c('8c6537'), c('e83828'), c('e8eef4'), c('7ac850')

class P:
    def __init__(s, fill=(0, 0, 0, 0)): s.im = Image.new('RGBA', (16, 16), fill); s.px = s.im.load()
    def r(s, x0, y0, x1, y1, col):
        for y in range(max(0, y0), min(15, y1) + 1):
            for x in range(max(0, x0), min(15, x1) + 1): s.px[x, y] = col
        return s
    def p(s, x, y, col): return s.r(x, y, x, y, col)
    def box(s, x0, y0, x1, y1, col):
        """A plate of metal: lit along the top and the left, dark along the bottom and the right, a rivet in each corner."""
        s.r(x0, y0, x1, y1, col); s.r(x0, y0, x1, y0, lt(col)); s.r(x0, y0, x0, y1, lt(col)); s.r(x0, y1, x1, y1, dk(col)); s.r(x1, y0, x1, y1, dk(col))
        for x, y in ((x0 + 1, y0 + 1), (x1 - 1, y0 + 1), (x0 + 1, y1 - 1), (x1 - 1, y1 - 1)): s.p(x, y, dk(col, 0.6))
        return s
    def arrow(s, col):
        """Pointing right, in the middle."""
        s.r(4, 7, 10, 8, col); s.p(9, 5, col); s.p(10, 6, col); s.p(11, 7, col); s.p(11, 8, col); s.p(10, 9, col); s.p(9, 10, col); return s
    def outline(s):
        q = s.im.copy().load()
        for y in range(16):
            for x in range(16):
                if q[x, y][3]: continue
                if any(0 <= x + a < 16 and 0 <= y + b < 16 and q[x + a, y + b][3] for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))): s.px[x, y] = INK
        return s
    def turned(s, quarter):
        t = P(); t.im = s.im.rotate(90 * quarter); t.px = t.im.load(); return t
    def flipped(s):
        t = P(); t.im = s.im.transpose(Image.FLIP_LEFT_RIGHT); t.px = t.im.load(); return t
    def save(s, kind, name):
        k = 3 if kind == 'items' else 2
        s.im.resize((16 * k, 16 * k), Image.NEAREST).save(os.path.join(tex, kind, name + '.png')); return s

def icon_of(name, tile_name=None, block=False):
    im = Image.open(os.path.join(tex, 'tiles', (tile_name or name) + '.png')).convert('RGBA')
    if block:
        out = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); out.paste(im, (8, 8), im); out.save(os.path.join(tex, 'items', name + '.png'))
    else:
        t = P(); t.im = im.resize((16, 16), Image.NEAREST); t.px = t.im.load(); t.outline().save('items', name)

tiles, items, recipes, veins = [], [], [], []
def tile(name, label, solid=True, **kw):
    d = {'name': name, 'label': label, 'solid': solid, 'removable': True, 'mine_time': 0.4, 'drop': name, 'texture': 'tiles/%s.png' % name, 'tool': 'pickaxe'}
    if not solid: d['layer'] = 'background'
    d.update(kw); tiles.append({k: v for k, v in d.items() if v is not None}); return d
def item(name, label, cat, price, tier=4, **kw):
    d = {'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'category': cat, 'price': price}
    if cat not in ('benches', 'special'): d['tier'] = tier
    d.update(kw); items.append(d); return d
def rec(bench, out, count, ins, time=60, level=None, at=None):
    r = {'bench': bench, 'output': {'item': out, 'count': count}, 'inputs': [{'item': i, 'count': n} for i, n in ins], 'time': time}
    if level: r['level'] = level
    if at: r['at'] = at
    recipes.append(r)
G, T = 'gearworks', 'tinkers_bench'
DIRS = [('right', [1, 0], 0), ('up', [0, -1], 1), ('left', [-1, 0], 2), ('down', [0, 1], 3)]

# ---- materials -------------------------------------------------------------------------------------------------
def simple(name, draw): t = draw(); t.outline().save('items', name)
def ingot(col):
    def go():
        t = P(); t.r(2, 8, 13, 12, col); t.r(4, 6, 11, 7, lt(col)); t.r(2, 12, 13, 12, dk(col)); t.r(3, 8, 12, 8, lt(col, 1.15)); return t
    return go
def plate(col):
    def go(): return P().box(2, 4, 13, 11, col)
    return go
def gear(col):
    def go():
        t = P(); t.r(5, 3, 10, 12, col); t.r(3, 5, 12, 10, col)
        for x, y in ((7, 1), (8, 1), (7, 14), (8, 14), (1, 7), (1, 8), (14, 7), (14, 8), (3, 3), (12, 3), (3, 12), (12, 12)): t.p(x, y, col)
        t.r(6, 6, 9, 9, dk(col, 0.5)); t.r(5, 3, 10, 3, lt(col)); t.r(3, 5, 3, 10, lt(col)); return t
    return go
def spring():
    t = P()
    for y in (3, 6, 9, 12): t.r(4, y, 11, y, IRON); t.p(4, y + 1, dk(IRON)); t.p(11, y - 1 if y > 3 else y, dk(IRON))
    t.r(4, 3, 11, 3, lt(IRON)); return t
def rod():
    t = P()
    for i in range(3, 13): t.p(i, 15 - i, IRON); t.p(i + 1, 15 - i, dk(IRON))
    t.r(11, 2, 13, 4, BRASS); t.r(2, 11, 4, 13, BRASS); return t
def valve():
    t = P(); t.r(6, 7, 9, 13, COPPER); t.r(3, 9, 12, 11, COPPER); t.r(4, 3, 11, 4, c('a82020')); t.r(7, 5, 8, 6, DARK); t.p(3, 4, c('a82020')); t.p(12, 4, c('a82020')); t.r(3, 9, 12, 9, lt(COPPER)); return t
def dust(col):
    def go():
        t = P(); rnd = random.Random(str(col))
        for _ in range(26):
            x, y = rnd.randint(3, 12), rnd.randint(6, 12)
            if abs(x - 7.5) + abs(y - 10) * 1.6 < 7: t.p(x, y, rnd.choice([col, lt(col, 1.3), dk(col)]))
        return t
    return go
def blob(col):
    def go():
        t = P(); t.r(4, 6, 11, 12, col); t.r(3, 8, 12, 11, col); t.r(5, 5, 10, 5, lt(col)); t.p(6, 7, lt(col, 1.5)); t.r(4, 12, 11, 12, dk(col)); return t
    return go
MATS = [('brass_ingot', 'Brass ingot', 'ores', 420, ingot(BRASS)), ('brass_plate', 'Brass plate', 'crafted', 900, plate(BRASS)), ('brass_gear', 'Brass gear', 'crafted', 700, gear(BRASS)),
        ('iron_gear', 'Iron gear', 'crafted', 500, gear(IRON)), ('spring', 'Spring', 'crafted', 400, spring), ('piston_rod', 'Piston rod', 'crafted', 650, rod),
        ('steam_valve', 'Steam valve', 'crafted', 800, valve), ('spark_dust', 'Spark dust', 'ores', 120, dust(SPARK)), ('sticky_resin', 'Sticky resin', 'crafted', 200, blob(RESIN))]
for name, label, cat, price, draw in MATS:
    simple(name, draw)
    item(name, label, cat, price, **({'places': 'spark_dust'} if name == 'spark_dust' else {}))
rec('furnace', 'brass_ingot', 2, [('copper_ingot', 2), ('tin_ingot', 1), ('coal', 1)], 120, level=2)
rec('press', 'brass_plate', 1, [('brass_ingot', 2)], 80); rec('metalwork_bench', 'brass_gear', 2, [('brass_ingot', 1)], 80); rec('metalwork_bench', 'iron_gear', 2, [('iron_ingot', 1)], 80)
rec('metalwork_bench', 'spring', 2, [('iron_ingot', 1)], 80); rec('metalwork_bench', 'piston_rod', 1, [('iron_ingot', 1), ('brass_ingot', 1)], 80)
rec('metalwork_bench', 'steam_valve', 1, [('copper_ingot', 2), ('brass_gear', 1)], 80)
rec('furnace', 'spark_dust', 4, [('sparkstone_ore', 1)], 60, level=2); rec('mixing_bench', 'spark_dust', 2, [('coal', 2), ('copper_ingot', 1), ('flint', 1)], 120)
rec('mixing_bench', 'sticky_resin', 2, [('kelp', 2), ('flour', 1)], 80); rec('mixing_bench', 'sticky_resin', 4, [('raw_resin', 1)], 40)

# ---- the benches -----------------------------------------------------------------------------------------------
def bench_pic(name, top, body):
    t = P(); t.box(1, 6, 14, 15, body); t.r(0, 4, 15, 6, top); t.r(0, 4, 15, 4, lt(top)); t.r(3, 9, 6, 12, dk(body, 0.5)); t.r(4, 10, 5, 11, BRASS); t.r(9, 9, 12, 12, IRON); t.p(10, 10, lt(IRON)); t.r(11, 1, 12, 3, DARK); t.p(11, 0, STEAM)
    t.save('tiles', name); icon_of(name)
bench_pic('steam_workbench', BRASS, WOOD); bench_pic('gearworks', IRON, c('5a4a3a')); bench_pic('tinkers_bench', COPPER, c('4a3a5a'))
tile('steam_workbench', 'Steam workbench', False, bench=True, bench_kind='workbench', extends={'cost': [{'item': 'brass_plate', 'count': 4}, {'item': 'brass_gear', 'count': 4}]})
item('steam_workbench', 'Steam workbench', 'benches', 6000, places='steam_workbench')
for name, label in (('gearworks', 'Gearworks'), ('tinkers_bench', "Tinker's bench")):
    tile(name, label, False, bench=True, bench_kind=name, bench_level=1); item(name, label, 'benches', 3000, places=name)
rec('workbench', 'gearworks', 1, [('iron_ingot', 4), ('wood', 6), ('brass_gear', 2)], level=3); rec('workbench', 'tinkers_bench', 1, [('copper_ingot', 4), ('wood', 6), ('spark_dust', 4)], level=3)

# ---- signals: what makes them, carries them and reads them ------------------------------------------------------
def wire_pic():
    t = P()
    for x, y in ((7, 7), (8, 8), (7, 8), (8, 7), (3, 7), (5, 8), (10, 7), (12, 8), (7, 3), (8, 5), (7, 10), (8, 12), (1, 8), (14, 7), (7, 1), (8, 14)): t.p(x, y, SPARK)
    for x, y in ((6, 7), (9, 8), (7, 6), (8, 9)): t.p(x, y, dk(SPARK))
    t.save('tiles', 'spark_dust')
wire_pic()
tile('spark_dust', 'Spark dust', False, mine_time=0.05, attaches=True, floats=True, model='panel', signal={'wire': True})
def torch_pic(lit):
    t = P(); t.r(7, 7, 8, 14, WOOD); t.r(6, 4, 9, 7, SPARK if lit else dk(SPARK, 0.4)); t.p(7, 3, lt(SPARK, 1.3) if lit else dk(SPARK, 0.5)); t.p(8, 5, lt(SPARK, 1.4) if lit else dk(SPARK, 0.5)); return t
torch_pic(True).save('tiles', 'spark_torch'); icon_of('spark_torch')
tile('spark_torch', 'Spark torch', False, mine_time=0.1, attaches=True, floats=True, light=7, model='panel', signal={'in': [[0, 1]], 'invert': True})
item('spark_torch', 'Spark torch', 'energy', 260, places='spark_torch'); rec(T, 'spark_torch', 2, [('spark_dust', 1), ('stick', 1)])
t = P().box(0, 0, 15, 15, SPARK); t.r(4, 4, 11, 11, lt(SPARK, 1.3)); t.r(6, 6, 9, 9, c('ffd0a0')); t.save('tiles', 'spark_block'); icon_of('spark_block', block=True)
tile('spark_block', 'Spark block', light=9, signal={'out': 15}); item('spark_block', 'Spark block', 'energy', 1200, places='spark_block')
rec(T, 'spark_block', 1, [('spark_dust', 9)]); rec(T, 'spark_dust', 9, [('spark_block', 1)], 20)

def slab(col):
    """Something flat set on a wall: a plate with room on it for what it is."""
    return P().box(1, 4, 14, 11, col)
def facing(name, label, base, sig, price, ins, extra=None):
    """A thing that reads behind it and tells in front: one facing right, one left, and a click turns it."""
    for side, flip in (('right', False), ('left', True)):
        (base.flipped() if flip else base).save('tiles', '%s_%s' % (name, side))
        s = json.loads(json.dumps(sig)); k = -1 if flip else 1
        s['in'] = [[-k if a == 'back' else 0, 1 if a == 'below' else 0] for a in s['in']]; s['toward'] = [k, 0]
        tile('%s_%s' % (name, side), label, False, mine_time=0.15, attaches=True, floats=True, model='panel', drop=name, signal=s,
             use_becomes='%s_%s' % (name, 'left' if side == 'right' else 'right'), **(extra or {}))
    icon_of(name, name + '_right'); item(name, label, 'energy', price, places=name + '_right'); rec(T, name, 1, ins)
rp = slab(IRON); rp.r(3, 7, 5, 8, dk(SPARK, 0.6)); rp.arrow(SPARK); rp.r(12, 6, 13, 9, SPARK)
facing('repeater', 'Repeater', rp, {'in': ['back'], 'delay': 2}, 420, [('spark_dust', 2), ('spark_torch', 1), ('stone', 2)])
dl = slab(COPPER); dl.arrow(c('f0d060')); dl.r(2, 6, 3, 9, c('f0d060')); dl.r(12, 6, 13, 9, SPARK)
facing('delayer', 'Delayer', dl, {'in': ['back'], 'delay': 20}, 600, [('repeater', 1), ('spring', 1), ('brass_gear', 1)])
ag = slab(BRASS); ag.r(4, 6, 8, 9, DARK); ag.r(9, 7, 11, 8, DARK); ag.p(8, 5, DARK); ag.p(8, 10, DARK); ag.r(12, 6, 13, 9, SPARK); ag.r(7, 11, 8, 11, SPARK)
facing('and_gate', 'And gate', ag, {'in': ['back', 'below'], 'and': True}, 700, [('repeater', 2), ('brass_plate', 1)])

def pair_pic(name, off, on):
    off.save('tiles', name + '_off' if name in ('lever', 'latch', 'signal_lamp') else name); on.save('tiles', name + '_on' if name in ('lever', 'latch', 'signal_lamp') else name + '_down')
lv0 = P(); lv0.r(4, 11, 11, 13, IRON); lv0.r(4, 11, 11, 11, lt(IRON))
for i in range(6): lv0.p(7 - i // 2 - 1, 10 - i, WOOD)
lv0.r(3, 3, 5, 5, c('a82020')); lv1 = lv0.flipped(); lv1.r(10, 3, 12, 5, SPARK)
pair_pic('lever', lv0, lv1); icon_of('lever', 'lever_off')
tile('lever_off', 'Lever', False, mine_time=0.15, attaches=True, floats=True, model='panel', drop='lever', use_becomes='lever_on')
tile('lever_on', 'Lever', False, mine_time=0.15, attaches=True, floats=True, model='panel', drop='lever', use_becomes='lever_off', signal={'out': 15})
item('lever', 'Lever', 'energy', 220, places='lever_off'); rec(T, 'lever', 1, [('stick', 1), ('iron_ingot', 1), ('spark_dust', 1)])
b0 = P(); b0.r(4, 5, 11, 12, IRON); b0.r(5, 6, 10, 11, c('a82020')); b0.r(5, 6, 10, 6, c('d84040')); b1 = P(); b1.r(4, 5, 11, 12, IRON); b1.r(6, 7, 9, 10, SPARK)
b0.save('tiles', 'spark_button'); b1.save('tiles', 'spark_button_pressed'); icon_of('spark_button')
tile('spark_button', 'Button', False, mine_time=0.15, attaches=True, floats=True, model='panel', use_becomes='spark_button_pressed')
tile('spark_button_pressed', 'Button', False, mine_time=0.15, attaches=True, floats=True, model='panel', drop='spark_button', signal={'out': 15}, reverts={'to': 'spark_button', 'ticks': 20})
item('spark_button', 'Button', 'energy', 180, places='spark_button'); rec(T, 'spark_button', 2, [('stone', 1), ('spark_dust', 1)])
def plate_pic(col, down):
    t = P(); y = 14 if down else 12; t.r(1, y, 14, 15, col); t.r(1, y, 14, y, lt(col)); t.r(3, 15, 12, 15, dk(col, 0.6)); return t
for name, label, col, by_items, ins in (('pressure_plate', 'Pressure plate', c('a8a8b0'), False, [('stone', 2), ('spring', 1), ('spark_dust', 1)]),
                                        ('item_plate', 'Weighing plate', BRASS, True, [('brass_plate', 1), ('spring', 1), ('spark_dust', 1)])):
    plate_pic(col, False).save('tiles', name); plate_pic(col, True).save('tiles', name + '_down'); icon_of(name)
    tile(name, label, False, mine_time=0.15, on_floor=True, model='panel', stood_becomes=name + '_down', stood_by_items=by_items or None)
    tile(name + '_down', label, False, mine_time=0.15, on_floor=True, model='panel', drop=name, clear_becomes=name, stood_by_items=by_items or None, signal={'out': 15})
    item(name, label, 'energy', 300, places=name); rec(T, name, 1, ins)
l0 = slab(c('5a4a3a')); l0.r(5, 6, 10, 9, DARK); l1 = slab(c('5a4a3a')); l1.r(5, 6, 10, 9, SPARK); l1.r(6, 7, 9, 8, c('ffd0a0'))
l0.save('tiles', 'latch_off'); l1.save('tiles', 'latch_on'); icon_of('latch', 'latch_off')
tile('latch_off', 'Latch', False, mine_time=0.15, attaches=True, floats=True, model='panel', drop='latch', pulse_becomes='latch_on')
tile('latch_on', 'Latch', False, mine_time=0.15, attaches=True, floats=True, model='panel', drop='latch', pulse_becomes='latch_off', light=5)
item('latch', 'Latch', 'energy', 800, places='latch_off'); rec(T, 'latch', 1, [('repeater', 2), ('spark_torch', 2), ('brass_plate', 1)])
# (A latch is read, not made: the thing that tells the rest of the world about it is a block of its own beside it.)
def clock_pic(col):
    t = P(); t.r(3, 3, 12, 12, BRASS); t.r(4, 4, 11, 11, c('f0ece0')); t.r(7, 5, 7, 8, DARK); t.r(8, 8, 10, 8, col); t.r(3, 3, 12, 3, lt(BRASS)); t.r(3, 12, 12, 12, dk(BRASS)); return t
for name, label, period, col in (('slow_clock', 'Slow clock', 40, DARK), ('quick_clock', 'Quick clock', 10, SPARK)):
    clock_pic(col).save('tiles', name); icon_of(name)
    tile(name, label, False, mine_time=0.15, attaches=True, floats=True, model='panel', signal={'clock': period})
    item(name, label, 'energy', 900, places=name); rec(T, name, 1, [('brass_gear', 2), ('spring', 1), ('spark_torch', 1)])
ds = slab(DARK); ds.r(3, 6, 12, 9, c('3a62b8')); ds.r(6, 6, 9, 9, c('f0d060')); ds.save('tiles', 'daylight_sensor'); icon_of('daylight_sensor')
tile('daylight_sensor', 'Daylight sensor', False, mine_time=0.15, attaches=True, floats=True, model='panel', signal={'daylight': True})
item('daylight_sensor', 'Daylight sensor', 'energy', 900, places='daylight_sensor'); rec(T, 'daylight_sensor', 1, [('glass', 2), ('spark_dust', 2), ('silicon_wafer', 1)])

# ---- what a signal moves ---------------------------------------------------------------------------------------
lamp0 = P().box(0, 0, 15, 15, c('5a4a3a')); lamp0.r(3, 3, 12, 12, c('6a5a30')); lamp1 = P().box(0, 0, 15, 15, c('5a4a3a')); lamp1.r(3, 3, 12, 12, c('ffe080')); lamp1.r(5, 5, 10, 10, c('fff8d0'))
lamp0.save('tiles', 'signal_lamp_off'); lamp1.save('tiles', 'signal_lamp_on'); icon_of('signal_lamp', 'signal_lamp_off', block=True)
tile('signal_lamp_off', 'Signal lamp', drop='signal_lamp', powered_becomes='signal_lamp_on')
tile('signal_lamp_on', 'Signal lamp', drop='signal_lamp', unpowered_becomes='signal_lamp_off', light=15)
item('signal_lamp', 'Signal lamp', 'energy', 500, places='signal_lamp_off'); rec(T, 'signal_lamp', 1, [('glass', 2), ('spark_dust', 2), ('torch', 1)])
h0 = P().box(0, 0, 15, 15, IRON); h0.r(0, 7, 15, 8, dk(IRON, 0.5)); h0.r(6, 6, 9, 9, BRASS)
h1 = P(); h1.r(0, 0, 15, 2, IRON); h1.r(0, 13, 15, 15, IRON); h1.r(0, 0, 15, 0, lt(IRON)); h1.r(0, 15, 15, 15, dk(IRON))
h0.save('tiles', 'hatch_shut'); h1.save('tiles', 'hatch_open'); icon_of('signal_hatch', 'hatch_shut', block=True)
tile('hatch_shut', 'Signal hatch', drop='signal_hatch', wall=False, powered_becomes='hatch_open')
tile('hatch_open', 'Signal hatch', False, layer='foreground', drop='signal_hatch', unpowered_becomes='hatch_shut')
item('signal_hatch', 'Signal hatch', 'energy', 900, places='hatch_shut'); rec(G, 'signal_hatch', 1, [('iron_ingot', 2), ('piston_rod', 1), ('spark_dust', 1)])

def piston_pics(name, face, sticky):
    body = P().box(0, 0, 11, 15, c('7a7e8a')); body.r(12, 0, 15, 15, face); body.r(12, 0, 15, 0, lt(face)); body.r(12, 15, 15, 15, dk(face)); body.r(11, 0, 11, 15, DARK); body.r(3, 6, 8, 9, BRASS)
    if sticky: body.r(14, 2, 15, 13, RESIN)
    out = P().box(0, 0, 11, 15, c('7a7e8a')); out.r(3, 6, 8, 9, BRASS); out.r(12, 6, 15, 9, IRON); out.r(11, 0, 11, 15, DARK)
    return body, out
head = P(); head.r(0, 6, 11, 9, IRON); head.r(0, 6, 11, 6, lt(IRON)); head.r(12, 0, 15, 15, WOOD); head.r(12, 0, 15, 0, lt(WOOD)); head.r(12, 15, 15, 15, dk(WOOD))
for kind, label, face, sticky in (('piston', 'Piston', WOOD, False), ('sticky_piston', 'Sticky piston', WOOD, True)):
    body, out = piston_pics(kind, face, sticky)
    for i, (side, d, quarter) in enumerate(DIRS):
        nxt = DIRS[(i + 1) % 4][0]
        body.turned(quarter).save('tiles', '%s_%s' % (kind, side)); out.turned(quarter).save('tiles', '%s_%s_out' % (kind, side))
        (head if not sticky else head).turned(quarter).save('tiles', 'piston_head_%s' % side)
        tile('%s_%s' % (kind, side), label, drop=kind, wall=False, piston={'dir': d, 'head': 'piston_head_' + side, 'sticky': sticky}, powered_becomes='%s_%s_out' % (kind, side),
             use_becomes='%s_%s' % (kind, nxt))
        tile('%s_%s_out' % (kind, side), label, drop=kind, wall=False, immovable=True,
             piston={'dir': d, 'head': 'piston_head_' + side, 'sticky': sticky, 'out': True}, unpowered_becomes='%s_%s' % (kind, side))
    icon_of(kind, kind + '_right', block=True); item(kind, label, 'energy', 1400 if not sticky else 1800, places=kind + '_right')
for side, d, quarter in DIRS: tile('piston_head_' + side, 'Piston head', removable=False, drop=None, immovable=True, wall=False)
rec(G, 'piston', 1, [('wood', 3), ('stone', 4), ('piston_rod', 1), ('spark_dust', 1)]); rec(G, 'sticky_piston', 1, [('piston', 1), ('sticky_resin', 1)])
rb = P().box(0, 0, 15, 15, RESIN); rb.r(3, 3, 12, 12, lt(RESIN, 1.2)); rb.r(5, 5, 7, 7, c('d0ffb0')); rb.save('tiles', 'resin_block'); icon_of('resin_block', block=True)
tile('resin_block', 'Resin block', sticky=True, wall=False, see_through=True); item('resin_block', 'Resin block', 'crafted', 1800, places='resin_block'); rec(G, 'resin_block', 1, [('sticky_resin', 9)])

def turning(name, label, pic, extra, price, ins, cat='energy', storage=None):
    """A machine with a front: four blocks, one for each way it can face; a click turns one that holds nothing."""
    for i, (side, d, quarter) in enumerate(DIRS):
        pic.turned(quarter).save('tiles', '%s_%s' % (name, side))
        e = json.loads(json.dumps(extra).replace('"DIR"', json.dumps(d)))
        tile('%s_%s' % (name, side), label, drop=name if not storage else '%s_%s' % (name, side), wall=False, storage=storage,
             use_becomes=None if storage else '%s_%s' % (name, DIRS[(i + 1) % 4][0]), **e)
        if storage:
            icon_of('%s_%s' % (name, side), block=True)
            item('%s_%s' % (name, side), '%s (%s)' % (label, side), cat, price, places='%s_%s' % (name, side), container={'slots': storage['columns'] * storage['rows']})
            if side != 'right': rec(G, '%s_%s' % (name, side), 1, [('%s_right' % name, 1)], 5); rec(G, '%s_right' % name, 1, [('%s_%s' % (name, side), 1)], 5)
    if not storage:
        icon_of(name, name + '_right', block=True); item(name, label, cat, price, places=name + '_right'); rec(G, name, 1, ins)
    else:
        rec(G, name + '_right', 1, ins)
bore = P().box(0, 0, 10, 15, c('5a5e6a')); bore.r(11, 2, 13, 13, IRON); bore.r(14, 5, 15, 10, lt(IRON)); bore.r(11, 7, 15, 8, DARK); bore.r(3, 6, 7, 9, BRASS)
turning('bore', 'Bore', bore, {'breaks': 'DIR'}, 2600, [('iron_ingot', 4), ('piston_rod', 1), ('iron_gear', 2), ('spark_dust', 1)])
dep = P().box(0, 0, 15, 15, BRASS); dep.r(11, 3, 15, 12, DARK); dep.r(12, 5, 14, 10, c('1a1a20')); dep.r(3, 5, 8, 10, WOOD)
turning('deployer', 'Deployer', dep, {'deploys': 'DIR'}, 3200, [('brass_plate', 3), ('piston_rod', 1), ('chest', 1), ('spark_dust', 1)], storage={'columns': 2, 'rows': 1})
sp = P().box(0, 0, 12, 15, COPPER); sp.r(13, 5, 15, 10, c('3a86d8')); sp.r(4, 5, 9, 10, c('2a5aa8')); sp.r(5, 6, 8, 9, c('60a8f0'))
turning('spout', 'Spout', sp, {'spout': {'dir': 'DIR', 'amount': 4}}, 2400, [('copper_ingot', 4), ('steam_valve', 1), ('bucket', 1), ('spark_dust', 1)])
dr = P().box(0, 0, 12, 15, IRON); dr.r(13, 5, 15, 10, DARK); dr.r(4, 5, 9, 10, DARK); dr.r(5, 6, 8, 9, c('1a2a40'))
turning('drain', 'Drain', dr, {'spout': {'dir': 'DIR', 'amount': -8}}, 2400, [('iron_ingot', 4), ('steam_valve', 1), ('bucket', 1), ('spark_dust', 1)])
for name, label, radius, col, ins in (('blasting_charge', 'Blasting charge', 3, c('a82020'), [('coal', 4), ('spark_dust', 2), ('linen', 1)]),
                                      ('great_charge', 'Great charge', 6, c('5a1a5a'), [('blasting_charge', 4), ('spark_block', 1)])):
    t = P().box(1, 3, 14, 15, col); t.r(1, 7, 14, 9, c('e8dcc0')); t.r(7, 0, 8, 3, DARK); t.p(8, 0, c('f0d060')); t.save('tiles', name); icon_of(name, block=True)
    tile(name, label, blast=radius, wall=False, burns=True); item(name, label, 'energy', 600 * radius, places=name); rec(T, name, 1, ins)
wk = P().box(0, 0, 15, 15, c('4a4e5a')); wk.r(0, 0, 15, 3, DARK)
for x in range(1, 16, 5): wk.p(x, 1, BRASS); wk.p(x + 1, 2, BRASS); wk.p(x + 1, 1, BRASS); wk.p(x + 2, 1, lt(BRASS))
wk.r(3, 8, 6, 11, IRON); wk.r(9, 8, 12, 11, IRON)
for name, label, ticks, price, ins in (('walkway', 'Moving walkway', 4, 500, [('iron_ingot', 1), ('iron_gear', 1), ('leather', 1)]), ('swift_walkway', 'Swift walkway', 2, 1200, [('walkway', 2), ('brass_gear', 1)])):
    for side, k, pic in (('right', 1, wk), ('left', -1, wk.flipped())):
        pic.save('tiles', '%s_%s' % (name, side))
        tile('%s_%s' % (name, side), label, drop=name, wall=False, walkway={'dir': k, 'ticks': ticks}, use_becomes='%s_%s' % (name, 'left' if side == 'right' else 'right'))
    icon_of(name, name + '_right', block=True); item(name, label, 'energy', price, places=name + '_right'); rec(G, name, 4 if name == 'walkway' else 2, ins)

# ---- steam, and what it drives ---------------------------------------------------------------------------------
def machine(name, label, body, mark, price, ins, **kw):
    t = P().box(0, 2, 15, 15, body); mark(t); t.r(2, 0, 4, 2, DARK); t.p(3, 0, STEAM); t.save('tiles', name); icon_of(name)
    tile(name, label, False, popup_name=label.lower(), **kw); item(name, label, 'benches' if kw.get('bench') else 'energy', price, places=name,
                                                                 **({'container': {'slots': kw['storage']['columns'] * kw['storage']['rows']}} if kw.get('storage') else {}))
    rec('workbench', name, 1, ins, at='steam_workbench')
machine('boiler', 'Boiler', COPPER, lambda t: (t.r(4, 9, 11, 13, DARK), t.r(5, 10, 10, 12, c('f08030')), t.r(4, 4, 11, 6, lt(COPPER))), 3000,
        [('copper_ingot', 6), ('steam_valve', 2), ('bucket', 1)], bench=True, bench_kind='boiler', bench_level=1, generator={'power': 30, 'needs': 'fuel'}, fuel_capacity=6000)
machine('windmill', 'Windmill', WOOD, lambda t: (t.r(7, 3, 8, 13, IRON), t.r(2, 7, 13, 8, c('e8dcc0')), t.r(7, 2, 8, 2, c('e8dcc0'))), 2600,
        [('wood', 8), ('linen', 4), ('iron_gear', 2)], bench=True, bench_kind='windmill', bench_level=1, generator={'power': 12, 'needs': 'sky'})
machine('pressure_tank', 'Pressure tank', BRASS, lambda t: (t.r(3, 5, 12, 12, dk(BRASS, 0.6)), t.r(6, 7, 9, 10, c('f0ece0')), t.p(8, 8, c('a82020'))), 3600,
        [('brass_plate', 6), ('steam_valve', 2)], bench=True, bench_kind='pressure_tank', bench_level=1, battery=40000)
pp = P(); pp.r(0, 6, 15, 9, COPPER); pp.r(0, 6, 15, 6, lt(COPPER)); pp.r(0, 9, 15, 9, dk(COPPER)); pp.r(6, 5, 9, 10, BRASS); pp.save('tiles', 'steam_pipe'); icon_of('steam_pipe')
tile('steam_pipe', 'Steam pipe', False, mine_time=0.1, wire=True, floats=True, model='panel'); item('steam_pipe', 'Steam pipe', 'energy', 60, places='steam_pipe'); rec(G, 'steam_pipe', 8, [('copper_ingot', 1)])
def belt_pic(col, stopped=False):
    t = P(); t.r(0, 9, 15, 13, DARK); t.r(0, 9, 15, 9, col); t.r(0, 13, 15, 13, dk(col))
    for x in range(1, 16, 4): t.r(x, 10, x + 1, 12, col if not stopped else dk(col, 0.5))
    t.r(1, 14, 2, 15, IRON); t.r(13, 14, 14, 15, IRON); return t
for name, label, speed, col, ins in (('belt', 'Belt', 1, c('8a6a3a'), [('leather', 1), ('iron_gear', 1)]), ('swift_belt', 'Swift belt', 3, BRASS, [('belt', 2), ('brass_gear', 1)])):
    belt_pic(col).save('tiles', name); icon_of(name)
    tile(name, label, False, mine_time=0.1, floats=True, conveyor=speed); item(name, label, 'energy', 160 * speed, places=name); rec(G, name, 4, ins)
belt_pic(SPARK).save('tiles', 'gate_belt_open'); belt_pic(SPARK, True).save('tiles', 'gate_belt_shut'); icon_of('gate_belt', 'gate_belt_shut')
tile('gate_belt_shut', 'Gate belt', False, mine_time=0.1, floats=True, drop='gate_belt', powered_becomes='gate_belt_open')
tile('gate_belt_open', 'Gate belt', False, mine_time=0.1, floats=True, drop='gate_belt', conveyor=1, unpowered_becomes='gate_belt_shut')
item('gate_belt', 'Gate belt', 'energy', 500, places='gate_belt_shut'); rec(G, 'gate_belt', 2, [('belt', 2), ('spark_dust', 1)])
arm = P(); arm.r(6, 10, 9, 15, IRON); arm.r(7, 4, 8, 10, BRASS); arm.r(4, 2, 11, 4, BRASS); arm.r(3, 4, 4, 6, IRON); arm.r(11, 4, 12, 6, IRON); arm.save('tiles', 'mechanical_arm'); icon_of('mechanical_arm')
tile('mechanical_arm', 'Mechanical arm', False, mine_time=0.2, floats=True, extractor=True, storage={'columns': 1, 'rows': 1})
item('mechanical_arm', 'Mechanical arm', 'energy', 900, places='mechanical_arm', container={'slots': 1}); rec(G, 'mechanical_arm', 1, [('brass_gear', 2), ('piston_rod', 1), ('iron_ingot', 1)])
hp = P(); hp.r(1, 2, 14, 4, BRASS); hp.r(2, 5, 13, 8, dk(BRASS, 0.85)); hp.r(4, 9, 11, 12, dk(BRASS, 0.7)); hp.r(6, 13, 9, 15, DARK); hp.r(1, 2, 14, 2, lt(BRASS)); hp.save('tiles', 'brass_hopper'); icon_of('brass_hopper')
tile('brass_hopper', 'Brass hopper', False, popup_name='hopper', storage={'columns': 4, 'rows': 2}, sorts=4)
item('brass_hopper', 'Brass hopper', 'energy', 1800, places='brass_hopper', container={'slots': 8}); rec(G, 'brass_hopper', 1, [('brass_plate', 3), ('chest', 1)])
def marked(*rects): return lambda t: [t.r(*r) for r in rects]
machine('steam_assembler', 'Steam assembler', BRASS, marked((3, 5, 12, 12, DARK), (5, 7, 6, 10, IRON), (9, 7, 10, 10, IRON), (7, 8, 8, 9, BRASS)), 9000,
        [('brass_plate', 6), ('brass_gear', 4), ('mechanical_arm', 2), ('piston_rod', 2)], storage={'columns': 5, 'rows': 1}, draw=20, automat={'inputs': 4, 'speed': 1.0})
for name, label, kind, col, ins in (('steam_press', 'Steam press', 'press', IRON, [('press', 1)]), ('steam_furnace', 'Steam furnace', 'furnace', c('8a4a3a'), [('furnace', 1)]),
                                    ('steam_saw', 'Steam saw', 'woodwork_bench', WOOD, [('woodwork_bench', 1)]), ('steam_loom', 'Steam loom', 'tailor_bench', c('8a6aa8'), [('tailor_bench', 1)]),
                                    ('steam_mill', 'Steam mill', 'mixing_bench', c('a8a090'), [('mixing_bench', 1)]), ('steam_smith', 'Steam smith', 'metalwork_bench', c('5a5e6a'), [('metalwork_bench', 1)])):
    machine(name, label, col, marked((3, 6, 12, 12, DARK), (5, 8, 10, 10, BRASS), (7, 4, 8, 7, IRON)), 6000,
            ins + [('brass_plate', 4), ('brass_gear', 2), ('steam_valve', 1)], storage={'columns': 3, 'rows': 1}, draw=15, automat={'bench': kind, 'inputs': 2, 'speed': 1.5})
machine('steam_drill', 'Steam drill', c('5a5e6a'), marked((6, 5, 9, 13, IRON), (7, 13, 8, 15, lt(IRON)), (4, 5, 11, 6, BRASS)), 7000,
        [('iron_ingot', 6), ('iron_gear', 4), ('piston_rod', 2), ('steam_valve', 1)], storage={'columns': 2, 'rows': 2}, draw=10, miner=20)

# ---- in the ground ---------------------------------------------------------------------------------------------
def ore_pic(name, col, n):
    for k in range(n):
        base = Image.open(os.path.join(repo, 'data/textures/tiles/stone_%d.png' % k)).convert('RGBA').resize((16, 16), Image.NEAREST)
        t = P(); t.im = base; t.px = base.load(); rnd = random.Random(name + str(k))
        for _ in range(7):
            x, y = rnd.randint(1, 13), rnd.randint(1, 13); t.p(x, y, col); t.p(x + 1, y, lt(col)); t.p(x, y + 1, dk(col))
        t.save('tiles', '%s_%d' % (name, k))
    icon_of(name, name + '_0', block=True)
ore_pic('sparkstone_ore', SPARK, 2)
tile('sparkstone_ore', 'Sparkstone ore', texture=['tiles/sparkstone_ore_0.png', 'tiles/sparkstone_ore_1.png'], shade=12, ore=True, mine_time=1.2, light=3)
item('sparkstone_ore', 'Sparkstone ore', 'ores', 200, 3)
veins.append({'name': 'sparkstone', 'tile': 'sparkstone_ore', 'in': ['stone'], 'size': [3, 8], 'per_100_columns': 18, 'depth': [20, 190]})
for name, label, drops, col in (('coal_seep', 'Coal seep', 'coal', c('1a1a20')), ('iron_seep', 'Iron seep', 'iron_ore', c('c0a890')), ('copper_seep', 'Copper seep', 'copper_ore', COPPER)):
    ore_pic(name, col, 2)
    tile(name, label, texture=['tiles/%s_0.png' % name, 'tiles/%s_1.png' % name], shade=12, ore=True, mine_time=1.4, drop=drops, endless=True, immovable=True)
    veins.append({'name': name, 'tile': name, 'in': ['stone'], 'size': [1, 2], 'per_100_columns': 2.5, 'depth': [30, 190]})

# ---- written out -------------------------------------------------------------------------------------------------
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v))
                                    for k, v in obj.items()) + '\n}\n')
names = [t['name'] for t in tiles]; assert len(names) == len(set(names)), [n for n in names if names.count(n) > 1]
names = [t['name'] for t in items]; assert len(names) == len(set(names)), [n for n in names if names.count(n) > 1]
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes}); dump('worldgen.json', {'veins': veins})
json.dump({'label': 'Machinations of Madness', 'icon': 'icon.png', 'description': 'Signals, pistons and steam: machinery that moves the world about, and factories that run themselves.'},
          open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
Image.open(os.path.join(tex, 'items', 'brass_gear.png')).save(os.path.join(mod, 'icon.png'))
print('%d tiles, %d items, %d recipes' % (len(tiles), len(items), len(recipes)))
