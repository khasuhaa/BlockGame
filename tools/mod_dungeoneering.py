#!/usr/bin/env python3
"""Writes the Dungeoneering mod: its data files and its textures.

    python3 tools/mod_dungeoneering.py .

Ten dungeons, a gate to go into them by, a forge for what is found in them, and the loot itself - which is read from
tools/dungeoneering_loot.csv (the first ten dungeons of the owner's list: a name, a grade, a class, a base number, a
count of upgrades). The plan and what the owner decided are in ModDescriptions/Dungeoneering.txt.

Either edit this and run it again, or edit the mod's files by hand and stop using this: not both.
"""
import colorsys, copy, json, os, random, subprocess, sys, zlib
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
DATA = os.path.join(repo, 'data')
mod = os.path.join(repo, 'mods/dungeoneering'); TEX = os.path.join(mod, 'textures')
for d in ('tiles', 'items', 'mobs', 'worn'): os.makedirs(os.path.join(TEX, d), exist_ok=True)
# (Every picture here is drawn by this script or by the tools it runs: what was drawn last time goes first, so that nothing stale is left.)
for d in ('tiles', 'items', 'mobs', 'worn'):
    for f in os.listdir(os.path.join(TEX, d)):
        if f.endswith('.png'): os.remove(os.path.join(TEX, d, f))
BT = {t['name']: t for t in json.load(open(os.path.join(DATA, 'tiles.json')))['tiles']}
BI = {t['name']: t for t in json.load(open(os.path.join(DATA, 'items.json')))['items']}
BTR = {t['name']: t for t in json.load(open(os.path.join(DATA, 'trees.json')))['trees']}

tiles, items, recipes, trees, mobs, biomes, veins, decor, plants, structures, villages, sets = [], [], [], [], [], [], [], [], [], [], [], []
TP = {1: 30, 2: 100, 3: 300, 4: 900, 5: 2500, 6: 6000, 7: 15000, 8: 40000, 9: 120000, 10: 400000, 11: 1500000}
NUM = ['', 'I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X', 'XI']
def lo(r): return 2 * r - 1
def hi(r): return 2 * r

# ---- pictures --------------------------------------------------------------------------------------------------
INK = (22, 22, 30, 255)
def c(s): return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
def dk(col, f=0.72): return tuple(int(v * f) for v in col[:3]) + (255,)
def lt(col, f=1.25): return tuple(min(255, int(v * f + 10)) for v in col[:3]) + (255,)
def mix(a, b, t=0.5): return tuple(int(a[k] + (b[k] - a[k]) * t) for k in range(3)) + (255,)
def rnd(name): return random.Random(zlib.crc32(name.encode()))
WHITE, GOLD, PAPER, WOODC, IRONC, RED, JADE = c('ffffff'), c('f0c840'), c('efe2b8'), c('8c6537'), c('7a7e8a'), c('c8322a'), c('5fc89a')

class P:
    def __init__(s, fill=None):
        s.im = Image.new('RGBA', (16, 16), fill or (0, 0, 0, 0)); s.px = s.im.load()
    def r(s, x0, y0, x1, y1, col):
        for y in range(max(0, y0), min(15, y1) + 1):
            for x in range(max(0, x0), min(15, x1) + 1): s.px[x, y] = col
        return s
    def p(s, x, y, col):
        if 0 <= x < 16 and 0 <= y < 16: s.px[x, y] = col
        return s
    def disc(s, cx, cy, rad, col):
        for y in range(16):
            for x in range(16):
                if (x - cx) ** 2 + (y - cy) ** 2 <= rad * rad: s.px[x, y] = col
        return s
    def line(s, x0, y0, x1, y1, col):
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(n + 1): s.p(round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n), col)
        return s
    def speck(s, name, col, n, only=None):
        g = rnd(name)
        for _ in range(n):
            x, y = g.randrange(16), g.randrange(16)
            if s.px[x, y][3] and (only is None or s.px[x, y] == only): s.px[x, y] = col
        return s
    def outline(s):
        q = s.im.copy().load()
        for y in range(16):
            for x in range(16):
                if q[x, y][3]: continue
                if any(0 <= x + a < 16 and 0 <= y + b < 16 and q[x + a, y + b][3] for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))): s.px[x, y] = INK
        return s
    def save(s, kind, name):
        k = 3 if kind == 'items' else 2
        s.im.resize((16 * k, 16 * k), Image.NEAREST).save(os.path.join(TEX, kind, name + '.png')); return s
def icon_of(name, src=None):
    """A block's icon: its picture set in the middle of the square."""
    icon = Image.new('RGBA', (48, 48), (0, 0, 0, 0))
    icon.paste(Image.open(os.path.join(TEX, 'tiles', (src or name) + '.png')).convert('RGBA').resize((32, 32), Image.NEAREST), (8, 8))
    icon.save(os.path.join(TEX, 'items', name + '.png'))

def ramp(*cols):
    cols = [c(x)[:3] for x in cols]
    def at(t):
        t = max(0.0, min(1.0, t)) * (len(cols) - 1); i = min(int(t), len(cols) - 2); f = t - i
        return tuple(int(cols[i][k] + (cols[i + 1][k] - cols[i][k]) * f) for k in range(3))
    return at
