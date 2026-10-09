#!/usr/bin/env python3
"""Writes mods/hull_and_horizon: a game of its own ("standalone": true in its mod.json), loaded in place of the
game's data. There is no land: a sea that goes by, and a ship built of blocks that never moves.

    python3 tools/mod_hull_and_horizon.py .

Everything of the mod's is written here - data files and pictures - and nothing of the game's is copied. The
drawing helpers at the head are the same as those of the other generators (see mod_plunder_and_thunder.py):
change one, think of the others. The plan, and what was and was not built, is ModDescriptions/HullAndHorizon.txt.
Near the end it works out how many makings deep each keel is and asserts the least for each."""
import colorsys, copy, json, os, random, subprocess, sys, zlib
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
DATA = os.path.join(repo, 'data')
mod = os.path.join(repo, 'mods/hull_and_horizon'); TEX = os.path.join(mod, 'textures')
for d in ('tiles', 'items', 'mobs', 'worn'): os.makedirs(os.path.join(TEX, d), exist_ok=True)
# (Every picture here is drawn by this script or by the tools it runs: what was drawn last time goes first, so that nothing stale is left.)
for d in ('tiles', 'items', 'mobs', 'worn'):
    for f in os.listdir(os.path.join(TEX, d)):
        if f.endswith('.png'): os.remove(os.path.join(TEX, d, f))
BT, BI, BTR = {}, {}, {}

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

SEA, TAR, CANVAS, ROPE, BRASS, OAK = c('2a6a9a'), c('3a2a22'), c('e8dcc0'), c('b89a60'), c('c8a040'), c('8a5a30')
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

