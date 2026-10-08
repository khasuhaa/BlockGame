#!/usr/bin/env python3
"""Writes the Seeker of the Dao mod: its data files and its textures.

    python3 tools/mod_seeker_of_the_dao.py .

A road to immortality: five realms of ten stages, the qi to fill each, manuals, pills, bottlenecks and tribulations;
spirit herbs, spirit mining, blacksmithing, artificing, formations, techniques, talismans, spirit beasts and spirit
food; portals and trade to tier XI; and the World Tree, the Fallen Heaven and Heaven's Trials. The plan and what the
owner decided are in ModDescriptions/Xianxia.txt.

Metal things (ores, ingots, tools, armour) and what is sown are the game's own pictures in other colours; everything
else is drawn here. Either edit this and run it again, or edit the mod's files by hand and stop using this: not both.
"""
import colorsys, copy, json, os, random, subprocess, sys, zlib
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
DATA = os.path.join(repo, 'data')
mod = os.path.join(repo, 'mods/seeker_of_the_dao'); TEX = os.path.join(mod, 'textures')
for d in ('tiles', 'items', 'mobs', 'worn'): os.makedirs(os.path.join(TEX, d), exist_ok=True)
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

# ---- spirit stones: what qi is when it is stone ------------------------------------------------------------------
STONE = c('8a8a8a')
# name, label, tier, colour, what a block of it adds to the qi about it, how long one burns in a formation
STONES = [('spirit_stone', 'Spirit stone', 1, c('7ad8c0'), 0.5, 1200), ('mid_spirit_stone', 'Mid-grade spirit stone', 4, c('5aa0f0'), 1.0, 6000),
          ('high_spirit_stone', 'High-grade spirit stone', 7, c('b070f0'), 2.0, 30000), ('supreme_spirit_stone', 'Supreme spirit stone', 10, c('ffd860'), 4.0, 150000)]