def recolor(src, dst, r, sel='all', r2=None):
    """sel: 'all' every pixel by how light it is; 'gray' only the grey ones (a tool's head, not its handle);
    'ore' the coloured ones by r and the grey ones by r2; 'tint' everything, most of the way."""
    im = Image.open(src).convert('RGBA'); px = im.load(); w, h = im.size
    lum = lambda p: (0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2]) / 255.0
    grey = lambda p: colorsys.rgb_to_hsv(p[0] / 255, p[1] / 255, p[2] / 255)[1] < 0.2
    pick = lambda p: p[3] > 0 and lum(p) > 0.1 and (sel in ('all', 'tint', 'ore') or (sel == 'gray' and grey(p)))
    ls = [lum(px[x, y]) for y in range(h) for x in range(w) if pick(px[x, y])]
    a, b = (min(ls), max(ls)) if ls else (0, 1)
    for y in range(h):
        for x in range(w):
            p = px[x, y]
            if not pick(p): continue
            t = (lum(p) - a) / (b - a) if b > a else 0.5
            k = (r2 if sel == 'ore' and grey(p) and r2 else r)(t)
            if sel == 'tint': k = tuple((p[i] * 2 + k[i] * 3) // 5 for i in range(3))
            px[x, y] = k + (p[3],)
    im.save(dst)
def base_tex(kind, name): return os.path.join(DATA, 'textures', kind, name + '.png')

# Blocks: each kind of stuff has its own way of being drawn.
def rock(name, col, grain=40, vein=None):
    t = P(col); t.speck(name, dk(col, 0.86), grain).speck(name + 'l', lt(col, 1.08), grain // 2)
    g = rnd(name + 'c')
    for _ in range(3):
        x, y = g.randrange(13), g.randrange(13); t.line(x, y, x + g.randrange(1, 4), y + g.randrange(0, 3), dk(col, 0.72))
    if vein:
        for _ in range(5):
            x, y = g.randrange(1, 13), g.randrange(1, 13); t.r(x, y, x + 1, y + 1, vein); t.p(x, y, lt(vein, 1.3)); t.p(x + 1, y + 1, dk(vein))
    return t
def brick(name, col, mortar=None, rows=4):
    m = mortar or dk(col, 0.6); t = P(col); h = 16 // rows
    for row in range(rows):
        t.r(0, row * h + h - 1, 15, row * h + h - 1, m)
        for x in range((row % 2) * 4, 16, 8): t.r(x, row * h, x, row * h + h - 2, m)
        t.r(0, row * h, 15, row * h, lt(col, 1.1))
    return t.speck(name, dk(col, 0.9), 14)
def paving(name, col):
    t = P(col); t.r(0, 0, 15, 0, lt(col, 1.15)); t.r(0, 15, 15, 15, dk(col, 0.7)); t.r(7, 0, 7, 15, dk(col, 0.75)); t.r(8, 0, 8, 15, lt(col, 1.1))
    t.r(0, 7, 15, 7, dk(col, 0.75)); return t.speck(name, dk(col, 0.9), 12)
def roof(name, col):
    t = P(col)
    for row in range(4):
        y = row * 4
        t.r(0, y + 3, 15, y + 3, dk(col, 0.55))
        for x in range((row % 2) * 2, 16, 4): t.r(x, y, x, y + 2, dk(col, 0.75)); t.p(x + 1, y, lt(col, 1.3)); t.p(x + 2, y, lt(col, 1.3))
    return t
def bark(name, col, knot=None):
    t = P(col); g = rnd(name)
    for x in range(16):
        if g.random() < 0.45: t.r(x, 0, x, 15, dk(col, 0.8 + 0.1 * g.random()))
        elif g.random() < 0.3: t.r(x, 0, x, 15, lt(col, 1.08))
    for _ in range(4):
        x, y = g.randrange(16), g.randrange(14); t.r(x, y, x, y + 2, dk(col, 0.6))
    if knot: t.r(6, 6, 9, 9, dk(col, 0.6)); t.r(7, 7, 8, 8, knot)
    return t
def leaves(name, col, bloom=None):
    t = P(); g = rnd(name)
    for y in range(16):
        for x in range(16):
            if g.random() < 0.86: t.px[x, y] = [col, dk(col, 0.8), lt(col, 1.15), dk(col, 0.66)][g.randrange(4)]
    if bloom:
        for _ in range(5): t.p(g.randrange(16), g.randrange(16), bloom)
    return t
def planks(name, col):
    t = P(col)
    for y in (3, 7, 11, 15): t.r(0, y, 15, y, dk(col, 0.65))
    for row, x in enumerate((5, 11, 3, 9)): t.r(x, row * 4, x, row * 4 + 2, dk(col, 0.7))
    return t.speck(name, lt(col, 1.1), 10)
def crystal(col, tall=True):
    t = P()
    for x, top in ((4, 8), (7, 2 if tall else 6), (10, 6), (12, 10)):
        t.r(x, top, x + 2, 15, col); t.r(x, top, x, 15, lt(col, 1.3)); t.r(x + 2, top + 1, x + 2, 15, dk(col)); t.p(x + 1, top - 1, lt(col, 1.4))
    return t
def block_tex(kind, name, col, extra=None):
    return {'rock': rock, 'brick': brick, 'paving': paving, 'roof': roof, 'bark': bark, 'leaves': leaves, 'planks': planks}[kind](name, col, *( [extra] if extra and kind in ('brick', 'bark', 'leaves') else []))

# Things held.
def pill(col, grade=1, ring=None):
    t = P().disc(7.5, 8.5, 4.2, col); t.disc(8.5, 9.5, 3.0, dk(col, 0.85)); t.disc(7, 8, 3.0, col); t.p(6, 6, WHITE).p(5, 7, lt(col, 1.4)).p(6, 7, lt(col, 1.4))
    if ring: t.line(3, 9, 12, 8, ring)
    for i in range(grade - 1): t.p(4 + i * 2, 14, GOLD)
    return t.outline()
def book(col, trim=GOLD, mark=None):
    t = P().r(3, 2, 12, 14, col); t.r(3, 2, 4, 14, dk(col)); t.r(12, 3, 12, 14, PAPER); t.r(5, 14, 12, 14, PAPER); t.r(6, 4, 10, 7, PAPER); t.r(7, 5, 9, 5, INK); t.p(8, 6, INK)
    t.r(5, 2, 12, 2, trim); t.r(6, 10, 10, 10, trim); t.r(6, 12, 9, 12, trim)
    if mark: t.r(9, 9, 10, 13, mark)
    return t.outline()
def slip(col, glyph):
    t = P()
    for x in (4, 7, 10): t.r(x, 2, x + 1, 13, col); t.r(x, 2, x, 13, lt(col, 1.25)); t.r(x, 13, x + 1, 13, dk(col))
    t.r(3, 4, 12, 4, c('a04030')); t.r(3, 11, 12, 11, c('a04030'))
    for x, y in ((4, 6), (7, 7), (10, 6), (8, 9), (5, 9)): t.p(x, y, glyph)
    return t.outline()
def talisman(ink, paper=PAPER, seal=RED):
    t = P().r(5, 1, 10, 14, paper); t.r(5, 1, 10, 1, dk(paper, 0.85)); t.r(7, 3, 8, 3, ink); t.line(6, 5, 9, 5, ink); t.line(9, 5, 6, 8, ink); t.line(6, 8, 9, 8, ink); t.r(7, 9, 7, 11, ink); t.p(9, 10, ink).p(6, 10, ink)
    t.r(7, 12, 8, 13, seal); return t.outline()
def orb(col, core=None, spark=True):
    t = P().disc(7.5, 8, 4.6, dk(col, 0.8)); t.disc(7.5, 8, 3.6, col); t.disc(7, 7.5, 1.8, core or lt(col, 1.35))
    if spark: t.p(5, 5, WHITE).p(6, 5, WHITE).p(5, 6, WHITE)
    return t.outline()
def gemstone(col, big=False):
    t = P()
    pts = [(7, 2, 8), (6, 3, 9), (5, 4, 10), (4 if big else 5, 5, 11 if big else 10)]
    for x0, y, x1 in pts: t.r(x0, y, x1, y, col)
    t.r(4 if big else 5, 6, 11 if big else 10, 10, col); t.r(5, 11, 10, 11, col); t.r(6, 12, 9, 12, col); t.r(7, 13, 8, 13, col)
    t.r(7, 2, 7, 13, lt(col, 1.3)); t.r(9, 5, 10, 11, dk(col)); t.p(6, 5, WHITE).p(6, 6, WHITE); return t.outline()
def flag(col, pole=WOODC, mark=GOLD):
    t = P().r(4, 1, 4, 14, pole); t.r(5, 2, 11, 7, col); t.r(5, 2, 11, 2, lt(col)); t.r(9, 7, 11, 8, col); t.r(5, 7, 6, 8, col); t.r(7, 4, 9, 5, mark); t.p(8, 3, mark).p(8, 6, mark); t.r(3, 14, 5, 14, dk(pole)); return t.outline()
def bowl(food, dish=c('d8d0c0'), steam=True):
    t = P().r(3, 8, 12, 9, dish); t.r(4, 10, 11, 11, dish); t.r(5, 12, 10, 12, dk(dish)); t.r(4, 7, 11, 8, food); t.r(5, 6, 10, 6, lt(food)); t.p(6, 7, lt(food, 1.4)).p(9, 7, dk(food))
    if steam: t.p(6, 3, WHITE).p(7, 2, WHITE).p(9, 4, WHITE).p(10, 3, WHITE)
    return t.outline()
def cup(drink, clay=c('7a9a8a')):
    t = P().r(5, 6, 10, 12, clay); t.r(6, 13, 9, 13, dk(clay)); t.r(5, 6, 10, 7, drink); t.r(5, 6, 10, 6, lt(drink)); t.r(11, 8, 12, 10, clay); t.p(7, 3, WHITE).p(8, 2, WHITE); return t.outline()
def ingot(col):
    t = P().r(3, 8, 12, 11, col); t.r(4, 7, 11, 7, lt(col)); t.r(3, 11, 12, 11, dk(col)); t.r(4, 12, 11, 12, dk(col, 0.6)); t.p(5, 8, WHITE); return t.outline()
def hide(col, spot=None):
    t = P().r(4, 3, 11, 12, col); t.r(2, 3, 4, 5, col); t.r(11, 3, 13, 5, col); t.r(2, 10, 4, 12, col); t.r(11, 10, 13, 12, col); t.r(5, 4, 10, 4, lt(col))
    if spot: t.p(6, 6, spot).p(9, 8, spot).p(7, 10, spot)
    return t.outline()
def feather(col):
    t = P().line(4, 13, 11, 3, dk(col, 0.6))
    for i in range(7): t.r(6 + i, 9 - i, 8 + i, 10 - i, col); t.p(5 + i, 9 - i, lt(col))
    return t.outline()
def fang(col):
    t = P().r(6, 3, 9, 6, col); t.r(7, 7, 9, 9, col); t.r(8, 10, 9, 12, col); t.p(9, 13, col); t.r(6, 3, 6, 6, lt(col)); t.r(6, 2, 9, 2, dk(col, 0.6)); return t.outline()
def meat(col):
    t = P().disc(7, 8, 4, col); t.disc(6.5, 7.5, 2.2, lt(col)); t.r(10, 10, 12, 12, c('efe8d8')); t.r(12, 12, 13, 13, c('efe8d8')); t.p(5, 6, WHITE); return t.outline()
def herb(stem, bloom, kind=0):
    t = P().r(7, 8, 8, 14, stem); t.line(7, 11, 4, 9, stem); t.line(8, 10, 11, 8, stem)
    if kind == 0: t.disc(7.5, 5, 2.6, bloom); t.p(7, 5, lt(bloom, 1.4)); t.p(8, 4, WHITE)
    elif kind == 1: t.r(5, 4, 10, 6, bloom); t.r(6, 3, 9, 3, lt(bloom)); t.r(6, 7, 9, 7, dk(bloom))
    elif kind == 2:
        for x, y in ((5, 3), (8, 2), (10, 4), (6, 6), (9, 6)): t.r(x, y, x + 1, y + 1, bloom); t.p(x, y, lt(bloom, 1.3))
    else: t.line(7, 8, 4, 2, bloom); t.line(8, 8, 11, 2, bloom); t.line(7, 8, 7, 1, lt(bloom)); t.p(4, 2, WHITE).p(11, 2, WHITE)
    return t.outline()
def trinket(kind, col, trim=GOLD):
    t = P()
    if kind == 'bead': t.disc(7.5, 9, 3.2, col); t.p(6, 8, WHITE); t.line(7, 5, 5, 2, trim); t.line(8, 5, 10, 2, trim)
    elif kind == 'pendant': t.line(4, 2, 7, 6, trim); t.line(11, 2, 8, 6, trim); t.r(6, 7, 9, 12, col); t.r(7, 13, 8, 13, col); t.r(7, 8, 8, 10, lt(col, 1.3)); t.p(6, 7, WHITE)
    elif kind == 'ring': t.disc(7.5, 9, 4, trim); t.disc(7.5, 9, 2.4, (0, 0, 0, 0)); t.r(6, 3, 9, 5, col); t.p(7, 3, WHITE)
    elif kind == 'bell': t.r(7, 2, 8, 3, trim); t.r(5, 4, 10, 10, col); t.r(4, 10, 11, 11, col); t.r(5, 4, 5, 10, lt(col)); t.r(7, 12, 8, 13, trim)
    elif kind == 'mirror': t.disc(7.5, 6.5, 4.4, trim); t.disc(7.5, 6.5, 3.2, col); t.p(6, 5, WHITE).p(7, 4, WHITE); t.r(7, 11, 8, 14, trim)
    elif kind == 'gourd': t.disc(7.5, 10, 3.6, col); t.disc(7.5, 5, 2.2, col); t.r(7, 1, 8, 2, dk(col, 0.5)); t.r(5, 7, 10, 7, RED); t.p(6, 9, lt(col, 1.4))
    elif kind == 'seal': t.r(4, 9, 11, 13, col); t.r(6, 5, 9, 8, col); t.r(7, 3, 8, 4, trim); t.r(4, 13, 11, 13, RED); t.r(4, 9, 11, 9, lt(col))
    elif kind == 'fan':
        for i in range(-3, 4): t.line(7, 13, 7 + i * 2, 3 + abs(i), col if i % 2 else lt(col))
        t.r(7, 12, 8, 14, trim)
    elif kind == 'sword':
        t.line(3, 13, 12, 3, col); t.line(4, 13, 13, 3, lt(col, 1.3)); t.line(3, 12, 11, 3, dk(col)); t.line(2, 10, 6, 14, trim); t.r(2, 13, 3, 14, dk(trim, 0.6)); t.p(13, 2, WHITE)
    elif kind == 'crown': t.r(4, 8, 11, 11, trim); t.p(4, 6, trim).p(4, 7, trim).p(7, 5, trim).p(7, 6, trim).p(7, 7, trim).p(8, 6, trim).p(8, 7, trim).p(11, 6, trim).p(11, 7, trim); t.r(7, 9, 8, 10, col)
    elif kind == 'needle': t.line(3, 13, 12, 3, col); t.p(12, 3, WHITE); t.disc(4, 12, 1.5, trim)
    elif kind == 'brush': t.line(5, 12, 11, 4, WOODC); t.r(3, 12, 5, 14, col); t.p(3, 14, INK)
    elif kind == 'paper': t.r(4, 2, 11, 13, col); t.r(4, 2, 11, 2, dk(col, 0.85)); t.r(11, 3, 11, 13, dk(col, 0.9))
    elif kind == 'pot': t.r(5, 5, 10, 12, col); t.r(4, 7, 11, 11, col); t.r(6, 3, 9, 4, trim); t.r(5, 5, 10, 5, lt(col)); t.p(6, 7, WHITE)
    elif kind == 'cloth': t.r(3, 4, 12, 12, col); t.r(3, 4, 12, 4, lt(col)); t.r(3, 8, 12, 8, dk(col, 0.85)); t.r(7, 4, 7, 12, dk(col, 0.85)); t.r(3, 12, 12, 12, dk(col))
    elif kind == 'thread': t.r(6, 2, 9, 13, WOODC); t.r(4, 4, 11, 11, col); t.line(4, 5, 11, 7, lt(col)); t.line(4, 8, 11, 10, dk(col))
    elif kind == 'dust': t.r(4, 10, 11, 12, col); t.r(5, 8, 10, 9, col); t.r(6, 7, 8, 7, lt(col)); t.p(6, 9, WHITE).p(9, 11, dk(col))
    elif kind == 'plate': t.r(3, 4, 12, 11, col); t.r(3, 4, 12, 4, lt(col)); t.r(3, 11, 12, 11, dk(col)); t.p(4, 5, WHITE).p(11, 10, dk(col, 0.6)).p(4, 10, dk(col, 0.6)).p(11, 5, dk(col, 0.6))
    elif kind == 'key': t.disc(5, 5, 2.6, trim); t.disc(5, 5, 1.0, (0, 0, 0, 0)); t.line(7, 7, 12, 12, trim); t.r(10, 12, 11, 13, trim); t.r(12, 10, 13, 11, trim); t.p(5, 2, col)
    elif kind == 'egg': t.disc(7.5, 9, 4.2, col); t.disc(7.5, 6, 3.0, col); t.p(6, 5, WHITE); t.line(5, 9, 10, 10, trim)
    elif kind == 'rune': t.r(4, 3, 11, 13, col); t.r(4, 3, 11, 3, lt(col)); t.line(6, 5, 9, 5, trim); t.line(9, 5, 6, 9, trim); t.line(6, 9, 9, 11, trim)
    return t.outline()

# Benches and what stands about.
def bench_pic(kind, trim):
    t = P()
    if kind == 'pill_furnace':
        t.r(4, 5, 11, 12, c('8a5a3a')); t.r(3, 7, 12, 10, c('8a5a3a')); t.r(4, 5, 11, 5, trim); t.r(6, 2, 9, 4, c('6a4028')); t.r(7, 1, 8, 1, trim); t.r(6, 8, 9, 9, c('f08030')); t.p(7, 8, c('ffe070'))
        t.r(4, 13, 5, 15, c('5a3820')); t.r(10, 13, 11, 15, c('5a3820')); t.r(3, 7, 3, 10, trim); t.r(12, 7, 12, 10, trim)
    elif kind == 'spirit_forge':
        t.r(1, 9, 8, 15, c('5a5560')); t.r(2, 10, 7, 12, c('f06020')); t.r(3, 10, 6, 10, c('ffd060')); t.r(1, 9, 8, 9, trim); t.r(10, 8, 15, 9, c('8a8e9a')); t.r(11, 10, 14, 11, c('6a6e7a')); t.r(10, 12, 15, 15, c('4a4e5a')); t.r(9, 8, 10, 8, c('8a8e9a'))
        t.r(12, 3, 13, 7, WOODC); t.r(11, 2, 14, 3, trim)
    elif kind == 'artificers_table':
        t.r(1, 8, 14, 9, c('7a5a8a')); t.r(1, 8, 14, 8, trim); t.r(2, 10, 3, 15, c('5a4068')); t.r(12, 10, 13, 15, c('5a4068')); t.disc(5, 5, 2, c('60d0e0')); t.p(4, 4, WHITE); t.r(9, 4, 9, 7, IRONC); t.r(8, 3, 10, 3, IRONC); t.r(12, 5, 13, 7, trim)
    elif kind == 'talisman_desk':
        t.r(1, 9, 14, 10, c('a0522a')); t.r(2, 11, 3, 15, c('7a3a1a')); t.r(12, 11, 13, 15, c('7a3a1a')); t.r(4, 5, 8, 8, PAPER); t.r(5, 6, 7, 6, RED); t.p(6, 7, RED); t.line(10, 8, 13, 3, WOODC); t.p(10, 8, INK); t.r(1, 9, 14, 9, trim)
    elif kind == 'spirit_kitchen':
        t.r(1, 8, 14, 15, c('9a8a7a')); t.r(1, 8, 14, 8, trim); t.r(3, 10, 12, 13, c('3a3030')); t.r(5, 11, 10, 12, c('f07020')); t.r(3, 5, 12, 7, c('3a3a44')); t.r(4, 4, 11, 4, c('5a5a66')); t.p(6, 2, WHITE).p(9, 1, WHITE)
    elif kind == 'scripture_desk':
        t.r(6, 9, 9, 15, c('6a4a2a')); t.r(4, 14, 11, 15, c('5a3a1a')); t.r(2, 5, 13, 8, c('8a6a3a')); t.r(3, 4, 7, 7, PAPER); t.r(8, 4, 12, 7, PAPER); t.r(4, 5, 6, 5, INK); t.r(9, 6, 11, 6, INK); t.r(2, 8, 13, 8, trim)
    elif kind == 'formation_table':
        t.r(1, 9, 14, 10, c('4a6a7a')); t.r(2, 11, 3, 15, c('3a5060')); t.r(12, 11, 13, 15, c('3a5060')); t.disc(7.5, 6, 3.2, c('e8e0c8')); t.disc(7.5, 6, 1.2, INK); t.p(7, 3, RED).p(8, 9, RED); t.r(1, 9, 14, 9, trim)
    elif kind == 'spirit_loom':
        t.r(2, 2, 3, 15, WOODC); t.r(12, 2, 13, 15, WOODC); t.r(2, 2, 13, 3, dk(WOODC)); t.r(2, 12, 13, 13, dk(WOODC))
        for x in range(4, 12): t.r(x, 4, x, 11, trim if x % 2 else c('e8e0f0'))
        t.r(4, 8, 11, 8, c('a070d0'))
    return t
def array_pic(col, glow):
    t = P().disc(7.5, 10.5, 6.2, c('4a4a56')); t.disc(7.5, 10.5, 5.2, c('2a2a34')); t.disc(7.5, 10.5, 4.0, col); t.disc(7.5, 10.5, 2.6, c('2a2a34')); t.disc(7.5, 10.5, 1.4, glow)
    for x, y in ((7, 5), (2, 10), (13, 10), (7, 15), (4, 7), (11, 7)): t.p(x, y, glow)
    t.r(0, 0, 15, 3, (0, 0, 0, 0)); t.r(7, 1, 8, 4, glow); t.p(7, 0, WHITE); return t
def chest_pic(col, trim):
    t = P().r(2, 6, 13, 14, col); t.r(2, 6, 13, 7, lt(col)); t.r(2, 9, 13, 9, dk(col, 0.6)); t.r(7, 8, 8, 11, trim); t.r(2, 6, 2, 14, trim); t.r(13, 6, 13, 14, trim); t.r(2, 14, 13, 14, dk(col, 0.6)); t.r(3, 4, 12, 5, col); t.r(3, 4, 12, 4, trim); return t
def lantern_pic(col, glow):
    t = P().r(7, 0, 8, 2, c('3a2a1a')); t.r(5, 3, 10, 4, col); t.r(4, 5, 11, 11, glow); t.r(4, 5, 4, 11, col); t.r(11, 5, 11, 11, col); t.r(7, 5, 8, 11, lt(glow, 1.2)); t.r(5, 12, 10, 13, col); t.r(7, 14, 8, 15, GOLD); return t

# Creatures, facing right as the game's are drawn.
def beast(kind, body, accent, frame=0, eye=INK, big=False):
    t = P(); d = frame
    if kind == 'quad':
        t.r(3, 7, 11, 11, body); t.r(3, 7, 11, 7, lt(body)); t.r(10, 4, 14, 8, body); t.p(13, 6, eye); t.r(14, 7, 15, 8, accent); t.p(11, 3, accent).p(13, 3, accent)
        t.r(1, 6, 3, 7, accent); t.p(0, 5, accent)
        for x, sh in ((4, d), (6, -d), (9, d), (11, -d)): t.r(x + sh, 12, x + sh, 14, dk(body))
    elif kind == 'boar':
        t.r(2, 6, 11, 11, body); t.r(2, 5, 9, 5, accent); t.r(10, 6, 14, 10, body); t.p(13, 7, eye); t.r(14, 9, 15, 9, WHITE); t.p(15, 8, WHITE); t.p(11, 5, dk(body))
        for x, sh in ((3, d), (5, -d), (9, d), (11, -d)): t.r(x + sh, 12, x + sh, 14, dk(body))
    elif kind == 'serpent':
        ys = [11, 10, 9, 10, 11, 12, 11, 10, 9, 8, 8, 7] if not d else [10, 11, 12, 11, 10, 9, 10, 11, 10, 9, 8, 7]
        for x, y in enumerate(ys): t.r(x + 1, y, x + 1, y + 1, body); t.p(x + 1, y, lt(body) if x % 3 else accent)
        t.r(12, 5, 15, 8, body); t.p(14, 6, eye); t.p(15, 8, RED); t.p(13, 4, accent)
    elif kind == 'bird':
        t.r(5, 7, 11, 10, body); t.r(11, 5, 13, 8, body); t.p(13, 6, eye); t.r(14, 7, 15, 7, accent); t.r(2, 8, 4, 9, accent); t.p(1, 9, accent)
        if d: t.r(5, 3, 9, 6, accent); t.r(6, 2, 8, 2, lt(accent))
        else: t.r(5, 10, 9, 13, accent); t.r(6, 14, 8, 14, dk(accent))
        t.r(7, 11, 7, 12, dk(body)) if d else None
    elif kind == 'crane':
        t.r(4, 7, 10, 10, body); t.r(10, 3, 11, 8, body); t.r(11, 2, 13, 4, body); t.p(12, 2, RED); t.p(13, 3, eye); t.r(14, 4, 15, 4, accent); t.r(1, 8, 4, 9, accent)
        t.r(6 + d, 11, 6 + d, 15, accent); t.r(9 - d, 11, 9 - d, 15, accent)
    elif kind == 'rabbit':
        t.r(5, 8, 11, 12, body); t.r(10, 6, 13, 10, body); t.r(11, 2, 11, 5, body); t.r(13, 3, 13, 5, body); t.p(12, 8, eye); t.r(3, 10, 4, 11, accent)
        t.r(6 + d, 13, 7 + d, 14, dk(body)); t.r(10 - d, 13, 11 - d, 14, dk(body))
    elif kind == 'ape':
        t.r(5, 5, 10, 11, body); t.r(6, 2, 10, 5, body); t.r(7, 3, 9, 4, accent); t.p(8, 3, eye); t.p(9, 3, eye); t.r(3, 6, 4, 11 - d, body); t.r(11, 6, 12, 10 + d, body)
        t.r(5, 12, 6, 14, dk(body)); t.r(9, 12, 10, 14, dk(body)); t.line(4, 11, 1, 8, accent)
    elif kind == 'golem':
        t.r(4, 4, 11, 12, body); t.r(5, 1, 10, 4, body); t.r(6, 2, 9, 3, accent); t.p(7, 2, eye).p(9, 2, eye); t.r(2, 5, 3, 11 - d, dk(body)); t.r(12, 5, 13, 10 + d, dk(body)); t.r(7, 7, 8, 9, accent)
        t.r(4 + d, 13, 6 + d, 15, dk(body)); t.r(9 - d, 13, 11 - d, 15, dk(body)); t.r(4, 4, 11, 4, lt(body))
    elif kind == 'stag':
        t.r(3, 7, 10, 10, body); t.r(10, 4, 12, 8, body); t.r(12, 5, 13, 6, body); t.p(12, 5, eye); t.line(10, 3, 8, 0, accent); t.line(11, 3, 13, 0, accent); t.p(9, 1, accent).p(12, 1, accent)
        for x, sh in ((3, d), (5, -d), (8, d), (10, -d)): t.r(x + sh, 11, x + sh, 15, dk(body))
        t.p(2, 7, WHITE)
    elif kind == 'wraith':
        t.r(5, 2, 10, 9, body); t.r(4, 4, 11, 8, body); t.p(7, 5, accent).p(9, 5, accent)
        for x in (4, 6, 8, 10): t.r(x, 10, x, 12 + (d if x % 4 else -d) + 1, body)
        t.r(5, 2, 10, 2, lt(body))
    elif kind == 'fish':
        t.r(4, 6, 11, 10, body); t.r(5, 5, 10, 5, lt(body)); t.r(12, 7, 13, 9, body); t.p(11, 7, eye); t.r(1, 5 + d, 3, 11 - d, accent); t.r(6, 11, 8, 11, accent); t.r(6, 4, 8, 4, accent)
    elif kind == 'wyrm':
        t.r(1, 7, 12, 10, body); t.r(11, 5, 15, 9, body); t.p(14, 6, eye); t.r(12, 10, 15, 10, WHITE); t.r(0, 6 + d, 1, 11 - d, accent)
        for x in (3, 6, 9): t.r(x, 5, x, 6, accent)
        t.r(2, 10, 11, 10, lt(body, 1.3))
    elif kind == 'lion':
        t.r(2, 7, 10, 11, body); t.disc(11, 6.5, 3.6, accent); t.r(10, 5, 13, 9, body); t.p(12, 6, eye); t.p(14, 8, INK); t.line(2, 7, 0, 4, body); t.p(0, 3, accent)
        for x, sh in ((3, d), (5, -d), (8, d), (10, -d)): t.r(x + sh, 12, x + sh, 14, dk(body))
    elif kind == 'dragon':
        ys = [9, 8, 7, 8, 10, 11, 10, 8, 7, 6, 5, 5] if not d else [8, 9, 10, 9, 8, 7, 8, 10, 9, 7, 6, 5]
        for x, y in enumerate(ys): t.r(x, y, x, y + 2, body); t.p(x, y, accent if x % 2 else lt(body)); t.p(x, y + 2, dk(body))
        t.r(11, 2, 15, 6, body); t.p(14, 3, eye); t.line(12, 1, 10, 0, accent); t.line(14, 1, 13, 0, accent); t.r(15, 5, 15, 6, WHITE); t.line(14, 6, 15, 9, accent); t.p(4, 12, accent).p(8, 12, accent)
    elif kind == 'phoenix':
        t.r(6, 6, 10, 10, body); t.r(10, 3, 12, 7, body); t.p(12, 4, eye); t.r(13, 5, 14, 5, GOLD); t.line(11, 2, 12, 0, accent); t.line(5, 9, 0, 14, accent); t.line(5, 10, 1, 15, body); t.line(6, 10, 3, 15, GOLD)
        if d: t.r(4, 1, 9, 5, accent); t.r(5, 0, 8, 0, GOLD)
        else: t.r(4, 10, 9, 13, accent); t.r(5, 14, 8, 14, GOLD)
    elif kind == 'tortoise':
        t.disc(7, 9, 5.2, body); t.r(1, 10, 13, 15, (0, 0, 0, 0)); t.r(2, 10, 12, 11, dk(body)); t.r(5, 5, 9, 6, accent); t.r(3, 8, 11, 8, accent); t.r(7, 5, 7, 9, accent)
        t.r(12, 7, 15, 10, c('4a6a4a')); t.p(14, 8, eye); t.r(3 + d, 12, 4 + d, 14, c('4a6a4a')); t.r(10 - d, 12, 11 - d, 14, c('4a6a4a')); t.line(1, 9, 0, 6 + d, c('3a8a5a'))
    elif kind == 'tiger':
        t.r(2, 7, 11, 11, body); t.r(10, 4, 14, 9, body); t.p(13, 6, eye); t.p(11, 3, body).p(14, 3, body); t.r(14, 8, 15, 9, WHITE); t.line(2, 7, 0, 3 + d, body)
        for x in (3, 5, 7, 9): t.r(x, 7, x, 9, accent)
        t.p(11, 5, accent).p(12, 4, accent)
        for x, sh in ((3, d), (5, -d), (8, d), (10, -d)): t.r(x + sh, 12, x + sh, 14, dk(body))
    return t.outline()

# ---- definitions -------------------------------------------------------------------------------------------------
def clean(d): return {k: v for k, v in d.items() if v is not None}
def tile(name, label, solid=True, **kw):
    d = {'name': name, 'label': label, 'solid': solid, 'removable': True, 'mine_time': 0.6 if solid else 0.3, 'drop': name, 'texture': 'tiles/%s.png' % name, 'tool': 'pickaxe'}
    if not solid: d['layer'] = 'background'
    d.update(kw); d = clean(d); tiles.append(d); return d
def item(name, label, tier, cat='crafted', mult=1.0, **kw):
    d = {'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'category': cat, 'price': max(10, int(TP[tier] * mult))}
    if cat not in ('benches', 'portals', 'creatures', 'special'): d['tier'] = tier
    d.update(kw); d = clean(d); items.append(d); return d
def thing(pic, name, label, tier, cat='crafted', mult=1.0, **kw):
    pic.save('items', name); return item(name, label, tier, cat, mult, **kw)
def block(pic, name, label, tier, mult=0.05, solid=True, cat='blocks', iover=None, **kw):
    pic.save('tiles', name); icon_of(name); tile(name, label, solid, **kw); return item(name, label, tier, cat, mult, places=name, **(iover or {}))
def clone_tile(base, name, label, r, sel='all', r2=None, **over):
    d = copy.deepcopy(BT[base])
    for k in ('aliases', 'texture_seasons', 'upgrade'): d.pop(k, None)
    def tex(v):
        if isinstance(v, list): return [tex(x) for x in v]
        if isinstance(v, dict): return {k: tex(x) for k, x in v.items()}
        if isinstance(v, str) and v.endswith('.png'):
            f = os.path.basename(v); out = 'tiles/' + (f.replace(base, name) if base in f else name + '_' + f)
            recolor(os.path.join(DATA, 'textures', v), os.path.join(TEX, out), r, sel, r2); return out
        return v
    d = tex(d)
    if d.get('drop') == base: d['drop'] = name
    d.update(name=name, label=label); d.update(over); d = clean(d); tiles.append(d); return d
def clone_item(base, name, label, r, tier, cat, mult=1.0, sel='all', r2=None, **over):
    recolor(os.path.join(DATA, 'textures', BI[base]['texture']), os.path.join(TEX, 'items', name + '.png'), r, sel, r2)
    d = {k: v for k, v in BI[base].items() if k in ('max_stack', 'equip', 'tool', 'damage', 'range')}
    out = item(name, label, tier, cat, mult); out.update(d); out.update(over)
    for k in [k for k, v in out.items() if v is None]: out.pop(k)
    return out
def rec(bench, out, ins, count=1, level=None, at=None, time=120, **kw):
    r = {'bench': bench, 'output': {'item': out, 'count': count}, 'inputs': [{'item': i, 'count': n} for i, n in ins], 'time': time}
    if level and level > 1: r['level'] = level
    if at: r['at'] = at
    r.update(kw); recipes.append(r)
def chain(start):
    seen, todo = [], [start]
    while todo:
        n = todo.pop()
        if n in seen or n not in BT: continue
        seen.append(n); todo += [BT[n].get(k) for k in ('grows_into', 'grows_top', 'topped_by', 'cut_to') if BT[n].get(k)]
    return seen
def drop(i, a, b, chance=None): return clean({'item': i, 'min': a, 'max': b, 'chance': chance})
def some(*names, most=2): return {'one_of': [{'item': n, 'weight': w} for n, w in names], 'min': 1, 'max': most}

# ==== (shared with Plunder & Thunder's generator) ================================================================================================================
BM = {t['name']: t for t in json.load(open(os.path.join(DATA, 'mobs.json')))['mobs']}
SEA, TAR, CANVAS, ROPE, BRASS, OAK = c('2a6a9a'), c('3a2a22'), c('e8dcc0'), c('b89a60'), c('c8a040'), c('8a5a30')
def lasts(ticks, **boons): return {'ticks': ticks, 'boons': boons}
def make(bench, out, ins, g=1, count=1, time=160, need=None): rec(bench, out, ins, count, level=g, time=time, formula=need)
def comp(pic, name, label, tier, mult=0.8, cat='crafted', **kw): return thing(pic, name, label, tier, cat, mult, **kw)
def deco(pic, name, label, tier, ins, bench='shipwright', g=1, count=1, mult=1.0, **kw):
    pic.save('tiles', name); icon_of(name); kw.setdefault('mine_time', 0.3); kw.setdefault('tool', 'axe')
    tile(name, label, False, **kw); item(name, label, tier, 'crafted', mult, places=name)
    if ins: rec(bench, name, ins, count, level=g, time=120)
class Big:
    """A picture bigger than a block: so many blocks square, drawn as the rest is."""
    def __init__(s, n): s.n = n * 16; s.im = Image.new('RGBA', (s.n, s.n), (0, 0, 0, 0)); s.px = s.im.load()
    def r(s, x0, y0, x1, y1, col):
        for y in range(max(0, y0), min(s.n - 1, y1) + 1):
            for x in range(max(0, x0), min(s.n - 1, x1) + 1): s.px[x, y] = col
        return s
    def p(s, x, y, col): return s.r(x, y, x, y, col)
    def line(s, x0, y0, x1, y1, col):
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(n + 1): s.p(round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n), col)
        return s
    def outline(s):
        q = s.im.copy().load()
        for y in range(s.n):
            for x in range(s.n):
                if q[x, y][3]: continue
                if any(0 <= x + a < s.n and 0 <= y + b < s.n and q[x + a, y + b][3] for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))): s.px[x, y] = INK
        return s
    def save(s, kind, name): s.im.resize((s.n * 2, s.n * 2), Image.NEAREST).save(os.path.join(TEX, kind, name + '.png')); return s