def plant(arch, col, stage, stem=c('3a8a4a')):
    t = P(); h = [3, 6, 9, 12][stage]; top = 15 - h
    if arch == 'cane':
        for x in (5, 8, 11)[:1 + stage if stage < 3 else 3]: t.r(x, top, x, 15, col); [t.p(x, y, dk(col)) for y in range(top + 2, 15, 3)]; t.line(x, top, x + 2, top - 1 if top > 0 else 0, stem)
    elif arch == 'leaf':
        t.r(7, top + 2, 7, 15, stem)
        for k in range(stage + 1): y = 13 - k * 3; t.r(4, y - 1, 6, y, col); t.r(8, y - 2, 11, y - 1, col); t.p(4, y - 1, lt(col))
    elif arch == 'boll':
        t.r(7, top + 2, 7, 15, stem); t.line(7, 12, 4, 10, stem); t.line(7, 10, 11, 8, stem)
        if stage >= 2: t.disc(4, 9, 1.4, col); t.disc(11, 7, 1.4, col)
        if stage == 3: t.disc(7.5, top + 1, 1.8, col)
    elif arch == 'vine':
        ys = list(range(15, top, -1))
        for k, y in enumerate(ys): t.p(7 + (1 if (k // 2) % 2 else -1), y, stem)
        for k, y in enumerate(ys[2::3]): t.r(8 if k % 2 else 5, y, 9 if k % 2 else 6, y + 1, col)
    else:
        t.r(7, 13, 8, 15, dk(stem)); t.disc(7.5, 12 - stage, 1.5 + stage, stem)
        for k in range(stage * 2): t.p(5 + (k * 3) % 6, 13 - stage - (k * 2) % (2 + stage), col)
    return t
def creature(kind, name, label, body, accent, health, part, spawn, mean=False, **kw):
    for f in (0, 1): beast(kind, c(body), c(accent), f).save('mobs', name + ('_walk' if f else ''))
    flier = kind in ('bird',)
    m = {'name': name, 'label': label, 'texture': 'mobs/%s.png' % name, 'walk': ['mobs/%s_walk.png' % name, 'mobs/%s.png' % name], 'hop': 0.0 if kind in ('serpent', 'tortoise') else 0.06, 'step_ticks': 3 if mean else 4,
         'move_chance': 0.5 if flier else 0.2, 'turn_chance': 0.25, 'health': health, 'cap': 3, 'spawn': spawn, 'flies': True if flier else None, 'drops': part, 'sting': {'ticks': 40, 'rest_ticks': 60, 'sight': 7} if mean and not flier and 'swim' not in kw else None}
    m.update(kw); mobs.append(clean(m))

# ==== Hull & Horizon ===============================================================================================
DRIFT, PLANK, TARRED, COPPER, VERDI, IRON, STEEL, BLACK, BRASSY, CLOTH = c('9a8468'), c('b08a54'), c('4a382c'), c('c87a4a'), c('5aa890'), c('6a6e78'), c('9aa4b0'), c('2a2c38'), c('c8a040'), c('e8dcc0')
SAND, SOIL, GRASS, STONE, CLAY, CORAL, COAL, ICE, BONE = c('e0d0a0'), c('6a4a30'), c('5a9a48'), c('7a7c80'), c('b07a5a'), c('e07a8a'), c('2a2a30'), c('c0e0f0'), c('e8e0d0')
seas, structures, quests, tabs = [], [], [], []
RAW = set()        # what is got without making: adrift, on the bed, dug, grown, dropped
def raw(*names): RAW.update(names)

def T(pic, name, label, tier, solid=True, cat='blocks', mult=0.3, iover=None, **kw):
    """A block and the item that places it."""
    pic.save('tiles', name); icon_of(name); kw.setdefault('tool', 'axe')
    tile(name, label, solid, **kw); return item(name, label, tier, cat, mult, places=name, **(iover or {}))
def I(pic, name, label, tier, cat='crafted', mult=1.0, **kw): return thing(pic, name, label, tier, cat, mult, **kw)
def R(bench, out, ins, count=1, level=None, time=100, **kw): rec(bench, out, ins, count, level=level, time=time, **kw)

# ---- pictures of a ship's things ---------------------------------------------------------------------------------
def logs(name, col):
    t = P()
    for y in (0, 5, 10): t.r(0, y, 15, y + 4, col); t.r(0, y, 15, y, lt(col)); t.r(0, y + 4, 15, y + 4, dk(col, 0.6))
    t.r(0, 15, 15, 15, dk(col, 0.5)); t.r(4, 0, 5, 15, ROPE); t.r(11, 0, 12, 15, ROPE); t.r(4, 4, 5, 4, dk(ROPE)); t.r(11, 9, 12, 9, dk(ROPE)); return t.speck(name, dk(col, 0.85), 12, col)
def tarred(name):
    t = planks(name, TARRED)
    for x, y in ((2, 3), (9, 7), (5, 11), (13, 3)): t.r(x, y, x, y + 2, c('1a1410'))
    return t
def sheathed(name, col=COPPER):
    t = P(col); t.r(0, 7, 15, 7, dk(col, 0.6)); t.r(0, 15, 15, 15, dk(col, 0.6)); t.r(7, 0, 7, 6, dk(col, 0.7)); t.r(3, 8, 3, 14, dk(col, 0.7)); t.r(11, 8, 11, 14, dk(col, 0.7))
    t.r(0, 0, 15, 0, lt(col)); t.r(0, 8, 15, 8, lt(col))
    for x, y in ((1, 2), (5, 2), (9, 2), (13, 2), (1, 10), (6, 10), (13, 10)): t.p(x, y, dk(col, 0.5))
    return t.speck(name, VERDI, 9)
def ironbound(name):
    t = planks(name, c('8a6a44')); t.r(2, 0, 4, 15, IRON); t.r(11, 0, 13, 15, IRON); t.r(2, 0, 2, 15, lt(IRON)); t.r(4, 0, 4, 15, dk(IRON)); t.r(11, 0, 11, 15, lt(IRON)); t.r(13, 0, 13, 15, dk(IRON))
    for y in (2, 6, 10, 14): t.p(3, y, INK).p(12, y, INK)
    return t
def plate(name, col, rivets=True, belt=False):
    t = P(col); t.r(0, 0, 15, 0, lt(col, 1.15)); t.r(0, 0, 0, 15, lt(col, 1.1)); t.speck(name, dk(col, 0.92), 16)
    if rivets:
        for k in range(1, 16, 4): t.p(k, 2, dk(col, 0.55)); t.p(2, k, dk(col, 0.55))
    if belt: t.r(0, 6, 15, 9, dk(col, 0.75)); t.r(0, 6, 15, 6, lt(col, 1.2)); [t.p(x, 8, dk(col, 0.45)) for x in range(1, 16, 3)]
    return t
def deck(name, col):
    t = P(col)
    for x in (3, 7, 11, 15): t.r(x, 0, x, 15, dk(col, 0.7))
    t.r(0, 0, 15, 0, lt(col, 1.2)); t.r(0, 15, 15, 15, dk(col, 0.6)); return t.speck(name, dk(col, 0.88), 8, col)
def sail_pic(col, furled=False, lateen=False):
    t = P()
    if furled:
        t.r(0, 0, 15, 4, col); t.r(0, 4, 15, 4, dk(col, 0.7)); t.r(0, 0, 15, 0, lt(col))
        for x in (2, 7, 12): t.r(x, 0, x, 5, ROPE)
        return t
    t.r(0, 0, 15, 15, col)
    for x in (5, 10): t.r(x, 0, x, 15, dk(col, 0.86))
    for y in range(16): t.p(15, y, dk(col, 0.75)); t.p(0, y, lt(col, 1.1))
    t.r(0, 13, 15, 15, dk(col, 0.82)); t.r(0, 0, 15, 0, dk(col, 0.7))
    if lateen: t.line(0, 15, 15, 0, dk(col, 0.6)); t.line(1, 15, 15, 1, lt(col, 1.1))
    return t
def mast_pic(col, band=None):
    t = P().r(6, 0, 9, 15, col); t.r(6, 0, 6, 15, lt(col)); t.r(9, 0, 9, 15, dk(col, 0.65))
    if band: t.r(5, 6, 10, 7, band); t.r(5, 14, 10, 15, band)
    return t
def yard_pic(col): return P().r(0, 6, 15, 8, col).r(0, 6, 15, 6, lt(col)).r(0, 8, 15, 8, dk(col, 0.65)).r(7, 4, 8, 10, ROPE)
def wheel_pic(col, trim):
    t = P().disc(7.5, 7.5, 6.5, col).disc(7.5, 7.5, 4.6, (0, 0, 0, 0)).disc(7.5, 7.5, 1.6, trim)
    for a, b, cc, d in ((7, 0, 8, 15), (0, 7, 15, 8)): t.r(a, b, cc, d, col)
    t.line(2, 2, 13, 13, col); t.line(13, 2, 2, 13, col); t.disc(7.5, 7.5, 1.2, trim); return t.outline()
def tiller_pic(col): return P().line(2, 4, 12, 11, col).line(2, 5, 12, 12, dk(col)).r(11, 10, 13, 15, dk(col, 0.6)).outline()
def anchor_pic(col, stock=None):
    t = P().r(7, 2, 8, 13, col); t.r(4, 4, 11, 4, stock or col); t.disc(7.5, 1.5, 1.6, col); t.line(3, 10, 7, 14, col); t.line(12, 10, 8, 14, col); t.p(2, 9, col).p(13, 9, col); return t.outline()
def stone_on_rope(): return P().r(7, 0, 8, 6, ROPE).disc(7.5, 10, 4, STONE).p(6, 8, lt(STONE)).outline()
def net_pic(col, floats=c('c84a3a')):
    t = P()
    for k in range(0, 16, 4): t.r(k, 2, k, 15, col); t.r(0, k + 3, 15, k + 3, col)
    t.r(0, 1, 15, 2, dk(col)); [t.r(x, 0, x + 1, 1, floats) for x in (1, 6, 11)]; return t
def gun_pic(col, big=0, carriage=PLANK):
    t = P(); t.r(2, 6 - big, 13, 9, col); t.r(2, 6 - big, 13, 6 - big, lt(col)); t.r(13, 5 - big, 15, 10, col); t.p(15, 7, INK); t.r(1, 5 - big, 3, 10, dk(col))
    t.r(3, 10, 11, 12, carriage); t.disc(4, 13.5, 1.6, dk(carriage, 0.6)); t.disc(10, 13.5, 1.6, dk(carriage, 0.6)); return t.outline()
def turret_pic(col, barrels=2):
    t = P().r(2, 6, 11, 14, col); t.r(2, 6, 11, 6, lt(col)); t.r(3, 14, 10, 15, dk(col, 0.6)); t.p(4, 8, dk(col, 0.5)).p(8, 8, dk(col, 0.5))
    for k in range(barrels): t.r(11, 8 + 3 * k, 15, 9 + 3 * k, dk(col, 0.7))
    return t.outline()
def tube_pic(col): return P().r(1, 6, 14, 10, col).r(1, 6, 14, 6, lt(col)).r(13, 5, 15, 11, dk(col)).r(3, 11, 5, 14, dk(col, 0.6)).r(10, 11, 12, 14, dk(col, 0.6)).outline()
def funnel_pic(col, band): return P().r(4, 0, 11, 15, col).r(4, 0, 4, 15, lt(col)).r(11, 0, 11, 15, dk(col, 0.6)).r(4, 3, 11, 5, band)
def boiler_pic(col, fire=c('f08030')):
    t = P().r(1, 3, 14, 14, col); t.r(1, 3, 14, 3, lt(col)); t.r(1, 14, 14, 15, dk(col, 0.6)); t.r(5, 9, 10, 13, INK); t.r(6, 10, 9, 12, fire); t.p(7, 10, c('ffe070'))
    for x in (2, 6, 10, 13): t.p(x, 5, dk(col, 0.5))
    t.r(6, 0, 8, 2, dk(col)); return t.outline()
def engine_pic(col, trim, pistons=2):
    t = P().r(1, 8, 14, 14, col); t.r(1, 8, 14, 8, lt(col)); t.r(1, 14, 14, 15, dk(col, 0.55))
    for k in range(pistons): x = 3 + k * (10 // max(1, pistons)); t.r(x, 2, x + 2, 7, trim); t.r(x, 2, x + 2, 2, lt(trim)); t.p(x + 1, 1, IRON)
    t.disc(12, 11, 2, dk(col, 0.6)); t.p(12, 11, trim); return t.outline()
def turbine_pic(col, trim): return P().r(1, 5, 14, 13, col).r(1, 5, 14, 5, lt(col)).disc(4.5, 9, 3, trim).disc(4.5, 9, 1.4, dk(trim)).r(9, 7, 13, 11, dk(col, 0.7)).r(2, 13, 4, 15, dk(col, 0.5)).r(11, 13, 13, 15, dk(col, 0.5)).outline()
def dynamo_pic(col): return P().disc(7.5, 8, 5.5, col).disc(7.5, 8, 3, COPPER).disc(7.5, 8, 1.2, INK).r(2, 13, 13, 15, dk(col, 0.6)).p(5, 5, WHITE).outline()
def pump_pic(col, wood=PLANK): return P().r(6, 4, 9, 14, col).r(6, 4, 6, 14, lt(col)).r(3, 14, 12, 15, wood).line(9, 5, 14, 2, wood).r(2, 9, 6, 10, col).p(1, 11, c('60a0e0')).outline()
def capstan_pic(col): return P().r(5, 5, 10, 13, col).r(4, 13, 11, 15, dk(col, 0.6)).r(4, 3, 11, 5, lt(col)).r(0, 4, 15, 4, dk(col)).r(5, 8, 10, 9, ROPE).outline()
def crane_pic(col, steam=False):
    t = P().r(2, 12, 7, 15, col); t.r(4, 4, 5, 12, col); t.line(5, 4, 14, 1, col); t.line(5, 5, 14, 2, dk(col)); t.r(14, 2, 14, 9, ROPE); t.r(13, 9, 15, 10, IRON)
    if steam: t.r(1, 8, 4, 11, dk(col, 0.7)); t.r(2, 5, 2, 7, INK)
    return t.outline()
def pot_pic(col):
    t = P().r(3, 6, 12, 13, col); t.r(4, 7, 11, 12, (0, 0, 0, 0))
    for x in (5, 7, 9): t.r(x, 6, x, 13, col)
    t.r(3, 9, 12, 9, col); t.r(7, 0, 8, 5, ROPE); t.r(6, 0, 9, 1, c('c84a3a')); return t.outline()
def winch_pic(col, wood=PLANK): return P().r(2, 10, 13, 15, wood).disc(7.5, 7.5, 4, col).disc(7.5, 7.5, 2.4, ROPE).p(7, 7, INK).line(11, 6, 14, 3, IRON).outline()
def bell_dome(col, port=c('60d0f0')): return P().disc(7.5, 8, 6, col).r(1, 9, 14, 15, col).r(1, 13, 14, 15, dk(col, 0.6)).disc(7.5, 8, 2.4, port).p(7, 7, WHITE).r(7, 0, 8, 2, IRON).outline()
def sphere_pic(col, port=c('f0e070')): return P().disc(7.5, 8.5, 6.4, col).disc(5, 6, 1, lt(col, 1.4)).disc(7.5, 9, 2.6, port).disc(7.5, 9, 1.2, c('60d0f0')).r(7, 0, 8, 2, IRON).outline()
def lamp_pic(col, glow): return P().r(6, 1, 9, 2, col).r(5, 3, 10, 4, col).r(5, 5, 10, 11, glow).r(7, 6, 8, 10, lt(glow, 1.3)).r(5, 5, 5, 11, col).r(10, 5, 10, 11, col).r(4, 12, 11, 13, col).outline()
def searchlight_pic(): return P().r(6, 10, 9, 15, IRON).disc(7.5, 6, 5, STEEL).disc(8.5, 6, 3.4, c('fff4b0')).p(9, 5, WHITE).outline()
def box_pic(col, band=None, mark=None):
    t = P().r(2, 4, 13, 14, col); t.r(2, 4, 13, 4, lt(col)); t.r(2, 14, 13, 14, dk(col, 0.6)); t.r(2, 9, 13, 9, dk(col, 0.75))
    if band: t.r(4, 4, 5, 14, band); t.r(10, 4, 11, 14, band)
    if mark: t.r(7, 6, 8, 8, mark)
    return t.outline()
def planter_pic(col, soil=SOIL): return P(col).r(0, 0, 15, 2, soil).r(0, 3, 15, 3, dk(col, 0.6)).r(0, 9, 15, 9, dk(col, 0.7)).r(7, 4, 7, 15, dk(col, 0.75)).p(3, 1, GRASS).p(11, 0, GRASS)
def hammock_pic(col): return P().line(0, 4, 4, 9, ROPE).line(15, 4, 11, 9, ROPE).r(3, 9, 12, 11, col).r(4, 12, 11, 12, dk(col)).r(3, 9, 12, 9, lt(col))
def bunk_pic(wood, cloth): return P().r(0, 8, 15, 10, wood).r(0, 4, 1, 15, dk(wood)).r(14, 4, 15, 15, dk(wood)).r(2, 6, 13, 8, cloth).r(2, 6, 5, 7, WHITE)
def table_pic(col): return P().r(1, 7, 14, 8, col).r(1, 7, 14, 7, lt(col)).r(2, 9, 3, 15, dk(col)).r(12, 9, 13, 15, dk(col))
def figure_pic(col, trim): return P().r(4, 9, 8, 15, col).disc(8, 5, 3, col).line(8, 8, 13, 12, col).r(9, 4, 9, 4, INK).r(5, 1, 10, 2, trim).line(4, 9, 1, 13, trim).outline()
def beam(col): return P().line(2, 12, 12, 2, col).line(3, 13, 13, 3, col).line(4, 13, 13, 4, dk(col, 0.7)).line(2, 11, 11, 2, lt(col)).outline()
def rib(col): return P().line(3, 2, 3, 9, col).line(3, 9, 7, 13, col).line(7, 13, 13, 13, col).line(4, 2, 4, 9, lt(col)).line(4, 9, 8, 12, lt(col)).outline()
def bolt(col): return P().line(4, 12, 11, 5, col).r(10, 3, 13, 6, col).p(11, 4, lt(col, 1.4)).p(3, 13, dk(col)).outline()
def coil(col): return P().disc(7.5, 8, 5.5, col).disc(7.5, 8, 2.5, (0, 0, 0, 0)).disc(7.5, 8, 4, dk(col, 0.8)).disc(7.5, 8, 2.5, (0, 0, 0, 0)).line(11, 11, 14, 14, col).outline()
def chain_pic(col):
    t = P()
    for k in range(4): t.disc(3 + k * 3.2, 4 + k * 2.6, 2, col); t.p(round(3 + k * 3.2), round(4 + k * 2.6), (0, 0, 0, 0))
    return t.outline()
def sheet(col): return P().r(2, 5, 13, 12, col).r(2, 5, 13, 5, lt(col)).r(2, 12, 13, 12, dk(col)).p(3, 6, dk(col, 0.5)).p(12, 6, dk(col, 0.5)).p(3, 11, dk(col, 0.5)).p(12, 11, dk(col, 0.5)).outline()
def keel_pic(col, trim, ribs=3):
    t = P().r(0, 11, 15, 13, col); t.r(0, 11, 15, 11, lt(col)); t.line(13, 11, 15, 6, col)
    for k in range(ribs): x = 2 + k * (10 // ribs); t.r(x, 5, x, 10, trim)
    return t.outline()
def module(col, trim): return P().r(2, 3, 13, 13, col).r(2, 3, 13, 3, lt(col)).r(2, 8, 13, 8, dk(col, 0.6)).r(7, 3, 8, 13, dk(col, 0.6)).p(4, 5, trim).p(11, 5, trim).p(4, 11, trim).p(11, 11, trim).outline()
def lump(col, shine=None):
    t = P().disc(7, 9, 4.5, col).disc(10, 7, 3, col).disc(6, 8, 1.5, lt(col))
    if shine: t.p(9, 6, shine).p(5, 10, shine).p(11, 9, shine)
    return t.outline()
def fishpic(col, belly, big=False):
    t = P().disc(7, 8, 4 if big else 3, col).r(3, 9, 10, 10 if big else 9, belly).line(11, 8, 14, 5, col).line(11, 8, 14, 11, col).p(5, 7, INK); return t.outline()
def shell_pic(col): return P().disc(7.5, 9, 5, col).r(2, 10, 13, 15, (0, 0, 0, 0)).r(4, 10, 11, 11, col).line(7, 4, 7, 10, dk(col)).line(4, 6, 7, 10, dk(col)).line(11, 6, 8, 10, dk(col)).outline()
def frond(col): return P().line(2, 13, 13, 3, dk(col)).line(4, 11, 3, 6, col).line(6, 9, 6, 4, col).line(8, 7, 9, 2, col).line(5, 12, 10, 12, col).line(8, 9, 13, 9, col).outline()
def crop_item(col, stem=c('3a8a4a')): return herb(stem, col, 2)

# ---- what the sea gives: adrift, on the bed, off what comes alongside -------------------------------------------
RAWS = [  # name, label, tier, picture, extra
    ('driftwood', 'Driftwood', 1, beam(DRIFT), {'fuel': 200}), ('kelp', 'Kelp', 1, frond(c('4a7a3a')), {'food': 0.06}), ('tangled_rope', 'Tangled rope', 1, coil(dk(ROPE)), {}),
    ('torn_canvas', 'Torn canvas', 1, trinket('cloth', dk(CLOTH, 0.85)), {}), ('old_cask', 'Old cask', 1, barrel(DRIFT).outline(), {'fuel': 300}), ('glass_bottle', 'Glass bottle', 1, bottle(c('a0d0c0')), {}),
    ('coconut', 'Coconut', 1, P().disc(7.5, 8, 5, c('7a5a3a')).p(6, 6, INK).p(9, 6, INK).p(7, 8, INK).outline(), {'food': 0.15, 'plants': 'palm'}), ('flotsam_crate', 'Flotsam crate', 1, box_pic(DRIFT, dk(DRIFT)), {'fuel': 200}),
    ('sea_shell', 'Sea shell', 1, shell_pic(c('f0d8c8')), {}), ('bamboo', 'Bamboo', 2, P().r(6, 0, 8, 15, c('a8c060')).r(6, 4, 8, 4, c('6a8a3a')).r(6, 10, 8, 10, c('6a8a3a')).line(9, 3, 12, 1, c('5a9a40')).outline(), {'fuel': 150}),
    ('palm_frond', 'Palm frond', 2, frond(c('5a9a40')), {}), ('sponge', 'Sponge', 2, lump(c('e0b850'), c('a07828')), {}), ('copper_scrap', 'Copper sheathing scrap', 2, sheet(VERDI), {}),
    ('lead_scrap', 'Lead scrap', 2, lump(c('6a6a7a')), {}), ('sand', 'Sand', 2, sack(SAND), {}), ('clay', 'Clay', 2, lump(CLAY), {}), ('island_soil_bag', 'Sack of soil', 2, sack(SOIL), {}),
    ('ballast_rock', 'Ballast rock', 2, lump(STONE), {}), ('coral_branch', 'Coral', 2, P().line(7, 14, 7, 6, CORAL).line(7, 9, 3, 4, CORAL).line(7, 8, 11, 3, CORAL).line(7, 6, 8, 2, CORAL).outline(), {}),
    ('guano', 'Guano', 2, lump(c('e8e8d8'), c('b0b0a0')), {}), ('iron_scrap', 'Iron scrap', 3, lump(c('8a5a48'), IRON), {}), ('cloth_bale', 'Bale of cloth', 3, sack(c('a04a4a')), {}),
    ('tar_barrel', 'Barrel of tar', 3, barrel(TARRED).outline(), {'fuel': 800}), ('lamp_oil', 'Lamp oil', 3, bottle(c('e0c040')), {'fuel': 600}), ('glass_float', 'Glass float', 3, orb(c('60c0a0')), {}),
    ('lost_spar', 'Lost spar', 3, beam(PLANK), {}), ('castaway', 'Castaway on a spar', 3, person(c('8a6a4a'), c('c84a3a'), 0, blade=False, hatkind=1), {'max_stack': 1}),
    ('salt_cask', 'Cask of salt pork', 3, barrel(c('c0a080'), mark=c('c07a6a')).outline(), {'food': 0.4}), ('great_timber', 'Great timber', 4, beam(c('7a5a34')), {}),
    ('whale_blubber', 'Whale blubber', 4, lump(c('f0e0d0'), c('e0a0a0')), {'fuel': 900}), ('ambergris', 'Ambergris', 4, lump(c('b0a890'), GOLD), {}), ('brass_scrap', 'Brass scrap', 4, lump(BRASSY, WHITE), {}),
    ('coal', 'Coal', 4, lump(COAL, c('5a5a68')), {'fuel': 1500}), ('sulphur', 'Sulphur', 4, lump(c('e0d040'), WHITE), {}), ('ironstone', 'Ironstone', 4, lump(c('8a4a3a'), c('c08060')), {}),
    ('obsidian_shard', 'Obsidian', 4, gemstone(c('3a2a4a')), {}), ('tin_nodule', 'Tin nodule', 3, lump(c('a8b0b8'), WHITE), {}), ('manganese_nodule', 'Manganese nodule', 4, lump(c('4a4458'), c('8a80a0')), {}),
    ('ice_chunk', 'Ice', 5, gemstone(ICE, True), {}), ('frozen_fish', 'Frozen fish', 5, fishpic(c('a0c0d0'), WHITE), {'food': 0.1}), ('whale_bone', 'Whale bone', 5, fang(BONE), {}),
    ('ration_tin', 'Old ration tin', 5, P().r(4, 4, 11, 13, c('a0a8b0')).r(4, 7, 11, 10, c('c84a3a')).r(4, 4, 11, 4, WHITE).outline(), {'food': 0.45, 'cooked': True}),
    ('drowned_plate', 'Drowned plate', 6, sheet(c('5a6a70')), {}), ('engine_wreckage', 'Engine wreckage', 6, module(c('5a5048'), VERDI), {}), ('ghost_oil', 'Ghost oil', 6, bottle(c('80f0d0'), c('2a3a4a')), {'fuel': 4000}),
    ('black_ore', 'Black ore', 6, lump(BLACK, c('8060d0')), {}), ('abyssal_pearl', 'Abyssal pearl', 6, orb(c('d0d0f0'), c('8060d0')), {}), ('seed_pod', 'Drifting seed pod', 2, P().disc(7.5, 8, 4, c('8a7a3a')).line(7, 4, 9, 1, GRASS).outline(), {}),
]
for n, lab, tier, pic, kw in RAWS: I(pic, n, lab, tier, 'nature' if tier < 3 or 'food' in kw else 'ores' if n in ('iron_scrap', 'copper_scrap', 'lead_scrap', 'brass_scrap', 'coal', 'sulphur', 'ironstone', 'tin_nodule', 'manganese_nodule', 'black_ore', 'drowned_plate') else 'crafted', 0.25, **kw); raw(n)

# Blocks of what comes alongside: dug for what they are made of.
def nat(name, label, tier, pic, drops, tool='pickaxe', time=0.6, **kw):
    pic.save('tiles', name); icon_of(name)
    tile(name, label, True, tool=tool, mine_time=time, shade=10, drop=None, drops=[clean({'item': i, 'count': n, 'chance': ch}) for i, n, ch in drops], **kw)
    item(name, label, tier, 'special', 0.2, places=name)
nat('sand_bank', 'Sand bank', 2, rock('sand_bank', SAND, 24), [('sand', 1, None), ('sea_shell', 1, 0.15)], 'shovel', 0.3)
nat('island_soil', 'Island soil', 2, rock('island_soil', SOIL, 30), [('island_soil_bag', 1, None), ('seed_pod', 1, 0.08)], 'shovel', 0.35, fertile=1.5)
gr = rock('island_grass', SOIL, 30); gr.r(0, 0, 15, 3, GRASS); gr.r(0, 0, 15, 0, lt(GRASS)); [gr.p(x, 4, GRASS) for x in (1, 4, 6, 9, 12, 14)]
nat('island_grass', 'Island turf', 2, gr, [('island_soil_bag', 1, None), ('seed_pod', 1, 0.2)], 'shovel', 0.35, fertile=1.5)
nat('clay_bank', 'Clay bank', 2, rock('clay_bank', CLAY, 20), [('clay', 2, None)], 'shovel', 0.4)
nat('shore_rock', 'Shore rock', 2, rock('shore_rock', STONE), [('ballast_rock', 1, None)])
nat('coral_reef', 'Coral reef', 2, rock('coral_reef', CORAL, 30, c('f0c0a0')), [('coral_branch', 1, None), ('sponge', 1, 0.2)], time=0.5)
nat('guano_rock', 'Guano rock', 2, rock('guano_rock', c('d8d8c8'), 30), [('guano', 2, None)], time=0.4)
nat('wreck_planking', 'Wreck planking', 2, planks('wreck_planking', dk(DRIFT, 0.8)).speck('wp', VERDI, 10), [('driftwood', 2, None), ('copper_scrap', 1, 0.5)], 'axe', 0.5)
nat('wreck_sheathing', 'Wreck sheathing', 2, sheathed('wreck_sheathing', VERDI), [('copper_scrap', 2, None), ('lead_scrap', 1, 0.3)], time=0.7)
nat('wreck_ironwork', 'Wreck ironwork', 3, plate('wreck_ironwork', c('7a5a4a')), [('iron_scrap', 2, None)], time=0.9)
nat('wreck_brasswork', 'Wreck brasswork', 4, plate('wreck_brasswork', dk(BRASSY, 0.8)), [('brass_scrap', 2, None)], time=1.0)
nat('basalt_stack', 'Basalt', 4, brick('basalt_stack', c('3a3a44'), c('2a2a30'), 2), [('ballast_rock', 2, None), ('obsidian_shard', 1, 0.1)], time=1.0)
nat('coal_seam', 'Coal seam', 4, rock('coal_seam', c('3a3a44'), 30, COAL), [('coal', 2, None)], time=0.9)
nat('ironstone_seam', 'Ironstone seam', 4, rock('ironstone_seam', c('3a3a44'), 30, c('a05a40')), [('ironstone', 2, None)], time=1.0)
nat('sulphur_crust', 'Sulphur crust', 4, rock('sulphur_crust', c('5a5040'), 20, c('e0d040')), [('sulphur', 2, None)], time=0.6, light=3)
nat('obsidian_flow', 'Obsidian', 4, rock('obsidian_flow', c('2a2030'), 10, c('6a4a8a')), [('obsidian_shard', 2, None)], time=1.4)
nat('blubber_bank', 'Whale flank', 4, rock('blubber_bank', c('e8d8d0'), 20, c('d09090')), [('whale_blubber', 1, None), ('ambergris', 1, 0.05)], 'machete', 0.5)
nat('whale_rib', 'Whale rib', 4, bark('whale_rib', BONE), [('whale_bone', 1, None)], 'axe', 0.7)
nat('ice', 'Berg ice', 5, rock('ice', ICE, 20), [('ice_chunk', 1, None)], time=0.5)
nat('pack_snow', 'Packed snow', 5, rock('pack_snow', c('f0f4f8'), 16), [('ice_chunk', 1, 0.5)], 'shovel', 0.25)
nat('frozen_planking', 'Frozen planking', 5, planks('frozen_planking', c('8a9aa8')), [('great_timber', 1, 0.5), ('driftwood', 1, None)], 'axe', 0.7)
nat('drowned_hull', 'Drowned hull', 6, plate('drowned_hull', c('4a5a60')).speck('dh', VERDI, 20), [('drowned_plate', 1, None), ('engine_wreckage', 1, 0.08)], time=1.6)
nat('black_reef', 'Black reef', 6, rock('black_reef', BLACK, 30, c('8060d0')), [('black_ore', 1, 0.35), ('ballast_rock', 1, None)], time=1.8, light=2)
nat('leviathan_bone', 'Leviathan bone', 6, bark('leviathan_bone', c('c8c0d8')), [('whale_bone', 2, None), ('abyssal_pearl', 1, 0.04)], 'axe', 1.5)
rock('sea_floor', c('3a4450'), 20).save('tiles', 'sea_floor'); tile('sea_floor', 'The floor of the sea', True, removable=False, drop=None)

# What is found in a chest somebody else left.
def loot_chest(name, label, col, trim, loot):
    chest_pic(col, trim).save('tiles', name); icon_of(name)
    tile(name, label, False, storage={'columns': 4, 'rows': 3}, tool='axe', mine_time=0.5, loot=loot, ship={'weight': 1})
    item(name, label, 2, 'special', 1.0, places=name, container={'slots': 12})
def one(*pairs, lo=1, hi=2): return {'one_of': [{'item': n, 'weight': w} for n, w in pairs], 'min': lo, 'max': hi}

# ---- benches ------------------------------------------------------------------------------------------------------
def bench_pic(kind, trim=BRASSY):
    t = P(); w = PLANK
    def tbl(col=w): t.r(1, 9, 14, 10, col); t.r(1, 9, 14, 9, lt(col)); t.r(2, 11, 3, 15, dk(col)); t.r(12, 11, 13, 15, dk(col))
    if kind == 'shipwright': tbl(); t.line(3, 8, 12, 4, c('c8a070')); t.line(3, 7, 12, 3, dk(PLANK)); t.r(10, 6, 12, 8, IRON); t.r(1, 9, 14, 9, trim)
    elif kind == 'sawpit': tbl(DRIFT); t.r(7, 1, 8, 13, c('c0c8d0')); [t.p(9, y, c('c0c8d0')) for y in range(2, 13, 2)]; t.r(6, 0, 9, 0, trim); t.r(2, 7, 13, 8, PLANK)
    elif kind == 'ropewalk': tbl(DRIFT); t.disc(4, 6, 2.5, ROPE); t.disc(4, 6, 1, dk(ROPE)); t.line(6, 6, 14, 6, ROPE); t.line(6, 7, 14, 8, dk(ROPE)); t.r(13, 3, 14, 8, dk(w))
    elif kind == 'sail_loft': tbl(); t.r(2, 3, 12, 8, CLOTH); t.r(2, 3, 12, 3, lt(CLOTH)); t.line(4, 8, 9, 4, dk(CLOTH)); t.line(11, 2, 14, 6, IRON)
    elif kind == 'galley': t.r(2, 10, 13, 15, STONE); t.r(2, 10, 13, 10, lt(STONE)); t.r(5, 12, 10, 15, INK); t.r(6, 13, 9, 14, c('f08030')); t.r(4, 5, 11, 9, IRON); t.r(4, 5, 11, 5, lt(IRON)); t.p(7, 3, WHITE).p(9, 2, WHITE)
    elif kind == 'coopers_block': t.r(4, 11, 11, 15, dk(w)); t.r(3, 3, 12, 10, DRIFT); t.r(3, 5, 12, 5, IRON); t.r(3, 9, 12, 9, IRON); t.r(7, 3, 7, 10, dk(DRIFT))
    elif kind == 'carpenters_bench': tbl(); t.r(2, 6, 6, 8, IRON); t.line(8, 8, 13, 3, dk(w)); t.r(12, 2, 14, 4, IRON); t.r(1, 9, 14, 9, trim)
    elif kind == 'rain_catcher': t.line(1, 2, 7, 7, CLOTH); t.line(14, 2, 8, 7, CLOTH); t.r(1, 1, 1, 9, dk(w)); t.r(14, 1, 14, 9, dk(w)); t.r(4, 8, 11, 15, DRIFT); t.r(4, 10, 11, 10, IRON); t.r(5, 8, 10, 8, c('60a0e0'))
    elif kind == 'kiln': t.disc(7.5, 9, 6.5, c('a05a40')); t.r(1, 9, 14, 15, c('a05a40')); t.r(5, 10, 10, 15, INK); t.r(6, 12, 9, 14, c('f08030')); t.r(6, 0, 9, 3, dk(c('a05a40'))); t.speck('kiln', dk(c('a05a40'), 0.8), 14)
    elif kind == 'caulkers_kettle': t.disc(7.5, 8, 5.5, c('3a3a40')); t.r(2, 5, 13, 6, c('5a5a60')); t.r(3, 5, 12, 5, c('1a1410')); t.r(3, 13, 4, 15, IRON); t.r(11, 13, 12, 15, IRON); t.r(6, 14, 9, 15, c('f08030'))
    elif kind == 'forge': t.r(1, 8, 14, 15, STONE); t.r(1, 8, 14, 8, lt(STONE)); t.r(4, 10, 11, 14, INK); t.r(5, 11, 10, 13, c('f08030')); t.p(7, 11, c('ffe070')); t.r(10, 2, 13, 7, dk(STONE)); t.r(2, 5, 7, 7, IRON); t.r(1, 8, 14, 8, trim)
    elif kind == 'still': t.disc(5, 9, 4, COPPER); t.r(4, 3, 6, 6, COPPER); t.line(6, 3, 12, 5, COPPER); t.r(11, 5, 13, 13, dk(COPPER)); t.r(3, 13, 7, 15, STONE); t.p(4, 8, WHITE)
    elif kind == 'chart_table': tbl(); t.r(2, 5, 13, 8, PAPER); t.line(3, 7, 7, 6, c('2a6a9a')); t.line(7, 6, 12, 7, c('2a6a9a')); t.p(10, 6, RED); t.line(11, 2, 13, 5, trim)
    elif kind == 'gunfoundry': t.r(1, 10, 14, 15, c('7a4a3a')); t.r(1, 10, 14, 10, lt(c('7a4a3a'))); t.r(3, 5, 12, 8, c('4a4a52')); t.r(12, 4, 14, 9, c('4a4a52')); t.r(5, 12, 10, 15, INK); t.r(6, 13, 9, 14, c('ffb040'))
    elif kind == 'powder_mill': t.disc(5, 9, 4, STONE); t.disc(5, 9, 1.4, dk(STONE)); t.disc(11, 11, 3, STONE); t.r(2, 13, 14, 15, dk(w)); t.r(7, 3, 8, 12, dk(w)); t.r(6, 2, 9, 3, trim)
    elif kind == 'plate_mill': t.r(1, 11, 14, 15, IRON); t.r(2, 3, 4, 11, STEEL); t.r(11, 3, 13, 11, STEEL); t.r(4, 5, 11, 6, c('c0c8d0')); t.r(4, 8, 11, 9, c('c0c8d0')); t.r(0, 7, 15, 7, c('f08030')); t.r(2, 2, 13, 2, trim)
    elif kind == 'engine_shop': tbl(IRON); t.disc(5, 6, 3, STEEL); t.disc(5, 6, 1, INK); t.r(9, 3, 11, 8, COPPER); t.r(12, 5, 13, 8, STEEL); t.r(1, 9, 14, 9, trim)
    elif kind == 'electric_bench': tbl(IRON); t.disc(4.5, 6, 2.5, COPPER); t.r(8, 4, 12, 8, c('2a3a2a')); t.p(9, 5, c('60f080')).p(11, 6, c('f0e060')); t.line(6, 6, 8, 6, RED)
    elif kind == 'armoury': tbl(IRON); t.r(2, 5, 12, 7, STEEL); t.r(12, 4, 14, 8, dk(STEEL)); t.r(4, 2, 5, 4, BRASSY); t.r(7, 2, 8, 4, BRASSY); t.r(10, 2, 11, 4, BRASSY)
    return t.outline()
def bench(kind, label, tier, level=1, fuel=False, **kw):
    name = kind if level == 1 else '%s_%d' % (kind, level)
    tr = [None, BRASSY, c('d0d8e0'), c('60d0f0'), c('f05030'), c('c080f0')][min(level, 5)]
    bench_pic(kind, tr).save('tiles', name); icon_of(name)
    tile(name, label, False, bench=True, bench_kind=kind, bench_level=level, popup_name='bench', tool='axe', mine_time=0.4, fuel_capacity=3000 if fuel else None, light=8 if fuel else None, light_needs_fuel=True if fuel else None, ship={'weight': 2}, **kw)
    item(name, label, tier, 'benches', 1.0, places=name)
BENCHES = [  # kind, label, tier, the shipwright's level it is made at, burns fuel, what it is made of
    ('sawpit', 'Sawpit', 1, 1, False, [('driftwood', 4)]), ('ropewalk', 'Ropewalk', 1, 1, False, [('driftwood', 3), ('tangled_rope', 2)]), ('galley', 'Galley fire', 1, 1, True, [('driftwood', 4), ('sea_shell', 2)]),
    ('sail_loft', 'Sail loft', 1, 1, False, [('driftwood', 4), ('torn_canvas', 2)]), ('coopers_block', "Cooper's block", 1, 1, False, [('driftwood', 3), ('old_cask', 2)]),
    ('carpenters_bench', "Carpenter's bench", 1, 1, False, [('plank', 4), ('nails', 2)]), ('rain_catcher', 'Rain catcher', 1, 1, False, [('torn_canvas', 2), ('old_cask', 1)]),
    ('kiln', 'Kiln', 2, 2, True, [('clay', 8), ('ballast_rock', 4)]), ('forge', 'Forge', 2, 2, True, [('fired_brick', 6), ('ballast_rock', 4), ('plank', 2)]),
    ('caulkers_kettle', "Caulker's kettle", 2, 2, True, [('copper_ingot', 3), ('ballast_rock', 4)]), ('still', 'Still', 2, 2, True, [('copper_sheet', 2), ('fired_brick', 4)]),
    ('chart_table', 'Chart table', 2, 2, False, [('plank', 6), ('glass_bottle', 2), ('patched_canvas', 1)]), ('gunfoundry', 'Gun foundry', 3, 3, True, [('fired_brick', 12), ('iron_bar', 4)]),
    ('powder_mill', 'Powder mill', 3, 3, False, [('ballast_rock', 6), ('oak_beam', 4), ('copper_sheet', 2)]), ('plate_mill', 'Plate mill', 4, 4, True, [('iron_bar', 12), ('fired_brick', 16), ('cog_wheel', 4)]),
    ('engine_shop', 'Engine shop', 4, 4, False, [('iron_bar', 8), ('cog_wheel', 6), ('brass_ingot', 4)]), ('electric_bench', 'Electrical bench', 5, 5, False, [('steel_plate', 4), ('copper_wire', 8), ('glass_pane', 2)]),
    ('armoury', 'Armoury', 5, 5, False, [('steel_plate', 8), ('brass_ingot', 6), ('cog_wheel', 4)]),
]
KEELS = [None, None, ('Sailboat', 20, 10, 2), ('Caravel', 40, 18, 5), ('Galleon', 72, 30, 9), ('Dreadnought', 128, 40, 16)]
KEEL_COST = {2: [('keel_timber', 2), ('patched_canvas', 2), ('rope', 4)], 3: [('caravel_keel', 1), ('copper_sheet', 8), ('pitch', 6)], 4: [('galleon_keel', 1), ('iron_strap', 12), ('heavy_canvas', 8)], 5: [('dreadnought_keel', 1), ('steel_plate', 24), ('rivet', 40)]}
for lv in range(1, 6):
    up = {'to': 'shipwright_%d' % (lv + 1), 'cost': [{'item': i, 'count': n} for i, n in KEEL_COST[lv + 1]]} if lv < 5 else None
    keel = {'level': lv - 1, 'long': KEELS[lv][1], 'above': KEELS[lv][2], 'below': KEELS[lv][3]} if lv > 1 else None
    bench('shipwright', "Shipwright's bench" if lv == 1 else "Shipwright's bench: %s" % KEELS[lv][0].lower(), lv, lv, upgrade=up, keel=keel)
for kind, label, tier, lv, fuel, ins in BENCHES:
    bench(kind, label, tier, fuel=fuel, upgrade={'to': 'forge_2', 'cost': [{'item': 'fired_brick', 'count': 8}, {'item': 'iron_scrap', 'count': 6}]} if kind == 'forge' else None)
    R('shipwright', kind, ins, level=lv, time=160)
bench('forge', 'Bloomery forge', 3, 2, True, upgrade={'to': 'forge_3', 'cost': [{'item': 'fired_brick', 'count': 16}, {'item': 'coal', 'count': 20}, {'item': 'bellows', 'count': 2}]})
bench('forge', 'Steel converter', 4, 3, True)

# ---- the stuff things are made of: each a making or two ------------------------------------------------------------------
MATS = [  # name, label, tier, picture, bench, level, inputs, count, extra
    ('plank', 'Plank', 1, beam(PLANK), 'sawpit', 1, [('driftwood', 2)], 3, {'fuel': 100}), ('kelp_twine', 'Kelp twine', 1, trinket('thread', c('6a8a4a')), 'ropewalk', 1, [('kelp', 3)], 2, {}),
    ('rope', 'Rope', 1, coil(ROPE), 'ropewalk', 1, [('tangled_rope', 2)], 1, {}), ('nails', 'Nails', 1, P().line(4, 4, 10, 12, IRON).line(8, 3, 13, 10, IRON).p(3, 3, lt(IRON)).p(7, 2, lt(IRON)).outline(), 'galley', 1, [('old_cask', 1)], 3, {}),
    ('cask_stave', 'Cask stave', 1, beam(DRIFT), 'coopers_block', 1, [('old_cask', 1)], 4, {}), ('patched_canvas', 'Patched canvas', 1, trinket('cloth', CLOTH), 'sail_loft', 1, [('torn_canvas', 2), ('kelp_twine', 1)], 1, {}),
    ('trenail', 'Trenail', 1, bolt(PLANK), 'carpenters_bench', 1, [('plank', 1)], 4, {}), ('shell_lime', 'Shell lime', 1, sack(WHITE), 'galley', 1, [('sea_shell', 3)], 1, {}),
    ('fresh_water', 'Fresh water', 1, bottle(c('60a0e0'), c('a0c8d8')), 'rain_catcher', 1, [], 1, {'food': 0.02}), ('charcoal', 'Charcoal', 2, lump(c('3a3430')), 'kiln', 1, [('plank', 2)], 2, {'fuel': 800}),
    ('fired_brick', 'Fired brick', 2, P().r(3, 6, 12, 11, c('b05a40')).r(3, 6, 12, 6, lt(c('b05a40'))).r(3, 11, 12, 11, dk(c('b05a40'))).outline(), 'kiln', 1, [('clay', 2)], 2, {}),
    ('glass_pane', 'Glass pane', 2, sheet(c('b0e0e8')), 'kiln', 1, [('sand', 2), ('shell_lime', 1)], 2, {}), ('copper_ingot', 'Copper ingot', 2, ingot(COPPER), 'forge', 1, [('copper_scrap', 2)], 1, {}),
    ('lead_ingot', 'Lead ingot', 2, ingot(c('6a6a7a')), 'forge', 1, [('lead_scrap', 2)], 1, {}), ('copper_sheet', 'Copper sheet', 2, sheet(COPPER), 'forge', 1, [('copper_ingot', 1)], 2, {}),
    ('copper_rod', 'Copper rod', 2, bolt(dk(COPPER, 0.9)), 'forge', 1, [('copper_ingot', 1)], 2, {}), ('copper_bolt', 'Copper bolt', 2, bolt(COPPER), 'forge', 1, [('copper_rod', 1)], 3, {}),
    ('oak_beam', 'Oak beam', 3, beam(c('8a6a3c')), 'sawpit', 1, [('oak_log', 2)], 2, {}), ('seasoned_beam', 'Seasoned beam', 3, beam(c('a07a48')), 'kiln', 1, [('oak_beam', 2)], 2, {}),
    ('pine_resin', 'Pine resin', 3, lump(c('d0a040'), WHITE), 'caulkers_kettle', 1, [('pine_log', 2)], 2, {}), ('tar', 'Tar', 3, barrel(c('2a2018')).outline(), 'caulkers_kettle', 1, [('pine_resin', 2)], 1, {'fuel': 600}),
    ('pitch', 'Pitch', 3, lump(c('1a1410'), c('5a4a3a')), 'caulkers_kettle', 1, [('tar', 1), ('charcoal', 1)], 2, {}), ('oakum', 'Oakum', 3, trinket('thread', dk(ROPE)), 'ropewalk', 1, [('rope', 1), ('tar', 1)], 3, {}),
    ('rib_timber', 'Rib', 3, rib(c('a07a48')), 'carpenters_bench', 1, [('seasoned_beam', 1), ('trenail', 4)], 1, {}), ('compass_timber', 'Compass timber', 3, rib(c('8a6a3c')), 'sawpit', 1, [('oak_beam', 2)], 1, {}),
    ('flax_fibre', 'Flax fibre', 2, trinket('thread', c('d8c890')), 'ropewalk', 1, [('flax', 2)], 2, {}), ('linen_thread', 'Linen thread', 2, trinket('thread', CLOTH), 'sail_loft', 1, [('flax_fibre', 2)], 2, {}),
    ('canvas', 'Canvas', 2, trinket('cloth', c('f0e8d0')), 'sail_loft', 1, [('linen_thread', 3)], 1, {}), ('hemp_fibre', 'Hemp fibre', 3, trinket('thread', c('a89a60')), 'ropewalk', 1, [('hemp', 2)], 2, {}),
    ('hawser', 'Hawser', 3, coil(c('a08040')), 'ropewalk', 1, [('hemp_fibre', 4), ('tar', 1)], 1, {}), ('heavy_canvas', 'Heavy canvas', 4, trinket('cloth', c('d8cca8')), 'sail_loft', 1, [('canvas', 2), ('hemp_fibre', 2), ('tar', 1)], 1, {}),
    ('iron_bloom', 'Iron bloom', 3, lump(c('7a4a3a'), c('f08030')), 'forge', 2, [('iron_scrap', 2), ('charcoal', 1)], 1, {}), ('wrought_iron', 'Wrought iron', 3, ingot(c('5a5a64')), 'forge', 2, [('iron_bloom', 2)], 1, {}),
    ('iron_bar', 'Iron bar', 3, ingot(IRON), 'forge', 2, [('wrought_iron', 1)], 2, {}), ('iron_strap', 'Iron strap', 3, sheet(IRON), 'forge', 2, [('iron_bar', 1)], 2, {}),
    ('iron_bolt', 'Iron bolt', 3, bolt(IRON), 'forge', 2, [('iron_bar', 1)], 4, {}), ('chain_link', 'Chain', 3, chain_pic(IRON), 'forge', 2, [('iron_bar', 2)], 1, {}),
    ('bellows', 'Bellows', 3, P().r(3, 5, 10, 12, c('8a5a3a')).line(10, 8, 14, 8, IRON).r(3, 5, 10, 5, PLANK).r(3, 12, 10, 12, PLANK).outline(), 'carpenters_bench', 1, [('plank', 2), ('seal_skin', 1), ('iron_bolt', 2)], 1, {}),
    ('tin_ingot', 'Tin ingot', 3, ingot(c('b8c0c8')), 'forge', 1, [('tin_nodule', 2)], 1, {}), ('brass_ingot', 'Brass ingot', 4, ingot(BRASSY), 'forge', 2, [('brass_scrap', 2)], 1, {}),
    ('bronze_ingot', 'Gun bronze', 4, ingot(c('b88a4a')), 'forge', 2, [('copper_ingot', 3), ('tin_ingot', 1)], 3, {}), ('cog_wheel', 'Cog wheel', 4, P().disc(7.5, 7.5, 5, BRASSY).disc(7.5, 7.5, 1.6, INK).r(7, 0, 8, 15, BRASSY).r(0, 7, 15, 8, BRASSY).disc(7.5, 7.5, 3.4, dk(BRASSY)).disc(7.5, 7.5, 1.4, INK).outline(), 'forge', 2, [('brass_ingot', 1)], 2, {}),
    ('saltpetre', 'Saltpetre', 3, sack(c('f0f0e8')), 'kiln', 1, [('guano', 3)], 1, {}), ('gunpowder', 'Gunpowder', 3, barrel(c('3a3a40'), mark=RED).outline(), 'powder_mill', 1, [('saltpetre', 2), ('sulphur', 1), ('charcoal', 1)], 2, {}),
    ('round_shot', 'Round shot', 3, P().disc(7.5, 8, 4.5, c('3a3a44')).p(6, 6, c('8a8a94')).outline(), 'gunfoundry', 1, [('iron_bar', 1)], 4, {}), ('lamp_wick', 'Lamp wick', 2, trinket('thread', WHITE), 'ropewalk', 1, [('kelp_twine', 2)], 2, {}),
    ('whale_oil', 'Whale oil', 4, bottle(c('f0e0a0')), 'caulkers_kettle', 1, [('whale_blubber', 2)], 2, {'fuel': 1200}), ('pig_iron', 'Pig iron', 4, ingot(c('4a4048')), 'forge', 2, [('ironstone', 2), ('coal', 1)], 1, {}),
    ('steel_bloom', 'Steel bloom', 5, lump(c('8a8a98'), c('ffb040')), 'forge', 3, [('pig_iron', 2), ('manganese_nodule', 1)], 1, {}), ('steel_ingot', 'Steel ingot', 5, ingot(STEEL), 'forge', 3, [('steel_bloom', 2), ('coal', 1)], 1, {}),
    ('steel_billet', 'Steel billet', 5, ingot(c('b0b8c4')), 'forge', 3, [('steel_ingot', 2)], 2, {}), ('steel_plate', 'Steel plate', 5, sheet(STEEL), 'plate_mill', 1, [('steel_billet', 1)], 2, {}),
    ('rivet', 'Rivet', 5, bolt(STEEL), 'plate_mill', 1, [('steel_billet', 1)], 8, {}), ('steel_tube', 'Steel tube', 5, P().r(2, 6, 13, 9, STEEL).r(2, 6, 13, 6, lt(STEEL)).disc(13, 7.5, 2, STEEL).p(13, 7, INK).outline(), 'plate_mill', 1, [('steel_plate', 1)], 2, {}),
    ('copper_wire', 'Copper wire', 5, coil(COPPER), 'forge', 1, [('copper_rod', 1)], 4, {}), ('steam_pipe', 'Steam pipe', 5, P().r(1, 6, 14, 9, COPPER).r(4, 5, 5, 10, BRASSY).r(10, 5, 11, 10, BRASSY).r(1, 6, 14, 6, lt(COPPER)).outline(), 'engine_shop', 1, [('copper_sheet', 2), ('brass_ingot', 1)], 2, {}),
    ('bearing', 'Bearing', 5, P().disc(7.5, 8, 5, STEEL).disc(7.5, 8, 3.2, BRASSY).disc(7.5, 8, 1.4, INK).outline(), 'engine_shop', 1, [('steel_ingot', 1), ('brass_ingot', 1), ('whale_oil', 1)], 2, {}),
    ('piston', 'Piston', 5, P().r(5, 2, 10, 8, STEEL).r(7, 8, 8, 14, IRON).r(5, 2, 10, 2, lt(STEEL)).r(5, 5, 10, 5, dk(STEEL)).outline(), 'engine_shop', 1, [('steel_tube', 1), ('bearing', 1)], 1, {}),
    ('black_alloy', 'Black alloy', 6, ingot(BLACK), 'forge', 3, [('black_ore', 2), ('steel_ingot', 1)], 1, {}), ('armour_plate', 'Armour plate', 6, sheet(c('3a3e4c')), 'plate_mill', 1, [('black_alloy', 1), ('steel_plate', 2)], 2, {}),
    ('lens', 'Lens', 4, P().disc(7.5, 8, 5, c('b0e0f0')).disc(6, 6, 1.5, WHITE).outline(), 'kiln', 1, [('glass_pane', 2), ('sand', 1)], 1, {}), ('light_bulb', 'Arc lamp', 5, orb(c('fff0a0')), 'electric_bench', 1, [('glass_pane', 1), ('copper_wire', 2), ('charcoal', 1)], 2, {}),
]
for n, lab, tier, pic, b, lv, ins, cnt, kw in MATS: I(pic, n, lab, tier, 'ores' if 'ingot' in n or n in ('iron_bar', 'wrought_iron', 'pig_iron', 'steel_plate', 'black_alloy') else 'crafted', 0.5, **kw); R(b, n, ins, cnt, level=lv, time=600 if n == 'fresh_water' else 80)
# Other ways to the same things.
for b, out, ins, cnt, lv in [('sawpit', 'plank', [('flotsam_crate', 1)], 2, 1), ('sawpit', 'plank', [('palm_log', 1)], 2, 1), ('sawpit', 'plank', [('pine_log', 1)], 3, 1), ('sawpit', 'plank', [('bamboo', 2)], 2, 1), ('sawpit', 'plank', [('lost_spar', 1)], 4, 1),
                             ('ropewalk', 'rope', [('kelp_twine', 3)], 1, 1), ('ropewalk', 'rope', [('hemp_fibre', 2)], 2, 1), ('ropewalk', 'kelp_twine', [('palm_frond', 2)], 2, 1), ('forge', 'nails', [('copper_ingot', 1)], 6, 1), ('forge', 'nails', [('iron_bar', 1)], 10, 2),
                             ('still', 'fresh_water', [('glass_bottle', 1)], 2, 1), ('galley', 'fresh_water', [('ice_chunk', 1)], 2, 1), ('sail_loft', 'canvas', [('cloth_bale', 1)], 3, 1), ('caulkers_kettle', 'tar', [('tar_barrel', 1)], 3, 1), ('kiln', 'charcoal', [('driftwood', 3)], 2, 1),
                             ('caulkers_kettle', 'whale_oil', [('lamp_oil', 2)], 1, 1), ('kiln', 'fired_brick', [('coral_branch', 2), ('clay', 1)], 2, 1)]: R(b, out, ins, cnt, level=lv, time=80)

# The long makings: each keel is the end of one.
def chain_of(names, bench_for, start, sides, tier0):
    prev = start
    for k, (n, lab, pic) in enumerate(names):
        I(pic, n, lab, min(6, tier0 + k // 3), 'crafted', 1.5 + k)
        b, lv = bench_for[k]
        R(b, n, [(prev, 2)] + sides[k], 1, level=lv, time=160); prev = n
I(keel_pic(PLANK, DRIFT, 2), 'keel_timber', 'Keel timber', 2, 'crafted', 2.0); R('carpenters_bench', 'keel_timber', [('plank', 6), ('trenail', 4), ('rope', 2)], time=200)
chain_of([('scarfed_keel', 'Scarfed keel', keel_pic(c('a07a48'), COPPER, 2)), ('caravel_frame', 'Caravel frame', keel_pic(c('a07a48'), c('8a6a3c'), 4)), ('caravel_keel', 'Caravel keel', keel_pic(c('8a6a3c'), COPPER, 5))],
         [('carpenters_bench', 1)] * 3, 'seasoned_beam', [[('copper_bolt', 4)], [('rib_timber', 4)], [('pitch', 4), ('copper_sheet', 4)]], 3)
chain_of([('hanging_knee', 'Hanging knee', rib(IRON)), ('futtock', 'Futtock', rib(c('7a5a34'))), ('frame_bay', 'Frame bay', module(c('8a6a3c'), IRON)), ('keelson', 'Keelson', keel_pic(c('7a5a34'), IRON, 3)),
          ('galleon_backbone', 'Galleon backbone', keel_pic(c('6a4a2c'), IRON, 5)), ('galleon_keel', 'Galleon keel', keel_pic(c('5a3a24'), BRASSY, 6))],
         [('forge', 2), ('carpenters_bench', 1), ('carpenters_bench', 1), ('carpenters_bench', 1), ('carpenters_bench', 1), ('carpenters_bench', 1)], 'iron_strap',
         [[('compass_timber', 2)], [('seasoned_beam', 2)], [('iron_bolt', 8)], [('pitch', 4), ('oakum', 4)], [('copper_sheet', 8), ('seasoned_beam', 4)], [('chain_link', 4), ('hawser', 2)]], 4)
chain_of([('riveted_plate', 'Riveted plate', module(STEEL, dk(STEEL, 0.5))), ('frame_section', 'Frame section', rib(STEEL)), ('watertight_bulkhead', 'Watertight bulkhead', module(c('8a94a0'), BRASSY)), ('double_bottom', 'Double bottom', keel_pic(STEEL, IRON, 4)),
          ('keel_plate', 'Keel plate', keel_pic(c('8a94a0'), STEEL, 2)), ('hull_module', 'Hull module', module(c('7a8490'), RED)), ('engine_bed', 'Engine bed', module(IRON, BRASSY)), ('citadel', 'Citadel', module(c('5a6470'), STEEL)),
          ('dreadnought_frame', 'Dreadnought frame', keel_pic(c('6a7480'), STEEL, 6)), ('dreadnought_keel', 'Dreadnought keel', keel_pic(c('4a5460'), RED, 6))],
         [('plate_mill', 1)] * 6 + [('engine_shop', 1)] + [('plate_mill', 1)] * 3, 'steel_plate',
         [[('rivet', 8)], [('steel_tube', 1)], [('rivet', 8)], [('pitch', 2)], [('rivet', 12)], [('steel_tube', 2)], [('bearing', 2), ('steam_pipe', 2)], [('rivet', 16)], [('cog_wheel', 4)], [('bronze_ingot', 4), ('chain_link', 6)]], 5)

# ---- the hull: seven stuffs, each drawn its own way ---------------------------------------------------------------
logs('first_log', dk(DRIFT, 0.9)).save('tiles', 'first_log'); tile('first_log', 'The first log', True, removable=False, drop=None, ship={'burden': 6})
HULLS = [  # name, label, tier, strength, burden, picture, tool, bench, level, inputs
    ('lashed_driftwood', 'Lashed driftwood', 1, 1, 3, logs('lashed_driftwood', DRIFT), 'axe', 'sawpit', 1, [('driftwood', 3)]),
    ('plank_hull', 'Plank hull', 2, 2, 4, planks('plank_hull', PLANK), 'axe', 'sawpit', 1, [('plank', 3), ('nails', 1)]),
    ('tarred_hull', 'Tarred hull', 3, 3, 5, tarred('tarred_hull'), 'axe', 'caulkers_kettle', 1, [('plank_hull', 2), ('pitch', 1), ('oakum', 1)]),
    ('sheathed_hull', 'Copper-sheathed hull', 3, 4, 6, sheathed('sheathed_hull'), 'pickaxe', 'forge', 1, [('tarred_hull', 2), ('copper_sheet', 1), ('copper_bolt', 2)]),
    ('ironbound_hull', 'Ironbound hull', 4, 5, 8, ironbound('ironbound_hull'), 'pickaxe', 'forge', 2, [('seasoned_beam', 2), ('iron_strap', 1), ('pitch', 1)]),
    ('steel_hull', 'Steel plate hull', 5, 6, 12, plate('steel_hull', STEEL), 'pickaxe', 'plate_mill', 1, [('steel_plate', 2), ('rivet', 4)]),
    ('armoured_hull', 'Armoured hull', 6, 7, 14, plate('armoured_hull', c('4a505e'), True, True), 'pickaxe', 'plate_mill', 1, [('steel_hull', 2), ('armour_plate', 1)]),
]
for n, lab, tier, strength, burden, pic, tool, b, lv, ins in HULLS:
    T(pic, n, lab, tier, tool=tool, mine_time=0.3 + 0.15 * strength, shade=10, ship={'burden': burden, 'strength': strength}, burns=True if strength < 4 else None); R(b, n, ins, 2, level=lv)
DECKS = [('driftwood_deck', 'Driftwood deck', 1, DRIFT, 'axe', 'sawpit', 1, [('driftwood', 2)]), ('plank_deck', 'Plank deck', 2, PLANK, 'axe', 'sawpit', 1, [('plank', 2)]), ('oak_deck', 'Holystoned oak deck', 3, c('d8c8a0'), 'axe', 'sawpit', 1, [('oak_beam', 1)]),
         ('teak_deck', 'Great-timber deck', 4, c('9a6a3a'), 'axe', 'sawpit', 1, [('great_timber', 1)]), ('steel_deck', 'Steel deck', 5, c('8a94a0'), 'pickaxe', 'plate_mill', 1, [('steel_plate', 1)])]
for n, lab, tier, col, tool, b, lv, ins in DECKS:
    T(plate(n, col, True) if 'steel' in n else deck(n, col), n, lab, tier, tool=tool, mine_time=0.4, shade=6); R(b, n, ins, 2 if tier < 3 else 4, level=lv)

def ladder_pic(col, rope=False):
    t = P(); side = ROPE if rope else col
    t.r(4, 0, 5, 15, side); t.r(10, 0, 11, 15, side)
    for y in (2, 6, 10, 14): t.r(4, y, 11, y, col); t.r(4, y, 11, y, lt(col) if not rope else col)
    return t
def hatch_pic(col, iron=False):
    t = P().r(0, 5, 15, 9, col); t.r(0, 5, 15, 5, lt(col)); t.r(0, 9, 15, 9, dk(col, 0.6))
    for x in (3, 7, 11): t.r(x, 6, x, 8, dk(col, 0.7))
    t.r(13, 6, 14, 8, IRON if not iron else BRASSY); return t
def door_pic(col, top, trim):
    t = P(col); t.r(0, 0, 0, 15, dk(col, 0.6)); t.r(15, 0, 15, 15, dk(col, 0.6)); t.r(5, 0, 5, 15, dk(col, 0.8)); t.r(10, 0, 10, 15, dk(col, 0.8))
    if top: t.r(0, 0, 15, 0, dk(col, 0.6)); t.disc(7.5, 8, 2.6, c('a0d8e8')); t.p(7, 7, WHITE)
    else: t.r(12, 3, 13, 4, trim)
    return t
def rail_pic(col, knob=None):
    t = P().r(0, 6, 15, 7, col); t.r(0, 6, 15, 6, lt(col))
    for x in (2, 7, 12): t.r(x, 8, x + 1, 15, dk(col, 0.8))
    if knob: [t.p(x, 5, knob) for x in (2, 7, 12)]
    return t
def port_pic(col): return P().disc(7.5, 7.5, 5.5, col).disc(7.5, 7.5, 3.6, c('a0d8e8')).p(6, 6, WHITE).p(2, 7, dk(col)).p(13, 7, dk(col)).p(7, 2, dk(col)).p(7, 13, dk(col))
def grating_pic(col):
    t = P().r(0, 0, 15, 15, col)
    for a in (2, 6, 10, 14):
        for b in (2, 6, 10, 14): t.r(a - 1, b - 1, a, b, (0, 0, 0, 0))
    return t
PARTS = [  # name, label, tier, picture, solid, tile fields, bench, level, inputs, count
    ('rope_ladder', 'Rope ladder', 1, ladder_pic(DRIFT, True), False, {'climbable': True, 'attaches': True}, 'ropewalk', 1, [('rope', 2), ('driftwood', 1)], 3),
    ('ships_ladder', "Ship's ladder", 2, ladder_pic(PLANK), False, {'climbable': True, 'attaches': True}, 'carpenters_bench', 1, [('plank', 2), ('trenail', 2)], 3),
    ('iron_ladder', 'Iron ladder', 5, ladder_pic(STEEL), False, {'climbable': True, 'attaches': True, 'tool': 'pickaxe'}, 'plate_mill', 1, [('steel_tube', 1)], 3),
    ('deck_hatch', 'Deck hatch', 2, hatch_pic(PLANK), True, {'wall': False, 'passable': True, 'climbable': True, 'mimics': 'flat'}, 'carpenters_bench', 1, [('plank', 3), ('nails', 2)], 1),
    ('steel_hatch', 'Steel hatch', 5, hatch_pic(STEEL, True), True, {'wall': False, 'passable': True, 'climbable': True, 'mimics': 'flat', 'tool': 'pickaxe'}, 'plate_mill', 1, [('steel_plate', 1), ('rivet', 2)], 1),
    ('deck_grating', 'Deck grating', 3, grating_pic(c('8a6a3c')), True, {'wall': False, 'see_through': True}, 'carpenters_bench', 1, [('oak_beam', 1), ('trenail', 4)], 2),
    ('driftwood_rail', 'Driftwood rail', 1, rail_pic(DRIFT), False, {}, 'carpenters_bench', 1, [('driftwood', 1), ('trenail', 2)], 2),
    ('ships_rail', "Ship's rail", 3, rail_pic(c('8a6a3c'), BRASSY), False, {}, 'carpenters_bench', 1, [('oak_beam', 1), ('copper_bolt', 1)], 3),
    ('steel_rail', 'Guard rail', 5, rail_pic(STEEL), False, {'tool': 'pickaxe'}, 'plate_mill', 1, [('steel_tube', 1)], 4),
    ('porthole', 'Porthole', 3, port_pic(COPPER), False, {'window': {}, 'model': 'panel'}, 'forge', 1, [('copper_sheet', 1), ('glass_pane', 1)], 2),
    ('brass_scuttle', 'Brass scuttle', 5, port_pic(BRASSY), False, {'window': {}, 'model': 'panel', 'tool': 'pickaxe'}, 'engine_shop', 1, [('brass_ingot', 1), ('glass_pane', 1)], 2),
    ('ballast_block', 'Ballast', 2, brick('ballast_block', STONE, None, 2), True, {'tool': 'pickaxe'}, 'carpenters_bench', 1, [('ballast_rock', 4)], 2),
    ('brick_hearth', 'Brick hearth', 2, brick('brick_hearth', c('a85a40')), True, {'tool': 'pickaxe'}, 'kiln', 1, [('fired_brick', 4), ('shell_lime', 1)], 2),
    ('glass_block', 'Skylight glass', 3, P(c('b0e0e8')).r(0, 0, 15, 0, WHITE).r(0, 0, 0, 15, WHITE).line(3, 12, 12, 3, WHITE).r(0, 15, 15, 15, c('80b0c0')), True, {'tool': 'pickaxe', 'see_through': True, 'wall': False}, 'kiln', 1, [('glass_pane', 2), ('lead_ingot', 1)], 2),
]
for n, lab, tier, pic, solid, kw, b, lv, ins, cnt in PARTS: T(pic, n, lab, tier, solid, mine_time=0.3, **kw); R(b, n, ins, cnt, level=lv)
for n, lab, tier, col, trim, tool, b, ins in [('cabin_door', 'Cabin door', 2, PLANK, IRON, 'axe', 'carpenters_bench', [('plank', 4), ('nails', 2)]), ('great_cabin_door', 'Great cabin door', 4, c('7a4a2a'), BRASSY, 'axe', 'carpenters_bench', [('great_timber', 2), ('brass_ingot', 1), ('glass_pane', 1)]),
                                      ('bulkhead_door', 'Bulkhead door', 5, STEEL, RED, 'pickaxe', 'plate_mill', [('steel_plate', 2), ('rivet', 4), ('cog_wheel', 1)])]:
    door_pic(col, False, trim).save('tiles', n); door_pic(col, True, trim).save('tiles', n + '_top'); icon_of(n)
    tile(n, lab, True, tool=tool, mine_time=0.5, wall=False, passable=True, mimics='upright', height=2, texture_top='tiles/%s_top.png' % n); item(n, lab, tier, 'blocks', 0.5, places=n); R(b, n, ins)

# ---- what makes her go, hold, reach, see and fight: every one of these adds to what the ship is counted as -------------
def fit(pic, name, label, tier, ship, bench_, lv, ins, count=1, solid=False, cat='crafted', mult=1.0, iover=None, **kw):
    kw.setdefault('mine_time', 0.4); T(pic, name, label, tier, solid, cat, mult, iover, ship=ship, **kw); R(bench_, name, ins, count, level=lv, time=140)
for n, lab, tier, col, band, b, lv, ins in [('pole_mast', 'Pole mast', 1, DRIFT, None, 'carpenters_bench', 1, [('driftwood', 3), ('rope', 1)]), ('made_mast', 'Made mast', 3, c('a07a48'), IRON, 'carpenters_bench', 1, [('seasoned_beam', 2), ('iron_strap', 1), ('hawser', 1)]),
                                       ('steel_mast', 'Signal mast', 5, STEEL, RED, 'plate_mill', 1, [('steel_tube', 2), ('rivet', 2)])]:
    fit(mast_pic(col, band), n, lab, tier, {'weight': 1}, b, lv, ins, 2, climbable=True, attaches=True, supports=True)
fit(yard_pic(DRIFT), 'driftwood_yard', 'Driftwood yard', 1, {'weight': 1}, 'carpenters_bench', 1, [('driftwood', 2), ('rope', 1)], 2, attaches=True)
fit(yard_pic(c('a07a48')), 'ships_yard', 'Yard', 3, {'weight': 1}, 'carpenters_bench', 1, [('lost_spar', 1), ('hawser', 1)], 2, attaches=True)
fit(P().line(0, 15, 15, 0, ROPE).line(0, 9, 9, 0, dk(ROPE)).line(6, 15, 15, 6, dk(ROPE)), 'standing_rigging', 'Standing rigging', 2, {}, 'ropewalk', 1, [('rope', 2)], 4, attaches=True, climbable=True)
SAILS = [('rag_sail', 'Rag sail', 1, 3, dk(CLOTH, 0.82), False, 1, [('patched_canvas', 2), ('rope', 1)]), ('canvas_sail', 'Canvas sail', 2, 4, CLOTH, False, 1, [('canvas', 2), ('rope', 1)]), ('lateen_sail', 'Lateen sail', 3, 6, c('f0e8d0'), True, 1, [('canvas', 3), ('hawser', 1)]),
         ('course_sail', 'Course', 4, 9, c('d8cca8'), False, 1, [('heavy_canvas', 2), ('hawser', 1)]), ('storm_sail', 'Storm sail', 4, 7, c('a08a6a'), False, 1, [('heavy_canvas', 2), ('tar', 1), ('chain_link', 1)]), ('silk_sail', 'Silk royal', 4, 12, c('f0d0d0'), True, 1, [('cloth_bale', 3), ('hawser', 1)])]
for n, lab, tier, power, col, lat, lv, ins in SAILS:
    sail_pic(col, False, lat).save('tiles', n); sail_pic(col, True).save('tiles', n + '_furled'); icon_of(n)
    tile(n, lab, False, tool='machete', mine_time=0.2, attaches=True, model='panel', use_becomes=n + '_furled', ship={'sail': power, 'weight': 1}); tile(n + '_furled', lab + ', furled', False, tool='machete', mine_time=0.2, attaches=True, model='panel', use_becomes=n, drop=n, ship={'weight': 1})
    item(n, lab, tier, 'crafted', 1.0, places=n); R('sail_loft', n, ins, level=lv, time=140)
FITS = [  # picture, name, label, tier, what it adds, bench, level, inputs, count, tile fields
    (tiller_pic(DRIFT), 'steering_oar', 'Steering oar', 1, {'weight': 1}, 'carpenters_bench', 1, [('driftwood', 3), ('rope', 1)], 1, {'helm': True, 'bench': True, 'bench_kind': 'helm', 'bench_level': 1}),
    (tiller_pic(c('8a6a3c')), 'tiller', 'Tiller', 2, {'weight': 1}, 'carpenters_bench', 1, [('plank', 4), ('copper_bolt', 2)], 1, {'helm': True, 'bench': True, 'bench_kind': 'helm', 'bench_level': 1}),
    (wheel_pic(c('8a5a30'), BRASSY), 'ships_wheel', "Ship's wheel", 3, {'weight': 2}, 'carpenters_bench', 1, [('seasoned_beam', 2), ('brass_ingot', 1), ('hawser', 1)], 1, {'helm': True, 'bench': True, 'bench_kind': 'helm', 'bench_level': 1}),
    (wheel_pic(STEEL, RED), 'bridge_wheel', 'Bridge wheel and telegraph', 5, {'weight': 3}, 'engine_shop', 1, [('steel_tube', 2), ('brass_ingot', 2), ('cog_wheel', 2)], 1, {'helm': True, 'bench': True, 'bench_kind': 'helm', 'bench_level': 1, 'tool': 'pickaxe'}),
    (stone_on_rope(), 'killick', 'Killick', 2, {'weight': 2, 'cable': 1}, 'ropewalk', 1, [('ballast_rock', 2), ('rope', 3)], 1, {}),
    (anchor_pic(IRON, PLANK), 'grapnel_anchor', 'Grapnel', 3, {'weight': 3, 'cable': 2}, 'forge', 2, [('iron_bar', 3), ('hawser', 2)], 1, {}),
    (anchor_pic(c('4a4a52'), c('8a5a30')), 'bower_anchor', 'Bower anchor', 4, {'weight': 6, 'cable': 3}, 'forge', 2, [('iron_bar', 8), ('chain_link', 4), ('hawser', 4)], 1, {}),
    (capstan_pic(STEEL), 'steam_windlass', 'Steam windlass and chain cable', 5, {'weight': 10, 'cable': 4}, 'engine_shop', 1, [('bower_anchor', 1), ('chain_link', 12), ('piston', 1), ('steel_plate', 2)], 1, {'tool': 'pickaxe'}),
    (capstan_pic(c('8a5a30')), 'capstan', 'Capstan', 3, {'weight': 3}, 'carpenters_bench', 1, [('seasoned_beam', 2), ('iron_strap', 2)], 1, {}),
    (crane_pic(c('8a6a3c')), 'derrick', 'Derrick', 4, {'weight': 5, 'reach': 1}, 'carpenters_bench', 1, [('great_timber', 2), ('hawser', 2), ('iron_strap', 2), ('cog_wheel', 1)], 1, {}),
    (crane_pic(STEEL, True), 'steam_crane', 'Steam crane', 5, {'weight': 12, 'reach': 2}, 'engine_shop', 1, [('derrick', 1), ('piston', 1), ('steel_tube', 3), ('chain_link', 6)], 1, {'tool': 'pickaxe'}),
    (pump_pic(COPPER), 'bilge_pump', 'Bilge pump', 3, {'weight': 2, 'pump': 2}, 'forge', 1, [('copper_sheet', 2), ('copper_rod', 2), ('plank', 2)], 1, {}),
    (pump_pic(STEEL, IRON), 'steam_pump', 'Steam pump', 5, {'weight': 6, 'pump': 8}, 'engine_shop', 1, [('bilge_pump', 1), ('piston', 1), ('steam_pipe', 2)], 1, {'tool': 'pickaxe'}),
    (searchlight_pic(), 'searchlight', 'Searchlight', 5, {'weight': 4, 'light': 1}, 'electric_bench', 1, [('light_bulb', 2), ('lens', 1), ('steel_plate', 1), ('copper_wire', 4)], 1, {'light': 15, 'tool': 'pickaxe'}),
    (P().r(1, 9, 14, 14, c('8a5a30')).disc(7.5, 6, 4, c('8a5a30')).disc(7.5, 6, 2.4, c('5a3a20')).r(11, 4, 14, 5, IRON).outline(), 'treadwheel', 'Treadwheel', 3, {'weight': 4, 'thrust': 3}, 'carpenters_bench', 1, [('seasoned_beam', 4), ('iron_bolt', 4), ('hawser', 1)], 1, {}),
    (engine_pic(IRON, BRASSY, 1), 'steam_engine', 'Steam engine', 5, {'weight': 20, 'thrust': 40}, 'engine_shop', 1, [('piston', 2), ('steam_pipe', 4), ('bearing', 2), ('steel_plate', 4)], 1, {'fuel_capacity': 6000, 'tool': 'pickaxe', 'light': 4, 'light_needs_fuel': True}),
    (engine_pic(STEEL, BRASSY, 3), 'triple_engine', 'Triple-expansion engine', 5, {'weight': 40, 'thrust': 100}, 'engine_shop', 1, [('steam_engine', 2), ('piston', 3), ('bearing', 4), ('cog_wheel', 6)], 1, {'fuel_capacity': 9000, 'tool': 'pickaxe', 'light': 4, 'light_needs_fuel': True}),
    (turbine_pic(c('5a6470'), c('c0c8d0')), 'steam_turbine', 'Steam turbine', 6, {'weight': 60, 'thrust': 240}, 'engine_shop', 1, [('triple_engine', 1), ('black_alloy', 4), ('bearing', 6), ('steam_pipe', 8)], 1, {'fuel_capacity': 12000, 'tool': 'pickaxe', 'light': 4, 'light_needs_fuel': True}),
    (boiler_pic(IRON), 'scotch_boiler', 'Scotch boiler', 5, {'weight': 15}, 'engine_shop', 1, [('steel_plate', 6), ('rivet', 16), ('steam_pipe', 2)], 1, {'tool': 'pickaxe', 'light': 6}),
    (funnel_pic(c('d8c8a0'), INK), 'funnel', 'Funnel', 5, {'weight': 3}, 'plate_mill', 1, [('steel_plate', 2), ('rivet', 4)], 2, {'tool': 'pickaxe', 'attaches': True, 'supports': True}),
    (dynamo_pic(STEEL), 'dynamo', 'Dynamo', 5, {'weight': 8}, 'electric_bench', 1, [('copper_wire', 12), ('bearing', 2), ('steel_plate', 2)], 1, {'tool': 'pickaxe'}),
    (gun_pic(BRASSY, 0, PLANK), 'swivel_gun', 'Swivel gun', 3, {'weight': 3, 'guns': 2}, 'gunfoundry', 1, [('copper_ingot', 4), ('tin_ingot', 1), ('iron_bolt', 2)], 1, {'tool': 'pickaxe'}),
    (gun_pic(c('4a4a52'), 0), 'saker', 'Saker', 3, {'weight': 6, 'guns': 4}, 'gunfoundry', 1, [('iron_bar', 6), ('seasoned_beam', 2), ('gunpowder', 2), ('round_shot', 4)], 1, {'tool': 'pickaxe'}),
    (gun_pic(c('b88a4a'), 1), 'long_gun', 'Long gun', 4, {'weight': 10, 'guns': 7}, 'gunfoundry', 1, [('bronze_ingot', 6), ('seasoned_beam', 2), ('gunpowder', 4), ('round_shot', 8)], 1, {'tool': 'pickaxe'}),
    (gun_pic(c('3a3a44'), 2), 'carronade', 'Carronade', 4, {'weight': 12, 'guns': 10}, 'gunfoundry', 1, [('pig_iron', 6), ('iron_strap', 2), ('gunpowder', 6), ('round_shot', 8)], 1, {'tool': 'pickaxe'}),
    (gun_pic(STEEL, 1, IRON), 'breech_gun', 'Breech-loading gun', 5, {'weight': 16, 'guns': 16}, 'armoury', 1, [('steel_tube', 3), ('steel_plate', 2), ('cog_wheel', 2), ('gunpowder', 8)], 1, {'tool': 'pickaxe'}),
    (turret_pic(c('7a8490')), 'gun_turret', 'Gun turret', 5, {'weight': 40, 'guns': 34}, 'armoury', 1, [('breech_gun', 2), ('steel_plate', 8), ('bearing', 2), ('rivet', 16)], 1, {'tool': 'pickaxe'}),
    (turret_pic(c('3a3e4c'), 3), 'main_battery', 'Main battery', 6, {'weight': 80, 'guns': 80}, 'armoury', 1, [('gun_turret', 2), ('armour_plate', 6), ('black_alloy', 4)], 1, {'tool': 'pickaxe'}),
    (tube_pic(c('6a7480')), 'torpedo_tube', 'Torpedo tube', 6, {'weight': 20, 'guns': 50}, 'armoury', 1, [('steel_tube', 4), ('gunpowder', 12), ('piston', 1), ('black_alloy', 1)], 1, {'tool': 'pickaxe'}),
]
for pic, n, lab, tier, ship, b, lv, ins, cnt, kw in FITS: fit(pic, n, lab, tier, ship, b, lv, ins, cnt, **kw)
# Nets, pots and what goes down on a cable: each keeps what it takes.
CATCH = [  # picture, name, label, tier, columns, rows, fields, ship, bench, level, inputs
    (net_pic(dk(ROPE)), 'drift_net', 'Drift net', 1, 3, 2, {'catches': True, 'attaches': True, 'floats': True}, {'weight': 1}, 'ropewalk', 1, [('kelp_twine', 4), ('driftwood', 2)]),
    (net_pic(ROPE, c('60c0a0')), 'great_net', 'Great net', 3, 4, 3, {'catches': True, 'attaches': True, 'floats': True}, {'weight': 2}, 'ropewalk', 1, [('hawser', 2), ('glass_float', 3), ('rope', 4)]),
    (net_pic(STEEL, RED), 'steel_boom_net', 'Boom net', 5, 4, 4, {'catches': True, 'attaches': True, 'floats': True, 'tool': 'pickaxe'}, {'weight': 4}, 'plate_mill', 1, [('great_net', 1), ('steel_tube', 2), ('chain_link', 4)]),
    (pot_pic(DRIFT), 'lobster_pot', 'Lobster pot', 1, 2, 2, {'trawl': 900}, {'weight': 1, 'depth': 1}, 'coopers_block', 1, [('cask_stave', 4), ('kelp_twine', 2)]),
    (winch_pic(IRON), 'trawl_winch', 'Trawl winch', 3, 3, 3, {'trawl': 700}, {'weight': 4, 'depth': 2}, 'forge', 2, [('iron_bar', 4), ('hawser', 4), ('great_net', 1)]),
    (bell_dome(c('b88a4a')), 'diving_bell', 'Diving bell', 4, 4, 3, {'trawl': 600, 'tool': 'pickaxe'}, {'weight': 10, 'depth': 3}, 'gunfoundry', 1, [('bronze_ingot', 8), ('glass_pane', 2), ('chain_link', 8), ('lead_ingot', 4)]),
    (sphere_pic(c('3a3e4c')), 'bathysphere', 'Bathysphere', 6, 4, 4, {'trawl': 500, 'tool': 'pickaxe', 'light': 6}, {'weight': 30, 'depth': 4}, 'armoury', 1, [('armour_plate', 8), ('lens', 4), ('light_bulb', 2), ('chain_link', 24), ('steam_windlass', 1)]),
]
for pic, n, lab, tier, cols, rows, kw, ship, b, lv, ins in CATCH: fit(pic, n, lab, tier, ship, b, lv, ins, storage={'columns': cols, 'rows': rows}, iover={'container': {'slots': cols * rows}}, **kw)
STORES = [(box_pic(DRIFT, dk(DRIFT)), 'stowage_crate', 'Stowage crate', 1, 2, 2, 'coopers_block', [('driftwood', 3), ('nails', 1)]), (barrel(PLANK).outline(), 'water_cask', 'Cask', 1, 3, 2, 'coopers_block', [('cask_stave', 6), ('rope', 1)]),
          (chest_pic(c('8a5a30'), IRON), 'sea_chest', 'Sea chest', 2, 4, 3, 'coopers_block', [('plank', 6), ('nails', 4)]), (chest_pic(c('6a4a2a'), BRASSY), 'hold_locker', 'Hold locker', 3, 4, 4, 'coopers_block', [('seasoned_beam', 4), ('iron_strap', 2)]),
          (box_pic(c('7a4a3a'), BRASSY, RED), 'magazine', 'Magazine', 4, 4, 4, 'coopers_block', [('seasoned_beam', 4), ('copper_sheet', 6), ('lead_ingot', 2)]), (box_pic(STEEL, dk(STEEL), ICE), 'ice_room', 'Ice room', 5, 4, 4, 'plate_mill', [('steel_plate', 6), ('ice_chunk', 8), ('rivet', 8)])]
for pic, n, lab, tier, cols, rows, b, ins in STORES: fit(pic, n, lab, tier, {'weight': 1 + tier // 2}, b, 1, ins, storage={'columns': cols, 'rows': rows}, iover={'container': {'slots': cols * rows}})
# Living aboard, light, and what is only for show.
FURN = [(hammock_pic(CLOTH), 'hammock', 'Hammock', 1, 'sail_loft', [('patched_canvas', 2), ('rope', 2)], {'bed': {'speed': 1.5}}), (bunk_pic(PLANK, c('a04a4a')), 'ships_bunk', 'Bunk', 3, 'carpenters_bench', [('plank', 4), ('canvas', 2)], {'bed': {'speed': 2.0}}),
        (bunk_pic(c('7a4a2a'), c('3a5a9a')), 'great_cabin_bed', 'Great cabin bed', 4, 'carpenters_bench', [('great_timber', 2), ('cloth_bale', 2), ('gull_feather', 8)], {'bed': {'speed': 3.0}}),
        (table_pic(PLANK), 'mess_table', 'Mess table', 2, 'carpenters_bench', [('plank', 3), ('trenail', 2)], {}), (lamp_pic(IRON, c('f0c060')), 'ships_lantern', "Ship's lantern", 2, 'coopers_block', [('copper_sheet', 1), ('glass_pane', 1), ('lamp_wick', 1)], {'light': 12}),
        (lamp_pic(DRIFT, c('f0a040')), 'bottle_lamp', 'Bottle lamp', 1, 'coopers_block', [('glass_bottle', 1), ('kelp_twine', 1), ('driftwood', 1)], {'light': 9}), (lamp_pic(BRASSY, c('fff0a0')), 'arc_lamp', 'Electric lamp', 5, 'electric_bench', [('light_bulb', 1), ('brass_ingot', 1)], {'light': 15}),
        (lamp_pic(c('2a3a4a'), c('80f0d0')), 'ghost_lamp', 'Ghost lamp', 6, 'electric_bench', [('ghost_oil', 1), ('lens', 1), ('black_alloy', 1)], {'light': 15}),
        (figure_pic(c('c8a070'), BRASSY), 'mermaid_figurehead', 'Mermaid figurehead', 3, 'carpenters_bench', [('seasoned_beam', 3), ('abalone', 2)], {}), (figure_pic(c('8a5a30'), RED), 'lion_figurehead', 'Lion figurehead', 4, 'carpenters_bench', [('great_timber', 3), ('brass_ingot', 2)], {}),
        (figure_pic(BONE, c('8060d0')), 'leviathan_figurehead', 'Leviathan figurehead', 6, 'carpenters_bench', [('whale_bone', 6), ('abyssal_pearl', 2)], {'light': 5}),
        (P().disc(7.5, 6, 4.5, BRASSY).r(3, 9, 12, 10, BRASSY).r(7, 11, 8, 12, dk(BRASSY)).r(7, 0, 8, 1, IRON).outline(), 'watch_bell', 'Watch bell', 3, 'forge', [('brass_ingot', 2), ('rope', 1)], {}),
        (P().r(5, 8, 10, 15, c('8a5a30')).disc(7.5, 5, 3.5, BRASSY).disc(7.5, 5, 2, c('f0f0e0')).p(7, 4, RED).outline(), 'binnacle', 'Binnacle', 3, 'chart_table', [('brass_ingot', 2), ('lens', 1), ('seasoned_beam', 1)], {}),
        (flag(RED), 'red_ensign', 'Red ensign', 2, 'sail_loft', [('canvas', 1), ('coral_branch', 1), ('driftwood', 1)], {}), (flag(c('2a4a9a')), 'blue_ensign', 'Blue ensign', 3, 'sail_loft', [('canvas', 1), ('cloth_bale', 1)], {}), (flag(INK, WOODC, WHITE), 'black_flag', 'Black flag', 3, 'sail_loft', [('canvas', 1), ('tar', 1)], {}),
        (coil(ROPE), 'rope_coil', 'Coil of rope', 1, 'ropewalk', [('rope', 3)], {}), (P().r(5, 6, 10, 15, IRON).r(4, 4, 11, 6, IRON).r(4, 4, 11, 4, lt(IRON)).outline(), 'bollard', 'Bollard', 3, 'forge', [('iron_bar', 2)], {}),
        (P().r(6, 6, 9, 15, c('d8c8a0')).r(4, 1, 11, 6, c('d8c8a0')).r(5, 2, 10, 5, INK).outline(), 'ventilator', 'Ventilator cowl', 5, 'plate_mill', [('steel_plate', 1)], {}), (planter_pic(PLANK), 'planter_box', 'Planter box', 2, 'carpenters_bench', [('plank', 4), ('island_soil_bag', 2)], {}),
        (planter_pic(c('7a4a2a'), c('4a3420')), 'tree_tub', 'Tree tub', 3, 'coopers_block', [('cask_stave', 8), ('iron_strap', 1), ('island_soil_bag', 4)], {})]
for pic, n, lab, tier, b, ins, kw in FURN:
    solid = n in ('planter_box', 'tree_tub')
    if solid: kw = dict(kw, fertile=2.0, wall=False)
    fit(pic, n, lab, tier, {'weight': 1}, b, 2 if b == 'forge' and tier >= 3 else 1, ins, solid=solid, **kw)

# ---- tools, hooks, arms and what is worn ----------------------------------------------------------------------------
def pick_pic(col, grip=PLANK): return P().line(4, 13, 10, 5, grip).line(5, 3, 13, 7, col).line(5, 2, 13, 6, lt(col)).p(4, 3, col).p(14, 8, col).outline()
METALS = [('shell', 'Shell', 1, c('f0d8c8'), 1.5, 'carpenters_bench', 1, 'sea_shell'), ('copper', 'Copper', 2, COPPER, 2.5, 'forge', 1, 'copper_ingot'), ('iron', 'Iron', 3, IRON, 4.0, 'forge', 2, 'iron_bar'),
          ('steel', 'Steel', 5, STEEL, 6.5, 'forge', 3, 'steel_ingot'), ('black', 'Black-alloy', 6, c('4a3e6a'), 9.0, 'forge', 3, 'black_alloy')]
for key, lab, tier, col, speed, b, lv, mat in METALS:
    for cls, word, pic in (('axe', 'adze', weapon('axe', col)), ('pickaxe', 'pick', pick_pic(col)), ('shovel', 'shovel', weapon('shovel', col)), ('machete', 'knife', weapon('cutlass', col))):
        n = '%s_%s' % (key, word); I(pic, n, '%s %s' % (lab, word), tier, 'tools', 1.2, tool={'class': cls, 'speed': speed}, max_stack=1); R(b, n, [(mat, 2), ('driftwood' if tier < 3 else 'seasoned_beam', 1), ('kelp_twine' if tier < 3 else 'rope', 1)], level=lv)
for n, lab, tier, reach, col, b, lv, ins in [('driftwood_hook', 'Driftwood hook', 1, 5, DRIFT, 'carpenters_bench', 1, [('driftwood', 2), ('kelp_twine', 2)]), ('boathook', 'Boathook', 2, 8, COPPER, 'forge', 1, [('copper_rod', 1), ('plank', 2), ('rope', 1)]),
                                       ('grappling_iron', 'Grappling iron', 3, 12, IRON, 'forge', 2, [('iron_bar', 2), ('hawser', 1)]), ('harpoon_line', 'Harpoon and line', 5, 18, STEEL, 'armoury', 1, [('steel_tube', 1), ('hawser', 2), ('gunpowder', 1)])]:
    I(weapon('hook', col), n, lab, tier, 'tools', 1.0, hook=reach, max_stack=1); R(b, n, ins, level=lv)
ARMS = [('gaff', 'Gaff', 1, 3, 0, weapon('pike', c('f0d8c8'), DRIFT), 'carpenters_bench', 1, [('driftwood', 2), ('sea_shell', 1)]), ('copper_harpoon', 'Copper harpoon', 2, 6, 5, weapon('pike', COPPER), 'forge', 1, [('copper_rod', 2), ('plank', 1), ('rope', 1)]),
        ('cutlass', 'Cutlass', 3, 10, 0, weapon('cutlass', IRON), 'forge', 2, [('iron_bar', 3), ('rope', 1)]), ('boarding_pike', 'Boarding pike', 3, 9, 2, weapon('pike', IRON), 'forge', 2, [('iron_bar', 2), ('seasoned_beam', 1)]),
        ('sea_musket', 'Musket', 4, 14, 10, weapon('musket', c('4a4a52')), 'gunfoundry', 1, [('iron_bar', 3), ('seasoned_beam', 1), ('gunpowder', 2)]), ('officers_sabre', "Officer's sabre", 5, 18, 0, weapon('cutlass', STEEL, BRASSY), 'armoury', 1, [('steel_ingot', 3), ('brass_ingot', 1)]),
        ('service_rifle', 'Rifle', 5, 26, 14, weapon('musket', STEEL), 'armoury', 1, [('steel_tube', 2), ('seasoned_beam', 1), ('gunpowder', 4)]), ('harpoon_gun', 'Harpoon gun', 6, 40, 16, weapon('blunderbuss', c('4a3e6a')), 'armoury', 1, [('black_alloy', 3), ('steel_tube', 2), ('gunpowder', 6)])]
for n, lab, tier, dmg, rng, pic, b, lv, ins in ARMS: I(pic, n, lab, tier, 'tools', 1.5, damage=dmg, range=rng or None, max_stack=1); R(b, n, ins, level=lv)
I(P().r(4, 4, 11, 13, c('a0a8b0')).r(4, 4, 11, 4, WHITE).line(4, 4, 7, 1, IRON).line(11, 4, 8, 1, IRON).outline(), 'bailing_bucket', 'Bucket', 1, 'tools', 1.0, holds_water=8, max_stack=1); R('coopers_block', 'bailing_bucket', [('cask_stave', 3), ('rope', 1)])
I(weapon('spyglass', c('8a5a30')), 'spyglass', 'Spyglass', 3, 'tools', 2.0, max_stack=1); R('chart_table', 'spyglass', [('copper_sheet', 2), ('lens', 2)])
OUTFITS = [('sailcloth', 'Sailcloth', 1, c('d8ccb0'), 0.02, 0.05, 'bandana', 'sail_loft', 'patched_canvas', None), ('oilskin', 'Oilskin', 3, c('d0b040'), 0.03, 0.15, 'tricorn', 'sail_loft', 'canvas', 'tar'),
           ('diving', 'Diving', 4, c('b88a4a'), 0.05, 0.08, 'helm', 'gunfoundry', 'bronze_ingot', 'glass_pane'), ('sealskin', 'Sealskin', 5, c('8a8a90'), 0.05, 0.25, 'bandana', 'sail_loft', 'seal_skin', 'bear_fur'),
           ('officers', "Officer's", 5, c('2a3a6a'), 0.07, 0.12, 'tricorn', 'sail_loft', 'cloth_bale', 'brass_ingot')]
for key, lab, tier, col, armor, warm, hat, b, mat, extra in OUTFITS:
    for slot, word, kind, n_ in (('head', {'bandana': 'cap', 'tricorn': 'hat', 'helm': 'helmet'}[hat], hat, 2), ('chest', 'coat', 'coat', 4), ('legs', 'breeches', 'breeches', 3), ('boots', 'boots', 'boots', 2)):
        n = '%s_%s' % (key, word)
        I(hatpic(kind, col), n, '%s %s' % (lab, word), tier, 'clothing', 1.0, equip=slot, max_stack=1, armor=armor, warmth=warm, boons={'lung': 2.5} if key == 'diving' and slot == 'head' else None)
        R(b, n, [(mat, n_)] + ([(extra, 1)] if extra else []) + [('kelp_twine' if tier < 3 else 'linen_thread', 1)])

# ---- crops in boxes, trees in tubs -----------------------------------------------------------------------------------
SOILS, TUBS = ['planter_box', 'island_soil', 'island_grass'], ['tree_tub', 'island_soil', 'island_grass']
CROPS = [('flax', 'Flax', 2, 'leaf', '7a9ad0', 'flax_seed', 'Flax seed'), ('hemp', 'Hemp', 3, 'cane', '6a9a3a', 'hemp_seed', 'Hemp seed'), ('potato', 'Potato', 2, 'vine', 'c8a060', None, None), ('wheat', 'Wheat', 3, 'cane', 'e0c050', None, None), ('sugar_cane', 'Sugar cane', 4, 'cane', 'a8c860', None, None)]
for n, lab, tier, arch, col, seed, seed_lab in CROPS:
    for k in range(4):
        plant(arch, c(col), k).save('tiles', '%s_%d' % (n, k))
        tile('%s_%d' % (n, k), lab, False, tool='machete', mine_time=0.1, drop=None, soil=SOILS, grows_into='%s_%d' % (n, k + 1) if k < 3 else None,
             drops=[{'item': seed or n, 'count': 1}] if k < 3 else [{'item': n, 'count': 3}] + ([{'item': seed, 'count': 2}] if seed else []))
    I(crop_item(c(col)), n, lab, tier, 'nature', 0.3, sows=None if seed else n + '_0', food=0.12 if n == 'potato' else None); raw(n)
    if seed: I(P().disc(5, 9, 1.6, c(col)).disc(9, 7, 1.6, c(col)).disc(10, 11, 1.6, dk(c(col))).outline(), seed, seed_lab, tier, 'nature', 0.2, sows=n + '_0'); raw(seed)
for out, ins in (('flax_seed', [('seed_pod', 1)]), ('potato', [('seed_pod', 2)]), ('hemp_seed', [('seed_pod', 2), ('guano', 1)]), ('wheat', [('seed_pod', 2), ('island_soil_bag', 1)]), ('sugar_cane', [('seed_pod', 3), ('bamboo', 1)])): R('coopers_block', out, ins, 2)
TREES = [('palm', 'Palm', '9a8a5a', '5aa040', 'coconut', None, 'palm_frond', 2), ('oak', 'Oak', '6a5038', '4a8a3a', 'acorn', 'Acorn', None, 3), ('pine', 'Pine', '5a4030', '2a6a4a', 'pine_cone', 'Pine cone', None, 3), ('lime_tree', 'Lime tree', '7a5a3a', '5aa838', 'lime', 'Lime', None, 3)]
for n, lab, bark_c, leaf_c, fruit, fruit_lab, leaf_drop, tier in TREES:
    log, lv = (n.replace('_tree', '') + '_log'), (n.replace('_tree', '') + '_leaves')
    bark(log, c(bark_c)).save('tiles', log); leaves(lv, c(leaf_c), c('f0f060') if n == 'lime_tree' else None).save('tiles', lv)
    tiles.append({'name': log, 'label': lab + ' trunk', 'removable': True, 'climbable': True, 'mine_time': 0.5, 'drop': log, 'texture': 'tiles/%s.png' % log, 'shade': 6, 'tool': 'axe', 'burns': True})
    tiles.append(clean({'name': lv, 'label': lab + ' leaves', 'removable': True, 'climbable': True, 'mine_time': 0.15, 'drop': leaf_drop or 'kindling', 'texture': 'tiles/%s.png' % lv, 'shade': 12, 'tool': 'machete'}))
    I(P().r(3, 5, 12, 11, c(bark_c)).disc(3, 8, 3, lt(c(bark_c), 1.3)).disc(3, 8, 1.4, c(bark_c)).outline(), log, lab + ' log', tier, 'nature', 0.4, fuel=400); raw(log)
    if fruit_lab: I(P().disc(7.5, 9, 3.5, c('a0d030') if fruit == 'lime' else c('8a6a3a')).r(7, 4, 8, 5, c('4a3a20')).outline(), fruit, fruit_lab, tier, 'nature', 0.3, plants=n, food=0.1 if fruit == 'lime' else None); raw(fruit)
    trees.append({'name': n, 'log': log, 'leaves': lv, 'fruit': fruit, 'fruit_ticks': 1800, 'layer': 'background', 'soil': TUBS, 'max_height': 8 if n == 'palm' else 6, 'weight': 1,
                  'rows': [{'offset': 0, 'left': 2, 'right': 2, 'center': True}, {'offset': 1, 'left': 1, 'right': 1}] if n == 'palm' else [{'offset': -1, 'left': 1, 'right': 1, 'center': True}, {'offset': 0, 'left': 2, 'right': 2}, {'offset': 1, 'left': 2, 'right': 2}, {'offset': 2, 'left': 1, 'right': 1}]})
I(beam(c('6a5a40')), 'kindling', 'Kindling', 1, 'nature', 0.2, fuel=100); raw('kindling')

# ---- victuals -----------------------------------------------------------------------------------------------------------
CATCHES = [('small_fish', 'Small fish', 1, fishpic(c('8aa8c0'), WHITE)), ('big_fish', 'Big fish', 2, fishpic(c('4a6a9a'), c('c0d0e0'), True)), ('shark_steak', 'Shark steak', 2, meat(c('c0a0a8'))), ('crab_meat', 'Crab', 2, P().disc(7.5, 9, 4, c('e06a4a')).r(1, 6, 3, 8, c('e06a4a')).r(12, 6, 14, 8, c('e06a4a')).p(6, 7, INK).p(9, 7, INK).outline()),
           ('turtle_meat', 'Turtle meat', 2, meat(c('6a8a4a'))), ('squid', 'Squid', 3, P().disc(7.5, 5, 3.5, c('d08a9a')).line(5, 8, 3, 14, c('d08a9a')).line(7, 8, 7, 14, c('d08a9a')).line(10, 8, 12, 14, c('d08a9a')).p(6, 5, INK).p(9, 5, INK).outline()),
           ('whale_meat', 'Whale meat', 4, meat(c('7a3a4a'))), ('seal_meat', 'Seal meat', 5, meat(c('9a5a5a'))), ('goat_meat', 'Goat meat', 3, meat(c('b0705a'))), ('hen_egg', 'Egg', 3, trinket('egg', c('f0e8d8'), c('d0c0a0'))), ('clam', 'Clam', 1, shell_pic(c('c0b8a8'))), ('abalone', 'Abalone', 2, shell_pic(c('80c0b0')))]
for n, lab, tier, pic in CATCHES:
    I(pic, n, lab, tier, 'nature', 0.4, food=0.08); raw(n)
    I(bowl(c('c08a50')) if 'meat' in n or 'steak' in n else meat(c('c89a60')) if 'fish' in n else bowl(c('e0c090')), 'cooked_' + n, 'Cooked ' + lab.lower(), tier, 'nature', 0.8, food=0.22 + 0.04 * tier, cooked=True); R('galley', 'cooked_' + n, [(n, 1)], time=60)
for n, lab, tier, pic, kw in [('seal_skin', 'Seal skin', 5, hide(c('8a8a90')), {}), ('bear_fur', 'White bear fur', 5, hide(c('f0f0f0'), c('d0d0d8')), {}), ('gull_feather', 'Feather', 1, feather(c('f0f0f0')), {}), ('turtle_shell', 'Turtle shell', 2, shell_pic(c('6a8a4a')), {}),
                              ('shark_tooth', 'Shark tooth', 2, fang(WHITE), {}), ('pearl', 'Pearl', 3, orb(c('f0f0f8')), {}), ('light_gland', 'Light gland', 6, orb(c('80f0d0')), {}), ('kraken_ink', 'Kraken ink', 4, bottle(c('1a1a2a')), {}),
                              ('leviathan_scale', 'Leviathan scale', 6, sheet(c('5a3e8a')), {}), ('narwhal_tusk', 'Narwhal tusk', 5, fang(c('f0e8d0')), {}), ('raiders_mark', "Raider's mark", 3, trinket('seal', INK, RED), {})]: I(pic, n, lab, tier, 'nature', 0.6, **kw); raw(n)
def lasts(ticks, **boons): return {'ticks': ticks, 'boons': boons}
I(sack(c('f0e8d0')), 'flour', 'Flour', 3, 'nature', 0.4); R('galley', 'flour', [('wheat', 2)], 2); I(sack(c('f8f8f8')), 'sea_salt', 'Sea salt', 1, 'nature', 0.3); R('galley', 'sea_salt', [('kelp', 2)], 1); I(sack(c('e0d0b0')), 'sugar', 'Sugar', 4, 'nature', 0.5); R('galley', 'sugar', [('sugar_cane', 2)], 2)
MEALS = [('kelp_soup', 'Kelp soup', 1, '5a8a4a', [('kelp', 3), ('fresh_water', 1)], 0.2, None), ('coconut_milk', 'Coconut milk', 1, 'f0f0e8', [('coconut', 1)], 0.2, None), ('salt_fish', 'Salt fish', 1, 'c0c8d0', [('small_fish', 2), ('sea_salt', 1)], 0.35, None),
         ('fish_stew', 'Fish stew', 2, 'c08a50', [('big_fish', 1), ('potato', 2), ('fresh_water', 1)], 0.55, lasts(3000, regen=1.5)), ('chowder', 'Chowder', 2, 'e8d8b0', [('clam', 2), ('potato', 1), ('fresh_water', 1)], 0.5, None),
         ('turtle_soup', 'Turtle soup', 2, '7a9a4a', [('turtle_meat', 1), ('kelp', 1), ('fresh_water', 1)], 0.5, lasts(3000, lung=1.5)), ('hardtack', 'Hardtack', 3, 'd8c8a0', [('flour', 2), ('sea_salt', 1)], 0.4, None),
         ('ships_bread', "Ship's bread", 3, 'c89a50', [('flour', 3), ('fresh_water', 1)], 0.5, None), ('lime_cordial', 'Lime cordial', 3, 'c0e060', [('lime', 2), ('fresh_water', 1)], 0.15, lasts(6000, regen=2.0)),
         ('sea_pie', 'Sea pie', 3, 'b07a40', [('flour', 1), ('goat_meat', 1), ('potato', 1)], 0.7, lasts(3000, strength=1)), ('lobscouse', 'Lobscouse', 3, 'a06a40', [('salt_cask', 1), ('potato', 2), ('hardtack', 1)], 0.8, None),
         ('grog', 'Grog', 4, 'c88a40', [('sugar', 2), ('fresh_water', 1), ('lime', 1)], 0.1, lasts(3000, warmth=0.4)), ('plum_duff', 'Plum duff', 4, '8a4a5a', [('flour', 2), ('sugar', 1), ('hen_egg', 1)], 0.75, lasts(4000, craft=1.2)),
         ('whalers_stew', "Whaler's stew", 4, '7a3a3a', [('whale_meat', 1), ('potato', 2), ('sea_salt', 1)], 0.85, lasts(4000, warmth=0.3)), ('pemmican', 'Pemmican', 5, '8a5a3a', [('seal_meat', 2), ('whale_blubber', 1), ('sea_salt', 1)], 0.9, lasts(6000, warmth=0.5)),
         ('captains_table', "Captain's table", 5, 'd0a060', [('sea_pie', 1), ('plum_duff', 1), ('grog', 1)], 1.0, lasts(6000, regen=2.0, strength=2)), ('abyssal_broth', 'Abyssal broth', 6, '5a3e8a', [('light_gland', 1), ('squid', 1), ('fresh_water', 1)], 0.6, lasts(6000, lung=3.0, regen=1.5))]
for n, lab, tier, col, ins, food, eff in MEALS: I(cup(c(col)) if n in ('coconut_milk', 'lime_cordial', 'grog') else bowl(c(col)), n, lab, tier, 'nature', 1.0, food=food, cooked=True, effect=eff); R('galley', n, ins, time=100)

# ---- what lives in the sea, over it, and on what comes alongside -------------------------------------------------------
ALL = ['slick', 'shoals', 'trade_wind', 'roaring_sea', 'ice', 'black_water']
def sw(chance, seas_, **kw): return dict({'water': True, 'chance': chance, 'seas': seas_}, **kw)
def sky(chance, seas_): return {'sky': True, 'chance': chance, 'seas': seas_}
def on(tiles_, chance): return {'above': tiles_, 'chance': chance}
HUNT = lambda sight, bite: {'hunts': True, 'sight': sight, 'bite': bite, 'rest_ticks': 16}
BEASTS = [  # kind, name, label, body, accent, health, drops, spawn, mean, more
    ('fish', 'sardine', 'Sardine', '8aa8c0', 'c0d0e0', 3, [drop('small_fish', 1, 1)], sw(0.05, ALL[:4]), False, {'swim': {}, 'cap': 8}),
    ('fish', 'mackerel', 'Mackerel', '4a8a9a', '2a4a5a', 5, [drop('small_fish', 1, 2)], sw(0.03, ALL[:3]), False, {'swim': {}, 'cap': 6}),
    ('fish', 'reef_shark', 'Reef shark', '7a8a98', 'e0e0e8', 22, [drop('shark_steak', 1, 2), drop('shark_tooth', 0, 2)], sw(0.004, ALL[:2]), True, {'swim': HUNT(16, 0.15), 'cap': 2, 'size': [1, 2]}),
    ('bird', 'gull', 'Gull', 'f0f0f0', 'f0b030', 3, [drop('gull_feather', 1, 2)], sky(0.02, ALL[:4]), False, {}),
    ('bird', 'booby', 'Booby', 'e8e0d0', '4a8ad0', 4, [drop('gull_feather', 1, 3)], on(['guano_rock', 'shore_rock'], 0.3), False, {}),
    ('tortoise', 'sea_turtle', 'Sea turtle', '5a8a4a', '3a5a2a', 14, [drop('turtle_meat', 1, 2), drop('turtle_shell', 1, 1)], sw(0.008, ['shoals', 'trade_wind']), False, {'swim': {}, 'cap': 3}),
    ('tortoise', 'shore_crab', 'Shore crab', 'e06a4a', 'a03a2a', 5, [drop('crab_meat', 1, 2)], on(['sand_bank', 'coral_reef'], 0.3), False, {}),
    ('fish', 'parrotfish', 'Parrotfish', '40c0a0', 'f08040', 5, [drop('small_fish', 1, 2), drop('coral_branch', 0, 1)], sw(0.03, ['shoals']), False, {'swim': {}, 'cap': 6}),
    ('serpent', 'moray', 'Moray', '6a7a3a', 'c0b040', 12, [drop('big_fish', 1, 1)], sw(0.006, ['shoals']), True, {'swim': HUNT(8, 0.12), 'cap': 2}),
    ('quad', 'island_goat', 'Island goat', 'd8d0c0', '5a4a3a', 10, [drop('goat_meat', 1, 2)], on(['island_grass'], 0.25), False, {}),
    ('crane', 'jungle_hen', 'Hen', 'c87a3a', 'c83030', 4, [drop('hen_egg', 1, 2), drop('gull_feather', 1, 2)], on(['island_grass', 'island_soil'], 0.2), False, {}),
    ('ape', 'ships_monkey', 'Monkey', '8a5a30', 'e0b890', 8, [drop('coconut', 0, 2)], on(['island_grass'], 0.1), False, {}),
    ('fish', 'flying_fish', 'Flying fish', '60a0e0', 'e0f0ff', 4, [drop('small_fish', 1, 2)], sw(0.04, ['trade_wind']), False, {'swim': {}, 'cap': 8}),
    ('fish', 'tuna', 'Tuna', '2a4a7a', 'c0c8d0', 12, [drop('big_fish', 2, 3)], sw(0.015, ['trade_wind', 'roaring_sea']), False, {'swim': {}, 'cap': 4, 'size': [1, 2]}),
    ('fish', 'dolphin', 'Dolphin', '7a9ab0', 'e0e8f0', 20, [drop('big_fish', 1, 2)], sw(0.006, ['trade_wind']), False, {'swim': {}, 'cap': 3, 'size': [1, 2]}),
    ('fish', 'swordfish', 'Swordfish', '3a5a8a', 'd0d8e0', 26, [drop('big_fish', 2, 4)], sw(0.006, ['trade_wind', 'roaring_sea']), True, {'swim': HUNT(18, 0.2), 'cap': 2, 'size': [1, 2]}),
    ('fish', 'hammerhead', 'Hammerhead', '6a7480', 'c0c8d0', 40, [drop('shark_steak', 2, 3), drop('shark_tooth', 1, 3)], sw(0.004, ['trade_wind']), True, {'swim': HUNT(20, 0.25), 'cap': 2, 'size': [1, 2]}),
    ('rabbit', 'ships_rat', 'Rat', '5a5048', 'e0a0a0', 3, [], on(['wreck_planking', 'wreck_ironwork'], 0.15), False, {}),
    ('tiger', 'ships_cat', "Ship's cat", 'e0a050', 'f0f0f0', 8, [], on(['wreck_planking'], 0.05), False, {'companion': {'bite': 2, 'sight': 6}, 'tame': 4}),
    ('bird', 'albatross', 'Albatross', 'f8f8f8', '3a3a44', 10, [drop('gull_feather', 2, 4)], sky(0.01, ['roaring_sea', 'ice']), False, {}),
    ('fish', 'great_whale', 'Whale', '3a4a5a', 'c0c8d0', 260, [drop('whale_meat', 4, 8), drop('whale_blubber', 6, 10), drop('whale_bone', 2, 4), drop('ambergris', 0, 1)], sw(0.002, ['roaring_sea', 'ice']), False, {'swim': {}, 'cap': 1, 'size': [3, 4]}),
    ('fish', 'great_white', 'Great white', '8a94a0', 'f0f0f0', 90, [drop('shark_steak', 3, 5), drop('shark_tooth', 2, 4)], sw(0.004, ['roaring_sea']), True, {'swim': HUNT(24, 0.3), 'cap': 2, 'size': [2, 2]}),
    ('serpent', 'giant_squid', 'Giant squid', 'b04a5a', 'f0c0c0', 140, [drop('squid', 3, 6), drop('kraken_ink', 1, 2)], sw(0.003, ['roaring_sea'], time='night'), True, {'swim': HUNT(22, 0.3), 'cap': 1, 'size': [2, 2]}),
    ('quad', 'seal', 'Seal', '8a8a90', '5a5a60', 14, [drop('seal_meat', 1, 2), drop('seal_skin', 1, 1)], on(['ice', 'pack_snow'], 0.25), False, {}),
    ('boar', 'walrus', 'Walrus', '9a6a4a', 'f0e8d0', 60, [drop('seal_meat', 2, 4), drop('seal_skin', 1, 2), drop('narwhal_tusk', 0, 1)], on(['ice'], 0.08), False, {'size': [1, 2]}),
    ('lion', 'white_bear', 'White bear', 'f0f0f0', '2a2a2a', 110, [drop('bear_fur', 1, 2), drop('seal_meat', 1, 3)], on(['pack_snow', 'ice'], 0.06), True, {'size': [2, 2]}),
    ('fish', 'narwhal', 'Narwhal', 'b0b8c8', 'f0e8d0', 50, [drop('whale_meat', 1, 2), drop('narwhal_tusk', 1, 1)], sw(0.006, ['ice']), False, {'swim': {}, 'cap': 2, 'size': [1, 2]}),
    ('fish', 'orca', 'Orca', '1a1a22', 'f0f0f0', 160, [drop('whale_meat', 2, 4), drop('whale_blubber', 2, 4)], sw(0.003, ['ice']), True, {'swim': HUNT(26, 0.35), 'cap': 2, 'size': [2, 2]}),
    ('crane', 'penguin', 'Penguin', '1a1a22', 'f0f0f0', 5, [drop('gull_feather', 1, 2)], on(['pack_snow'], 0.2), False, {}),
    ('fish', 'light_fish', 'Light fish', '2a3a4a', '80f0d0', 8, [drop('light_gland', 1, 1), drop('small_fish', 1, 1)], sw(0.03, ['black_water']), False, {'swim': {}, 'cap': 6, 'light': 6}),
    ('fish', 'angler', 'Angler', '3a2a3a', 'f0e060', 220, [drop('light_gland', 1, 3), drop('big_fish', 2, 3)], sw(0.006, ['black_water']), True, {'swim': HUNT(20, 0.4), 'cap': 2, 'size': [2, 2]}),
    ('serpent', 'abyssal_eel', 'Abyssal eel', '2a2a4a', '8060d0', 300, [drop('big_fish', 2, 4), drop('abyssal_pearl', 0, 1)], sw(0.005, ['black_water']), True, {'swim': HUNT(24, 0.45), 'cap': 2, 'size': [1, 3]}),
    ('wraith', 'drowned_hand', 'Drowned hand', '4a6a70', '80f0d0', 180, [drop('drowned_plate', 0, 2), drop('ghost_oil', 0, 1)], on(['drowned_hull'], 0.2), True, {}),
    ('wyrm', 'sea_serpent', 'Sea serpent', '2a6a5a', 'e0c040', 700, [drop('leviathan_scale', 1, 3), drop('big_fish', 3, 6)], sw(0.002, ['black_water', 'roaring_sea'], time='night'), True, {'swim': HUNT(28, 0.5), 'cap': 1, 'size': [3, 4]}),
    ('dragon', 'leviathan', 'Leviathan', '3a2e5a', 'c080f0', 6000, [drop('leviathan_scale', 6, 12), drop('abyssal_pearl', 2, 5), drop('black_ore', 6, 12)], sw(0.0006, ['black_water']), True, {'swim': HUNT(34, 0.7), 'cap': 1, 'size': [6, 8]}),
]
for kind, n, lab, body, accent, health, drops_, spawn, mean, kw in BEASTS: creature(kind, n, lab, body, accent, health, drops_, spawn, mean, **kw)
# Raiders: people, on the decks of what comes for you.
for n, lab, coat, hat, hk, health, power, drops_ in [('sea_raider', 'Raider', '8a2a2a', '1a1a1a', 1, 40, 12, [drop('raiders_mark', 0, 1), drop('gunpowder', 0, 2), drop('cowrie', 4, 20)]), ('raider_captain', 'Raider captain', '2a2a4a', 'c8a040', 2, 120, 22, [drop('raiders_mark', 1, 2), drop('cutlass', 0, 1), drop('silver_piece', 1, 4)])]:
    for f in (0, 1): person(c(coat), c(hat), f, hatkind=hk).save('mobs', n + ('_walk' if f else ''))
    mobs.append({'name': n, 'label': lab, 'texture': 'mobs/%s.png' % n, 'walk': ['mobs/%s_walk.png' % n, 'mobs/%s.png' % n], 'hop': 0.06, 'step_ticks': 3, 'move_chance': 0.3, 'turn_chance': 0.2, 'health': health, 'cap': 4 if n == 'sea_raider' else 1,
                 'spawn': on(['raider_deck'], 0.3 if n == 'sea_raider' else 0.05), 'drops': drops_, 'sting': {'ticks': 30, 'rest_ticks': 40, 'sight': 10}, 'shape': 'person'})

# ---- coin, trade, and what is left in chests ------------------------------------------------------------------------------
for n, lab, col in (('cowrie', 'Cowrie', c('f0d8c8')), ('silver_piece', 'Silver piece', c('d0d8e0')), ('gold_doubloon', 'Gold doubloon', GOLD)):
    (shell_pic(col) if n == 'cowrie' else P().disc(7.5, 8, 5, col).disc(7.5, 8, 3.4, dk(col, 0.85)).p(6, 6, WHITE).outline()).save('items', n); items.append({'name': n, 'label': lab, 'texture': 'items/%s.png' % n})
P().r(6, 8, 9, 15, c('8a5a30')).disc(7.5, 5, 4.5, BRASSY).r(3, 8, 12, 9, BRASSY).r(7, 0, 8, 1, IRON).p(6, 3, WHITE).outline().save('tiles', 'ships_bell')
tiles.append({'name': 'ships_bell', 'label': "Ship's bell", 'bench': True, 'bench_kind': 'portal', 'front': True, 'texture': 'tiles/ships_bell.png', 'bench_level': 1, 'model': 'panel', 'ship': {'weight': 1}})
person(c('8a6a4a'), c('c84a3a'), 1, blade=False, hatkind=1).save('items', 'new_hand'); items.append({'name': 'new_hand', 'label': 'New hand', 'texture': 'items/new_hand.png', 'becomes_player': True})
R('portal', 'new_hand', [('castaway', 1), ('fresh_water', 2), ('cooked_small_fish', 2)], time=200)
for lv, cost in ((1, [('silver_piece', 5), ('lens', 1)]), (2, [('silver_piece', 30), ('brass_ingot', 4)]), (3, [('gold_doubloon', 1), ('cog_wheel', 4)]), (4, [('gold_doubloon', 6), ('steel_plate', 8)]), (5, None)):
    n = 'trade_desk' if lv == 1 else 'trade_desk_%d' % lv
    t = P().r(1, 8, 14, 9, PLANK).r(2, 10, 3, 15, dk(PLANK)).r(12, 10, 13, 15, dk(PLANK)).r(3, 4, 7, 7, PAPER).r(9, 5, 12, 7, [BRASSY, c('d0d8e0'), GOLD, c('60d0f0'), c('c080f0')][lv - 1]).outline(); t.save('tiles', n); icon_of(n)
    tile(n, 'Trade desk', False, trade=True, trade_level=lv + 1, tool='axe', mine_time=0.4, ship={'weight': 2}, upgrade={'to': 'trade_desk_%d' % (lv + 1), 'cost': [{'item': i, 'count': k} for i, k in cost]} if cost else None)
    item(n, 'Trade desk', min(6, lv + 1), 'benches', 1.0, places=n)
R('shipwright', 'trade_desk', [('plank', 6), ('patched_canvas', 2), ('sea_shell', 10)], level=2, time=160)
for n, lab, lv, col in (('chandlers_stall', "Chandler's stall", 3, c('c84a3a')), ('company_factor', "Company factor's desk", 5, c('2a4a9a')), ('black_market', 'Black market', 6, c('4a3e6a'))):
    P().r(1, 8, 14, 9, PLANK).r(2, 10, 3, 15, dk(PLANK)).r(12, 10, 13, 15, dk(PLANK)).r(0, 0, 15, 3, col).r(0, 3, 15, 3, WHITE).r(1, 4, 1, 8, dk(PLANK)).r(14, 4, 14, 8, dk(PLANK)).outline().save('tiles', n); icon_of(n)
    tile(n, lab, False, trade=True, trade_level=lv, removable=False, drop=None); item(n, lab, lv, 'special', 1.0, places=n)
planks('raider_deck', c('6a3a2a')).save('tiles', 'raider_deck'); icon_of('raider_deck'); tile('raider_deck', 'Raider deck', True, tool='axe', drop=None, drops=[{'item': 'driftwood', 'count': 1}]); item('raider_deck', 'Raider deck', 3, 'special', 0.2, places='raider_deck')
loot_chest('washed_chest', 'Washed-up chest', DRIFT, VERDI, [drop('copper_scrap', 1, 4), drop('rope', 0, 3), drop('cowrie', 5, 30), one(('seed_pod', 6), ('glass_bottle', 3), ('pearl', 1), ('flax_seed', 4), ('sea_shell', 4))])
loot_chest('wreck_chest', 'Wreck chest', c('8a5a30'), IRON, [drop('iron_scrap', 2, 6), drop('cloth_bale', 0, 2), drop('cowrie', 20, 60), drop('silver_piece', 0, 3), one(('castaway', 2), ('lamp_oil', 4), ('hemp_seed', 4), ('acorn', 4), ('pine_cone', 4), ('lime', 3), ('tar_barrel', 3), lo=2, hi=3)])
loot_chest('captains_chest', "Captain's chest", c('6a3a2a'), BRASSY, [drop('brass_scrap', 2, 6), drop('silver_piece', 3, 12), drop('gunpowder', 0, 4), one(('ambergris', 2), ('pearl', 3), ('sugar_cane', 4), ('great_timber', 4), ('coal', 6), ('castaway', 1), lo=2, hi=4)])
loot_chest('frozen_stores', 'Frozen stores', c('8a9aa8'), ICE, [drop('ration_tin', 2, 6), drop('coal', 2, 8), drop('silver_piece', 2, 10), one(('whale_oil', 4), ('bear_fur', 2), ('seal_skin', 4), ('great_timber', 4), ('manganese_nodule', 4), lo=2, hi=4)])
loot_chest('drowned_safe', 'Drowned safe', c('4a5a60'), c('80f0d0'), [drop('drowned_plate', 2, 6), drop('gold_doubloon', 0, 2), drop('ghost_oil', 0, 2), one(('engine_wreckage', 4), ('abyssal_pearl', 2), ('black_ore', 4), ('light_gland', 3), lo=2, hi=4)])
loot_chest('raider_strongbox', "Raider's strongbox", c('6a3a2a'), RED, [drop('gunpowder', 2, 6), drop('round_shot', 2, 8), drop('silver_piece', 1, 8), drop('raiders_mark', 1, 2), one(('iron_bar', 4), ('cloth_bale', 3), ('lamp_oil', 3), ('sugar', 3), lo=1, hi=3)])

# ---- what comes alongside: built here, set down by the sea -------------------------------------------------------------------
import math
KEYS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789#@%&*+=<>?!$'
class Built:
    """A grid of squares, each (front, back, wall) or left as the world has it; level 0 is the top row of the water."""
    def __init__(s, wide, above, sunk): s.w, s.above, s.sunk = wide, above, sunk; s.g = [[None] * wide for _ in range(above + sunk)]
    def row(s, level): return s.above - level
    def at(s, x, level): return s.g[s.row(level)][x] if 0 <= x < s.w and 0 <= s.row(level) < len(s.g) else None
    def put(s, x, level, front=None, back=None, wall=None, over=True):
        r = s.row(level)
        if not (0 <= x < s.w and 0 <= r < len(s.g)): return
        old = s.g[r][x] or (None, None, None)
        if not over and (old[0] or old[1]): return
        s.g[r][x] = (front if front is not None else old[0], back if back is not None else old[1], wall if wall is not None else old[2])
    def top(s, x):
        for level in range(s.above, -s.sunk, -1):
            cell = s.at(x, level)
            if cell and cell[0]: return level
        return None
    def tree(s, x, kind, tall):
        base = s.top(x)
        if base is None: return
        log, lv = kind + '_log', kind + '_leaves'
        for k in range(1, tall + 1): s.put(x, base + k, back=log, over=False)
        for dx, dy in ((-1, 0), (1, 0), (0, 1), (-1, 1), (1, 1), (-2, 0), (2, 0)): s.put(x + dx, base + tall + dy, back=lv, over=False)
    def stand(s, x, back):
        base = s.top(x)
        if base is not None: s.put(x, base + 1, back=back, over=False)
    def done(s, name):
        legend, rows = {}, []
        for line in s.g:
            text = ''
            for cell in line:
                if cell is None or not any(cell): text += '~' if cell is None else ' '; continue
                key = next((k for k, v in legend.items() if v == cell), None)
                if key is None: key = KEYS[len(legend)]; legend[key] = cell
                text += key
            rows.append(text)
        assert len(rows) <= 20 and s.w <= 32, name
        structures.append({'name': name, 'rows': rows, 'legend': {k: clean({'front': v[0], 'back': v[1], 'wall': v[2]}) for k, v in legend.items()}}); return s.sunk
def isle(name, wide, peak, sunk, top, body, core=None, veins=(), trees=(), things=(), room=8):
    b = Built(wide, peak + room, sunk); g = rnd(name)
    for x in range(wide):
        f = math.sin(math.pi * (x + 0.5) / wide); high = round(peak * f); deep = max(1, round(sunk * f ** 0.6))
        for level in range(high, -deep, -1):
            t = top if level == high and level >= 1 else core if core and level < high - 2 else body
            for v, share in veins:
                if level < high and g.random() < share: t = v
            b.put(x, level, front=t)
    for x, kind, tall in trees: b.tree(x, kind, tall)
    for x, back in things: b.stand(x, back)
    return b.done(name)
def hulk(name, long, free, deep, hull, deck_, inside=(), on_deck=(), masts=(), hatch='deck_hatch', mast='pole_mast'):
    """A ship: a shell of hull with a deck on it, dry inside, a hatch to get in by."""
    b = Built(long, free + 8, deep)
    for x in range(long):
        f = math.sin(math.pi * (x + 0.5) / long) ** 0.6; d = max(1, round(deep * f))
        for level in range(free, -d, -1):
            edge = level == -d + 1 or x in (0, long - 1) or round(deep * math.sin(math.pi * (x - 0.5) / long) ** 0.6 if x > 0 else 0) < 1 - level or round(deep * math.sin(math.pi * (x + 1.5) / long) ** 0.6 if x < long - 1 else 0) < 1 - level
            if level == free: b.put(x, level, front=deck_, wall=hull)
            elif edge: b.put(x, level, front=hull, wall=hull)
            else: b.put(x, level, wall=hull)
    if hatch: b.g[b.row(free)][long // 2] = (hatch, None, hull)
    for x, back in inside:
        level = free - 1
        while b.at(x, level - 1) and not b.at(x, level - 1)[0]: level -= 1
        b.put(x, level, back=back)
    for x, back in on_deck: b.put(x, free + 1, back=back)
    for x, tall in masts:
        for k in range(1, tall + 1): b.put(x, free + k, back=mast)
    return b.done(name)
SUNK = {}
SUNK['drifting_wreckage'] = isle('drifting_wreckage', 6, 1, 1, 'wreck_planking', 'wreck_planking', things=[(3, 'washed_chest')], room=2)
SUNK['sandbar'] = isle('sandbar', 9, 1, 3, 'sand_bank', 'sand_bank', 'shore_rock', things=[(4, 'washed_chest')], room=2)
SUNK['long_sandbar'] = isle('long_sandbar', 20, 2, 4, 'sand_bank', 'sand_bank', 'shore_rock', veins=[('clay_bank', 0.12)], trees=[(6, 'palm', 4), (14, 'palm', 5)])
SUNK['palm_islet'] = isle('palm_islet', 16, 3, 5, 'island_grass', 'island_soil', 'shore_rock', trees=[(4, 'palm', 5), (8, 'palm', 4), (12, 'palm', 5)], things=[(6, 'flax_3'), (10, 'potato_3'), (7, 'washed_chest')])
SUNK['wooded_islet'] = isle('wooded_islet', 20, 3, 5, 'island_grass', 'island_soil', 'shore_rock', trees=[(4, 'oak', 4), (9, 'pine', 5), (14, 'oak', 5)], things=[(7, 'flax_3'), (11, 'washed_chest')])
SUNK['coral_head'] = isle('coral_head', 10, 0, 5, 'coral_reef', 'coral_reef', 'shore_rock', room=1)
SUNK['clay_islet'] = isle('clay_islet', 14, 2, 4, 'sand_bank', 'clay_bank', 'shore_rock', veins=[('sand_bank', 0.2)], trees=[(7, 'palm', 4)])
SUNK['bird_rock'] = isle('bird_rock', 8, 5, 4, 'guano_rock', 'shore_rock', veins=[('guano_rock', 0.3)], room=2)
SUNK['shoal_wreck'] = hulk('shoal_wreck', 12, 2, 3, 'wreck_sheathing', 'wreck_planking', inside=[(4, 'washed_chest'), (8, 'washed_chest')], masts=[(5, 4)])
SUNK['jungle_islet'] = isle('jungle_islet', 28, 4, 6, 'island_grass', 'island_soil', 'shore_rock', veins=[('clay_bank', 0.06)], trees=[(4, 'oak', 4), (9, 'pine', 5), (14, 'lime', 3), (19, 'oak', 5), (24, 'pine', 4)], things=[(6, 'hemp_3'), (11, 'wheat_3'), (16, 'flax_3'), (21, 'hemp_3'), (12, 'wreck_chest')])
SUNK['hemp_cay'] = isle('hemp_cay', 14, 2, 4, 'island_grass', 'island_soil', 'shore_rock', trees=[(3, 'palm', 4), (11, 'lime', 3)], things=[(5, 'hemp_3'), (6, 'hemp_2'), (7, 'hemp_3'), (8, 'wheat_3'), (9, 'potato_3')])
SUNK['derelict_sloop'] = hulk('derelict_sloop', 14, 2, 4, 'wreck_planking', 'wreck_planking', inside=[(4, 'wreck_chest'), (6, 'wreck_ironwork'), (9, 'wreck_ironwork'), (10, 'wreck_chest')], masts=[(6, 6)])
SUNK['castaway_rock'] = isle('castaway_rock', 7, 3, 3, 'shore_rock', 'shore_rock', things=[(3, 'wreck_chest')], room=2)
SUNK['floating_market'] = hulk('floating_market', 18, 1, 3, 'plank_hull', 'plank_deck', on_deck=[(4, 'chandlers_stall'), (8, 'ships_lantern'), (12, 'red_ensign'), (14, 'water_cask')], masts=[(9, 5)], hatch=None)
SUNK['raider_cutter'] = hulk('raider_cutter', 14, 2, 3, 'tarred_hull', 'raider_deck', inside=[(7, 'raider_strongbox')], on_deck=[(3, 'black_flag')], masts=[(7, 6)])
SUNK['iron_wreck'] = hulk('iron_wreck', 16, 1, 5, 'wreck_ironwork', 'wreck_planking', inside=[(5, 'wreck_chest'), (10, 'wreck_chest')])
SUNK['sea_stack'] = isle('sea_stack', 9, 8, 6, 'basalt_stack', 'basalt_stack', veins=[('coal_seam', 0.16), ('ironstone_seam', 0.14)], room=1)
SUNK['volcanic_vent'] = isle('volcanic_vent', 16, 3, 6, 'sulphur_crust', 'basalt_stack', 'obsidian_flow', veins=[('sulphur_crust', 0.12)], room=1)
SUNK['whale_carcass'] = isle('whale_carcass', 12, 3, 3, 'blubber_bank', 'blubber_bank', 'whale_rib', room=1)
SUNK['bird_cliff'] = isle('bird_cliff', 14, 7, 5, 'guano_rock', 'basalt_stack', veins=[('guano_rock', 0.25), ('coal_seam', 0.05)], room=1)
SUNK['galleon_wreck'] = hulk('galleon_wreck', 24, 3, 6, 'wreck_brasswork', 'wreck_planking', inside=[(6, 'captains_chest'), (12, 'captains_chest'), (17, 'wreck_ironwork'), (9, 'wreck_ironwork')], masts=[(7, 5), (15, 4)])
SUNK['collier_wreck'] = hulk('collier_wreck', 20, 2, 5, 'wreck_ironwork', 'coal_seam', inside=[(6, 'captains_chest'), (12, 'coal_seam'), (13, 'coal_seam')])
SUNK['merchant_hulk'] = hulk('merchant_hulk', 22, 2, 4, 'sheathed_hull', 'oak_deck', on_deck=[(5, 'company_factor'), (9, 'ships_lantern'), (15, 'blue_ensign'), (17, 'hold_locker')], masts=[(11, 6)], hatch=None, mast='made_mast')
SUNK['raider_brig'] = hulk('raider_brig', 22, 3, 5, 'tarred_hull', 'raider_deck', inside=[(8, 'raider_strongbox'), (14, 'raider_strongbox')], on_deck=[(4, 'black_flag')], masts=[(8, 5), (15, 5)])
SUNK['iceberg'] = isle('iceberg', 12, 6, 6, 'pack_snow', 'ice', room=1)
SUNK['great_berg'] = isle('great_berg', 26, 10, 9, 'pack_snow', 'ice', room=1)
SUNK['bear_floe'] = isle('bear_floe', 22, 1, 2, 'pack_snow', 'ice', things=[(11, 'frozen_stores')], room=2)
SUNK['frozen_ship'] = hulk('frozen_ship', 20, 2, 5, 'frozen_planking', 'pack_snow', inside=[(6, 'frozen_stores'), (10, 'frozen_stores'), (14, 'frozen_stores')], masts=[(10, 5)])
SUNK['whalers_cache'] = isle('whalers_cache', 16, 4, 5, 'pack_snow', 'ice', 'whale_rib', things=[(6, 'frozen_stores'), (10, 'frozen_stores')], room=2)
SUNK['drowned_ironclad'] = hulk('drowned_ironclad', 28, 1, 8, 'drowned_hull', 'drowned_hull', inside=[(8, 'drowned_safe'), (14, 'drowned_safe'), (20, 'drowned_safe')], on_deck=[(10, 'ghost_lamp')], hatch='steel_hatch')
SUNK['black_spire'] = isle('black_spire', 12, 9, 8, 'black_reef', 'basalt_stack', 'black_reef', veins=[('obsidian_flow', 0.15)], room=1)
SUNK['ghost_hulk'] = hulk('ghost_hulk', 22, 3, 6, 'drowned_hull', 'drowned_hull', inside=[(8, 'drowned_safe'), (13, 'drowned_safe')], on_deck=[(5, 'ghost_lamp'), (16, 'ghost_lamp')], hatch='steel_hatch')
SUNK['leviathan_boneyard'] = isle('leviathan_boneyard', 26, 6, 7, 'leviathan_bone', 'leviathan_bone', 'black_reef', veins=[('black_reef', 0.1)], room=1)
SUNK['black_market_hulk'] = hulk('black_market_hulk', 20, 2, 5, 'steel_hull', 'steel_deck', on_deck=[(5, 'black_market'), (9, 'ghost_lamp'), (14, 'ice_room')], hatch=None)
b = Built(2, 1, 1); b.put(0, 0, front='lashed_driftwood'); b.put(1, 0, front='lashed_driftwood'); b.done('castaways_raft')

# ---- the seas ------------------------------------------------------------------------------------------------------------------
def D(*pairs, **kw): return [dict({'item': i, 'weight': w}, **kw) for i, w in pairs]
def A(name, label, weight, cable=0, reach=0, stay=1500): return {'structure': name, 'label': label, 'weight': weight, 'cable': cable, 'reach': reach, 'stay': stay, 'sunk': SUNK[name]}
def bed(*rows): return [{'item': i, 'weight': w, 'depth': d} for i, w, d in rows]
BED1 = [('sea_shell', 8, 1), ('clam', 10, 1), ('crab_meat', 6, 1), ('kelp', 6, 1), ('small_fish', 8, 1)]
seas += [
    {'name': 'slick', 'label': 'The Slick', 'needs': {}, 'passage_ticks': 300, 'drift_per_1000_ticks': 26, 'sky': [150, 196, 226], 'water': [44, 112, 170, 120], 'swell': 0.5, 'alongside_every': 5000,
     'drift': D(('driftwood', 34), ('kelp', 14), ('tangled_rope', 12), ('torn_canvas', 12), ('old_cask', 8), ('glass_bottle', 4), ('coconut', 5), ('flotsam_crate', 6), ('sea_shell', 6), ('seed_pod', 1)) + D(('gull_feather', 3), when='day') + D(('clam', 4), when='night'),
     'bed': bed(*BED1), 'alongside': [A('drifting_wreckage', 'Drifting wreckage', 1, stay=900)]},
    {'name': 'shoals', 'label': 'The Shoals', 'needs': {'keel': 1, 'way': 1}, 'passage_ticks': 500, 'drift_per_1000_ticks': 24, 'sky': [130, 200, 230], 'water': [40, 150, 170, 110], 'swell': 0.7, 'alongside_every': 2600,
     'drift': D(('driftwood', 20), ('bamboo', 12), ('palm_frond', 10), ('coconut', 8), ('kelp', 8), ('sea_shell', 8), ('sponge', 5), ('copper_scrap', 5), ('seed_pod', 6), ('tangled_rope', 6), ('old_cask', 5), ('torn_canvas', 4), ('lead_scrap', 2)),
     'bed': bed(*BED1, ('sand', 8, 1), ('clay', 6, 2), ('sponge', 5, 2), ('coral_branch', 5, 2), ('abalone', 4, 2), ('pearl', 1, 2), ('tin_nodule', 2, 2)),
     'alongside': [A('sandbar', 'Sandbar', 6, stay=900), A('coral_head', 'Coral head', 4, stay=900), A('long_sandbar', 'Long sandbar', 4, 1), A('palm_islet', 'Palm islet', 5, 1), A('wooded_islet', 'Wooded islet', 5, 1), A('clay_islet', 'Clay bank', 4, 1), A('bird_rock', 'Bird rock', 3, 1), A('shoal_wreck', 'Wreck on the shoal', 3, 1)]},
    {'name': 'trade_wind', 'label': 'The Trade Wind', 'needs': {'keel': 2, 'way': 2, 'strength': 3, 'cable': 1, 'freeboard': 1}, 'passage_ticks': 900, 'drift_per_1000_ticks': 22, 'sky': [110, 170, 230], 'water': [30, 90, 180, 120], 'swell': 1.0, 'alongside_every': 2400,
     'drift': D(('driftwood', 14), ('lost_spar', 8), ('iron_scrap', 7), ('cloth_bale', 5), ('tar_barrel', 5), ('lamp_oil', 4), ('glass_float', 5), ('salt_cask', 4), ('castaway', 1), ('old_cask', 6), ('coconut', 4), ('copper_scrap', 4), ('seed_pod', 4), ('tangled_rope', 5)),
     'bed': bed(*BED1, ('tin_nodule', 8, 2), ('abalone', 5, 2), ('pearl', 2, 2), ('clay', 4, 2), ('iron_scrap', 4, 2), ('ironstone', 3, 3)),
     'alongside': [A('jungle_islet', 'Jungle islet', 5, 2), A('hemp_cay', 'Hemp cay', 4, 1), A('derelict_sloop', 'Derelict sloop', 5, 1), A('iron_wreck', 'Iron wreck', 4, 2), A('castaway_rock', "Castaway's rock", 2, 1, stay=900), A('floating_market', 'Floating market', 3, 1), A('raider_cutter', 'Raider cutter', 3, 0, stay=1200), A('palm_islet', 'Palm islet', 2, 1)]},
    {'name': 'roaring_sea', 'label': 'The Roaring Sea', 'needs': {'keel': 3, 'way': 3, 'strength': 4, 'cable': 2, 'freeboard': 3}, 'passage_ticks': 1200, 'drift_per_1000_ticks': 18, 'sky': [96, 110, 130], 'water': [26, 60, 110, 140], 'swell': 2.2, 'alongside_every': 2400,
     'drift': D(('great_timber', 10), ('whale_blubber', 8), ('brass_scrap', 6), ('iron_scrap', 8), ('ambergris', 1), ('lost_spar', 6), ('driftwood', 10), ('tar_barrel', 4), ('lamp_oil', 4), ('castaway', 1), ('salt_cask', 3), ('cloth_bale', 3)),
     'bed': bed(*BED1, ('tin_nodule', 6, 2), ('manganese_nodule', 8, 3), ('ironstone', 8, 3), ('coal', 5, 3), ('pearl', 2, 2), ('brass_scrap', 3, 3)),
     'alongside': [A('sea_stack', 'Sea stack', 5, 3), A('volcanic_vent', 'Volcanic vent', 4, 3), A('whale_carcass', 'Dead whale', 4, 2), A('bird_cliff', 'Bird cliffs', 3, 3), A('galleon_wreck', 'Galleon wreck', 3, 3, 1), A('collier_wreck', "Collier's wreck", 4, 3), A('merchant_hulk', 'Company hulk', 3, 2), A('raider_brig', 'Raider brig', 3, 0, stay=1500)]},
    {'name': 'ice', 'label': 'The Ice', 'needs': {'keel': 3, 'way': 3, 'strength': 5, 'cable': 2, 'freeboard': 3}, 'passage_ticks': 1500, 'drift_per_1000_ticks': 16, 'sky': [190, 206, 220], 'water': [50, 100, 130, 130], 'swell': 1.2, 'alongside_every': 2400,
     'drift': D(('ice_chunk', 24), ('frozen_fish', 10), ('whale_bone', 6), ('ration_tin', 4), ('great_timber', 6), ('whale_blubber', 6), ('driftwood', 6), ('coal', 2)),
     'bed': bed(*BED1, ('manganese_nodule', 8, 3), ('ironstone', 6, 3), ('whale_bone', 4, 2), ('pearl', 2, 2)),
     'alongside': [A('iceberg', 'Iceberg', 6, 2), A('great_berg', 'Great berg', 3, 3), A('bear_floe', 'Bear floe', 4, 2), A('frozen_ship', 'Frozen ship', 4, 3), A('whalers_cache', "Whalers' cache", 3, 3)]},
    {'name': 'black_water', 'label': 'The Black Water', 'needs': {'keel': 4, 'way': 5, 'strength': 6, 'cable': 3, 'freeboard': 5, 'light': 1, 'guns': 20}, 'passage_ticks': 2000, 'drift_per_1000_ticks': 14, 'sky': [16, 18, 30], 'water': [10, 16, 34, 170], 'swell': 1.6, 'dark': True, 'alongside_every': 2400,
     'drift': D(('drowned_plate', 16), ('engine_wreckage', 6), ('ghost_oil', 5), ('whale_bone', 5), ('great_timber', 5), ('coal', 6), ('brass_scrap', 6), ('castaway', 1)),
     'bed': bed(('manganese_nodule', 8, 3), ('coal', 6, 3), ('black_ore', 10, 4), ('abyssal_pearl', 3, 4), ('leviathan_scale', 2, 4), ('drowned_plate', 6, 3), ('light_gland', 4, 2)),
     'alongside': [A('drowned_ironclad', 'Drowned ironclad', 5, 4, 1), A('black_spire', 'Black spire', 5, 4), A('ghost_hulk', 'Ghost hulk', 4, 0, stay=1800), A('leviathan_boneyard', 'Leviathan boneyard', 3, 4, 2), A('black_market_hulk', 'Black market', 3, 3)]},
]

# ---- checks: names, and how many makings deep each keel is --------------------------------------------------------------------
names_i, names_t = {i['name'] for i in items}, {t['name'] for t in tiles}
assert len(names_i) == len(items), [n for n in names_i if sum(i['name'] == n for i in items) > 1]
assert len(names_t) == len(tiles), [n for n in names_t if sum(t['name'] == n for t in tiles) > 1]
for n in names_i | names_t | {m['name'] for m in mobs}: assert len(n) <= 31 - (6 if n in {m['name'] for m in mobs} else 0), n
for r in recipes:
    for n in [r['output']['item']] + [i['item'] for i in r['inputs']]: assert n in names_i, 'a recipe names %s' % n
for t in tiles:
    for n in ([t['drop']] if t.get('drop') else []) + [d['item'] for d in t.get('drops', [])] + [c_['item'] for c_ in (t.get('upgrade') or {}).get('cost', [])]: assert n in names_i, '%s gives %s' % (t['name'], n)
for s in seas:
    for d in s['drift'] + s['bed']: assert d['item'] in names_i, d
    raw(*[d['item'] for d in s['drift'] + s['bed']])
for t in tiles: raw(*[d['item'] for d in t.get('drops', [])])
for m in mobs: raw(*[d['item'] for d in m.get('drops', [])])
depth = {n: 0 for n in RAW}
for _ in range(40):
    for r in recipes:
        if r['output']['item'] in [i['item'] for i in r['inputs']]: continue
        if all(i['item'] in depth for i in r['inputs']):
            d = 1 + max([depth[i['item']] for i in r['inputs']] or [0])
            if d < depth.get(r['output']['item'], 99): depth[r['output']['item']] = d
given = {t['upgrade']['to'] for t in tiles if t.get('upgrade')} | {'shipwright'}   # (what a bench is raised to, and what a castaway begins with)
never = sorted(n for n in names_i if n not in depth and n not in given and n not in ('cowrie', 'silver_piece', 'gold_doubloon') and not any(t['name'] == n and (t.get('loot') or not t.get('removable', True) or 'drops' in t) for t in tiles))
assert not never, 'nothing makes or gives: %s' % never
KEEL_DEPTH = {lv: 1 + max(depth[i] for i, _ in KEEL_COST[lv]) for lv in KEEL_COST}
print('keels are', {KEELS[lv][0]: d for lv, d in KEEL_DEPTH.items()}, 'makings deep')
assert KEEL_DEPTH[2] >= 4 and KEEL_DEPTH[3] >= 6 and KEEL_DEPTH[4] >= 10 and KEEL_DEPTH[5] >= 15, KEEL_DEPTH

# ---- a book of quests: a tab a keel, each a tree ---------------------------------------------------------------------------------
LABEL = {i['name']: i['label'] for i in items}
def have(n, k=1): return {'have': n, 'count': k} if k > 1 else {'have': n}
def near(n): return {'near': n}
def kill(n, k=1): return {'kill': n, 'count': k}
BOOK = [  # tab, label, icon, [branch: (head label, text, task, icon, rewards, [children: (label, text, task, icon, rewards)])]
    ('raft', 'The raft', 'lashed_driftwood', [
        ('Adrift', 'Two logs and a hook. Haul in what goes by.', have('driftwood', 4), 'driftwood_hook', [('kelp', 4), ('rope', 2)], [('Rope', 'Untangle some at a ropewalk.', have('rope', 2), 'rope', [('driftwood', 4)]), ('Planks', 'Saw driftwood at a sawpit.', have('plank', 4), 'plank', [('nails', 4)]), ('A bigger raft', 'Lash more driftwood on.', have('lashed_driftwood', 4), 'lashed_driftwood', [('rope', 3)]), ('A net', 'Hang it in the water ahead of her.', near('drift_net'), 'drift_net', [('kelp_twine', 4)])]),
        ('A fire', 'A galley fire to cook at.', near('galley'), 'galley', [('small_fish', 3), ('driftwood', 4)], [('Supper', 'Cook a fish.', have('cooked_small_fish'), 'cooked_small_fish', [('sea_salt', 2)]), ('Water', 'A rain catcher fills by itself.', have('fresh_water', 2), 'fresh_water', [('coconut', 2)]), ('Soup', 'Kelp and water.', have('kelp_soup'), 'kelp_soup', [('kelp', 4)]), ('Pots', 'A lobster pot fishes for you.', near('lobster_pot'), 'lobster_pot', [('cask_stave', 4)])]),
        ('A workshop', "A carpenter's bench makes tools.", near('carpenters_bench'), 'carpenters_bench', [('sea_shell', 4), ('plank', 4)], [('An adze', 'Shell and driftwood.', have('shell_adze'), 'shell_adze', [('kelp_twine', 2)]), ('A mast', 'A pole to hang a sail on.', near('pole_mast'), 'pole_mast', [('rope', 2)]), ('A sail', 'Rags, but they draw.', near('rag_sail'), 'rag_sail', [('patched_canvas', 1)]), ('Somewhere to sleep', 'A hammock.', near('hammock'), 'hammock', [('bottle_lamp', 1)])]),
        ('A keel', "Lay a keel at the shipwright's bench: its upgrade.", near('shipwright_2'), 'keel_timber', [('plank', 8), ('nails', 6)], [('Keel timber', 'Planks, trenails and rope.', have('keel_timber', 2), 'keel_timber', [('trenail', 8)]), ('A steering oar', 'Where a course is set.', near('steering_oar'), 'steering_oar', [('rope', 2)]), ('Stowage', 'Somewhere to put it all.', near('stowage_crate'), 'stowage_crate', [('nails', 3)])])]),
    ('sailboat', 'The sailboat', 'canvas_sail', [
        ('The Shoals', 'Set a course at the helm.', have('sand', 1), 'tiller', [('copper_scrap', 3), ('seed_pod', 2)], [('Clay', 'Dig a bank that comes alongside.', have('clay', 4), 'clay', [('ballast_rock', 4)]), ('A kiln', 'Clay and rock.', near('kiln'), 'kiln', [('clay', 4)]), ('An anchor', 'A killick holds what comes alongside for longer.', near('killick'), 'killick', [('rope', 4)]), ('Soil aboard', 'A planter box.', near('planter_box'), 'planter_box', [('flax_seed', 4)])]),
        ('Copper', 'A forge, and scrap to melt.', near('forge'), 'forge', [('copper_scrap', 4), ('charcoal', 4)], [('Ingots', 'Melt copper scrap.', have('copper_ingot', 2), 'copper_ingot', [('copper_scrap', 3)]), ('Copper tools', 'A copper pick.', have('copper_pick'), 'copper_pick', [('copper_ingot', 2)]), ('A boathook', 'It reaches further.', have('boathook'), 'boathook', [('rope', 2)]), ('Pitch', "A caulker's kettle.", near('caulkers_kettle'), 'caulkers_kettle', [('charcoal', 4)])]),
        ('Canvas', 'Flax, grown and spun.', have('canvas', 2), 'canvas', [('flax_seed', 4), ('linen_thread', 2)], [('Flax', 'Sow it in a box and wait.', have('flax', 4), 'flax', [('fresh_water', 2)]), ('A real sail', 'Canvas on a yard.', near('canvas_sail'), 'canvas_sail', [('rope', 3)]), ('Plank hull', 'Better than logs.', have('plank_hull', 6), 'plank_hull', [('nails', 8)]), ('A sea chest', 'Twelve slots.', near('sea_chest'), 'sea_chest', [('nails', 4)])]),
        ('A caravel', 'Oak, copper and pitch: her keel.', near('shipwright_3'), 'caravel_keel', [('copper_sheet', 4), ('pitch', 2)], [('Seasoned oak', 'Oak dried at the kiln.', have('seasoned_beam', 2), 'seasoned_beam', [('acorn', 2)]), ('Tarred hull', 'It keeps the worm out.', have('tarred_hull', 6), 'tarred_hull', [('pitch', 2)]), ('The keel', 'Three makings on from seasoned oak.', have('caravel_keel'), 'caravel_keel', [('copper_bolt', 6)]), ('Trade', 'A desk to buy and sell at.', near('trade_desk'), 'trade_desk', [('cowrie', 40), ('sea_shell', 4)])])]),
    ('caravel', 'The caravel', 'ships_wheel', [
        ('The Trade Wind', 'Tarred, anchored and two-masted.', have('iron_scrap', 2), 'ships_wheel', [('iron_scrap', 4), ('tar_barrel', 1)], [('Below the waterline', 'Hull under water, pumped dry.', near('bilge_pump'), 'bilge_pump', [('copper_sheet', 2)]), ('A castaway', 'Ring the bell for a new hand.', have('castaway'), 'castaway', [('cooked_big_fish', 2), ('fresh_water', 2)]), ('A market', 'Buy at a floating market.', have('silver_piece'), 'silver_piece', [('cowrie', 60), ('cloth_bale', 1)]), ('Lateen', 'A sail that points higher.', near('lateen_sail'), 'lateen_sail', [('hawser', 1)])]),
        ('Iron', 'A bloomery: the forge, raised.', near('forge_2'), 'forge_2', [('iron_scrap', 6), ('charcoal', 6)], [('Bar iron', 'Bloom, wrought, bar.', have('iron_bar', 4), 'iron_bar', [('iron_scrap', 4)]), ('A grapnel', 'It holds an islet.', near('grapnel_anchor'), 'grapnel_anchor', [('hawser', 2)]), ('Iron tools', 'An iron adze.', have('iron_adze'), 'iron_adze', [('iron_bar', 2)]), ('A trawl', 'It drags the bed.', near('trawl_winch'), 'trawl_winch', [('hawser', 2)])]),
        ('Guns', 'A gun foundry.', near('gunfoundry'), 'gunfoundry', [('iron_bar', 4), ('tin_nodule', 4)], [('Tin', 'Nodules, from the trawl.', have('tin_ingot', 2), 'tin_ingot', [('tin_nodule', 3)]), ('A swivel', 'Her first gun.', near('swivel_gun'), 'swivel_gun', [('copper_ingot', 3)]), ('Raiders', 'They come for you.', kill('sea_raider', 3), 'raiders_mark', [('iron_bar', 4), ('silver_piece', 2)]), ('A cutlass', 'For when they board.', have('cutlass'), 'cutlass', [('iron_bar', 2)])]),
        ('A galleon', 'Ten makings from scrap iron to her keel.', near('shipwright_4'), 'galleon_keel', [('iron_strap', 6), ('heavy_canvas', 2)], [('Knees', 'Iron strap and compass timber.', have('hanging_knee', 2), 'hanging_knee', [('compass_timber', 2)]), ('Keelson', 'The spine of her.', have('keelson'), 'keelson', [('pitch', 4)]), ('The keel', 'Backbone, chain and hawser.', have('galleon_keel'), 'galleon_keel', [('chain_link', 4)]), ('Sheathing', 'Copper over tar.', have('sheathed_hull', 8), 'sheathed_hull', [('copper_sheet', 4)])])]),
    ('galleon', 'The galleon', 'course_sail', [
        ('The Roaring Sea', 'Sheathed, high-sided, and holding.', have('coal', 2), 'bower_anchor', [('coal', 6), ('great_timber', 2)], [('A bower', 'Three cables.', near('bower_anchor'), 'bower_anchor', [('chain_link', 4)]), ('A derrick', 'It sways the heavy things across.', near('derrick'), 'derrick', [('hawser', 2)]), ('A whale', 'Alive or dead.', have('whale_blubber', 4), 'whale_blubber', [('whale_oil', 2)]), ('Courses', 'The great sails.', near('course_sail'), 'course_sail', [('heavy_canvas', 2)])]),
        ('The Ice', 'Ironbound, with a stove.', have('ice_chunk', 4), 'ice_chunk', [('ration_tin', 3), ('whale_oil', 2)], [('Ironbound', 'Oak and iron.', have('ironbound_hull', 8), 'ironbound_hull', [('iron_strap', 4)]), ('Furs', 'A white bear.', kill('white_bear'), 'bear_fur', [('seal_skin', 2), ('pemmican', 2)]), ('Sealskin', 'Warm at last.', have('sealskin_coat'), 'sealskin_coat', [('seal_skin', 2)]), ('A tusk', 'A narwhal.', have('narwhal_tusk'), 'narwhal_tusk', [('whale_bone', 3)])]),
        ('The foundry', 'Bronze and cast iron of your own.', have('bronze_ingot', 4), 'bronze_ingot', [('tin_nodule', 4), ('copper_ingot', 4)], [('Long guns', 'Bronze.', near('long_gun'), 'long_gun', [('round_shot', 8)]), ('A diving bell', 'It fetches from three deep.', near('diving_bell'), 'diving_bell', [('lead_ingot', 2)]), ('Pig iron', 'Ironstone and coal.', have('pig_iron', 4), 'pig_iron', [('coal', 6)]), ('Powder', 'Saltpetre, sulphur, charcoal.', have('gunpowder', 2), 'gunpowder', [('saltpetre', 2), ('sulphur', 2)])]),
        ('A dreadnought', 'Fifteen makings from ironstone to her keel.', near('shipwright_5'), 'dreadnought_keel', [('steel_plate', 8), ('rivet', 16)], [('Steel', 'The converter: the forge, raised again.', near('forge_3'), 'forge_3', [('manganese_nodule', 4), ('coal', 8)]), ('Plate', 'A plate mill.', have('steel_plate', 8), 'steel_plate', [('steel_billet', 2)]), ('Bulkheads', 'Watertight.', have('watertight_bulkhead'), 'watertight_bulkhead', [('rivet', 16)]), ('The keel', 'Laid.', have('dreadnought_keel'), 'dreadnought_keel', [('bronze_ingot', 4)])])]),
    ('dreadnought', 'The dreadnought', 'gun_turret', [
        ('Steam', 'No masts.', near('steam_engine'), 'steam_engine', [('coal', 16), ('steam_pipe', 4)], [('A boiler', 'It wants coal.', near('scotch_boiler'), 'scotch_boiler', [('coal', 12)]), ('Triple expansion', 'A hundred of thrust.', near('triple_engine'), 'triple_engine', [('bearing', 4)]), ('Steel hull', 'Riveted plate.', have('steel_hull', 12), 'steel_hull', [('rivet', 24)]), ('Funnels', 'Two, for the look of her.', near('funnel'), 'funnel', [('steel_plate', 2)])]),
        ('Light', 'A dynamo and a searchlight.', near('searchlight'), 'searchlight', [('light_bulb', 2), ('copper_wire', 8)], [('A dynamo', 'Copper wire, a great deal.', near('dynamo'), 'dynamo', [('copper_wire', 8)]), ('Electric lamps', 'No more oil.', near('arc_lamp'), 'arc_lamp', [('light_bulb', 2)]), ('A steam crane', 'Two of reach.', near('steam_crane'), 'steam_crane', [('chain_link', 6)]), ('Chain cable', 'Four cables.', near('steam_windlass'), 'steam_windlass', [('chain_link', 8)])]),
        ('The battery', 'Turrets.', near('gun_turret'), 'gun_turret', [('gunpowder', 12), ('steel_plate', 6)], [('Breech guns', 'From the armoury.', near('breech_gun'), 'breech_gun', [('gunpowder', 8)]), ('A rifle', 'For the hands.', have('service_rifle'), 'service_rifle', [('gunpowder', 4)]), ('Sea serpent', 'It comes at night.', kill('sea_serpent'), 'leviathan_scale', [('black_alloy', 2), ('abyssal_pearl', 1)]), ('Torpedoes', 'For what guns will not stop.', near('torpedo_tube'), 'torpedo_tube', [('black_alloy', 2)])]),
        ('The Black Water', 'Steel, steam, a light and guns.', have('drowned_plate', 4), 'ghost_lamp', [('ghost_oil', 2), ('drowned_plate', 6)], [('Black ore', 'Only the bathysphere brings it up.', have('black_ore', 4), 'black_ore', [('abyssal_pearl', 1)]), ('A bathysphere', 'Four deep.', near('bathysphere'), 'bathysphere', [('lens', 2)]), ('Armour', 'Black alloy on steel.', have('armoured_hull', 12), 'armoured_hull', [('armour_plate', 4)]), ('Turbines', 'Two hundred and forty of thrust.', near('steam_turbine'), 'steam_turbine', [('black_alloy', 4)]), ('Leviathan', 'There is no end to the sea. There is this.', kill('leviathan'), 'leviathan_scale', [('leviathan_scale', 6), ('gold_doubloon', 5)]), ('The main battery', 'Eighty of guns.', near('main_battery'), 'main_battery', [('armour_plate', 4)])])]),
]
prev_tab_end = None
for order, (tab, tab_label, tab_icon, branches) in enumerate(BOOK):
    tabs.append({'name': 'hh_' + tab, 'label': tab_label, 'order': order, 'icon': tab_icon})
    root = 'hh.%s.start' % tab
    quests.append({'name': root, 'tab': 'hh_' + tab, 'label': tab_label, 'text': 'What she is, and what she wants next.', 'task': {'check': True}, 'rewards': [{'item': 'fresh_water', 'count': 2}, {'item': 'cowrie', 'count': 10 * (order + 1)}], 'icon': tab_icon, 'at': [0, 0], 'hub': True})
    for k, (lab, text, task, icon, rew, kids) in enumerate(branches):
        head = 'hh.%s.%d' % (tab, k)
        quests.append({'name': head, 'tab': 'hh_' + tab, 'label': lab, 'text': text, 'needs': [root], 'task': task, 'rewards': [{'item': i, 'count': n} for i, n in rew], 'at': [2 + 4 * k, 0], 'hub': True, 'pick': True, 'crystals': 5 * (order + 1), 'icon': icon})
        for j, (klab, ktext, ktask, kicon, krew) in enumerate(kids):
            quests.append({'name': '%s.%d' % (head, j), 'tab': 'hh_' + tab, 'label': klab, 'text': ktext, 'needs': [head], 'task': ktask, 'rewards': [{'item': i, 'count': n} for i, n in krew], 'at': [2 + 4 * k + j % 3, 1 + j // 3], 'icon': kicon})
for q in quests:
    for n in [r['item'] for r in q['rewards']] + [q['icon']] + [q['task'][k] for k in ('have',) if k in q['task']]: assert n in names_i, 'quest %s names %s' % (q['name'], n)
    if 'near' in q['task']: assert q['task']['near'] in names_t, q['task']
    if 'kill' in q['task']: assert q['task']['kill'] in {m['name'] for m in mobs}, q['task']

# ---- rules, the world, trade: the whole of a game -----------------------------------------------------------------------------------
rules = json.load(open(os.path.join(DATA, 'rules.json')))    # (the game's numbers - how fast a day goes, how a body hungers - are kept; what they name is not)
rules['player']['start_items'] = [{'item': 'shipwright', 'count': 1}, {'item': 'driftwood_hook', 'count': 1}, {'item': 'kelp', 'count': 3}, {'item': 'fresh_water', 'count': 2}]
rules['players']['max'] = 3
rules['questbook'] = True   # (the book of quests is part of this game, not a mod of it)
rules['meditation'] = {'ticks': rules['meditation']['ticks'], 'loot': [{'item': 'sea_shell', 'weight': 16}, {'item': 'kelp', 'weight': 8}, {'item': 'driftwood', 'weight': 8}, {'item': 'pearl', 'weight': 1}]}
rules['fog'] = {'enabled': False, 'radius': 20, 'fade': 8}
rules['water']['color'] = [44, 112, 170, 120]
base_gen = json.load(open(os.path.join(DATA, 'worldgen.json')))
worldgen = {'width': 768, 'height': 160, 'plains': {'level': 0, 'swells': [], 'gap': [50, 120]}, 'features': [],
            'layers': {'surface': 'island_grass', 'soil': 'sea_floor', 'rock': 'sea_floor', 'soil_depth': 4, 'soil_variation': 0.5, 'bare_at': 0.6},
            'spawn': {'column': [30, 50], 'first_feature_at': 80, 'flat_radius': 3, 'ease_radius': 24, 'portal': {'base': 'first_log', 'body': 'ships_bell', 'height': 1}},
            'sea_level': 0, 'climate': base_gen['climate'], 'biomes': [{'name': 'open_sea', 'label': 'Open sea', 'surface': 'island_grass', 'soil': 'island_soil', 'tree_one_in': 0}],
            'options': dict(base_gen['options'], sizes=[{'label': 'The sea', 'width': 768}]), 'tree_placement': base_gen['tree_placement'],
            'adrift': {'waterline': 64, 'start': 'castaways_raft', 'keel': {'long': 10, 'above': 4, 'below': 1}, 'drift_ticks': 8, 'ahead': 34},
            'seas': seas, 'structures': structures}
trade = {'default_price': 10, 'sell_share': 0.9, 'discover': True,
         'categories': [{'name': 'blocks', 'label': 'Hull and timber'}, {'name': 'nature', 'label': 'Stores', 'also': ['food']}, {'name': 'tools', 'label': 'Tools and arms'}, {'name': 'clothing', 'label': 'Slops'}, {'name': 'ores', 'label': 'Metal'},
                        {'name': 'crafted', 'label': 'Fittings', 'also': ['materials', 'building']}, {'name': 'benches', 'label': 'Benches', 'free_only': True}, {'name': 'creatures', 'label': 'Creatures', 'free_only': True, 'also': ['animals']}, {'name': 'special', 'label': 'Special', 'free_only': True}],
         'currencies': [{'item': 'cowrie', 'value': 1}, {'item': 'silver_piece', 'value': 100}, {'item': 'gold_doubloon', 'value': 10000}], 'crystals': {'start': 0, 'icon': 'ui/crystal.png'}, 'tier_prices': [0, 50, 150, 500, 1500, 5000]}

def out(name, obj):
    # (A definition to a line: the tools that add to these files - a cage for a creature, what a thing looks like worn - go by lines.)
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v)) for k, v in obj.items()) + '\n}\n')
json.dump({'label': 'Hull & Horizon', 'icon': 'icon.png', 'standalone': True, 'description': 'A game of its own: no land, a sea that goes by, and a ship built of blocks - raft, sailboat, caravel, galleon, dreadnought. Nothing of the base game but its engine.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
out('tiles.json', {'tiles': tiles}); out('items.json', {'items': items}); out('recipes.json', {'recipes': recipes}); out('trees.json', {'trees': trees})
out('worldgen.json', worldgen); out('rules.json', rules); out('trade.json', trade); out('mobs.json', {'spawn_radius': 50, 'cap': 10, 'water_cap': 10, 'mobs': mobs}); out('quests.json', {'tabs': tabs, 'quests': quests})
ic = Image.new('RGBA', (64, 64), c('2a6a9a')); px = ic.load()
for x in range(64):
    for y in range(40, 64): px[x, y] = c('1a4a7a')
    if 12 <= x <= 52:
        for y in range(34, 42): px[x, y] = c('8a5a30')
    if x in (31, 32):
        for y in range(8, 34): px[x, y] = c('6a4a2a')
    if 33 <= x <= 48:
        for y in range(10 + (x - 33) // 2, 30): px[x, y] = c('e8dcc0')
ic.save(os.path.join(mod, 'icon.png'))
print('%d blocks, %d items, %d recipes, %d creatures, %d structures, %d seas, %d quests' % (len(tiles), len(items), len(recipes), len(mobs), len(structures), len(seas), len(quests)))
# What is worn, a cage for every creature, a body for every creature: the tools every mod uses.
# (They go over the game and every mod, and worn_art.py does not draw another mod's pictures twice alike: whatever of
# anybody else's they change is put back as it was.)
import glob
kept = {f: open(f, 'rb').read() for f in glob.glob(os.path.join(repo, 'data', '**', '*'), recursive=True) + glob.glob(os.path.join(repo, 'mods', '**', '*'), recursive=True)
        if os.path.isfile(f) and os.sep + 'hull_and_horizon' + os.sep not in f and (f.endswith('.json') or os.sep + 'worn' + os.sep in f or os.sep + 'player' + os.sep in f)}
for tool in ('worn_art.py', 'caged_art.py', 'mob_models.py'): subprocess.run([sys.executable, os.path.join(repo, 'tools', tool), repo], check=True, stdout=subprocess.DEVNULL)
for f, was in kept.items():
    if open(f, 'rb').read() != was: open(f, 'wb').write(was)