HOSTC = {'stone': STONE, 'rootstone': c('4a4038'), 'heaven_marble': c('d8d8e8')}
for n, lab, tier, col, qi, burn in STONES:
    thing(gemstone(col, tier > 4), n, lab, tier, 'ores', 1.0)
    t = P(mix(col, c('202030'), 0.25)); t.speck(n, lt(col, 1.3), 40).speck(n + 'd', dk(col, 0.7), 30); t.r(0, 0, 15, 0, lt(col)); t.r(0, 15, 15, 15, dk(col, 0.6)); t.r(0, 0, 0, 15, lt(col)); t.r(15, 0, 15, 15, dk(col, 0.6))
    block(t, n + '_block', lab + ' block', tier, 9.0, qi=qi, qi_reach=6, light=6 + tier // 2, mine_time=1.0)
for n, host in (('spirit_stone', 'stone'), ('mid_spirit_stone', 'stone'), ('high_spirit_stone', 'heaven_marble')):
    col = next(s[3] for s in STONES if s[0] == n)
    rock(n + '_ore', HOSTC[host], vein=col).save('tiles', n + '_ore')
    tile(n + '_ore', next(s[1] for s in STONES if s[0] == n) + ' vein', ore=True, mine_time=1.2, drop=n, qi=0.2, qi_reach=4, light=4, hardness=7 if host == 'heaven_marble' else None)

# ---- the stuff of each place ---------------------------------------------------------------------------------------
# name, label, how it is drawn, colour, tier, and whatever else it is
BLOCKS = [('sect_brick', 'Sect brick', 'brick', '8a8478', 1, {}), ('sect_roof', 'Glazed roof tile', 'roof', '2f7a66', 1, {}), ('vermilion_planks', 'Vermilion planks', 'planks', 'a83a2a', 1, {'tool': 'axe'}),
          ('sect_paving', 'Sect paving', 'paving', 'a8a090', 1, {}), ('jade_brick', 'Jade brick', 'brick', '5fc89a', 3, {}),
          ('rootstone', 'Rootstone', 'rock', '4a4038', 5, {'mine_time': 1.0}), ('heaven_marble', 'Heaven marble', 'rock', 'd8d8e8', 7, {'mine_time': 1.2, 'hardness': 5}),
          ('cloud_jade', 'Cloud jade', 'rock', 'a8e0d0', 7, {'mine_time': 1.0, 'hardness': 5}), ('heaven_paving', 'Heaven paving', 'paving', 'e8e0c8', 7, {}), ('heaven_brick', 'Heaven brick', 'brick', 'c8c0d8', 7, {}),
          ('ruined_heaven_brick', 'Ruined heaven brick', 'brick', '8a849a', 7, {}), ('heaven_roof', 'Golden roof tile', 'roof', 'd0a030', 7, {}), ('heaven_soil', 'Heaven soil', 'rock', '6a5a7a', 7, {'tool': 'shovel', 'mine_time': 0.4}),
          ('scorched_marble', 'Scorched marble', 'rock', '4a4450', 9, {'mine_time': 1.2, 'hardness': 5}), ('star_glass', 'Star glass', 'rock', '2a2a6a', 9, {'light': 5, 'hardness': 7}),
          ('heaven_bridge', 'Heaven bridge', 'planks', 'c8b890', 7, {'tool': 'axe'}), ('star_wood', 'Starwood', 'planks', '5a5a9a', 7, {'tool': 'axe', 'burns': True}), ('spirit_wood', 'Spirit wood', 'planks', '8a6a3a', 5, {'tool': 'axe', 'burns': True})]
COL = {}
for n, lab, kind, col, tier, kw in BLOCKS:
    COL[n] = c(col); block(block_tex(kind, n, c(col)), n, lab, tier, 0.04, **kw)
# Soil that herbs like, and the grass of the Fallen Heaven.
t = rock('spirit_soil', c('5a4a3a'), vein=JADE); block(t, 'spirit_soil', 'Spirit soil', 2, 0.5, tool='shovel', mine_time=0.3, qi=0.3, qi_reach=2)
t = rock('star_grass', COL['heaven_soil']); t.r(0, 0, 15, 3, c('7a8ae0')); t.r(0, 0, 15, 0, c('b0c0ff')); [t.p(x, 4, c('7a8ae0')) for x in (1, 4, 6, 9, 12, 14)]; t.p(3, 1, WHITE).p(11, 2, WHITE)
block(t, 'star_grass', 'Star grass', 7, 0.04, tool='shovel', mine_time=0.4, drop='heaven_soil')
# The edge of each place, and the courts of the trials: neither comes up.
rock('heaven_wall', c('1a1830'), 20).speck('hw', c('8080d0'), 6).save('tiles', 'heaven_wall'); tile('heaven_wall', 'The edge of heaven', removable=False, drop=None)
COURTS = [('dragon', 'Azure Dragon', '2f8a6a'), ('phoenix', 'Vermilion Phoenix', 'b03a26'), ('tortoise', 'Black Tortoise', '2a2e3a'), ('tiger', 'White Tiger', 'd8d8d0')]
for n, lab, col in COURTS:
    paving(n + '_court', c(col)).save('tiles', n + '_court'); tile(n + '_court', lab + ' court', removable=False, drop=None, light=4)
# The World Tree itself.
bark('world_tree_bark', c('6a4a2a')).save('tiles', 'world_tree_bark'); tile('world_tree_bark', 'World Tree bark', tool='axe', mine_time=1.4, drop='spirit_wood', shade=6)
t = bark('world_tree_bough', c('7a5a32')); t.im = t.im.transpose(Image.ROTATE_90); t.px = t.im.load(); t.r(0, 0, 15, 1, c('4a9a4a')); t.p(3, 2, c('4a9a4a')).p(9, 2, c('4a9a4a')).p(13, 2, c('4a9a4a')); t.save('tiles', 'world_tree_bough')
tile('world_tree_bough', 'World Tree bough', tool='axe', mine_time=1.0, drop='spirit_wood')
leaves('world_tree_leaves', c('3a9a5a'), c('f0e070')).save('tiles', 'world_tree_leaves'); tile('world_tree_leaves', 'World Tree leaves', False, tool='machete', mine_time=0.15, drop='world_tree_leaf', climbable=True, shade=4)
thing(P().r(7, 9, 7, 13, c('3a6a3a')).disc(7.5, 6, 3.6, c('3a9a5a')).p(6, 5, c('f0e070')).r(7, 4, 7, 8, c('2a7a4a')).outline(), 'world_tree_leaf', 'World Tree leaf', 5, 'nature', 0.05)
t = P(); [t.r(x, 0, x, 15, c('3a8a4a')) for x in (5, 9)]; [t.p(x, y, c('6ad070')) for x, y in ((4, 2), (6, 5), (10, 3), (8, 8), (4, 10), (10, 12), (6, 14))]; t.p(6, 9, c('f0a0d0')).p(9, 6, c('f0a0d0')); t.save('tiles', 'spirit_vine'); icon_of('spirit_vine')
tile('spirit_vine', 'Spirit vine', False, tool='machete', mine_time=0.1, climbable=True); item('spirit_vine', 'Spirit vine', 5, 'nature', 0.05, places='spirit_vine')
t = rock('tree_amber', c('e09020')); t.speck('ta', c('ffe090'), 20); block(t, 'tree_amber', 'Tree amber', 5, 0.5, 'ores', light=6, mine_time=1.0, cat='ores')
crystal(c('f0a0d0')).save('tiles', 'glow_blossom'); icon_of('glow_blossom'); tile('glow_blossom', 'Glow blossom', False, tool='machete', mine_time=0.2, light=11, pulse=0.1); item('glow_blossom', 'Glow blossom', 5, 'nature', 0.1, places='glow_blossom')
# Crystals of the Fallen Heaven.
for n, lab, col, tier, light in (('star_crystal', 'Star crystal', 'a0b0ff', 9, 10), ('lightning_glass', 'Lightning glass', 'f0f0a0', 9, 11)):
    crystal(c(col)).save('tiles', n); icon_of(n); tile(n, lab, True, mine_time=1.2, light=light, pulse=0.12, hardness=7); item(n, lab, tier, 'ores', 0.3, places=n)
for n, lab, col, tier in (('soul_crystal', 'Soul crystal', ramp('3a1a6a', 'b080ff', 'ffffff'), 7), ('fallen_star', 'Fallen star', ramp('2a3a8a', '90b0ff', 'ffffff'), 9)):
    clone_tile('diamond', n, lab, col, hardness=7, light=9); clone_item('diamond', n, lab, col, tier, 'ores', 1.0)
# Cinnabar and jade, under the world as it began.
for n, lab, col, tier in (('cinnabar', 'Cinnabar', 'd03a2a', 1), ('spirit_jade', 'Spirit jade', '5fc89a', 3)):
    rock(n + '_ore', STONE, vein=c(col)).save('tiles', n + '_ore'); tile(n + '_ore', lab + ' ore', ore=True, mine_time=1.1, drop=n)
    thing(gemstone(c(col)) if n == 'spirit_jade' else trinket('dust', c(col)), n, lab, tier, 'ores', 0.6)

# ---- metals: Spirit Mining and Blacksmithing -----------------------------------------------------------------------
# name, label, realm (6: an immortal's), colours, the rock it lies in, hardness to dig, its pick's power, tool speed, armour
METALS = [('spirit_iron', 'Spirit iron', 1, ramp('2a4a5a', '7ab8c8', 'e0fbff'), 'stone', 0, 3, 6.0, 0.12), ('azure_copper', 'Azure copper', 2, ramp('0a3a6a', '2a8ad8', 'b0e8ff'), 'stone', 0, 5, 8.0, 0.16),
          ('star_silver', 'Star silver', 3, ramp('5a5a8a', 'c8d0f0', 'ffffff'), 'rootstone', 5, 7, 10.5, 0.2), ('void_gold', 'Void gold', 4, ramp('3a1a5a', 'c090e0', 'ffe890'), 'heaven_marble', 7, 9, 13.0, 0.24),
          ('dragonbone', 'Dragonbone steel', 5, ramp('5a1a1a', 'e0c8a8', 'fff8e8'), 'heaven_marble', 9, 11, 16.0, 0.27), ('chaos_iron', 'Chaos iron', 6, ramp('0a0a14', '5a3a8a', 'f0a0ff'), None, 0, 13, 20.0, 0.3)]
MR = {}
for n, lab, realm, r, host, hard, power, speed, armor in METALS:
    tl, th = (lo(realm), hi(realm)) if realm < 6 else (11, 11); MR[n] = r
    if host:
        hc = HOSTC[host][:3]; clone_tile('copper_ore', n + '_ore', lab + ' ore', r, 'ore', lambda t, hc=hc: tuple(int(v * (0.7 + 0.5 * t)) for v in hc), hardness=hard or None, mine_time=1.3 + realm * 0.2)
        clone_item('copper_ore', n + '_ore', lab + ' ore', r, tl, 'ores', 0.15, 'ore', lambda t, hc=hc: tuple(int(v * (0.7 + 0.5 * t)) for v in hc))
    clone_item('iron_ingot', n + '_ingot', lab + ' ingot', r, tl, 'ores', 0.5)
    clone_tile('iron_block', n + '_block', lab + ' block', r); clone_item('iron_block', n + '_block', lab + ' block', r, tl, 'blocks', 4.5, places=n + '_block')
    for tool, base, cls, k in (('pickaxe', 'iron_pickaxe', 'pickaxe', 2.0), ('axe', 'iron_axe', 'axe', 1.6), ('shovel', 'gold_shovel', 'shovel', 1.2), ('machete', 'iron_machete', 'machete', 1.2)):
        clone_item(base, n + '_' + tool, lab + ' ' + tool, r, th, 'tools', k, 'gray', tool={'class': cls, 'speed': speed}, power=power if tool == 'pickaxe' else None)
    clone_item('iron_sword', n + '_sword', lab + ' sword', r, th, 'tools', 1.6, 'gray', damage=int(speed * 2.4))
    if realm == 6: continue
    for piece, k in (('helmet', 1.0), ('chestplate', 2.5), ('leggings', 1.5), ('boots', 1.0)):
        clone_item('iron_' + piece, n + '_' + piece, lab + ' ' + piece, r, th, 'clothing', k, armor=armor, warmth=0.25, boons={'qi': round(1.0 + 0.04 * realm, 2)})

# ---- benches --------------------------------------------------------------------------------------------------------
TRIM = [c('8a8a8a'), c('2a8ad8'), c('d8e0f8'), c('c090e0'), c('f0c840')]
GRADE_COST = {2: [('azure_copper_ingot', 4), ('spirit_jade', 2)], 3: [('star_silver_ingot', 4), ('beast_core_3', 1)], 4: [('void_gold_ingot', 4), ('soul_crystal', 2)], 5: [('dragonbone_ingot', 4), ('fallen_star', 2)]}
# kind, label, grades, whether it burns fuel, the tool that takes it up
BENCHES = [('pill_furnace', 'Pill furnace', 5, True), ('spirit_forge', 'Spirit forge', 5, True), ('artificers_table', "Artificer's table", 5, False), ('talisman_desk', 'Talisman desk', 1, False),
           ('spirit_kitchen', 'Spirit kitchen', 1, False), ('scripture_desk', 'Scripture desk', 1, False), ('formation_table', 'Formation table', 1, False), ('spirit_loom', 'Spirit loom', 1, False)]
for kind, lab, grades, fuel in BENCHES:
    for g in range(1, grades + 1):
        n = kind if g == 1 else '%s_%d' % (kind, g)
        bench_pic(kind, TRIM[g - 1]).save('tiles', n); icon_of(n)
        tile(n, lab, False, mine_time=0.4, bench=True, bench_kind=kind, bench_level=g, fuel_capacity=3000 if fuel else None, light=8 if fuel else None, light_needs_fuel=True if fuel else None,
             upgrade={'to': '%s_%d' % (kind, g + 1), 'cost': [{'item': i, 'count': k} for i, k in GRADE_COST[g + 1]]} if g < grades else None)
        item(n, lab + (' ' + NUM[g] if grades > 1 else ''), lo(g), 'benches', 3.0, places=n)
rec('workbench', 'pill_furnace', [('clay', 10), ('copper_ingot', 4), ('spirit_stone', 5)], level=2); rec('workbench', 'spirit_forge', [('stone', 20), ('iron_ingot', 5), ('spirit_stone', 5)], level=2)
rec('workbench', 'artificers_table', [('wood', 10), ('spirit_iron_ingot', 3), ('spirit_stone', 10)], level=2); rec('workbench', 'talisman_desk', [('wood', 8), ('cinnabar', 4)], level=2)
rec('workbench', 'spirit_kitchen', [('stone', 12), ('clay', 6), ('spirit_stone', 2)], level=2); rec('workbench', 'scripture_desk', [('wood', 10), ('linen', 4)])
rec('workbench', 'formation_table', [('stone', 10), ('spirit_jade', 2), ('spirit_stone', 10)], level=2); rec('workbench', 'spirit_loom', [('wood', 10), ('string', 6), ('spirit_stone', 3)], level=2)
def forge(out, ins, g=1, count=1, time=160): rec('spirit_forge', out, ins, count, level=g, time=time)
def art(out, ins, g=1, count=1, time=160): rec('artificers_table', out, ins, count, level=g, time=time)
def brew(out, ins, g=1, count=1, time=200): rec('pill_furnace', out, ins, count, level=g, time=time)

# Portals and trade to tier XI.
TIERCOL = {7: ramp('2a1a5a', 'a070e0', 'ffffff'), 8: ramp('1a3a6a', '60b0f0', 'ffffff'), 9: ramp('5a4a10', 'f0d060', 'ffffff'), 10: ramp('6a1a1a', 'f07050', 'fff0d0'), 11: ramp('3a3a3a', 'f0f0ff', 'ffffff')}
PORTAL_COST = {7: 'star_silver_block', 8: 'soul_crystal', 9: 'void_gold_block', 10: 'fallen_star', 11: 'heaven_tempered_essence'}
for tier in range(7, 12):
    for base, n, lab, cost, crystals in (('portal_6', 'dao_portal_%d' % tier, 'Portal', [(PORTAL_COST[tier], 1)], 0),
                                         ('trade_portal_6', 'dao_trade_portal_%d' % tier, 'Trade portal', [(PORTAL_COST[tier], 1), ('platinum_coin', (tier - 6) * 2)], (tier - 6) * 400)):
        e = {'cost': [{'item': i, 'count': k} for i, k in cost]}
        if crystals: e['crystals'] = crystals
        clone_tile(base, n, lab, TIERCOL[tier], 'tint', bench_level=None, trade_level=None, extends=e)
        clone_item(base, n, lab + ' ' + NUM[tier], TIERCOL[tier], tier, 'portals', 1.0, 'tint', places=n)
def portal(tier, out, ins, count=1, time=300, **kw):
    if tier <= 6: rec('portal', out, ins, count, level=tier, time=time, **kw)
    else: rec('portal', out, ins, count, at='dao_portal_%d' % tier, time=time, **kw)

# ---- herbs and crops: Herbalism --------------------------------------------------------------------------------------
SOIL = ['dirt', 'grass', 'compost', 'spirit_soil', 'world_tree_bough', 'star_grass', 'heaven_soil']
# name, label, realm, the crop it grows like, colour, the qi it must have to grow, how it is drawn in the hand, where it is wild
HERBS = [('spirit_grass', 'Spirit grass', 1, 'wheat', '60d0a0', 0, 3, [('plains', 26), ('orchard', 30), ('cherry_grove', 30)]), ('mountain_ginseng', 'Mountain ginseng', 1, 'carrot', 'd8b070', 1.3, 1, [('pine_forest', 40), ('maple_forest', 40)]),
         ('purple_lingzhi', 'Purple lingzhi', 2, 'tomato', '8a4ac0', 1.8, 1, [('maple_forest', 60), ('tropics', 60)]), ('frost_lotus', 'Frost lotus', 2, 'sunflower', 'a0e0ff', 2.2, 0, [('pine_forest', 60)]),
         ('dragon_beard_grass', 'Dragon-beard grass', 3, 'flax', '40c070', 2.8, 3, []), ('fire_cloud_flower', 'Fire-cloud flower', 3, 'chilli', 'ff7030', 3.2, 0, []),
         ('soul_orchid', 'Soul orchid', 4, 'sunflower', 'c0a0ff', 4.0, 2, []), ('nine_leaf_fern', 'Nine-leaf fern', 4, 'corn', '30a090', 4.5, 3, []),
         ('phoenix_blossom', 'Phoenix blossom', 5, 'tomato', 'ff4060', 6.0, 0, []), ('heaven_root', 'Heaven root', 5, 'carrot', 'fff0a0', 6.5, 1, []),
         ('spirit_rice', 'Spirit rice', 1, 'wheat', 'f0f0d0', 0, 3, [('plains', 40)]), ('spirit_cotton', 'Spirit cotton', 1, 'flax', 'e8e8ff', 0, 2, [('plains', 40), ('orchard', 40)]), ('spirit_tea', 'Spirit tea leaf', 2, 'flax', '3a9a4a', 1.5, 1, [('cherry_grove', 40)])]
for n, lab, realm, base, col, needs, look, wild in HERBS:
    k = c(col); r = ramp('202020', col, 'ffffff')
    for i, t in enumerate(chain(base + '_0')):
        d = clone_tile(t, t.replace(base, n), lab, r, 'tint', soil=SOIL, needs_qi=needs if (i > 0 and needs) else None, light=5 if realm >= 3 else None)
        for key in ('grows_into', 'grows_top', 'topped_by', 'cut_to'):
            if key in d: d[key] = d[key].replace(base, n)
        many = [dr.get('count', 1) for dr in BT[t].get('drops', [])][:1]
        d.pop('drops', None); d.pop('drop', None)
        d['drops'] = [{'item': n, 'count': many[0] if many else 1}]
    thing(herb(c('3a8a4a'), k, look), n, lab, lo(realm), 'nature', 0.4, sows=n + '_0')
    for b, one_in in wild: plants.append({'biome': b, 'tile': n + '_0', 'one_in': one_in})

# ---- trees ------------------------------------------------------------------------------------------------------------
# name, label, the tree whose shape it has, its fruit (label, colour, food, qi), trunk and leaf, tier, what it leaves when cut
WOODS = [('spirit_peach', 'Spirit peach', 'apple', ('Spirit peach', 'f0a0b0', 0.25, 30), '8a5a3a', 'f0b0c8', 5, 'spirit_wood'), ('spirit_pine', 'Spirit pine', 'pine', ('Spirit pine cone', '6a8a5a', 0, 0), '5a3a2a', '2a7a5a', 5, 'spirit_wood'),
         ('moon_laurel', 'Moon laurel', 'maple', ('Moon laurel berry', 'c0c8f0', 0.1, 60), '9a9aa8', 'd0d8f8', 5, 'spirit_wood'), ('immortal_peach', 'Immortal peach', 'apple', ('Immortal peach', 'ffd0a0', 0.6, 20000), 'c8b8a0', 'ffc0d8', 9, 'star_wood'),
         ('star_willow', 'Star willow', 'cherry', ('Star willow seed', 'a0b0ff', 0, 0), '4a4a8a', '8090f0', 7, 'star_wood')]
for n, lab, shape, (flab, fcol, food, qi), trunk, leaf, tier, wood in WOODS:
    near = lambda col: ramp('%02x%02x%02x' % dk(c(col), 0.62)[:3], col, '%02x%02x%02x' % lt(c(col), 1.12)[:3])
    clone_tile('oak_log', n + '_log', lab + ' trunk', near(trunk), drop=wood); clone_tile('oak_leaves', n + '_leaves', lab + ' leaves', near(leaf), light=6 if tier >= 7 else None)
    fruit = n if 'peach' in n else n + '_seed'
    t = P().disc(7.5, 9, 3.8, c(fcol)); t.p(6, 7, WHITE); t.r(7, 3, 8, 5, c('5a3a2a')); t.r(9, 3, 10, 4, c('4a9a4a')); t.p(9, 11, dk(c(fcol))); t.outline().save('items', fruit)
    item(fruit, flab, tier, 'nature', 0.1 if not qi else 0.3, plants=n, food=food or None, qi=qi or None, toxin=0.02 if qi else None)
    t = copy.deepcopy(BTR[shape]); t.update(name=n, log=n + '_log', leaves=n + '_leaves', fruit=fruit, soil=SOIL); t.pop('needs_water', None); trees.append(t)

# ---- what stands about ------------------------------------------------------------------------------------------------
lantern_pic(RED, c('ffd070')).save('tiles', 'paper_lantern'); icon_of('paper_lantern'); tile('paper_lantern', 'Paper lantern', False, tool='axe', mine_time=0.2, light=12); item('paper_lantern', 'Paper lantern', 1, 'crafted', 1.0, places='paper_lantern')
lantern_pic(c('5a6a8a'), c('a0e8ff')).save('tiles', 'spirit_lamp'); icon_of('spirit_lamp'); tile('spirit_lamp', 'Spirit lamp', False, mine_time=0.2, light=14, qi=0.2, qi_reach=3); item('spirit_lamp', 'Spirit lamp', 3, 'crafted', 1.0, places='spirit_lamp')
t = P().r(5, 9, 10, 13, c('b08a40')); t.r(4, 10, 11, 12, c('b08a40')); t.r(6, 14, 9, 15, c('7a5a20')); t.r(5, 8, 10, 8, GOLD); t.p(7, 6, c('d0d0d0')).p(8, 4, c('e8e8e8')).p(7, 2, WHITE).p(9, 1, WHITE); t.save('tiles', 'incense_burner'); icon_of('incense_burner')
tile('incense_burner', 'Incense burner', False, mine_time=0.3, qi=0.3, qi_reach=3, light=5); item('incense_burner', 'Incense burner', 2, 'crafted', 2.0, places='incense_burner')
t = P().r(2, 13, 13, 15, c('c8a860')); t.r(2, 13, 13, 13, c('e8d090')); t.r(4, 14, 11, 14, c('a08040')); t.r(6, 13, 9, 13, RED); t.save('tiles', 'meditation_mat'); icon_of('meditation_mat')
tile('meditation_mat', 'Meditation mat', False, tool='axe', mine_time=0.2, qi=0.25, qi_reach=1, burns=True); item('meditation_mat', 'Meditation mat', 1, 'crafted', 2.0, places='meditation_mat')
art('paper_lantern', [('spirit_paper', 2), ('stick', 1), ('coal', 1)], 1, 2); art('spirit_lamp', [('spirit_jade', 1), ('mid_spirit_stone', 1), ('glass', 2)], 2, 2)
art('incense_burner', [('copper_ingot', 3), ('spirit_grass', 4)], 1); rec('spirit_loom', 'meditation_mat', [('spirit_cotton', 4), ('flax', 4)])
# What is left in the ruins. (name, label, colours, tier, slots across and down, what is in it)
def vault(name, label, col, trim, tier, cols, rows, loot):
    chest_pic(c(col), c(trim)).save('tiles', name); icon_of(name)
    tile(name, label, False, tool='axe', mine_time=0.5, storage={'columns': cols, 'rows': rows}, loot=loot, light=4, model='box')
    item(name, label, tier, 'crafted', 1.0, places=name, container={'slots': cols * rows})
vault('sect_chest', 'Sect chest', '8a3a2a', 'f0c840', 2, 4, 2, [drop('spirit_stone', 3, 9), some(('qi_gathering_pill', 5), ('spirit_grass', 5), ('mountain_ginseng', 3), ('cinnabar', 4), ('spirit_peach', 2), ('talisman_paper', 4)),
      some(('basic_breathing_manual', 6), ('spirit_bolt', 3), ('spirit_iron_ingot', 4), ('formation_flag', 3), ('fire_talisman', 3), ('meridian_opening_pill', 2), ('spirit_bead', 1), ('sharp_metal_scripture', 1), ('flowing_water_scripture', 1), most=1)])
vault('ancient_cache', 'Ancient cache', '5a6a3a', '5fc89a', 5, 4, 3, [drop('mid_spirit_stone', 2, 6), drop('tree_amber', 1, 4), some(('core_nourishing_pill', 4), ('dragon_beard_grass', 4), ('fire_cloud_flower', 3), ('star_silver_ingot', 4), ('beast_core_3', 2)),
      some(('five_phases_canon', 2), ('sword_qi_slash', 2), ('jade_formation_flag', 4), ('star_mirror', 1), ('spirit_gourd', 1), ('marrow_washing_pill', 3), ('clear_mind_pill', 3), most=1)])
vault('heavenly_coffer', 'Heavenly coffer', 'd8d0e8', 'f0c840', 8, 4, 4, [drop('high_spirit_stone', 2, 6), drop('soul_crystal', 1, 3), some(('soul_tempering_pill', 4), ('soul_orchid', 4), ('nine_leaf_fern', 3), ('void_gold_ingot', 4), ('beast_core_4', 2), ('immortal_peach', 2)),
      some(('nine_heavens_scripture', 2), ('thunder_palm', 2), ('star_formation_flag', 3), ('soul_seal', 1), ('void_fan', 1), ('tribulation_pill', 3), ('fallen_star', 2), ('dragonbone_ingot', 2), ('heaven_defying_pill', 1), most=2)])

# ---- what beasts leave ---------------------------------------------------------------------------------------------------
CORE = [None, c('70d0a0'), c('50a0f0'), c('c0d0ff'), c('c080f0'), c('ffb040')]
for g in range(1, 6): thing(orb(CORE[g]), 'beast_core_%d' % g, 'Beast core (%s)' % ['', 'mortal', 'earth', 'sky', 'king', 'saint'][g], hi(g), 'nature', 0.4)
thing(trinket('pot', c('a02020'), c('5a3a2a')), 'beast_blood', 'Beast blood', 1, 'nature', 0.5); thing(meat(c('c04a5a')), 'spirit_beast_meat', 'Spirit beast meat', 2, 'nature', 0.3, food=0.2)
PARTS = [('fox_fur', 'Spirit fox fur', hide(c('e08a3a'), WHITE), 1), ('boar_tusk', 'Iron boar tusk', fang(c('d8d0c0')), 2), ('venom_sac', 'Jade serpent venom', orb(c('60c040'), spark=False), 3), ('hawk_plume', 'Thunder hawk plume', feather(c('f0e060')), 4),
         ('moon_fur', 'Moon rabbit fur', hide(c('e8e8ff')), 5), ('ape_sinew', 'Vine ape sinew', trinket('thread', c('c0a070')), 5), ('crane_feather', 'Spirit crane feather', feather(c('f8f8ff')), 6),
         ('wyrm_scale', 'Lake wyrm scale', trinket('plate', c('2a8a9a')), 6), ('guardian_rune', 'Guardian rune', trinket('rune', c('9a94a8'), c('60e0ff')), 7), ('stag_antler', 'Star stag antler', fang(c('a0b0ff')), 8),
         ('wraith_silk', 'Wraith silk', trinket('thread', c('8a70c0')), 8), ('lion_mane', 'Celestial lion mane', hide(c('f0c840'), c('fff0a0')), 10)]
for n, lab, pic, tier in PARTS: thing(pic, n, lab, tier, 'nature', 0.3)
thing(P().r(4, 6, 11, 10, c('3a7ad0')).r(5, 5, 10, 5, c('70b0f0')).r(12, 7, 13, 9, c('3a7ad0')).r(1, 5, 3, 11, c('f0c840')).p(11, 7, INK).outline(), 'azure_carp', 'Azure carp', 5, 'nature', 0.2, food=0.2)
ESS = [('azure_dragon_essence', 'Azure Dragon essence', '2fd09a'), ('vermilion_phoenix_essence', 'Vermilion Phoenix essence', 'ff5030'), ('black_tortoise_essence', 'Black Tortoise essence', '4a5a8a'), ('white_tiger_essence', 'White Tiger essence', 'f0f0f0')]
for n, lab, col in ESS: thing(orb(c(col), GOLD), n, lab, 11, 'special', 1.0)
thing(orb(c('f0f0ff'), c('ffe060')), 'heaven_tempered_essence', 'Heaven-tempered essence', 11, 'special', 1.0)
thing(gemstone(c('f8f8e0'), True), 'immortal_jade', 'Immortal jade', 11, 'special', 0.5)

# ---- creatures: spirit beasts ------------------------------------------------------------------------------------------------
def creature(kind, name, label, body, accent, health, drops, spawn, big=False, **kw):
    beast(kind, c(body), c(accent), 0).save('mobs', name); beast(kind, c(body), c(accent), 1).save('mobs', name + '_walk')
    m = {'name': name, 'label': label, 'texture': 'mobs/%s.png' % name, 'walk': ['mobs/%s_walk.png' % name, 'mobs/%s.png' % name], 'hop': 0.04, 'step_ticks': 4, 'move_chance': 0.2, 'turn_chance': 0.25,
         'health': health, 'cap': 3, 'spawn': spawn, 'drops': drops}
    m.update(kw); mobs.append(clean(m))
def on(tiles_, biomes_, chance): return {'above': tiles_, 'biome': biomes_, 'chance': chance}
HOME = ['plains', 'cherry_grove', 'orchard', 'pine_forest', 'maple_forest']
WT_B, FH_B = ['sunlit_canopy', 'mistwood', 'amber_hollows', 'firefly_boughs'], ['shattered_court', 'immortal_orchard', 'star_fields', 'thunder_scar', 'lotus_terraces']
creature('quad', 'spirit_fox', 'Spirit fox', 'e08a3a', 'ffffff', 14, [drop('fox_fur', 1, 2), drop('beast_core_1', 0, 1), drop('beast_blood', 0, 1)], on(['grass'], HOME, 0.12), hop=0.1, step_ticks=3)
creature('boar', 'iron_boar', 'Iron boar', '6a6e7a', '3a3e4a', 24, [drop('spirit_beast_meat', 1, 3), drop('boar_tusk', 0, 2), drop('beast_core_1', 0, 1), drop('beast_blood', 1, 1)], on(['grass'], ['pine_forest', 'maple_forest', 'plains'], 0.1), step_ticks=5)
creature('serpent', 'jade_serpent', 'Jade serpent', '3aa870', 'c0ffd0', 30, [drop('venom_sac', 1, 1), drop('beast_core_2', 0, 1), drop('beast_blood', 1, 2)], on(['grass', 'sand'], ['tropics', 'desert', 'cherry_grove'], 0.08),
         hop=0.0, step_ticks=3, sting={'ticks': 60, 'rest_ticks': 80, 'sight': 7})
creature('bird', 'thunder_hawk', 'Thunder hawk', '5a5a7a', 'f0e060', 26, [drop('hawk_plume', 1, 2), drop('beast_core_2', 0, 1)], {'sky': True, 'time': 'day', 'chance': 0.003}, flies=True, cap=2, move_chance=0.5)
creature('rabbit', 'moon_rabbit', 'Moon rabbit', 'e8e8ff', 'ffffff', 40, [drop('moon_fur', 1, 2), drop('beast_core_3', 0, 1), drop('spirit_beast_meat', 1, 2)], on(['world_tree_bough'], WT_B, 0.2), hop=0.14, step_ticks=3, cap=5)
creature('ape', 'vine_ape', 'Vine ape', '6a4a2a', 'd0a070', 60, [drop('ape_sinew', 1, 2), drop('beast_core_3', 0, 1), drop('beast_blood', 1, 2)], {'chance': 0.2},
         perch={'trees': ['spirit_pine', 'moon_laurel', 'spirit_peach'], 'ambush_range': 3, 'ambush_depth': 12, 'bite': 0.2, 'chase_ticks': 140, 'rest_ticks': 12, 'seek_radius': 24}, step_ticks=2, hop=0.06)
creature('crane', 'spirit_crane', 'Spirit crane', 'f8f8ff', '2a2a3a', 36, [drop('crane_feather', 1, 3), drop('beast_core_3', 0, 1)], on(['world_tree_bough'], ['sunlit_canopy', 'firefly_boughs'], 0.12), step_ticks=5, cap=4)
creature('fish', 'azure_carp_fish', 'Azure carp', '3a7ad0', 'f0c840', 8, [drop('azure_carp', 1, 1)], {'water': True, 'biome': WT_B, 'chance': 0.04}, swim={}, step_ticks=3, move_chance=0.96, turn_chance=0.06, cap=10, hop=None)
creature('wyrm', 'lake_wyrm', 'Lake wyrm', '2a8a9a', '80f0e0', 90, [drop('wyrm_scale', 1, 3), drop('beast_core_3', 1, 2)], {'water': True, 'biome': WT_B, 'chance': 0.004},
         swim={'hunts': True, 'sight': 24, 'bite': 0.3, 'rest_ticks': 16}, step_ticks=2, move_chance=0.96, turn_chance=0.04, cap=2, hop=None, size=[1, 2])
creature('golem', 'stone_guardian', 'Stone guardian', '9a94a8', '60e0ff', 140, [drop('guardian_rune', 1, 2), drop('beast_core_4', 0, 1), drop('high_spirit_stone', 0, 1)], on(['heaven_paving'], ['shattered_court'], 0.2),
         hop=0.0, step_ticks=4, sting={'ticks': 80, 'rest_ticks': 60, 'sight': 8}, cap=4)
creature('stag', 'star_stag', 'Star stag', '4a4a9a', 'c0d0ff', 100, [drop('stag_antler', 0, 2), drop('spirit_beast_meat', 2, 4), drop('beast_core_4', 0, 1)], on(['star_grass'], ['immortal_orchard', 'star_fields', 'lotus_terraces'], 0.15), step_ticks=4, hop=0.08)
creature('wraith', 'void_wraith', 'Void wraith', '4a3a7a', 'ff60c0', 120, [drop('wraith_silk', 1, 3), drop('beast_core_4', 0, 1)], on(['scorched_marble', 'heaven_paving'], ['thunder_scar', 'shattered_court'], 0.1),
         hop=0.0, step_ticks=3, sting={'ticks': 100, 'rest_ticks': 50, 'sight': 9}, cap=3)
creature('lion', 'celestial_lion', 'Celestial lion', 'e8d8b0', 'f0c840', 260, [drop('lion_mane', 1, 2), drop('beast_core_5', 1, 1), drop('beast_blood', 2, 4)], on(['star_grass', 'heaven_paving'], ['star_fields', 'thunder_scar'], 0.05),
         hop=0.05, step_ticks=2, sting={'ticks': 120, 'rest_ticks': 40, 'sight': 10}, cap=2)
# The Four Sacred Beasts: one to a court.
for kind, (short, lab, col), acc, ess, hp in zip(('dragon', 'phoenix', 'tortoise', 'tiger'), COURTS, ('f0c840', 'ffb040', '3ad07a', '2a2a2a'), ESS, (900, 800, 1400, 1000)):
    body = {'dragon': '2fa07a', 'phoenix': 'e04028', 'tortoise': '3a4258', 'tiger': 'f0f0e8'}[kind]
    creature(kind, 'sacred_' + short, lab, body, acc, hp, [drop(ess[0], 1, 1), drop('immortal_jade', 2, 4), drop('supreme_spirit_stone', 1, 3), drop('beast_core_5', 2, 4)], on([short + '_court'], [short + '_trial'], 0.02),
             cap=1, hop=0.0, step_ticks=2, move_chance=0.5, sting={'ticks': 200, 'rest_ticks': 30, 'sight': 14}, size=[3, 3], flies=True if kind == 'phoenix' else None)

# ---- smelting, tools and armour: Blacksmithing -------------------------------------------------------------------------------
FLUX = {1: 'spirit_stone', 2: 'spirit_stone', 3: 'mid_spirit_stone', 4: 'mid_spirit_stone', 5: 'high_spirit_stone'}
for n, lab, realm, r, host, hard, power, speed, armor in METALS:
    g = min(realm, 5)
    if host: forge(n + '_ingot', [(n + '_ore', 2), (FLUX[realm], 1)], g)
    else: forge(n + '_ingot', [('dragonbone_ingot', 2), ('heaven_tempered_essence', 1), ('supreme_spirit_stone', 1)], 5, 2, 600)
    forge(n + '_block', [(n + '_ingot', 9)], g); forge(n + '_ingot', [(n + '_block', 1)], g, 9, 60)
    for tool, k in (('pickaxe', 3), ('axe', 3), ('shovel', 2), ('machete', 2), ('sword', 4)): forge(n + '_' + tool, [(n + '_ingot', k), ('stick', 2)] + ([('beast_core_%d' % g, 1)] if tool in ('pickaxe', 'sword') else []), g, time=240)
    if realm == 6: continue
    for piece, k in (('helmet', 4), ('chestplate', 7), ('leggings', 5), ('boots', 3)): forge(n + '_' + piece, [(n + '_ingot', k), ('leather', 2)], g, time=300)
# Spirit stones up and down, and into blocks.
for (a, _, _, _, _, _), (b, _, _, _, _, _), g in zip(STONES, STONES[1:], (2, 4, 5)): art(b, [(a, 10)], g, time=200); art(a, [(b, 1)], 1, 9, 60)
for n, _, tier, _, _, _ in STONES: art(n + '_block', [(n, 9)], 1); art(n, [(n + '_block', 1)], 1, 9, 60)
art('spirit_soil', [('compost', 2), ('spirit_stone', 1)], 1, 4)
# Building.
forge('sect_brick', [('stone', 4), ('clay', 1)], 1, 8, 80); forge('sect_roof', [('clay', 4), ('spirit_grass', 1)], 1, 8, 80); art('vermilion_planks', [('wood', 4), ('cinnabar', 1)], 1, 8, 80)
forge('sect_paving', [('stone', 4)], 1, 4, 60); art('jade_brick', [('spirit_jade', 1), ('stone', 4)], 2, 8, 80); forge('heaven_brick', [('heaven_marble', 4)], 4, 4, 80); forge('heaven_paving', [('heaven_marble', 2), ('cloud_jade', 2)], 4, 4, 80)
forge('heaven_roof', [('clay', 4), ('void_gold_ingot', 1)], 4, 16, 120); art('heaven_bridge', [('star_wood', 4)], 4, 4, 60); forge('star_glass', [('glass', 4), ('star_crystal', 1)], 5, 4, 120)
art('sect_chest', [('vermilion_planks', 6), ('spirit_iron_ingot', 2)], 1); art('ancient_cache', [('spirit_wood', 8), ('spirit_jade', 2)], 3); art('heavenly_coffer', [('heaven_brick', 8), ('void_gold_ingot', 2)], 4)

# ---- paper, ink and cloth ------------------------------------------------------------------------------------------------------
thing(trinket('paper', PAPER), 'spirit_paper', 'Spirit paper', 1, 'crafted', 0.4); thing(trinket('paper', c('f0d870')), 'talisman_paper', 'Talisman paper', 1, 'crafted', 0.5)
thing(trinket('pot', c('c8322a'), c('3a2a2a')), 'cinnabar_ink', 'Cinnabar ink', 1, 'crafted', 1.0); thing(trinket('brush', c('e08a3a')), 'spirit_brush', 'Spirit brush', 1, 'tools', 2.0)
thing(slip(JADE, c('2a6a4a')), 'blank_jade_slip', 'Blank jade slip', 3, 'crafted', 1.0)
rec('scripture_desk', 'spirit_paper', [('flax', 3), ('spirit_grass', 1)], 4, time=60); rec('talisman_desk', 'talisman_paper', [('spirit_paper', 2), ('cinnabar', 1)], 4, time=60)
rec('talisman_desk', 'cinnabar_ink', [('cinnabar', 2), ('charcoal', 1), ('beast_blood', 1)], 2, time=80); rec('talisman_desk', 'spirit_brush', [('stick', 1), ('fox_fur', 1)])
art('blank_jade_slip', [('spirit_jade', 1)], 2, 2)
# name, label, colour, realm, what it is woven of
CLOTHS = [('spirit_cloth', 'Spirit cloth', 'd8e0f0', 1, [('spirit_cotton', 3)]), ('azure_silk', 'Azure silk', '4a90e0', 2, [('spirit_cloth', 2), ('hawk_plume', 1), ('frost_lotus', 1)]),
          ('moonweave', 'Moonweave', 'c8d0ff', 3, [('azure_silk', 1), ('moon_fur', 2), ('spirit_vine', 2)]), ('void_weave', 'Void weave', '7a50b0', 4, [('moonweave', 1), ('wraith_silk', 2), ('soul_orchid', 1)]),
          ('sovereign_brocade', 'Sovereign brocade', 'f0c040', 5, [('void_weave', 1), ('lion_mane', 1), ('phoenix_blossom', 1)])]
for n, lab, col, realm, ins in CLOTHS:
    thing(trinket('cloth', c(col)), n, lab, lo(realm), 'crafted', 1.0); rec('spirit_loom', n, ins, time=120)

# ---- pills: Alchemy -----------------------------------------------------------------------------------------------------------
def pill_(name, label, realm, col, ins, g=None, count=2, ring=None, mult=1.0, tier=None, **kw):
    kw.setdefault('max_stack', 20)
    thing(pill(c(col), realm if realm < 6 else 5, ring), name, label, (tier or lo(realm)) if realm < 6 else 11, 'pills', mult, **kw); brew(name, ins, g or min(realm, 5), count)
def lasts(ticks, **boons): return {'ticks': ticks, 'boons': boons}
QI_PILLS = [('qi_gathering_pill', 'Qi gathering pill', 1, '70d0a0', 40, [('spirit_grass', 3)]), ('spirit_condensing_pill', 'Spirit condensing pill', 2, '50a0f0', 300, [('purple_lingzhi', 2), ('spirit_grass', 2)]),
            ('core_nourishing_pill', 'Core nourishing pill', 3, 'e0d060', 2500, [('dragon_beard_grass', 2), ('purple_lingzhi', 1)]), ('soul_tempering_pill', 'Soul tempering pill', 4, 'c080f0', 20000, [('soul_orchid', 2), ('dragon_beard_grass', 2)]),
            ('divine_essence_pill', 'Divine essence pill', 5, 'fff0c0', 150000, [('phoenix_blossom', 2), ('soul_orchid', 2)])]
for n, lab, realm, col, qi, ins in QI_PILLS: pill_(n, lab, realm, col, ins, qi=qi, toxin=0.08)
# The pill each realm is entered by: made of what the realm before it has.
REALM_PILLS = [('foundation_pill', 'Foundation establishment pill', 2, 'a0d8ff', [('mountain_ginseng', 3), ('spirit_grass', 5), ('beast_core_1', 2), ('spirit_stone', 5)], 1),
               ('core_formation_pill', 'Core formation pill', 3, 'ffd040', [('frost_lotus', 3), ('purple_lingzhi', 3), ('beast_core_2', 3), ('mid_spirit_stone', 3)], 2),
               ('nascent_soul_pill', 'Nascent soul pill', 4, 'e0a0ff', [('fire_cloud_flower', 3), ('dragon_beard_grass', 3), ('beast_core_3', 3), ('tree_amber', 2)], 3),
               ('divinity_pill', 'Divinity pill', 5, 'ffffff', [('soul_orchid', 3), ('nine_leaf_fern', 3), ('beast_core_4', 3), ('soul_crystal', 2)], 4)]
for n, lab, realm, col, ins, g in REALM_PILLS: pill_(n, lab, realm, col, ins, g, 1, GOLD, 4.0, tier=hi(realm - 1), toxin=0.05)
pill_('blood_restoring_pill', 'Blood restoring pill', 1, 'e04040', [('mountain_ginseng', 1), ('spirit_grass', 2)], effect=lasts(300, regen=12.0))
pill_('fasting_pill', 'Fasting pill', 1, 'c8b080', [('spirit_rice', 4), ('mountain_ginseng', 1)], food=1.0, cooked=True)
pill_('detox_pill', 'Cleansing pill', 1, 'e8e8e8', [('spirit_grass', 2), ('charcoal', 2)], toxin=-0.5)
pill_('meridian_opening_pill', 'Meridian opening pill', 1, 'f0a030', [('mountain_ginseng', 2), ('beast_core_1', 1), ('cinnabar', 1)], count=1, ring=RED, mult=3.0, opens_meridian=True, toxin=0.15)
pill_('clear_mind_pill', 'Clear mind pill', 2, 'b0f0f0', [('frost_lotus', 1), ('spirit_tea', 2)], effect=lasts(1200, breakthrough=0.15))
pill_('marrow_washing_pill', 'Marrow washing pill', 2, '9a6a4a', [('purple_lingzhi', 2), ('frost_lotus', 2), ('beast_core_2', 1)], count=1, ring=WHITE, mult=4.0, washes_roots=True, toxin=0.2)
pill_('iron_skin_pill', 'Iron skin pill', 2, '8a8e9a', [('purple_lingzhi', 1), ('boar_tusk', 1), ('spirit_iron_ingot', 1)], effect=lasts(2400, armor=0.2))
pill_('swift_wind_pill', 'Swift wind pill', 3, 'a0f0c0', [('dragon_beard_grass', 2), ('crane_feather', 1)], effect=lasts(2400, walk=1.4))
pill_('body_tempering_pill', 'Body tempering pill', 3, 'c07040', [('fire_cloud_flower', 3), ('beast_core_3', 2), ('star_silver_ingot', 1)], count=1, ring=GOLD, mult=6.0, max_stack=1, effect={'forever': True, 'boons': {'armor': 0.05, 'strength': 1}})
pill_('tribulation_pill', 'Tribulation warding pill', 4, '6070d0', [('nine_leaf_fern', 2), ('beast_core_4', 1), ('guardian_rune', 1)], effect=lasts(1200, ward=0.4))
pill_('spirit_sight_pill', 'Spirit sight pill', 4, 'f0f080', [('soul_orchid', 1), ('glow_blossom', 2)], effect=lasts(3600, sight=1.0))
pill_('heaven_defying_pill', 'Heaven defying pill', 5, 'ff6080', [('heaven_root', 2), ('beast_core_5', 1)], count=1, ring=GOLD, mult=3.0, effect=lasts(1200, breakthrough=0.3))
pill_('longevity_pill', 'Longevity pill', 5, 'ffc0d0', [('heaven_root', 3), ('immortal_peach', 3), ('beast_core_5', 2)], count=1, ring=GOLD, mult=6.0, max_stack=1, effect={'forever': True, 'boons': {'regen': 1.5}})
pill_('nine_revolutions_pill', 'Nine revolutions pill', 6, 'ffe890', [('heaven_tempered_essence', 1), ('heaven_root', 5), ('phoenix_blossom', 5)], 5, 1, RED, 1.0, max_stack=1, qi=2000000, cures=True, effect={'forever': True, 'boons': {'qi': 1.25}})

# ---- spirit food ---------------------------------------------------------------------------------------------------------------
def dish(name, label, tier, pic, ins, **kw): thing(pic, name, label, tier, 'nature', 1.0, cooked=True, **kw); rec('spirit_kitchen', name, ins, time=120)
dish('spirit_rice_bowl', 'Spirit rice bowl', 1, bowl(c('f8f8e8')), [('spirit_rice', 3)], food=0.4, qi=15)
dish('ginseng_soup', 'Ginseng dodo soup', 2, bowl(c('e0c070')), [('mountain_ginseng', 1), ('dodo_meat', 1)], food=0.6, effect=lasts(1200, regen=4.0))
dish('spirit_tea_cup', 'Spirit tea', 3, cup(c('7ac060')), [('spirit_tea', 3)], effect=lasts(2400, qi=1.25))
dish('boar_stew', 'Iron boar stew', 2, bowl(c('9a5a3a')), [('spirit_beast_meat', 2), ('carrot', 1)], food=0.7, effect=lasts(2400, strength=1))
dish('lingzhi_broth', 'Lingzhi broth', 4, bowl(c('8a5ab0')), [('purple_lingzhi', 2), ('spirit_rice', 2)], food=0.5, qi=150)
dish('hundred_herb_congee', 'Hundred-herb congee', 4, bowl(c('a0d0a0')), [('spirit_rice', 3), ('spirit_grass', 3), ('frost_lotus', 1)], food=0.6, cures=True, toxin=-0.3)
dish('steamed_carp', 'Steamed azure carp', 5, bowl(c('5a90d0'), c('e8e0d0')), [('azure_carp', 1), ('spirit_tea', 1)], food=0.7, effect=lasts(3600, lung=3.0))
dish('dragon_beard_noodles', 'Dragon-beard noodles', 6, bowl(c('60c080')), [('dragon_beard_grass', 2), ('flour', 2)], food=0.8, qi=800, effect=lasts(2400, walk=1.2))
dish('stag_roast', 'Star stag roast', 8, bowl(c('7a4a6a'), steam=True), [('spirit_beast_meat', 3), ('stag_antler', 1), ('nine_leaf_fern', 1)], food=1.0, effect=lasts(2400, qi=1.5))
dish('immortal_peach_wine', 'Immortal peach wine', 10, cup(c('ffb0c0'), c('e8e0f0')), [('immortal_peach', 3), ('spirit_rice', 4)], effect=lasts(2400, qi=1.5, breakthrough=0.1))

# ---- manuals, techniques, talismans --------------------------------------------------------------------------------------------
def first(realm): return (realm - 1) * 10 + 1
def manual(name, label, realm, rate, element, col, ins, mark=None):
    thing(book(c(col), GOLD if realm > 1 else c('c0c0c0'), mark), name, label, lo(min(realm, 5)) if realm < 6 else 11, 'scriptures', 3.0, max_stack=1, manual=clean({'rate': rate, 'element': element if element >= 0 else None}), needs_stage=first(realm) if realm > 1 else None)
    rec('scripture_desk', name, ins, time=300)
manual('basic_breathing_manual', 'Basic breathing manual', 1, 1.0, -1, '6a7a8a', [('spirit_paper', 4), ('spirit_grass', 2)])
for i, (n, lab, col, what) in enumerate((('sharp_metal_scripture', 'Sharp Metal scripture', 'c8c8d8', 'spirit_iron_ingot'), ('verdant_wood_scripture', 'Verdant Wood scripture', '3a9a4a', 'mountain_ginseng'),
                                         ('flowing_water_scripture', 'Flowing Water scripture', '3a7ad0', 'frost_lotus'), ('blazing_fire_scripture', 'Blazing Fire scripture', 'd04020', 'cinnabar'), ('thick_earth_scripture', 'Thick Earth scripture', '9a7a3a', 'spirit_jade'))):
    manual(n, lab, 2, 4.0, i, col, [('spirit_paper', 6), ('cinnabar_ink', 3), (what, 3), ('beast_core_1', 2)])
manual('five_phases_canon', 'Five Phases canon', 3, 16.0, -1, '8a4ac0', [('spirit_paper', 10), ('cinnabar_ink', 5), ('beast_core_3', 2), ('tree_amber', 2)], JADE)
manual('sword_heart_sutra', 'Sword Heart sutra', 3, 20.0, 0, 'd8e0f8', [('spirit_paper', 10), ('cinnabar_ink', 5), ('star_silver_sword', 1), ('beast_core_3', 2)], RED)
manual('nine_heavens_scripture', 'Nine Heavens scripture', 4, 60.0, -1, '2a2a6a', [('spirit_paper', 12), ('soul_crystal', 3), ('beast_core_4', 2), ('wraith_silk', 2)], c('a0b0ff'))
manual('primordial_chaos_canon', 'Primordial Chaos canon', 5, 250.0, -1, '1a1a24', [('spirit_paper', 16), ('fallen_star', 3), ('beast_core_5', 2), ('lion_mane', 1)], c('f0a0ff'))
manual('dao_of_heaven', 'The Dao of Heaven', 6, 1000.0, -1, 'f8f8e8', [(e[0], 1) for e in ESS] + [('immortal_jade', 4)], RED)
# name, label, the stage it takes, qi each time, glyph colour, what it does, what it is written with
def technique(name, label, stage, qi, col, ins, rng=9, **kw):
    cast = kw.pop('cast', None)
    thing(slip(JADE if stage < 21 else c('c8d0f0') if stage < 31 else c('c090e0') if stage < 41 else GOLD, c(col)), name, label, min(10, (stage + 4) // 5), 'scriptures', 2.0, max_stack=1, uses=100, range=rng,
          cast=cast, technique={'qi': qi, 'stage': stage}, **kw)
    rec('scripture_desk', name, [('blank_jade_slip', 1), ('cinnabar_ink', 1)] + ins, time=240)
technique('spirit_bolt', 'Spirit bolt', 1, 5, 'ffffff', [('spirit_stone', 3)], cast={'damage': 8})
technique('flame_palm', 'Flame palm', 5, 15, 'f04020', [('cinnabar', 4), ('beast_core_1', 1)], cast={'damage': 20, 'splash': 1, 'cooks': True})
technique('iron_body_art', 'Iron body art', 6, 30, '8a8e9a', [('boar_tusk', 2), ('spirit_iron_ingot', 2)], 0, effect=lasts(1200, armor=0.3))
technique('water_veil', 'Water veil', 8, 20, '3a7ad0', [('frost_lotus', 1), ('beast_core_1', 1)], cast={'shield': 1.0})
technique('earth_splitting_fist', 'Earth-splitting fist', 11, 30, '9a7a3a', [('spirit_jade', 2), ('beast_core_2', 1)], 8, cast={'dig': 4, 'splash': 2})
technique('wind_step', 'Wind step', 12, 60, 'a0f0c0', [('hawk_plume', 2), ('beast_core_2', 1)], 0, effect=lasts(1200, walk=1.4))
technique('verdant_touch', 'Verdant touch', 13, 40, '3a9a4a', [('purple_lingzhi', 2), ('spirit_soil', 2)], 8, cast={'grow': 2})
technique('frost_seal', 'Frost seal', 16, 60, 'a0e0ff', [('frost_lotus', 3), ('beast_core_2', 2)], cast={'freeze': 2, 'damage': 20, 'splash': 2})
technique('heavenly_eye', 'Heavenly eye', 18, 100, 'f0f080', [('venom_sac', 1), ('mid_spirit_stone', 2)], 0, effect=lasts(2400, sight=1.0))
technique('sword_qi_slash', 'Sword-qi slash', 21, 150, 'd8e0f8', [('star_silver_sword', 1), ('beast_core_3', 1)], 12, cast={'damage': 70, 'splash': 1})
technique('rain_calling', 'Rain calling', 24, 400, '5a90d0', [('wyrm_scale', 2), ('azure_carp', 2)], 0, cast={'weather': 'rain', 'weather_ticks': 12000})
technique('golden_bell', 'Golden bell', 26, 500, 'f0c840', [('tree_amber', 3), ('beast_core_3', 2)], 0, effect=lasts(1200, armor=0.5, ward=0.3))
technique('thunder_palm', 'Thunder palm', 31, 1500, 'f0e060', [('lightning_glass', 2), ('beast_core_4', 1)], 12, cast={'damage': 180, 'splash': 2})
technique('soul_binding', 'Soul binding', 33, 2000, 'c080f0', [('wraith_silk', 2), ('soul_crystal', 1)], 10, cast={'hold': 300, 'splash': 3})
technique('mountain_mover', 'Mountain mover', 35, 3000, 'b09050', [('guardian_rune', 2), ('void_gold_ingot', 2)], 10, cast={'dig': 10, 'splash': 4})
technique('void_walk', 'Void walk', 36, 5000, '7a50b0', [('wraith_silk', 3), ('beast_core_4', 2)], 0, effect=lasts(2400, flight=1.0))
technique('heavens_wrath', "Heaven's wrath", 41, 20000, 'ffffff', [('fallen_star', 2), ('beast_core_5', 1)], 14, cast={'damage': 700, 'splash': 3})
technique('dawn_summoning', 'Dawn summoning', 45, 100000, 'ffd070', [('phoenix_blossom', 3), ('fallen_star', 2)], 0, cast={'time': 'sunrise'})
# Talismans: used once, by anyone.
def charm(name, label, tier, ink, ins, rng=9, count=2, **kw):
    thing(talisman(c(ink)), name, label, tier, 'scriptures', 1.0, uses=1, range=rng, **kw); rec('talisman_desk', name, [('talisman_paper', 1), ('cinnabar_ink', 1)] + ins, count, time=100)
charm('fire_talisman', 'Fire talisman', 1, 'e03020', [('coal', 1)], cast={'damage': 14, 'splash': 1, 'cooks': True})
charm('water_talisman', 'Water talisman', 1, '3070d0', [('spirit_grass', 1)], cast={'water': 8})
charm('earth_talisman', 'Earth talisman', 2, '8a6a3a', [('stone', 4)], 8, cast={'dig': 3, 'splash': 1})
charm('growth_talisman', 'Growth talisman', 2, '3a9a4a', [('compost', 1)], 8, cast={'grow': 2})
charm('ward_talisman', 'Warding talisman', 3, 'd0a020', [('spirit_jade', 1)], cast={'shield': 1.0})
charm('ice_talisman', 'Ice talisman', 3, '70c0e0', [('frost_lotus', 1)], cast={'freeze': 2, 'damage': 10, 'splash': 2})
charm('thunder_talisman', 'Thunder talisman', 5, '8a50d0', [('hawk_plume', 1), ('mid_spirit_stone', 1)], 12, cast={'damage': 60, 'splash': 2})
charm('binding_talisman', 'Binding talisman', 5, '5a5a6a', [('ape_sinew', 1)], cast={'hold': 200, 'splash': 2})
charm('swift_talisman', 'Swiftness talisman', 2, '40b0a0', [('feather', 2)], 0, effect=lasts(1200, walk=1.3))
charm('recall_talisman', 'Homeward talisman', 4, '2a2a2a', [('mid_spirit_stone', 1)], 0, 1, recalls=True)
charm('sun_talisman', 'Sunrise talisman', 6, 'f0a020', [('fire_cloud_flower', 1), ('tree_amber', 1)], 0, 1, cast={'time': 'sunrise'})
charm('moon_talisman', 'Moonrise talisman', 6, '5a6ab0', [('moon_fur', 1), ('tree_amber', 1)], 0, 1, cast={'time': 'moonrise'})
thing(talisman(c('f0f0ff'), c('2a2a4a'), GOLD), 'heaven_calling_talisman', 'Heaven-calling talisman', 11, 'scriptures', 0.5, max_stack=5, calls_tribulation=True, needs_stage=51)
rec('talisman_desk', 'heaven_calling_talisman', [('talisman_paper', 3), ('cinnabar_ink', 3), ('lightning_glass', 2), ('supreme_spirit_stone', 1)], time=600)

# ---- formations ------------------------------------------------------------------------------------------------------------------
FLAGS = [('formation_flag', 'Formation flag', 'd8e0f0', 1, [('stick', 1), ('spirit_cloth', 1), ('cinnabar_ink', 1)]), ('jade_formation_flag', 'Jade formation flag', '5fc89a', 5, [('spirit_jade', 1), ('moonweave', 1), ('mid_spirit_stone', 1)]),
         ('star_formation_flag', 'Star formation flag', 'a0b0ff', 9, [('fallen_star', 1), ('void_weave', 1), ('void_gold_ingot', 1)])]
for n, lab, col, tier, ins in FLAGS:
    t = flag(c(col)); t.save('items', n); q = flag(c(col)); q.im = q.im.copy(); q.px = q.im.load()
    P.save(flag(c(col)), 'tiles', n); tile(n, lab, False, tool='axe', mine_time=0.2); item(n, lab, tier, 'crafted', 1.0, places=n); rec('formation_table', n, ins, 4, time=120)
FUELS = [{'item': s[0], 'ticks': s[5]} for s in STONES]
# name, label, colour, glow, tier, its flags, how they are set, how far off, what it does, what it is made of
ARRAYS = [('spirit_gathering_array', 'Spirit gathering array', '5fc89a', 'a0ffd0', 2, 'formation_flag', 'cross', [2, 6], {'qi': 1.0}, [('spirit_stone_block', 1), ('spirit_jade', 2), ('cinnabar_ink', 2)]),
          ('verdant_array', 'Verdant array', '3a9a4a', 'c0ff80', 2, 'formation_flag', 'cross', [2, 6], {'grows': 1.0}, [('spirit_stone_block', 1), ('spirit_soil', 4), ('mountain_ginseng', 2)]),
          ('killing_array', 'Killing array', 'a02020', 'ff6050', 3, 'formation_flag', 'square', [2, 6], {'harms': 5}, [('spirit_stone_block', 1), ('spirit_iron_sword', 2), ('beast_blood', 4)]),
          ('swift_array', 'Swift array', '40b0a0', 'c0fff0', 4, 'formation_flag', 'cross', [2, 8], {'aura': {'walk': 1.3, 'craft': 1.3}}, [('mid_spirit_stone_block', 1), ('hawk_plume', 4), ('azure_copper_ingot', 4)]),
          ('greater_gathering_array', 'Greater gathering array', '5aa0f0', 'c0e0ff', 6, 'jade_formation_flag', 'square', [3, 8], {'qi': 2.5}, [('mid_spirit_stone_block', 2), ('tree_amber', 4), ('star_silver_ingot', 4)]),
          ('protection_array', 'Protection array', 'd0a020', 'fff0a0', 6, 'jade_formation_flag', 'square', [3, 8], {'aura': {'armor': 0.3, 'ward': 0.3, 'regen': 2.0}}, [('mid_spirit_stone_block', 1), ('wyrm_scale', 4), ('star_silver_ingot', 4)]),
          ('teleport_array', 'Teleport array', '7a50b0', 'e0b0ff', 6, 'jade_formation_flag', 'cross', [2, 6], {'gate': True}, [('mid_spirit_stone_block', 2), ('crane_feather', 6), ('tree_amber', 4)]),
          ('sword_array', 'Sword array', 'c8d0f0', 'ffffff', 8, 'jade_formation_flag', 'ring', [3, 10], {'harms': 30}, [('high_spirit_stone_block', 1), ('star_silver_sword', 4), ('guardian_rune', 2)]),
          ('heaven_gathering_array', 'Heaven gathering array', 'ffd860', 'ffffff', 10, 'star_formation_flag', 'ring', [3, 10], {'qi': 5.0}, [('high_spirit_stone_block', 2), ('fallen_star', 4), ('dragonbone_ingot', 4)]),
          ('tribulation_array', 'Tribulation array', '6070d0', 'f0f0a0', 10, 'star_formation_flag', 'ring', [3, 10], {'aura': {'ward': 0.6}}, [('high_spirit_stone_block', 2), ('lightning_glass', 6), ('dragonbone_ingot', 4)])]
for n, lab, col, glow, tier, flags_, pattern, dist, does, ins in ARRAYS:
    array_pic(c(col), c(glow)).save('tiles', n); icon_of(n)
    tile(n, lab, False, mine_time=0.5, bench=True, bench_kind=n, bench_level=1, fuel_capacity=300000, fuels=FUELS, light=9, light_needs_fuel=True, formation=dict({'flags': flags_, 'pattern': pattern, 'distance': dist}, **does))
    item(n, lab, tier, 'benches', 2.0, places=n); rec('formation_table', n, ins, time=300)

# ---- artifacts: Artificing ---------------------------------------------------------------------------------------------------------
def artifact(name, label, kind, col, realm, ins, boons, set_=None, g=None, stage=None, trim=GOLD, tier=None):
    thing(trinket(kind, c(col), trim), name, label, tier or hi(min(realm, 5)), 'artifacts', 2.0, max_stack=1, relic=True, boons=boons, set=set_, needs_stage=stage if stage is not None else (first(realm) if realm > 1 else None))
    art(name, ins, g or min(realm, 5), time=300)
artifact('spirit_bead', 'Spirit-gathering bead', 'bead', '7ad8c0', 1, [('spirit_stone', 6), ('string', 2)], {'qi': 1.1}, 'wanderer')
artifact('jade_pendant', 'Jade pendant', 'pendant', '5fc89a', 1, [('spirit_jade', 2), ('string', 2)], {'armor': 0.05, 'luck': 0.05}, 'wanderer', stage=0)
artifact('beast_bell', 'Beast-calming bell', 'bell', 'c8a040', 2, [('azure_copper_ingot', 3), ('beast_core_2', 1)], {'unseen': 0.5}, 'wanderer')
artifact('azure_ring', 'Azure ring', 'ring', '2a8ad8', 2, [('azure_copper_ingot', 2), ('mid_spirit_stone', 1)], {'mine': 1.2}, 'wanderer')
artifact('flying_sword', 'Flying sword', 'sword', '9ad0e8', 2, [('azure_copper_sword', 1), ('hawk_plume', 4), ('mid_spirit_stone', 4), ('beast_core_2', 2)], {'flight': 1.0})
artifact('star_mirror', 'Star mirror', 'mirror', 'c8d0f0', 3, [('star_silver_ingot', 3), ('glass', 2), ('beast_core_3', 1)], {'sight': 1.0}, 'elder')
artifact('spirit_gourd', 'Spirit gourd', 'gourd', 'c8a050', 3, [('tree_amber', 3), ('spirit_vine', 4), ('beast_core_3', 1)], {'regen': 1.5, 'appetite': 0.6}, 'elder')
artifact('soul_seal', 'Soul seal', 'seal', 'b080ff', 4, [('soul_crystal', 2), ('void_gold_ingot', 2), ('guardian_rune', 1)], {'ward': 0.25, 'armor': 0.1}, 'elder')
artifact('void_fan', 'Void fan', 'fan', '7a50b0', 4, [('wraith_silk', 4), ('void_gold_ingot', 1), ('crane_feather', 4)], {'walk': 1.25, 'glide': 1.0}, 'elder')
artifact('void_flying_sword', 'Void-piercing sword', 'sword', 'c090e0', 4, [('flying_sword', 1), ('void_gold_sword', 1), ('soul_crystal', 2)], {'flight': 1.0, 'walk': 1.3, 'strength': 2})
artifact('divine_crown', 'Divine crown', 'crown', 'ff6080', 5, [('dragonbone_ingot', 3), ('fallen_star', 2), ('lion_mane', 1)], {'qi': 1.5, 'breakthrough': 0.05})
artifact('heavens_needle', "Heaven's needle", 'needle', 'fff8e8', 5, [('dragonbone_ingot', 2), ('lightning_glass', 2), ('beast_core_5', 1)], {'strength': 4, 'thorns': 6, 'luck': 0.15})
artifact('four_symbols_seal', 'Four Symbols seal', 'seal', 'f8f8e0', 6, [(e[0], 1) for e in ESS] + [('immortal_jade', 4), ('chaos_iron_ingot', 4)], {'qi': 2.0, 'ward': 0.5, 'luck': 0.3, 'magnet': 1.0}, g=5, stage=51, trim=RED, tier=11)
sets.append({'name': 'wanderer', 'label': 'Wandering cultivator', 'bonuses': [{'pieces': 2, 'boons': {'qi': 1.1}}, {'pieces': 4, 'boons': {'qi': 1.2, 'breakthrough': 0.05}}]})
sets.append({'name': 'elder', 'label': 'Sect elder', 'bonuses': [{'pieces': 2, 'boons': {'craft': 1.2}}, {'pieces': 4, 'boons': {'qi': 1.3, 'craft': 1.4}}]})

# ---- robes ----------------------------------------------------------------------------------------------------------------------
ROBES = [('disciple', 'Disciple', 'd8e0f0'), ('inner_sect', 'Inner sect', '4a90e0'), ('elder', 'Elder', 'c8d0ff'), ('patriarch', 'Patriarch', '7a50b0'), ('sovereign', 'Sovereign', 'f0c040')]
for realm, (n, lab, col) in enumerate(ROBES, 1):
    cloth, r = CLOTHS[realm - 1][0], ramp('181820', col, 'ffffff')
    for base, piece, plab, k in (('linen_cap', 'hood', "'s hood", 2), ('linen_shirt', 'robe', "'s robe", 5), ('linen_pants', 'trousers', "'s trousers", 3), ('leather_boots', 'shoes', "'s shoes", 2)):
        clone_item(base, n + '_' + piece, lab + plab, r, hi(realm), 'clothing', 0.6 * k, 'tint', armor=round(0.02 * realm, 2), warmth=0.35, cooling=0.15 if realm >= 3 else None, max_stack=1,
                   boons={'qi': round(1.0 + 0.06 * realm, 2)}, set=n + '_robes', needs_stage=first(realm) if realm > 1 else None)
        rec('spirit_loom', n + '_' + piece, [(cloth, k)] + ([('beast_core_%d' % realm, 1)] if piece == 'robe' else []), time=240)
    sets.append({'name': n + '_robes', 'label': lab + "'s robes", 'bonuses': [{'pieces': 2, 'boons': {'qi': round(1.05 + 0.05 * realm, 2)}}, {'pieces': 4, 'boons': {'breakthrough': round(0.03 * realm, 2), 'regen': 1.0 + 0.2 * realm}}]})
# What an immortal wears: four sets, one from each of the Sacred Beasts.
SACRED = [('azure_dragon', 'Azure Dragon', ramp('0a3a2a', '2fd09a', 'e0fff0'), [{'regen': 3.0, 'walk': 1.3}, {'flight': 1.0, 'lung': 10.0, 'magnet': 1.0}]),
          ('vermilion_phoenix', 'Vermilion Phoenix', ramp('5a0a0a', 'ff5030', 'ffe090'), [{'fireproof': 1.0, 'warmth': 0.5}, {'flight': 1.0, 'regen': 5.0, 'ward': 0.5}]),
          ('black_tortoise', 'Black Tortoise', ramp('0a0a14', '4a5a8a', 'c0d0f0'), [{'armor': 0.2, 'thorns': 10}, {'ward': 0.9, 'stamina': 2.0, 'lung': 10.0}]),
          ('white_tiger', 'White Tiger', ramp('5a5a5a', 'f0f0f0', 'ffffff'), [{'strength': 6, 'mine': 1.5}, {'luck': 0.5, 'craft': 2.0, 'unseen': 1.0}])]
for (n, lab, r, bonus), ess in zip(SACRED, ESS):
    for piece, plab, k in (('helmet', 'crown', 2), ('chestplate', 'mail', 4), ('leggings', 'greaves', 3), ('boots', 'treads', 2)):
        clone_item('titanium_' + piece, '%s_%s' % (n, plab), '%s %s' % (lab, plab), r, 11, 'clothing', 0.5 * k, armor=0.3, warmth=0.4, cooling=0.4, max_stack=1, boons={'qi': 1.5}, set=n, needs_stage=51)
        forge('%s_%s' % (n, plab), [('chaos_iron_ingot', k), (ess[0], 1), ('immortal_jade', k)], 5, time=900)
    sets.append({'name': n, 'label': 'The ' + lab, 'bonuses': [{'pieces': 2, 'boons': bonus[0]}, {'pieces': 4, 'boons': bonus[1]}]})

# ---- the ways through: each a long making --------------------------------------------------------------------------------------
def part(pic, name, label, tier, where, ins, count=1):
    thing(pic, name, label, tier, 'crafted', 1.0)
    where(name, ins) if callable(where) else portal(where, name, ins)
def gate(name, label, swirl, frame, leads, tier, ins, **over):
    clone_tile('portal_6', name, label, swirl, 'ore', frame, bench=None, bench_kind=None, bench_level=None, leads_to=leads, **over)
    clone_item('portal_6', name, label, swirl, tier, 'portals', 4.0, 'ore', frame, places=name)
    portal(*ins)
# The World Tree: a seed of it, woken by stages.
part(trinket('egg', c('8a6a3a'), c('5a3a1a')), 'dormant_seed', 'Dormant seed', 4, lambda n, i: art(n, i, 2), [('acorn', 8), ('spirit_soil', 4), ('mid_spirit_stone', 2)])
part(trinket('egg', c('9a8a3a'), c('5fc89a')), 'stirring_seed', 'Stirring seed', 4, lambda n, i: brew(n, i, 2, 1), [('dormant_seed', 1), ('purple_lingzhi', 4), ('frost_lotus', 2)])
part(trinket('egg', c('7aa850'), c('a0ffd0')), 'quickened_seed', 'Quickened seed', 4, lambda n, i: art(n, i, 2), [('stirring_seed', 1), ('beast_core_2', 4), ('azure_copper_ingot', 6)])
part(trinket('key', c('5fc89a'), c('2a8ad8')), 'root_key', 'Root key', 4, lambda n, i: forge(n, i, 2), [('azure_copper_block', 2), ('spirit_jade', 6), ('venom_sac', 2)])
part(trinket('rune', c('5a4a3a'), c('a0ffd0')), 'grove_tablet', 'Grove tablet', 4, lambda n, i: rec('scripture_desk', n, i, time=300), [('blank_jade_slip', 4), ('cinnabar_ink', 4), ('quickened_seed', 1)])
gate('world_tree_gate', 'World Tree gate', ramp('0a4a2a', '40d080', 'f0ffc0'), ramp('3a2a1a', '7a5a32', 'c8a870'), 'world_tree', 4, (5, 'world_tree_gate', [('grove_tablet', 1), ('root_key', 1), ('mid_spirit_stone_block', 2), ('jade_brick', 16)], 1, 900))
# The Fallen Heaven: a stair rebuilt.
part(trinket('plate', c('c8d0f0'), GOLD), 'celestial_chart', 'Celestial chart', 6, lambda n, i: rec('scripture_desk', n, i, time=300), [('spirit_paper', 12), ('cinnabar_ink', 6), ('star_mirror', 1)])
part(trinket('rune', c('d8d8e8'), c('f0c840')), 'heaven_stair_stone', 'Heaven-stair stone', 6, lambda n, i: forge(n, i, 3, 3), [('star_silver_block', 1), ('tree_amber', 4), ('rootstone', 16)])
part(trinket('bead', c('f8f8ff'), c('c8d0f0')), 'cloud_anchor', 'Cloud anchor', 6, lambda n, i: art(n, i, 3), [('crane_feather', 8), ('moonweave', 2), ('beast_core_3', 4)])
part(trinket('key', c('f0c840'), c('c8d0f0')), 'heaven_key', 'Heaven key', 6, 7, [('celestial_chart', 1), ('cloud_anchor', 1), ('wyrm_scale', 4)])
gate('fallen_heaven_gate', 'Fallen Heaven gate', ramp('1a1a5a', '8090ff', 'ffffff'), ramp('8a8a9a', 'd8d8e8', 'ffffff'), 'fallen_heaven', 7, (7, 'fallen_heaven_gate', [('heaven_key', 1), ('heaven_stair_stone', 9), ('high_spirit_stone', 4)], 1, 1200), opens_above=40)
# Heaven's Trials: one gate to each court, for an immortal.
for (short, lab, col), ess_col, need in zip(COURTS, ('2fd09a', 'ff5030', '4a5a8a', 'f0f0f0'), (('dragon_beard_grass', 'wyrm_scale'), ('phoenix_blossom', 'lightning_glass'), ('heaven_root', 'guardian_rune'), ('nine_leaf_fern', 'lion_mane'))):
    part(trinket('seal', c(ess_col), GOLD), short + '_summons', lab + ' summons', 11, 11, [('heaven_tempered_essence', 1), ('immortal_jade', 1) if short != 'dragon' else ('fallen_star', 4), (need[0], 9), (need[1], 4)])
    gate(short + '_trial_gate', lab + ' trial gate', ramp('101010', ess_col, 'ffffff'), ramp('2a2a34', '8a849a', 'e8e0f0'), short + '_trial', 11, (11, short + '_trial_gate', [(short + '_summons', 1), ('heaven_brick', 16), ('supreme_spirit_stone', 2)], 1, 1500))

# ---- the road itself ---------------------------------------------------------------------------------------------------------------
REALMS = [{'label': 'Qi Refining', 'stages': 10, 'qi': 100, 'growth': 1.22, 'boons': {'regen': 1.2}},
          {'label': 'Foundation Building', 'stages': 10, 'qi': 1000, 'growth': 1.22, 'boons': {'appetite': 0.5, 'lung': 2.0, 'regen': 1.5}, 'pill': 'foundation_pill', 'bolts': 3, 'bolt': 0.18},
          {'label': 'Core Formation', 'stages': 10, 'qi': 10000, 'growth': 1.22, 'boons': {'appetite': 0.05, 'lung': 4.0, 'regen': 2.0, 'armor': 0.1}, 'pill': 'core_formation_pill', 'bolts': 6, 'bolt': 0.2},
          {'label': 'Nascent Soul', 'stages': 10, 'qi': 100000, 'growth': 1.22, 'boons': {'appetite': 0.02, 'lung': 8.0, 'regen': 3.0, 'armor': 0.2, 'sight': 1.0}, 'pill': 'nascent_soul_pill', 'bolts': 9, 'bolt': 0.22, 'revives': True},
          {'label': 'Divinity', 'stages': 10, 'qi': 1000000, 'growth': 1.2, 'boons': {'appetite': 0.01, 'lung': 20.0, 'regen': 4.0, 'armor': 0.3, 'sight': 1.0, 'flight': 1.0, 'fireproof': 1.0}, 'pill': 'divinity_pill', 'bolts': 12, 'bolt': 0.24, 'revives': True}]
HERB_OF = {r: [h[0] for h in HERBS[:10] if h[2] == r] for r in range(1, 6)}
METAL_OF = {r: METALS[r - 1][0] + '_ingot' for r in range(1, 6)}
needs = []
for r in range(1, 6):
    needs.append({'stage': first(r) + 2, 'items': [{'item': HERB_OF[r][0], 'count': 4}]})
    needs.append({'stage': first(r) + 5, 'items': [{'item': 'beast_core_%d' % r, 'count': 2}, {'item': METAL_OF[r], 'count': 2}]})
    needs.append({'stage': first(r) + 8, 'items': [{'item': HERB_OF[r][1], 'count': 3}, {'item': STONES[min(r - 1, 2)][0], 'count': 5 if r != 2 else 30}]})
# (Out of Divinity and into immortality: the last step asks most.)
needs.append({'stage': 50, 'items': [{'item': 'fallen_star', 'count': 9}, {'item': 'supreme_spirit_stone', 'count': 3}, {'item': 'longevity_pill', 'count': 1}]})
cultivation = {'realms': REALMS, 'immortal': 'Immortal', 'immortal_boons': {'appetite': 0.0, 'lung': 100.0, 'regen': 8.0, 'armor': 0.4, 'sight': 1.0, 'flight': 1.0, 'fireproof': 1.0, 'luck': 0.2}, 'stage_gain': 0.02, 'rate': 0.05,
               'chance_first': 0.95, 'chance_last': 0.6, 'fail_loss': 0.3, 'toxin_fade': 0.00005, 'needs': needs, 'called': {'bolts': 9, 'bolt': 0.3, 'leaves': {'item': 'heaven_tempered_essence', 'count': 1}},
               'elements': ['Metal', 'Wood', 'Water', 'Fire', 'Earth']}

# ---- the places ---------------------------------------------------------------------------------------------------------------------
def biome(name, label, dim, surface, soil, under, trees_=(), one_in=0, wild=(), spikes=(), treasure=None, **over):
    b = {'name': name, 'label': label, 'dimension': dim, 'surface': surface, 'soil': soil, 'under': {'tile': under, 'depth': 40}}
    if trees_: b.update(tree_one_in=one_in, trees=[{'tree': t, 'weight': w} for t, w in trees_])
    if wild: b['plants'] = [{'tile': t + '_0', 'one_in': n} for t, n in wild]
    if spikes: b['spikes'] = [clean({'tile': t, 'one_in': n, 'height': h, 'hang': hang or None, 'arch': arch or None}) for t, n, h, hang, arch in spikes]
    if treasure: b['treasure'] = {'tile': treasure[0], 'one_in': treasure[1]}
    b.update(over); biomes.append(b)
B, RS = 'world_tree_bough', 'rootstone'
biome('sunlit_canopy', 'Sunlit canopy', 'world_tree', B, B, RS, [('spirit_peach', 3), ('moon_laurel', 1)], 9, [('dragon_beard_grass', 9), ('spirit_grass', 8), ('spirit_tea', 14), ('spirit_cotton', 16)], [('glow_blossom', 14, [1, 2], False, False)])
biome('mistwood', 'Mistwood', 'world_tree', B, B, RS, [('spirit_pine', 3), ('moon_laurel', 1)], 6, [('purple_lingzhi', 8), ('frost_lotus', 10), ('mountain_ginseng', 10)], [('spirit_vine', 6, [2, 9], True, False)], qi=0.3)
biome('amber_hollows', 'Amber hollows', 'world_tree', B, B, RS, [('moon_laurel', 1)], 14, [('fire_cloud_flower', 7), ('dragon_beard_grass', 12)], [('tree_amber', 9, [1, 3], False, False), ('tree_amber', 7, [1, 4], True, False)], ('ancient_cache', 22), qi=0.5)
biome('firefly_boughs', 'Firefly boughs', 'world_tree', B, B, RS, [('spirit_peach', 1), ('spirit_pine', 1)], 10, [('fire_cloud_flower', 12), ('spirit_rice', 10)], [('glow_blossom', 5, [1, 3], False, False), ('glow_blossom', 5, [1, 4], True, False)], ('ancient_cache', 50))
HM = 'heaven_marble'
biome('shattered_court', 'Shattered court', 'fallen_heaven', 'heaven_paving', HM, HM, [], 0, [], [('heaven_brick', 6, [3, 9], False, False), ('ruined_heaven_brick', 8, [4, 7], False, True), ('ruined_heaven_brick', 5, [1, 5], True, False)], ('heavenly_coffer', 26))
biome('immortal_orchard', 'Immortal orchard', 'fallen_heaven', 'star_grass', 'heaven_soil', HM, [('immortal_peach', 2), ('star_willow', 3)], 6, [('soul_orchid', 9), ('nine_leaf_fern', 12)], [('heaven_marble', 8, [1, 4], True, False)], qi=0.5)
biome('star_fields', 'Star fields', 'fallen_heaven', 'star_grass', 'heaven_soil', HM, [('star_willow', 1)], 16, [('heaven_root', 12), ('soul_orchid', 14)], [('star_crystal', 5, [2, 8], False, False), ('star_crystal', 4, [2, 7], True, False)], ('heavenly_coffer', 70), qi=1.0)
biome('thunder_scar', 'Thunder scar', 'fallen_heaven', 'scorched_marble', 'scorched_marble', HM, [], 0, [('phoenix_blossom', 9)], [('lightning_glass', 5, [2, 9], False, False), ('scorched_marble', 4, [2, 8], False, False), ('scorched_marble', 4, [2, 8], True, False)], ('heavenly_coffer', 50), qi=1.5)
biome('lotus_terraces', 'Lotus terraces', 'fallen_heaven', 'star_grass', 'cloud_jade', 'cloud_jade', [('immortal_peach', 1)], 12, [('phoenix_blossom', 10), ('heaven_root', 10), ('nine_leaf_fern', 8), ('frost_lotus', 8)], [('cloud_jade', 8, [3, 6], False, True)], qi=1.0)
for short, lab, col in COURTS: biome(short + '_trial', lab + "'s court", short + '_trial', short + '_court', short + '_court', short + '_court')

def vein(name, tile_, hosts, size, per, depth, dim=None): veins.append(clean({'name': name, 'tile': tile_, 'dimension': dim, 'in': hosts, 'size': size, 'per_100_columns': per, 'depth': depth}))
vein('dao_spirit_stone', 'spirit_stone_ore', ['stone'], [2, 5], 12, [6, 220]); vein('dao_cinnabar', 'cinnabar_ore', ['stone'], [2, 5], 8, [8, 160]); vein('dao_spirit_iron', 'spirit_iron_ore', ['stone'], [3, 6], 9, [16, 240])
vein('dao_spirit_jade', 'spirit_jade_ore', ['stone'], [2, 4], 5, [60, 320]); vein('dao_azure_copper', 'azure_copper_ore', ['stone'], [3, 6], 7, [100, 420]); vein('dao_mid_spirit_stone', 'mid_spirit_stone_ore', ['stone'], [2, 4], 4, [130, 420])
vein('dao_star_silver', 'star_silver_ore', [RS, 'world_tree_bark'], [3, 6], 40, [0, 260], 'world_tree'); vein('dao_tree_amber', 'tree_amber', ['world_tree_bark'], [2, 4], 30, [0, 240], 'world_tree')
vein('dao_wt_spirit', 'mid_spirit_stone_ore', [RS, 'world_tree_bark'], [2, 5], 24, [0, 260], 'world_tree'); vein('dao_wt_jade', 'spirit_jade_ore', [RS], [2, 5], 14, [200, 260], 'world_tree')
FHR = [HM, 'cloud_jade', 'scorched_marble']
vein('dao_void_gold', 'void_gold_ore', FHR, [3, 6], 26, [0, 300], 'fallen_heaven'); vein('dao_high_spirit', 'high_spirit_stone_ore', FHR, [2, 5], 14, [0, 300], 'fallen_heaven')
vein('dao_dragonbone', 'dragonbone_ore', FHR, [2, 5], 14, [130, 300], 'fallen_heaven'); vein('dao_cloud_jade', 'cloud_jade', [HM], [6, 16], 10, [0, 300], 'fallen_heaven')
decor.append({'name': 'dao_soul_crystal', 'tile': 'soul_crystal', 'dimension': 'fallen_heaven', 'per_100_columns': 6, 'depth': [0, 300]})
decor.append({'name': 'dao_wt_vine', 'tile': 'spirit_vine', 'dimension': 'world_tree', 'per_100_columns': 20, 'depth': [0, 230], 'hang': [2, 9]})
# The ore blocks of the home rocks stand in for stone in other hosts' colours where they are found elsewhere.
for t in tiles:
    if t['name'] in ('mid_spirit_stone_ore', 'spirit_jade_ore'): t['hardness'] = None
tiles[:] = [clean(t) for t in tiles]

# What was built, and what is left of it. Rows from the top; ~ leaves what is there.
def structure(name, rows, legend): assert len({len(r) for r in rows}) == 1, name; structures.append({'name': name, 'rows': rows, 'legend': legend})
SECT = {'R': {'front': 'sect_roof'}, '#': {'front': 'sect_brick', 'wall': 'sect_brick'}, '.': {'wall': 'sect_brick'}, 'P': {'front': 'vermilion_planks', 'wall': 'vermilion_planks'}, 'p': {'wall': 'vermilion_planks'},
        'D': {'front': 'wooden_door', 'wall': 'sect_brick'}, 'F': {'front': 'sect_paving', 'wall': 'sect_paving'}, 'L': {'wall': 'sect_brick', 'back': 'paper_lantern'}, 'l': {'back': 'paper_lantern'}, 'C': {'wall': 'sect_brick', 'back': 'sect_chest'},
        'M': {'wall': 'sect_brick', 'back': 'meditation_mat'}, 'A': {'wall': 'sect_brick', 'back': 'incense_burner'}, 'S': {'front': 'spirit_soil', 'wall': 'spirit_soil'}, 'g': {'back': 'spirit_grass_0'}, 'h': {'back': 'mountain_ginseng_0'},
        'J': {'front': 'jade_brick', 'wall': 'jade_brick'}, 'j': {'wall': 'jade_brick'}, 'c': {'wall': 'jade_brick', 'back': 'sect_chest'}, 'm': {'wall': 'jade_brick', 'back': 'meditation_mat'}, 'a': {'wall': 'jade_brick', 'back': 'spirit_lamp'},
        'O': {'front': 'spirit_stone_ore', 'wall': 'jade_brick'}}
structure('sect_hall', ['~~~RRRRRRRRRR~~~', '~RRRRRRRRRRRRRR~', 'RRRRRRRRRRRRRRRR', '~#..L......L..#~', '~D............D~', '~D.C..M..M..A.D~', '~FFFFFFFFFFFFFF~'], SECT)
structure('sect_pagoda', ['~~~~RRRR~~~~', '~~RRRRRRRR~~', '~~~#.L..#~~~', '~~~#.M.C#~~~', '~RRRRRRRRRR~', 'RRRRRRRRRRRR', '~~#..L...#~~', '~~D......D~~', '~~D..A...D~~', '~~FFFFFFFF~~'], SECT)
structure('sect_gate', ['RRRRRRRRRR', '~RRRRRRRR~', '~Pl~~~~lP~', '~P~~~~~~P~', '~P~~~~~~P~', '~FFFFFFFF~'], SECT)
structure('sect_garden', ['~l~~~~~~l~', '~P~g~h~gP~', '~SSSSSSSS~'], SECT)
structure('sect_shrine', ['~~RRRR~~', '~RRRRRR~', '~p.L..p~', '~p.AC.p~', '~FFFFFF~'], SECT)
structure('sect_library', ['~RRRRRRRRRR~', 'RRRRRRRRRRRR', '~#.L....L.#~', '~#C.....C.#~', '~D........D~', '~D..M..M..D~', '~FFFFFFFFFF~'], SECT)
structure('cave_abode', ['JJJJJJJJJJ', 'JjjajjajjJ', '~jjjjjjjj~', '~jmjcjjOj~', 'JJJJJJJJJJ'], SECT)
structure('cave_nook', ['JJJJJJJ', '~jjajj~', '~gjhjg~', 'JSSSSSJ'], SECT)
villages.append({'name': 'ruined_sect', 'where': 'ground', 'at_least': 1, 'per_1000_columns': 0.9, 'houses': [3, 6], 'centre': 'sect_hall', 'street': 'sect_paving', 'ground': 'dirt',
                 'buildings': ['sect_pagoda', 'sect_gate', 'sect_garden', 'sect_shrine', 'sect_library', 'sect_garden']})
villages.append({'name': 'immortal_cave', 'where': 'cave', 'at_least': 1, 'per_1000_columns': 0.7, 'houses': [1, 2], 'centre': 'cave_abode', 'street': 'jade_brick', 'ground': 'stone', 'buildings': ['cave_nook', 'cave_nook']})
HEAVEN = {'R': {'front': 'heaven_roof'}, '#': {'front': 'heaven_brick', 'wall': 'heaven_brick'}, 'r': {'front': 'ruined_heaven_brick', 'wall': 'ruined_heaven_brick'}, '.': {'wall': 'heaven_brick'}, ',': {'wall': 'ruined_heaven_brick'},
          'F': {'front': 'heaven_paving', 'wall': 'heaven_paving'}, 'L': {'wall': 'heaven_brick', 'back': 'spirit_lamp'}, 'l': {'back': 'spirit_lamp'}, 'C': {'wall': 'heaven_brick', 'back': 'heavenly_coffer'}, 'X': {'front': 'star_crystal'},
          'G': {'front': 'star_glass', 'wall': 'heaven_brick'}, 'A': {'wall': 'heaven_brick', 'back': 'incense_burner'}, 'M': {'wall': 'heaven_brick', 'back': 'meditation_mat'}, 'H': {'front': 'high_spirit_stone_ore'}}
structure('ruined_pavilion', ['~~RRRRRR~~', '~RRRRRRRR~', 'RRRR~~RRRR', '~#~l~~~~#~', '~#~~~~~~r~', '~r~~C~~~#~', '~FFFFFFFF~'], HEAVEN)
structure('broken_palace', ['~~~~RRRRRRRR~~~~~~', '~~RRRRRRRRRRRR~~~~', '~RRRRRRRRR~~RRR~~~', '~#..L..,,~~~~.r~~~', '~#.....,~~~~~~#~~~', '~#G....~~~~~~.#~~~', '~~......,~~~~L#~~~', '~~.M.C..,,~~A.r~~~', '~FFFFFFFFFFrFFFF~~'], HEAVEN)
structure('star_altar', ['~~~~~X~~~~~', '~~~~###~~~~', '~l~#####~l~', '~#~~~~~~~#~', '~#FFFFFFF#~', 'FFFFHFHFFFF'], HEAVEN)
structure('heaven_gatehouse', ['RRRRRRRRRR', '~RRRRRRRR~', '~##~~~~##~', '~#l~~~~l#~', '~#~~~~~~r~', '~r~~~~~~#~', '~#~~~~~~#~', '~FFFFFFFF~'], HEAVEN)
structure('fallen_pagoda', ['~~~RRRR~~~', '~~RRRRRR~~', '~~~#.L#~~~', '~RRRRRRRR~', 'RRRRR~~RRR', '~~#.,,~r~~', '~~#C.,~~~~', '~RRRRRRRR~', '~~r.M..#~~', '~~~..A.#~~', '~~FFFFFF~~'], HEAVEN)
for short, lab, col in COURTS:
    k = short + '_court'
    structure(short + '_arena', ['PPPP~~~~~~~~~~~~~~PPPP', 'PP~~~~~~~~~~~~~~~~~~PP', 'Pl~~~~~~~~~~~~~~~~~~lP', 'P~~~~~~~~~~~~~~~~~~~~P', 'P~~~~~~~~~~~~~~~~~~~~P', 'P~~~~~~~~~~~~~~~~~~~~P', 'FFFFFFFFFFFFFFFFFFFFFF'],
              {'P': {'wall': k}, 'l': {'wall': k, 'back': 'spirit_lamp'}, 'F': {'front': k, 'wall': k}})

SKY = {'dragon': ([20, 70, 60], [6, 30, 28]), 'phoenix': ([110, 30, 20], [50, 10, 8]), 'tortoise': ([14, 18, 40], [4, 6, 16]), 'tiger': ([190, 190, 200], [60, 60, 70])}
dimensions = [
    {'name': 'world_tree', 'label': 'The World Tree', 'generator': 'grove', 'height': 260, 'rock': RS, 'edge': 'heaven_wall', 'platform': B, 'climate_wavelength': 110, 'qi': 2.5,
     'grove': {'trunk': 'world_tree_bark', 'bough': B, 'leaf': 'world_tree_leaves', 'vine': 'spirit_vine', 'width': 9, 'gap': 64}, 'sky_day': [112, 176, 140], 'sky_night': [8, 34, 44], 'ambient': 6},
    {'name': 'fallen_heaven', 'label': 'The Fallen Heaven', 'generator': 'islands', 'height': 300, 'rock': HM, 'edge': 'heaven_wall', 'platform': 'heaven_paving', 'falls_home': True, 'climate_wavelength': 130, 'qi': 5.0,
     'islands': {'per_100_columns': 7, 'length': [14, 84], 'depth': [5, 22]}, 'structures': ['ruined_pavilion', 'broken_palace', 'star_altar', 'heaven_gatehouse', 'fallen_pagoda'], 'structure_share': 0.4, 'bridge': 'heaven_bridge',
     'sky_day': [30, 22, 70], 'sky_night': [6, 4, 22], 'ambient': 7, 'geodes': [{'shell': 'star_glass', 'gem': 'fallen_star', 'per_100_columns': 0.9, 'radius': [3, 6]}]}]
for short, lab, col in COURTS:
    dimensions.append({'name': short + '_trial', 'label': "Heaven's Trial: the " + lab, 'generator': 'void', 'width': 128, 'height': 96, 'rock': short + '_court', 'edge': 'heaven_wall', 'platform': short + '_court', 'qi': 8.0,
                       'void': {'length': 48, 'depth': 9}, 'structures': [short + '_arena'], 'structure_share': 1.0, 'sky_day': SKY[short][0], 'sky_night': SKY[short][1], 'ambient': 8})

# ---- written out ---------------------------------------------------------------------------------------------------------------------
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v)) for k, v in obj.items()) + '\n}\n')
for kind, group in (('tile', tiles), ('item', items), ('creature', mobs)):
    names = [t['name'] for t in group]; assert len(names) == len(set(names)), (kind, sorted({n for n in names if names.count(n) > 1}))