def ship_pic(n, hull, sail, flag, masts, guns=0, trim=None, frame=0, tatter=False, oars=False):
    """A ship, bows to the right. (It is drawn round the square it floats in, and the water covers what is below the
    top of that square: so her hull sits just above the middle of the picture, and only her keel is under.)"""
    t = Big(n); N = t.n; wl = N // 2 - 5; L = int(N * 0.92); x0 = (N - L) // 2; deep = max(4, N // 7)
    top = wl - deep
    for k in range(deep + 3):
        y = top + k
        inset = max(0, k - deep + 2) * 2 + (1 if k > deep // 2 else 0)
        t.r(x0 + inset, y, x0 + L - 1 - inset * 2, y, lt(hull) if k == 0 else dk(hull, 0.8) if y > wl else hull)
    # (A raised stern and a bowsprit.)
    t.r(x0, top - max(2, N // 16), x0 + max(3, L // 6), top - 1, hull); t.r(x0, top - max(2, N // 16), x0 + max(3, L // 6), top - max(2, N // 16), trim or lt(hull))
    t.line(x0 + L - 2, top, x0 + L + max(1, N // 12), top - max(2, N // 12), dk(hull, 0.7))
    if trim: t.r(x0 + 1, top + 2, x0 + L - 4, top + 2, trim)
    for g in range(guns): t.r(x0 + 5 + g * max(2, (L - 12) // max(1, guns)), top + 3, x0 + 5 + g * max(2, (L - 12) // max(1, guns)), top + 4, INK)
    tall = max(6, top - 3)
    for m in range(masts):
        mx = x0 + (m + 1) * L // (masts + 1); mtop = top - tall + (2 if m != masts // 2 else 0)
        t.r(mx, mtop, mx, top - 1, c('5a3a22'))
        rows = 1 if N <= 32 else 2 if N <= 64 else 3
        for rr in range(rows):
            y0 = mtop + 2 + rr * (tall - 3) // rows; h = max(2, (tall - 4) // rows - 1); w = max(3, L // (masts + 1) // 2 - rr % 2)
            for yy in range(y0, y0 + h):
                belly = (1 if (yy - y0) in (h // 2, h // 2 - 1) else 0) + (frame % 2 if yy % 2 else 0)
                if tatter and (yy * 7 + mx + (yy // 2) * 3) % 5 == 0: continue
                t.r(mx - w + 1, yy, mx + w - 1 + belly, yy, lt(sail, 1.08) if yy == y0 else dk(sail, 0.92) if (yy - y0) == h - 1 else sail)
        t.r(mx + 1, max(0, mtop - 1), mx + 3 + frame % 2, max(1, mtop), flag)
    if oars:
        for o in range(3): t.line(x0 + 4 + o * 5, top + 2, x0 + 2 + o * 5 + (2 if frame else 0), wl + 4, c('8a6a3a'))
    return t.outline()
def person(coat, hat, frame=0, skin=c('e0b890'), blade=True, hatkind=0):
    t = P(); d = frame
    t.r(6, 2, 9, 5, skin); t.p(7, 4, INK).p(9, 4, INK)
    if hatkind == 0: t.r(5, 1, 10, 2, hat); t.r(6, 0, 9, 0, hat)
    elif hatkind == 1: t.r(6, 1, 9, 2, hat); t.p(10, 2, hat)
    elif hatkind == 2: t.r(4, 2, 11, 2, hat); t.r(6, 0, 9, 1, hat)
    else: t.r(6, 0, 9, 1, hat); t.p(5, 1, c('f0e0a0')).p(10, 0, c('f0e0a0'))
    t.r(5, 6, 10, 11, coat); t.r(7, 6, 8, 11, lt(coat)); t.r(5, 9, 10, 9, dk(coat, 0.6)); t.r(3, 6, 4, 9 - d, coat); t.r(11, 6, 12, 8 + d, coat)
    if blade: t.line(12, 8 + d, 15, 4, c('d0d8e0'))
    t.r(5 + d, 12, 6 + d, 14, dk(coat, 0.5)); t.r(9 - d, 12, 10 - d, 14, dk(coat, 0.5)); return t.outline()
def barrel(col, band=c('5a5560'), mark=None):
    t = P().r(4, 3, 11, 13, col); t.r(3, 5, 12, 11, col); t.r(4, 3, 11, 3, lt(col)); t.r(3, 5, 12, 5, band); t.r(3, 11, 12, 11, band); t.r(7, 3, 7, 13, dk(col, 0.8))
    if mark: t.r(6, 7, 9, 9, mark)
    return t
def sack(col, tie=ROPE):
    t = P().r(4, 6, 11, 13, col); t.r(5, 4, 10, 5, col); t.r(6, 3, 9, 3, tie); t.r(4, 6, 4, 13, lt(col)); t.r(11, 7, 11, 13, dk(col)); return t.outline()
def bottle(col, glass=c('3a6a4a')):
    t = P().r(7, 1, 8, 4, glass); t.r(5, 5, 10, 13, glass); t.r(6, 7, 9, 12, col); t.r(6, 7, 9, 7, lt(col)); t.p(6, 6, WHITE); t.r(7, 0, 8, 0, c('b08850')); return t.outline()
def hatpic(kind, col, trim=GOLD):
    t = P()
    if kind == 'tricorn': t.r(3, 8, 12, 9, col); t.r(5, 5, 10, 7, col); t.line(3, 8, 5, 5, col); t.line(12, 8, 10, 5, col); t.r(4, 9, 11, 9, trim)
    elif kind == 'bandana': t.r(4, 5, 11, 9, col); t.r(4, 5, 11, 5, lt(col)); t.line(11, 8, 14, 11, col); t.p(6, 7, WHITE).p(9, 6, WHITE)
    elif kind == 'patch': t.line(3, 4, 12, 9, INK); t.disc(9.5, 8, 2.2, col)
    elif kind == 'coat': t.r(4, 3, 11, 13, col); t.r(2, 3, 3, 10, col); t.r(12, 3, 13, 10, col); t.r(7, 3, 8, 13, lt(col)); [t.p(6, y, trim) for y in (5, 8, 11)]; t.r(4, 3, 11, 3, trim)
    elif kind == 'breeches': t.r(4, 2, 11, 5, col); t.r(4, 6, 6, 12, col); t.r(9, 6, 11, 12, col); t.r(4, 2, 11, 2, trim)
    elif kind == 'boots': t.r(3, 5, 6, 13, col); t.r(9, 5, 12, 13, col); t.r(2, 12, 6, 13, dk(col)); t.r(9, 12, 13, 13, dk(col)); t.r(3, 5, 6, 6, trim); t.r(9, 5, 12, 6, trim)
    elif kind == 'helm': t.disc(7.5, 7.5, 5, col); t.r(5, 6, 10, 9, c('60d0f0')); t.p(6, 6, WHITE); t.r(4, 12, 11, 13, dk(col)); t.r(7, 1, 8, 2, trim)
    return t.outline()
def weapon(kind, col, grip=OAK):
    t = P()
    if kind == 'cutlass': t.line(4, 12, 11, 4, col); t.line(5, 12, 12, 4, lt(col, 1.3)); t.line(11, 4, 13, 5, col); t.r(2, 11, 5, 13, grip); t.line(2, 10, 5, 14, GOLD)
    elif kind == 'axe': t.line(4, 13, 11, 3, grip); t.r(9, 2, 13, 6, col); t.r(9, 2, 13, 2, lt(col)); t.p(8, 4, col)
    elif kind == 'pike': t.line(2, 14, 12, 3, grip); t.r(11, 1, 13, 3, col); t.p(13, 1, WHITE)
    elif kind == 'pistol': t.r(3, 6, 11, 7, col); t.r(3, 6, 11, 6, lt(col)); t.r(2, 7, 5, 11, grip); t.p(6, 8, GOLD); t.p(12, 6, INK)
    elif kind == 'musket': t.r(1, 7, 14, 8, col); t.r(1, 8, 5, 11, grip); t.p(7, 9, GOLD); t.p(15, 7, INK)
    elif kind == 'blunderbuss': t.r(3, 7, 10, 8, col); t.r(10, 5, 13, 10, col); t.r(2, 8, 5, 12, grip); t.p(13, 7, INK)
    elif kind == 'grenado': t.disc(7.5, 9, 4, c('2a2a2a')); t.r(7, 3, 8, 5, ROPE); t.p(9, 2, c('f08030')); t.p(6, 7, c('8a8a8a'))
    elif kind == 'hook': t.line(4, 13, 9, 7, grip); t.line(9, 7, 12, 4, col); t.line(12, 4, 13, 7, col); t.line(9, 7, 7, 3, col); t.line(7, 3, 4, 5, col)
    elif kind == 'spyglass': t.r(2, 8, 7, 10, BRASS); t.r(8, 7, 13, 11, col); t.p(13, 9, c('60d0f0'))
    elif kind == 'shovel': t.line(4, 13, 10, 5, grip); t.r(9, 2, 13, 6, col); t.r(9, 2, 13, 2, lt(col))
    return t.outline()

# ---- ground, stone and timber ----------------------------------------------------------------------------------------------------------

# ==== Dungeoneering ======================================================================================================================
import csv, re
STONE_D = c('5a5a66')
def slug(s): return re.sub(r'[^a-z0-9]+', '_', s.lower().replace("'", '')).strip('_')
def hue(col, name, amount=0.06):
    """The same colour, a little its own for each name."""
    h, l, s_ = colorsys.rgb_to_hls(*[v / 255 for v in col[:3]]); k = rnd(name)
    r_, g_, b_ = colorsys.hls_to_rgb((h + k.uniform(-amount, amount)) % 1.0, max(0.12, min(0.9, l + k.uniform(-0.06, 0.06))), s_)
    return (int(r_ * 255), int(g_ * 255), int(b_ * 255), 255)
GRADE = [None, c('aaaab2'), c('60c860'), c('5a96f0'), c('b464e6'), c('f0be3c')]
GRADE_NAME = [None, 'Common', 'Uncommon', 'Rare', 'Epic', 'Legendary']

# ---- what a thing is made of, by a word in its name -----------------------------------------------------------------------------------
STUFF = [('bronze', 'c8964a', 'e0c080'), ('iron', '8a8e9a', 'c0c4d0'), ('steel', 'b0b8c4', 'e0e8f0'), ('blacksteel', '3a3e4a', '8a8e9a'), ('titanium', 'c8d4e0', 'ffffff'), ('granite', '8a8480', 'b8b0a8'), ('stone', '8a8a84', 'b8b8b0'), ('coldstone', '7a8a9a', 'c0d0e0'),
         ('glacial', '80d0f0', 'e0f8ff'), ('frozen', '80c8f0', 'e0f4ff'), ('ice', '90d8f8', 'f0fcff'), ('snow', 'd8e8f8', 'ffffff'), ('winter', 'a0c8e8', 'ffffff'), ('water', '3a80d0', 'a0d0ff'), ('ocean', '2a70b0', '80c8f0'), ('sea', '2a8a9a', '80e0d0'),
         ('bluefire', '3a70f0', 'a0e0ff'), ('magma', 'e05020', 'ffc040'), ('molten', 'f06020', 'ffd060'), ('flame', 'f07020', 'ffe060'), ('fire', 'f06020', 'ffd060'), ('heat', 'e86030', 'ffc060'), ('infernal', 'c02818', 'ff8030'), ('solar', 'f0a020', 'fff0a0'),
         ('sun', 'f0b020', 'fff0a0'), ('blazing', 'f08020', 'ffe080'), ('phoenix', 'f06030', 'ffd040'), ('demon', '8a1a1a', 'f04030'), ('devilish', '9a1a1a', 'f05030'), ('crimson', 'b01828', 'f06070'), ('ruby', 'd02838', 'ff8090'), ('blood', '8a0a14', 'e03040'),
         ('onyx', '2a2a34', '6a6a80'), ('dark', '2a2434', '7a5a9a'), ('void', '3a1a5a', 'b060f0'), ('vampiric', '5a0a2a', 'd03060'), ('ghost', '9ad0c0', 'e0fff8'), ('phantom', '8ac0d0', 'e0f8ff'), ('ghoul', '6a8a5a', 'c0e0a0'), ('soul', '70d0c0', 'e0fff8'),
         ('sapphire', '2a50c8', '80b0ff'), ('amethyst', '8a40c8', 'd0a0ff'), ('violet', '7a4ad0', 'c8a8ff'), ('arcane', '8a50e0', 'e0b0ff'), ('mana', '4a70e8', 'a0d0ff'), ('magic', '7a60e0', 'd0c0ff'), ('runic', '4a90c0', 'a0f0ff'), ('moon', 'a8b8e0', 'f0f4ff'),
         ('jade', '3aa878', 'a0f0c8'), ('emerald', '2aa850', '90f0b0'), ('toxic', '70c020', 'd0ff60'), ('poison', '60a830', 'c0f070'), ('nature', '4a9a3a', 'b0e080'), ('mossy', '5a8a4a', 'a0c880'), ('bioforged', '50b060', 'c0ffa0'), ('earth', '8a6a3a', 'c8a870'),
         ('gold', 'e8b830', 'fff0a0'), ('royal', 'd8a828', 'fff0a0'), ('king', 'e0b030', 'fff0b0'), ('holy', 'f0e080', 'ffffff'), ('heavenly', 'f0e8a0', 'ffffff'), ('angelic', 'f0f0d0', 'ffffff'), ('celestial', 'c0d8ff', 'ffffff'), ('godly', 'f8e890', 'ffffff'),
         ('god', 'f8e890', 'ffffff'), ('divine', 'f8f0b0', 'ffffff'), ('luminous', 'f0f0a0', 'ffffff'), ('light', 'f0f0c0', 'ffffff'), ('crystal', 'a0e8f0', 'ffffff'), ('diamond', 'b0f0f8', 'ffffff'), ('pearl', 'f0c8d8', 'ffffff'), ('sakura', 'f0a0c0', 'ffe0f0'),
         ('pink', 'f080b0', 'ffd0e8'), ('bone', 'e8e0c8', 'ffffff'), ('tooth', 'e8e0c8', 'ffffff'), ('tribal', 'a06a3a', 'e0c080'), ('goblin', '6a9a3a', 'c0e080'), ('beast', '8a5a3a', 'd0a070'), ('barbaric', '8a5a3a', 'd0a070'), ('elven', '70b070', 'e0ffd0'),
         ('pirate', '8a5a30', 'f0c840'), ('captain', '2a3a6a', 'f0c840'), ('kraken', '6a3a8a', 'f080c0'), ('serpent', '3a9a8a', 'a0f0e0'), ('shark', '7a8a9a', 'e0e8f0'), ('samurai', 'a02828', 'f0c840'), ('honorable', 'c0c8d0', 'c03030'), ('ancestral', '6a8a9a', 'f0e0a0'),
         ('hextech', '30c0d0', 'f0ff60'), ('plasma', 'e040c0', 'ffa0f0'), ('fusion', 'f0a030', '60e0ff'), ('industrial', '8a7a5a', 'f0a030'), ('inventor', 'c89a4a', '60e0ff'), ('war-forged', '7a5a4a', 'f08030'), ('electron', '60d0ff', 'ffffff'), ('energized', '80f0a0', 'ffffff'),
         ('galactic', '5a3ad0', '60f0ff'), ('space', '4a5a8a', 'c0d0ff'), ('alien', '50d070', 'e0ff80'), ('commander', '3a5a7a', 'f0c840'), ('star', 'f0d040', 'ffffff'), ('timelost', '6a9ab0', 'f0e0a0'), ('ancient', '9a8a5a', 'e0d090'), ('elder', '7a6a9a', 'd0c0f0'),
         ('overlord', '5a1a6a', 'f04080'), ('salvaged', '8a7a6a', 'c0b090'), ('rouge', '8a2a3a', 'e0c0a0'), ('raider', '6a5a4a', 'c03030'), ('hunter', '4a6a4a', 'c0a060'), ('mythical', '4a8ad0', 'f0f080'), ('titan', '6a7a9a', 'f0c060'), ('glorious', 'f0c040', 'ffffff'),
         ('desert', 'd8b060', 'fff0b0'), ('novice', '8a8a94', 'c0c0c8'), ('apprentice', '7a6a4a', '60a0e0'), ('mercenary', '7a6a5a', 'c0a070'), ('crusader', 'c0c4d0', 'c03030'), ('knight', 'b03030', 'e0e0e8'), ('guardian', '8a94a8', 'f0c840'), ('plate', '9aa0b0', 'e0e4f0'),
         ('forgotten', '6a7a7a', 'a0c0c0'), ('enchanted', '5a80e0', 'c0e0ff'), ('spiked', '7a5a3a', 'c0c4d0'), ('grand', 'b0b8c8', 'f0c840'), ('mighty', 'a0a8b8', 'f0c840'), ('reinforced', '9aa0aa', 'd0d8e0'), ('dragon', 'c03828', 'f0c040'), ('eagle', 'c8a060', 'ffffff')]
def stuff(name):
    low = name.lower()
    for word, a, b in STUFF:
        if word in low: return hue(c(a), name, 0.03), hue(c(b), name + '!', 0.03)
    k = rnd(name); col = tuple(k.randint(90, 210) for _ in range(3)) + (255,)
    return col, lt(col, 1.4)
KINDS = [('fists', ('fist',)), ('fan', ('fan',)), ('stars', ('stars',)), ('wings', ('wings',)), ('anchor', ('anchor',)), ('book', ('spellbook', 'tome')), ('grail', ('grail',)), ('device', ('device', 'orb')), ('trident', ('trident',)), ('scythe', ('scythe', 'sickle', 'reaver')),
         ('katana', ('katana', 'kris', 'cutlass', 'scimitar', 'rapier')), ('greataxe', ('greataxe', 'battleaxe', 'cleaver', 'royal axe', 'axes', 'bloodthrister')), ('axe', ('axe',)), ('hammer', ('hammer', 'maul', 'mallet', 'club', 'pulverizer', 'warhammer')),
         ('spear', ('spear', 'pike', 'polearm', 'battlestaff')), ('wand', ('wand',)), ('staff', ('staff', 'greatstaff')), ('dagger', ('dagger', 'knives', 'knife', 'fang', 'slicer', 'star')), ('greatsword', ('greatsword', 'claymore', 'godsword', 'slayer', 'fury', 'strongsword', 'flatblade', 'rageblade', 'splitter')),
         ('sword', ('sword', 'blade', 'defender', 'triumph'))]
def kind_of(name):
    low = name.lower()
    for k, words in KINDS:
        if any(w in low for w in words): return k
    return 'sword'
def arm(name, cls):
    """The picture of a weapon: what it is by a word in its name, what it is made of by another, and no two quite alike."""
    a, b = stuff(name); kind = kind_of(name); grip = hue(c('6a4a2a'), name + 'grip', 0.04); low = name.lower(); k = rnd(name + 'shape')
    def one(t, flip=False, short=0):
        X = (lambda x: 15 - x) if flip else (lambda x: x)
        L = lambda x0, y0, x1, y1, col: t.line(X(x0), y0, X(x1), y1, col)
        if kind in ('sword', 'greatsword', 'dagger', 'katana'):
            tip = (12, 2) if kind != 'dagger' else (10, 5); tip = (tip[0] - short, tip[1] + short)
            L(5, 10, tip[0], tip[1], a); L(6, 10, tip[0] + 1, tip[1], b if kind != 'katana' else a); L(5, 9, tip[0], tip[1] - 1 + (2 if kind == 'katana' else 0), a)
            if kind == 'greatsword': L(4, 10, tip[0] - 1, tip[1] - 1, a); L(7, 10, tip[0] + 1, tip[1] + 1, b)
            L(3, 9, 7, 13, lt(grip) if kind != 'katana' else GOLD); L(2, 13, 4, 11, grip); t.p(X(1), 14, b if k.random() < 0.6 else GOLD)
        elif kind in ('axe', 'greataxe'):
            L(3, 13, 11, 3, grip); L(4, 13, 12, 3, dk(grip)); t.r(*(sorted((X(8), X(12)))[0], 2, sorted((X(8), X(12)))[1], 6), a); t.r(*(sorted((X(11), X(13)))[0], 1, sorted((X(11), X(13)))[1], 7), b)
            if kind == 'greataxe': t.r(*(sorted((X(6), X(8)))[0], 1, sorted((X(6), X(8)))[1], 4), a); t.p(X(5), 2, b)
        elif kind == 'hammer':
            L(3, 13, 10, 5, grip); L(4, 13, 11, 5, dk(grip)); x0, x1 = sorted((X(7), X(13))); t.r(x0, 2, x1, 6, a); t.r(x0, 2, x1, 2, b); t.r(x0, 6, x1, 6, dk(a))
            if 'spiked' in low or k.random() < 0.3: t.p(x0 - 1, 4, b); t.p(x1 + 1, 4, b); t.p((x0 + x1) // 2, 1, b)
        elif kind in ('spear', 'trident'):
            L(2, 14, 11, 4, grip); L(11, 4, 13, 2, a); t.p(X(12), 2, b); t.p(X(13), 3, b)
            if kind == 'trident' or 'pike' in low: t.p(X(10), 2, a); t.p(X(13), 5, a); t.p(X(9), 1, b); t.p(X(14), 6, b)
        elif kind == 'scythe':
            L(3, 14, 10, 3, grip); L(10, 3, 13, 4, a); L(13, 4, 14, 8, a); L(10, 2, 14, 4, b); t.p(X(14), 9, b)
        elif kind == 'staff':
            L(3, 14, 10, 5, grip); L(4, 14, 11, 5, dk(grip)); t.disc(X(11) + (0.5 if not flip else -0.5), 3.5, 2.6, a); t.p(X(11), 3, b); t.p(X(10), 2, WHITE); t.p(X(13), 6, b); t.p(X(8), 2, b)
        elif kind == 'wand':
            L(4, 13, 10, 6, grip); t.p(X(11), 5, a); t.p(X(12), 4, b); t.p(X(11), 3, b); t.p(X(13), 5, b); t.p(X(12), 2, a); t.p(X(14), 4, a); t.p(X(3), 14, GOLD)
    t = P()
    if kind == 'book': t.r(3, 3, 12, 13, a); t.r(3, 3, 4, 13, dk(a)); t.r(12, 4, 12, 13, PAPER); t.r(6, 6, 10, 9, b); t.p(8, 7, WHITE); t.r(5, 11, 10, 11, b)
    elif kind == 'grail': t.r(4, 3, 11, 7, a); t.r(5, 8, 10, 8, a); t.r(7, 9, 8, 11, dk(a)); t.r(5, 12, 10, 13, a); t.r(4, 3, 11, 3, b); t.p(7, 5, WHITE)
    elif kind == 'device': t.disc(7.5, 7.5, 5, a); t.disc(7.5, 7.5, 3, b); t.disc(6.5, 6.5, 1, WHITE); t.r(7, 12, 8, 14, grip)
    elif kind == 'fists': t.r(2, 6, 6, 11, a); t.r(9, 6, 13, 11, a); t.r(2, 6, 6, 6, b); t.r(9, 6, 13, 6, b); t.r(3, 12, 5, 13, grip); t.r(10, 12, 12, 13, grip); t.p(4, 8, WHITE); t.p(11, 8, WHITE)
    elif kind == 'fan': [t.line(7, 13, x, 4 + abs(x - 7) // 2, a if x % 2 else b) for x in range(2, 14)]; t.r(7, 12, 8, 14, grip)
    elif kind == 'stars': [t.line(cx - 3, cy, cx + 3, cy, a) or t.line(cx, cy - 3, cx, cy + 3, a) or t.line(cx - 2, cy - 2, cx + 2, cy + 2, b) or t.line(cx - 2, cy + 2, cx + 2, cy - 2, b) or t.p(cx, cy, INK) for cx, cy in ((5, 5), (10, 10))]
    elif kind == 'wings': t.line(7, 12, 2, 3, a); t.line(8, 12, 13, 3, a); t.line(6, 11, 2, 6, b); t.line(9, 11, 13, 6, b); t.line(5, 11, 3, 9, a); t.line(10, 11, 12, 9, a); t.r(7, 9, 8, 13, b)
    elif kind == 'anchor': t.r(7, 3, 8, 12, a); t.disc(7.5, 3, 1.6, b); t.line(3, 9, 7, 13, a); t.line(12, 9, 8, 13, a); t.r(5, 6, 10, 6, b)
    elif 'dual' in low or 'twin' in low or low.endswith('s') and kind in ('dagger', 'axe', 'katana', 'greataxe', 'scythe'): one(t, False, 2); one(t, True, 2)
    else: one(t)
    return t.outline()
def graded(pic, grade):
    """The same picture with the colour of its grade under it."""
    q = P(); q.im.paste(pic.im, (0, 0)); q.px = q.im.load(); q.r(1, 15, 14, 15, GRADE[grade]); q.p(0, 14, GRADE[grade]); q.p(15, 14, GRADE[grade]); return q
def piece(name, slot, guardian):
    a, b = stuff(name); t = P()
    if slot == 'head':
        t.r(4, 4, 11, 11, a); t.r(4, 4, 11, 5, b); t.r(5, 8, 10, 9, INK if guardian else dk(a)); t.r(3, 10, 4, 12, a); t.r(11, 10, 12, 12, a)
        if guardian: t.r(7, 2, 8, 4, b); t.r(7, 6, 8, 11, dk(a))
        else: t.p(7, 3, b); t.p(8, 3, b)
    else:
        t.r(3, 4, 12, 13, a); t.r(1, 4, 3, 8, a); t.r(12, 4, 14, 8, a); t.r(6, 3, 9, 4, dk(a)); t.r(3, 4, 12, 4, b); t.r(7, 5, 8, 13, dk(a))
        if guardian: t.r(1, 3, 4, 5, b); t.r(11, 3, 14, 5, b); t.r(4, 9, 11, 9, b)
        else: t.r(4, 11, 11, 11, b)
    return t.outline()

# ---- the ten ------------------------------------------------------------------------------------------------------------------------------------
# code, name, label, tier, wall (kind, colour), floor, back, glow, creatures (name, label, beast or person, body, accent, role), boss (name, label, beast, body, accent), decor (shape, colours), hanging
DUNGEONS = [
 ('DT', 'desert_temple', 'Desert Temple', 1, ('brick', 'd8b870'), 'c8a860', '8a6a3a', 'ffc040',
  [('sand_scarab', 'Sand scarab', 'creature', '3a5a8a', 'f0c840', 'swarm'), ('temple_mummy', 'Mummy', 'person', 'e0d8b8', 'c8b890', 'grunt'), ('tomb_archer', 'Tomb archer', 'person', '8a5a2a', 'f0c840', 'ranged'), ('dune_jackal', 'Dune jackal', 'quad', 'c89a50', '3a2a1a', 'fast'), ('sand_golem', 'Sand golem', 'golem', 'd8b060', '8a6a3a', 'tank')],
  ('sand_king', 'The Sand King', 'golem', 'e8c060', '3a8ad0'), [('urn', 'c07a4a', '8a4a2a'), ('pillar', 'd8c890', 'a89860'), ('statue', 'e0c060', '3a6ab0')], [('banner', 'c03030', 'f0c840'), ('chain', '8a7a4a', 'f0c840')]),
 ('WO', 'winter_outpost', 'Winter Outpost', 2, ('rock', 'a8c0d8'), 'd8e8f8', '5a7090', '80e0ff',
  [('frost_wolf', 'Frost wolf', 'quad', 'd0e0f0', '80c0f0', 'fast'), ('ice_soldier', 'Ice soldier', 'person', '80b0e0', 'e0f4ff', 'grunt'), ('snow_caster', 'Snow caster', 'person', 'f0f8ff', '60a0e0', 'ranged'), ('ice_imp', 'Ice imp', 'ape', '90d0f0', 'ffffff', 'swarm'), ('glacier_brute', 'Glacier brute', 'golem', '90c8e8', 'e0f8ff', 'tank')],
  ('ice_elemental', 'The Ice Elemental', 'golem', 'b0e8ff', '3a70d0'), [('crystal', '90d8f8', 'f0fcff'), ('crate', '7a8a9a', 'd0e0f0'), ('brazier', '5a6a7a', '80e0ff')], [('stalactite', 'b0e0f8', 'ffffff'), ('banner', '3a60b0', 'e0f0ff')]),
 ('PI', 'pirate_island', 'Pirate Island', 3, ('planks', '8a5a30'), 'a07840', '4a3018', 'ffd070',
  [('deck_rat', 'Bilge rat', 'rabbit', '6a5a4a', 'c0a080', 'swarm'), ('island_pirate', 'Pirate', 'person', 'a02828', '1a1a1a', 'grunt'), ('pirate_gunner', 'Pirate gunner', 'person', '2a3a6a', 'f0c840', 'ranged'), ('cove_parrot', 'Cutthroat parrot', 'bird', 'e03030', 'f0d040', 'fast'), ('pirate_brute', 'Pirate brute', 'ape', '8a5a3a', 'c03030', 'tank')],
  ('pirate_captain', 'The Pirate Captain', 'ape', '2a2a4a', 'f0c840'), [('barrel', '8a5a30', '5a5560'), ('crate', 'a07840', '6a4a22'), ('anchor', '5a5a66', '8a8e9a')], [('chain', '7a7e8a', 'c0c4d0'), ('lantern', '3a2a22', 'ffd070')]),
 ('KC', 'kings_castle', "King's Castle", 4, ('brick', '8a8a94'), '7a7a84', '4a4a54', 'ffb040',
  [('castle_hound', 'Castle hound', 'quad', '5a4a3a', 'c03030', 'fast'), ('castle_guard', 'Castle guard', 'person', 'c0c4d0', 'c03030', 'grunt'), ('royal_archer', 'Royal archer', 'person', '3a6a3a', 'f0c840', 'ranged'), ('court_jester', 'Mad jester', 'person', 'c03090', '30c0c0', 'swarm'), ('armoured_ogre', 'Armoured ogre', 'golem', '7a8a6a', 'c0c4d0', 'tank')],
  ('beast_master', 'The Beast Master', 'lion', 'a06a3a', 'f0c840'), [('statue', 'b0b8c8', 'f0c840'), ('pillar', '9a9aa4', '6a6a74'), ('brazier', '4a4a54', 'ffb040')], [('banner', '2a3a9a', 'f0c840'), ('chain', '6a6a74', 'a0a0aa')]),
 ('UW', 'underworld', 'The Underworld', 5, ('rock', '4a2a2a'), '3a2222', '2a1414', 'ff5020',
  [('hell_hound', 'Hellhound', 'quad', '3a1a1a', 'f06020', 'fast'), ('lesser_demon', 'Lesser demon', 'ape', '9a1a1a', 'f0c040', 'grunt'), ('fire_caster', 'Fire caster', 'person', '5a1a1a', 'f08020', 'ranged'), ('ember_bat', 'Ember bat', 'bird', '4a1a1a', 'f06020', 'swarm'), ('lava_golem', 'Lava golem', 'golem', '3a2a2a', 'f06020', 'tank')],
  ('demon_lord', 'The Demon Lord', 'dragon', '8a1414', 'f0a020'), [('bones', 'e8e0d0', '9a8a7a'), ('crystal', 'f05020', 'ffc040'), ('brazier', '2a1a1a', 'ff5020')], [('stalactite', '5a2a2a', '8a3a3a'), ('chain', '3a3a3a', '8a2a2a')]),
 ('SP', 'samurai_palace', 'Samurai Palace', 6, ('planks', '8a3a2a'), 'c8b890', '5a2a22', 'ffd0a0',
  [('palace_fox', 'Spirit fox', 'quad', 'f0f0f0', 'f08030', 'fast'), ('palace_samurai', 'Samurai', 'person', 'a02828', '2a2a2a', 'grunt'), ('palace_archer', 'Palace archer', 'person', '2a2a4a', 'f0c840', 'ranged'), ('paper_spirit', 'Paper spirit', 'wraith', 'f0e8d8', 'c03030', 'swarm'), ('oni_guard', 'Oni', 'golem', 'c03030', 'f0e0a0', 'tank')],
  ('samurai_lord', 'The Shogun', 'tiger', 'c03030', 'f0c840'), [('urn', '3a5a8a', 'f0f0f0'), ('statue', '6a8a6a', 'c0d0b0'), ('crate', 'c8b890', '8a3a2a')], [('lantern', 'c03030', 'ffd0a0'), ('banner', 'f0f0f0', 'c03030')]),
 ('TC', 'the_canals', 'The Canals', 7, ('brick', '5a7a7a'), '4a6a6a', '2a4444', '60f0c0',
  [('canal_leech', 'Canal leech', 'serpent', '3a5a3a', '90c060', 'swarm'), ('canal_thug', 'Canal thug', 'person', '3a4a4a', '8a2a2a', 'grunt'), ('canal_alchemist', 'Canal alchemist', 'person', '4a2a6a', '60f0a0', 'ranged'), ('sewer_croc', 'Canal crocodile', 'tortoise', '4a7a4a', 'c0d060', 'tank'), ('canal_wisp', 'Marsh wisp', 'wraith', '60f0c0', 'f0fff8', 'fast')],
  ('canal_overlord', 'The Overlord', 'wyrm', '5a1a6a', 'f04080'), [('barrel', '4a6a5a', '2a3a3a'), ('pillar', '6a8a8a', '3a5a5a'), ('machine', '5a6a6a', '60f0c0')], [('chain', '4a5a5a', '8aa0a0'), ('stalactite', '3a6a5a', '60c0a0')]),
 ('GH', 'ghastly_harbor', 'Ghastly Harbor', 8, ('planks', '3a4a4a'), '2a3a3a', '1a2424', '60ffd0',
  [('harbor_crab', 'Ghost crab', 'creature', '6ab0a0', 'e0fff8', 'swarm'), ('drowned_deckhand', 'Drowned deckhand', 'person', '2a4a44', '9ad0c0', 'grunt'), ('ghost_gunner', 'Ghost gunner', 'person', '3a5a6a', '60ffd0', 'ranged'), ('harbor_wraith', 'Harbour wraith', 'wraith', '8ad0c0', 'ffffff', 'fast'), ('barnacle_hulk', 'Barnacle hulk', 'golem', '3a5a4a', '9ad0c0', 'tank')],
  ('sea_serpent_king', 'The Sea Serpent', 'serpent', '2a7a6a', '60ffd0'), [('anchor', '3a5a5a', '6ab0a0'), ('barrel', '3a4a44', '6ab0a0'), ('bones', 'c0e0d8', '6a8a84')], [('lantern', '2a3a3a', '60ffd0'), ('chain', '3a4a4a', '6a8a8a')]),
 ('SS', 'steampunk_sewers', 'Steampunk Sewers', 9, ('paving', '8a6a3a'), '6a5a4a', '3a2e22', 'ffb060',
  [('gear_rat', 'Clockwork rat', 'rabbit', 'c89a4a', '60e0ff', 'swarm'), ('sewer_engineer', 'Mad engineer', 'person', '6a5a3a', 'f0a030', 'grunt'), ('tesla_caster', 'Spark caster', 'person', '3a4a6a', '60e0ff', 'ranged'), ('steam_drone', 'Steam drone', 'bird', 'b08850', 'ffffff', 'fast'), ('boiler_golem', 'Boiler golem', 'golem', 'a07840', 'f06020', 'tank')],
  ('mad_inventor', 'The Inventor', 'golem', 'c8a040', '60e0ff'), [('machine', 'a07840', '60e0ff'), ('barrel', '8a6a3a', 'c8a040'), ('crate', '6a5a4a', 'c8a040')], [('chain', 'a07840', 'e0c080'), ('lantern', '6a4a22', 'ffb060')]),
 ('OO', 'orbital_outpost', 'Orbital Outpost', 10, ('paving', '6a7a9a'), '4a5a7a', '1a2038', '80c0ff',
  [('void_mite', 'Void mite', 'creature', '5a3ad0', '60f0ff', 'swarm'), ('outpost_trooper', 'Outpost trooper', 'person', 'c0d0e0', '3a5a9a', 'grunt'), ('plasma_caster', 'Plasma caster', 'person', '4a2a8a', 'f060f0', 'ranged'), ('star_hunter', 'Star hunter', 'phoenix', '60c0ff', 'ffffff', 'fast'), ('alien_brute', 'Alien brute', 'golem', '50d070', 'e0ff80', 'tank')],
  ('alien_emperor', 'The Emperor', 'wyrm', '3a2a8a', '60f0ff'), [('machine', '6a7a9a', '80c0ff'), ('crystal', '8060f0', 'f0e0ff'), ('crate', '8a9ab0', '3a5a9a')], [('lantern', '4a5a7a', '80c0ff'), ('stalactite', '5a6a8a', 'a0c0ff')])]
STONES = ['amethyst', 'sapphire', 'emerald', 'ruby', 'diamond', 'dg_palace_keystone', 'dg_canal_keystone', 'dg_harbor_keystone', 'dg_sewer_keystone', None]
def shape(kind, a, b):
    t = P()
    if kind == 'urn': t.r(6, 5, 9, 6, a); t.disc(7.5, 10.5, 3.6, a); t.r(6, 14, 9, 15, b); t.r(5, 9, 10, 9, b); t.p(6, 8, lt(a))
    elif kind == 'pillar': t.r(5, 2, 10, 15, a); t.r(4, 0, 11, 2, a); t.r(4, 14, 11, 15, a); t.r(5, 2, 5, 14, lt(a)); t.r(8, 3, 8, 13, b); t.r(10, 3, 10, 13, b)
    elif kind == 'statue': t.r(4, 13, 11, 15, b); t.r(6, 6, 9, 12, a); t.disc(7.5, 4, 2, a); t.r(4, 7, 5, 10, a); t.r(10, 7, 11, 10, a); t.p(7, 4, INK); t.r(6, 6, 9, 6, lt(a))
    elif kind == 'crate': t.r(2, 6, 13, 15, a); t.r(2, 6, 13, 6, lt(a)); t.r(2, 10, 13, 10, b); t.r(7, 6, 8, 15, b)
    elif kind == 'barrel': t.r(4, 5, 11, 15, a); t.r(3, 7, 12, 13, a); t.r(3, 7, 12, 7, b); t.r(3, 12, 12, 12, b); t.r(4, 5, 11, 5, lt(a))
    elif kind == 'bones': t.disc(5, 12, 2.5, a); t.p(4, 12, INK); t.p(6, 12, INK); t.r(8, 13, 14, 14, a); t.r(9, 11, 9, 15, a); t.r(12, 11, 12, 15, a); t.r(3, 15, 7, 15, b)
    elif kind == 'crystal': t.line(4, 15, 6, 6, a); t.line(5, 15, 7, 6, a); t.line(8, 15, 8, 2, b); t.line(9, 15, 9, 3, a); t.line(11, 15, 12, 8, a); t.line(12, 15, 13, 9, b); t.r(3, 15, 13, 15, dk(a))
    elif kind == 'brazier': t.r(7, 9, 8, 15, a); t.r(5, 14, 10, 15, a); t.r(4, 7, 11, 9, a); t.r(5, 4, 10, 6, b); t.r(6, 2, 9, 4, lt(b)); t.p(7, 1, WHITE)
    elif kind == 'anchor': t.r(7, 3, 8, 13, a); t.disc(7.5, 3, 1.8, b); t.line(3, 10, 7, 14, a); t.line(12, 10, 8, 14, a); t.r(5, 6, 10, 6, b)
    elif kind == 'machine': t.r(3, 6, 12, 15, a); t.r(3, 6, 12, 6, lt(a)); t.disc(7.5, 10.5, 2.4, dk(a)); t.disc(7.5, 10.5, 1.2, b); t.r(5, 2, 6, 5, a); t.p(5, 1, b); t.r(10, 8, 11, 9, b)
    elif kind == 'banner': t.r(3, 0, 12, 1, c('6a4a2a')); t.r(4, 2, 11, 11, a); t.r(4, 12, 6, 13, a); t.r(9, 12, 11, 13, a); t.r(6, 4, 9, 7, b); t.r(4, 2, 4, 11, lt(a))
    elif kind == 'chain': [t.r(7, y, 8, y + 1, a if (y // 2) % 2 == 0 else b) for y in range(0, 12, 2)]; t.r(6, 12, 9, 14, a); t.p(7, 13, b)
    elif kind == 'stalactite': t.r(3, 0, 12, 1, a); t.r(4, 2, 6, 6, a); t.r(5, 7, 5, 10, a); t.r(8, 2, 11, 4, a); t.r(9, 5, 10, 12, a); t.p(9, 13, b); t.p(5, 11, b); t.r(4, 2, 4, 5, b)
    elif kind == 'lantern': t.r(7, 0, 8, 3, c('5a5560')); t.r(5, 4, 10, 5, a); t.r(5, 6, 10, 11, b); t.r(5, 6, 5, 11, a); t.r(10, 6, 10, 11, a); t.r(5, 12, 10, 13, a); t.p(7, 8, WHITE)
    return t

# Things every dungeon has.
block(rock('dg_bedrock', c('1a1a22')), 'dg_bedrock', 'Dungeon rock', 1, 0.04, removable=False, mine_time=99.0)
for n, lab, glow, which in (('dg_mark', 'Stirring', '8a8aa0', 1), ('dg_boss_mark', 'Presence', 'c03030', 2)):
    t = P(); t.p(7, 13, c(glow)); t.p(8, 14, c(glow)); t.p(6, 15, c(glow)); t.p(9, 15, c(glow)); t.save('tiles', n)
    tile(n, lab, False, removable=False, drop=None, dungeon_spawn=which, model='panel')
t = P().r(2, 6, 13, 14, c('4a3a2a')).r(2, 6, 13, 8, c('6a5030')).r(2, 9, 13, 9, GOLD).r(7, 8, 8, 11, GOLD).r(2, 6, 2, 14, GOLD).r(13, 6, 13, 14, GOLD).p(7, 9, c('60d0f0'))
t.save('tiles', 'dg_reward_chest'); icon_of('dg_reward_chest'); tile('dg_reward_chest', 'Dungeon chest', False, removable=False, storage={'columns': 4, 'rows': 4}, light=9, model='box'); item('dg_reward_chest', 'Dungeon chest', 3, 'special', 1.0, places='dg_reward_chest', container={'slots': 16})
t = P().r(3, 2, 12, 15, c('5a5a66')).r(5, 4, 10, 15, c('1a2a4a')).r(6, 6, 9, 15, c('3a6ad0')).r(7, 8, 8, 15, c('a0d0ff')).r(3, 2, 12, 2, c('8a8e9a')).p(7, 3, GOLD).p(8, 3, GOLD)
t.save('tiles', 'dg_way_out'); tile('dg_way_out', 'Way out', False, removable=False, drop=None, leads_to='dg_dungeon', dungeon_exit=True, light=10, model='panel')
t = P().r(2, 1, 13, 15, c('3a3a44')).r(4, 3, 11, 15, c('2a1a3a')).r(5, 5, 10, 15, c('6a3ab0')).r(6, 7, 9, 15, c('b080f0')).r(2, 1, 13, 1, c('8a8e9a')).p(4, 2, RED).p(11, 2, RED).disc(7.5, 10, 1.4, WHITE)
t.save('tiles', 'dungeon_gate'); icon_of('dungeon_gate'); tile('dungeon_gate', 'Dungeon gate', False, tool='pickaxe', mine_time=0.6, bench=True, dungeon_gate={'beside': 'portal', 'reach': 8}, light=8, model='panel')
item('dungeon_gate', 'Dungeon gate', 2, 'benches', 3.0, places='dungeon_gate'); rec('workbench', 'dungeon_gate', [('stone', 20), ('iron_ingot', 4), ('gold_coin', 10)], level=2)
t = P().r(1, 10, 9, 15, c('5a5560')).r(2, 11, 8, 13, c('f06020')).r(3, 11, 7, 11, c('ffd060')).r(10, 8, 15, 10, c('2a2a2a')).r(11, 11, 14, 12, c('8a5a30')).r(1, 10, 9, 10, GOLD).r(11, 4, 12, 7, c('c0c8d0')).p(11, 3, c('60d0f0'))
t.save('tiles', 'loot_forge'); icon_of('loot_forge'); tile('loot_forge', 'Forge of fortune', False, tool='pickaxe', mine_time=0.6, bench=True, loot_forge=True, light=9)
item('loot_forge', 'Forge of fortune', 2, 'benches', 3.0, places='loot_forge'); rec('workbench', 'loot_forge', [('stone', 12), ('iron_ingot', 6), ('charcoal', 4)], level=2)

LOOT = {}
for row in csv.DictReader(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'dungeoneering_loot.csv'))): LOOT.setdefault(row['dungeon'], []).append(row)
dungeons, used, prev_top, prev_health, summary = [], set(), 0, 0, []
ROLE = {'swarm': (1.5, 24, 15, 2, 3), 'grunt': (5, 12, 30, 4, 2), 'fast': (3, 16, 20, 2, 2), 'tank': (12, 8, 45, 6, 1), 'ranged': (3, 14, 50, 5, 2)}   # health in blows, blows to kill, rest, step ticks, how many at once
for k, (code, name, label, tier, (wkind, wcol), fcol, bcol, glow, foes, boss, decor_, hang_) in enumerate(DUNGEONS):
    rows = LOOT[code]; d = 'dg_' + name
    # What it is built of.
    block(block_tex(wkind, d + '_wall', c(wcol), dk(c(wcol), 0.7)) if wkind == 'brick' else block_tex(wkind, d + '_wall', c(wcol)), d + '_wall', label + ' wall', min(tier, 6), 0.04, removable=False); items.pop(); 
    block(paving(d + '_floor', c(fcol)), d + '_floor', label + ' floor', min(tier, 6), 0.04, removable=False); items.pop()
    block(rock(d + '_back', c(bcol), 18), d + '_back', label + ' backwall', min(tier, 6), 0.04, removable=False); items.pop()
    for t_ in tiles[-3:]: t_['drop'] = None; t_.pop('drop')
    q = P().r(1, 0, 14, 15, dk(c(wcol), 0.5)).r(3, 0, 12, 15, c(bcol)).r(3, 4, 12, 5, c(glow)).r(3, 10, 12, 11, c(glow)).r(7, 0, 8, 15, dk(c(bcol), 0.6)).p(6, 7, c(glow)).p(9, 7, c(glow))
    q.save('tiles', d + '_door'); tile(d + '_door', 'Sealed door', True, removable=False, mine_time=99.0, dungeon_door=True, light=5); tiles[-1].pop('drop')
    # The same wall and floor to build with at home, made at the builder's bench from what is left of them.
    for part, col, kind_ in (('wall', wcol, wkind), ('floor', fcol, 'paving')):
        n = '%s_%s_block' % (d, part)
        block(block_tex(kind_, n, c(col), dk(c(col), 0.7)) if kind_ == 'brick' else block_tex(kind_, n, c(col)), n, '%s %s' % (label, part), min(tier, 6), 0.04)
        rec('builder_bench', n, [('stone', 4), ('dg_%s_dust' % name, 1)], 4, time=80)
    thing(trinket('dust', c(wcol), c(glow)), 'dg_%s_dust' % name, label + ' dust', min(tier, 6), 'nature', 0.3)
    deco(lantern_pic(dk(c(wcol), 0.5), c(glow)), d + '_lamp', label + ' lamp', min(tier, 6), [('dg_%s_dust' % name, 2), ('glass', 1)], 'builder_bench', light=13)
    decor_names, hang_names = [], []
    for i, (kind_, a, b) in enumerate(decor_):
        n = '%s_%s' % (d, kind_); deco(shape(kind_, c(a), c(b)), n, '%s %s' % (label, kind_), min(tier, 6), [('dg_%s_dust' % name, 2), ('stone', 2)], 'builder_bench', light=8 if kind_ in ('brazier', 'crystal', 'machine') else None); decor_names.append(n)
    for i, (kind_, a, b) in enumerate(hang_):
        n = '%s_%s' % (d, kind_); deco(shape(kind_, c(a), c(b)), n, '%s %s' % (label, kind_), min(tier, 6), [('dg_%s_dust' % name, 1), ('string', 1)], 'builder_bench', light=10 if kind_ == 'lantern' else None, model='panel'); hang_names.append(n)
    # What is found there.
    weapons = {}; armours = {}
    for r_ in rows: (armours if r_['class'] in ('armor', 'guardian') else weapons).setdefault(r_['name'], []).append(r_)
    order = sorted((n for n in weapons if weapons[n][0]['grade'] != '5'), key=lambda n: int(weapons[n][0]['power']))
    low = int(weapons[order[0]][0]['power']); top = max(int(r_['power']) for n in order for r_ in weapons[n])
    lootlist = []
    def loot_item(nm, lab, r_, rank, extra, pic, price_k):
        grade = int(r_['grade']); assert nm not in used and len(nm) <= 31, nm; used.add(nm)
        graded(pic, grade).save('items', nm)
        item(nm, lab[:31], tier, 'loot', price_k * (0.5 + 0.3 * grade) * (0.6 + 0.8 * rank), grade=grade, max_stack=1, **extra)
        lootlist.append({'item': nm, 'rank': round(rank, 3)})
    for n, grades in weapons.items():
        rank = order.index(n) / max(1, len(order) - 1) if n in order else 1.0; pic = arm(n, grades[0]['class'])
        for r_ in grades:
            cls = r_['class']
            loot_item('dg_%s_%s' % (slug(n)[:24], r_['grade']), n, r_, rank, dict(rolled={'power': int(r_['power']), 'upgrades': int(r_['upgrades']), 'gain': float(r_['gain']), 'class': cls}, range=7 if cls == 'mage' else None), pic, 1.0 if r_['grade'] != '5' else 3.0)
    lines = sorted(armours, key=lambda n: (armours[n][0]['class'], int(armours[n][0]['power'])))
    for n, grades in armours.items():
        guardian = grades[0]['class'] == 'guardian'; same = [x for x in lines if (armours[x][0]['class'] == 'guardian') == guardian]
        rank = (same.index(n) + 0.5) / len(same); clean_ = re.sub(r'\s*\((Hat|Chest)\)', '', n.replace(' | ', ' / ').replace('/', ' / ').replace('  ', ' ')).strip()
        for slot, suffix, word in (('head', 'h', 'helm'), ('chest', 'c', 'armour')):
            if ('(Hat)' in n and slot != 'head') or ('(Chest)' in n and slot != 'chest'): continue
            pic = piece(n, slot, guardian)
            for r_ in grades:
                roll = {'health': int(r_['power']), 'upgrades': int(r_['upgrades']), 'health_gain': float(r_['gain'])} if guardian else {'power': int(r_['power']), 'health': int(r_['health']), 'upgrades': int(r_['upgrades']), 'gain': float(r_['gain'])}
                lab = '%s %s' % (clean_ if len(clean_) + len(word) < 31 else clean_[:30 - len(word)].strip(), word)
                loot_item('dg_%s_%s_%s' % (slug(n + (' g' if guardian and 'guardian' not in n.lower() else ''))[:21], suffix, r_['grade']), lab, r_, rank, dict(rolled=roll, equip=slot, warmth=0.3), pic, 0.8)
    assert len(lootlist) <= 320, (code, len(lootlist))
    # Who is met there: their numbers set against what somebody brings from the dungeon before.
    blow = max(6, int(prev_top * 0.6)) if k else 6; can_take = 100 + 2 * prev_health
    names = []
    for n, lab, kind_, body, acc, role in foes:
        hp, kills, rest, step, cap = ROLE[role]; mn = 'dg_' + n; names.append(mn)
        base = {'name': mn, 'label': lab, 'texture': 'mobs/%s.png' % mn, 'walk': ['mobs/%s_walk.png' % mn, 'mobs/%s.png' % mn], 'hop': 0.06, 'step_ticks': step, 'move_chance': 0.2, 'turn_chance': 0.25, 'health': int(blow * hp), 'cap': 12,
                'drops': [drop('copper_coin', 0, 2 + tier), drop('dg_%s_dust' % name, 0, 1, 0.3)]}
        for f in (0, 1): (person(c(body), c(acc), f, hue(c('e0b890'), n, 0.02), role != 'ranged', k % 3) if kind_ == 'person' else beast(kind_, c(body), c(acc), f)).save('mobs', mn + ('_walk' if f else ''))
        if kind_ == 'bird' or kind_ == 'phoenix' or kind_ == 'wraith': base['flies'] = True; base['move_chance'] = 0.5
        if role == 'ranged': base['strike'] = {'power': max(1, can_take // kills), 'range': 9, 'rest_ticks': rest}
        else: base['sting'] = {'power': max(1, can_take // kills), 'rest_ticks': rest, 'sight': 12}
        mobs.append(clean(base))
    bn, blab, bkind, bbody, bacc = boss; mn = 'dg_' + bn; names.append(mn)
    for f in (0, 1): beast(bkind, c(bbody), c(bacc), f, big=True).save('mobs', mn + ('_walk' if f else ''))
    mobs.append({'name': mn, 'label': blab, 'texture': 'mobs/%s.png' % mn, 'walk': ['mobs/%s_walk.png' % mn, 'mobs/%s.png' % mn], 'hop': 0.0, 'step_ticks': 5, 'move_chance': 0.3, 'turn_chance': 0.2, 'health': int(blow * 90), 'cap': 1, 'size': [3, 3], 'boss': True,
                 'sting': {'power': max(1, can_take // 5), 'rest_ticks': 35, 'sight': 18}, 'strike': {'power': max(1, can_take // 9), 'range': 11, 'rest_ticks': 70}, 'drops': [drop('gold_coin', tier, 3 * tier), drop('dg_%s_dust' % name, 2, 5)]})
    dungeons.append(clean({'name': name, 'label': label, 'tier': tier, 'wall': d + '_wall', 'floor': d + '_floor', 'back': d + '_back', 'door': d + '_door', 'mark': 'dg_mark', 'boss_mark': 'dg_boss_mark', 'chest': 'dg_reward_chest', 'exit': 'dg_way_out',
                           'light': d + '_lamp', 'decor': decor_names, 'hanging': hang_names, 'rooms': [7, 10] if tier < 4 else [9, 12], 'mobs': names, 'stone': STONES[k], 'gold': TP[tier] * 2, 'loot': lootlist}))
    summary.append('%s: %d things, blow %d against %d, health %d' % (label, len(lootlist), blow, int(blow * 5), can_take))
    prev_top = top; prev_health = max(int(r_['health']) for n in armours for r_ in armours[n] if r_['class'] == 'armor')

# ---- portals VII to X, raised by what dungeons VI to IX give -----------------------------------------------------------------------------------
TIERCOL = {7: ramp('5a2a22', 'f0a0c0', 'ffffff'), 8: ramp('1a4444', '60f0c0', 'ffffff'), 9: ramp('1a2a2a', '60ffd0', 'ffffff'), 10: ramp('4a3018', 'ffb060', 'fff0e0')}
for tier, (stone, lab, col) in zip(range(7, 11), (('dg_palace_keystone', 'Palace keystone', 'f0a0c0'), ('dg_canal_keystone', 'Canal keystone', '60f0c0'), ('dg_harbor_keystone', 'Harbour keystone', '60ffd0'), ('dg_sewer_keystone', 'Sewer keystone', 'ffb060'))):
    thing(gemstone(c(col), True), stone, lab, 6, 'special', 3.0 * (tier - 5))
    for base, n, plab, cost, crystals in (('portal_6', 'dg_portal_%d' % tier, 'Portal', [(stone, 1)], 0), ('trade_portal_6', 'dg_trade_portal_%d' % tier, 'Trade portal', [(stone, 1), ('platinum_coin', (tier - 6) * 2)], (tier - 6) * 400)):
        e = {'cost': [{'item': i, 'count': n_} for i, n_ in cost]}
        if crystals: e['crystals'] = crystals
        clone_tile(base, n, plab, TIERCOL[tier], 'tint', bench_level=None, trade_level=None, extends=e); clone_item(base, n, plab + ' ' + NUM[tier], TIERCOL[tier], tier, 'portals', 1.0, 'tint', places=n)

# ---- written out -------------------------------------------------------------------------------------------------------------------------------
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v)) for k, v in obj.items()) + '\n}\n')
for r_ in recipes:
    if r_.get('formula') is None: r_.pop('formula', None)
for kind, group in (('tile', tiles), ('item', items), ('creature', mobs)):
    names = [t['name'] for t in group]; assert len(names) == len(set(names)), (kind, sorted({n for n in names if names.count(n) > 1}))
    long_ = [t['name'] for t in group if len(('caged_' if kind == 'creature' and not t.get('caged') else '') + t['name']) > 31]; assert not long_, long_
have_i = set(BI) | {i['name'] for i in items}; have_t = set(BT) | {t['name'] for t in tiles}
bad = sorted({i['item'] for r in recipes for i in r['inputs'] if i['item'] not in have_i} | {r['output'].get('item') for r in recipes if r['output'].get('item') not in have_i} | {d['item'] for m in mobs for d in m.get('drops', []) if d['item'] not in have_i})
bad += sorted({i['places'] for i in items if i.get('places') and i['places'] not in have_t} | {t['drop'] for t in tiles if t.get('drop') and t['drop'] not in have_i} | {d_[k_] for d_ in dungeons for k_ in ('wall', 'floor', 'back', 'door', 'light') if d_[k_] not in have_t})
bad += sorted({x for d_ in dungeons for x in d_['decor'] + d_['hanging'] if x not in have_t} | {d_['stone'] for d_ in dungeons if d_.get('stone') and d_['stone'] not in have_i})
assert not bad, bad
for n, i in enumerate(items):
    if i.get('equip'):
        o = {}
        for k_, v in i.items():
            o[k_] = v
            if k_ == 'equip': o['worn'] = 'worn/%s.png' % i['name']
        items[n] = o
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes}); dump('mobs.json', {'mobs': mobs})
dump('worldgen.json', {'dimensions': [{'name': 'dg_dungeon', 'label': 'Dungeon', 'generator': 'dungeon', 'height': 44, 'rock': 'dg_bedrock', 'edge': 'dg_bedrock', 'platform': 'dg_bedrock', 'ambient': 7}], 'dungeons': dungeons})
dump('trade.json', {'categories': [{'name': 'loot', 'label': 'Loot', 'free_only': True}]})
HARD = [('Easy', 1, 1, 0.0, 0.35, [600, 300, 100, 0, 0], [3, 4], 0.34, 1.0, 0), ('Hard', 3, 2, 0.25, 0.65, [350, 350, 220, 80, 0], [3, 5], 1.0, 2.5, 1), ('Insane', 10, 4, 0.5, 0.9, [150, 300, 300, 246, 4], [4, 5], 1.5, 5.0, 2), ('Nightmare', 30, 8, 0.7, 1.0, [50, 200, 320, 418, 12], [4, 6], 2.0, 10.0, 3)]
json.dump({'combat': {'health': 100}, 'dungeons': {'upgrade_cost': 2.0, 'upgrade_steps': 50, 'grades': [{'label': GRADE_NAME[g], 'color': list(GRADE[g][:3])} for g in range(1, 6)],
           'difficulties': [{'label': l, 'health': h, 'damage': dm, 'from': a, 'to': b, 'grades': gr, 'loot': lt_, 'stone': st, 'gold': go, 'more': mo} for l, h, dm, a, b, gr, lt_, st, go, mo in HARD]}}, open(os.path.join(mod, 'rules.json'), 'w'), indent=1)
json.dump({'label': 'Dungeoneering', 'icon': 'icon.png', 'description': 'Ten dungeons behind the ten tiers of portal: rooms to fight through, a boss, and a chest of loot whose numbers are its own.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
Image.open(os.path.join(TEX, 'tiles', 'dungeon_gate.png')).save(os.path.join(mod, 'icon.png'))
print('\n'.join(summary))
print('%d tiles, %d items, %d recipes, %d creatures, %d dungeons' % (len(tiles), len(items), len(recipes), len(mobs), len(dungeons)))
tools = os.path.dirname(os.path.abspath(__file__))
subprocess.run([sys.executable, os.path.join(tools, 'worn_art.py'), repo], check=True)
subprocess.run([sys.executable, os.path.join(tools, 'caged_art.py'), repo], check=True)