# Everything named is something there is: a wrong name is found here, not when the game starts.
have_i = set(BI) | {i['name'] for i in items}; have_t = set(BT) | {t['name'] for t in tiles}
bad = sorted({i['item'] for r in recipes for i in r['inputs'] if i['item'] not in have_i} | {r['output']['item'] for r in recipes if r['output']['item'] not in have_i})
bad += sorted({d['item'] for m in mobs for d in m['drops'] if d['item'] not in have_i} | {i['places'] for i in items if i.get('places') and i['places'] not in have_t})
bad += sorted({x['item'] for t in tiles for l in t.get('loot', []) for x in (l.get('one_of') or [l]) if x.get('item') and x['item'] not in have_i})
bad += sorted({n['item'] for s in needs for n in s['items'] if n['item'] not in have_i} | {t['drop'] for t in tiles if t.get('drop') and t['drop'] not in have_i})
assert not bad, bad
for n, i in enumerate(items):
    if i.get('equip'):
        o = {}
        for k, v in i.items():
            o[k] = v
            if k == 'equip': o['worn'] = 'worn/%s.png' % i['name']
        items[n] = o
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'sets': sets, 'items': items}); dump('recipes.json', {'recipes': recipes}); dump('trees.json', {'trees': trees}); dump('mobs.json', {'mobs': mobs})
dump('worldgen.json', {'structures': structures, 'villages': villages, 'dimensions': dimensions, 'biomes': biomes, 'biome_plants': plants, 'veins': veins, 'cave_decor': decor})
dump('rules.json', {'cultivation': cultivation})
dump('trade.json', {'categories': [{'name': 'pills', 'label': 'Pills'}, {'name': 'scriptures', 'label': 'Scriptures'}, {'name': 'artifacts', 'label': 'Artifacts'}]})
json.dump({'label': 'Seeker of the Dao', 'icon': 'icon.png', 'description': 'Cultivate toward immortality: qi, manuals, pills, formations, techniques, spirit beasts, the World Tree and the Fallen Heaven.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
book(c('2a2a6a'), GOLD, RED).im.resize((48, 48), Image.NEAREST).save(os.path.join(mod, 'icon.png'))
made = {}
for r in recipes: made.setdefault(r['output']['item'], []).append([i['item'] for i in r['inputs']])
def depth(i, seen=()):
    if i not in made or i in seen: return 0
    return 1 + max((max((depth(x, seen + (i,)) for x in ins), default=0) for ins in made[i]), default=0)
print('%d tiles, %d items, %d recipes, %d trees, %d creatures, %d biomes, %d structures, %d sets' % (len(tiles), len(items), len(recipes), len(trees), len(mobs), len(biomes), len(structures), len(sets)))
print('stages: ' + ', '.join('%s %d' % (n, depth(n)) for n in ('world_tree_gate', 'fallen_heaven_gate', 'dragon_trial_gate', 'azure_dragon_mail', 'dao_of_heaven')))
tools = os.path.dirname(os.path.abspath(__file__))
subprocess.run([sys.executable, os.path.join(tools, 'worn_art.py'), repo], check=True)
subprocess.run([sys.executable, os.path.join(tools, 'caged_art.py'), repo], check=True)
