#!/usr/bin/env python3
"""Writes the Plunder & Thunder mod: its data files and its textures.

    python3 tools/mod_plunder_and_thunder.py .

A world that is sea with islands in it; towns on the islands; ships to build, sail, fight, board and take; cargo to
carry between harbours; treasure buried and maps to it; wrecks on the bottom. The plan and what the owner decided
are in ModDescriptions/PlunderAndThunder.txt.

Either edit this and run it again, or edit the mod's files by hand and stop using this: not both.
"""
import colorsys, copy, json, os, random, subprocess, sys, zlib
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
DATA = os.path.join(repo, 'data')
mod = os.path.join(repo, 'mods/plunder_and_thunder'); TEX = os.path.join(mod, 'textures')
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

# ==== Plunder & Thunder ================================================================================================================
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
BLOCKS = [('jungle_soil', 'Jungle soil', 'rock', '4a3a22', 2, {'tool': 'shovel', 'mine_time': 0.3}), ('volcanic_ash', 'Volcanic ash', 'rock', '4a4448', 3, {'tool': 'shovel', 'mine_time': 0.3}), ('pumice_rock', 'Sea pumice', 'rock', '8a8078', 3, {}),
          ('mangrove_mud', 'Mangrove mud', 'rock', '5a4a3a', 2, {'tool': 'shovel', 'mine_time': 0.3}), ('white_sand', 'White sand', 'rock', 'f0e8d0', 1, {'tool': 'shovel', 'mine_time': 0.2}), ('skull_rock', 'Bone rock', 'rock', 'c8c0b0', 3, {}),
          ('quay_stone', 'Quay stone', 'paving', '8a8a84', 2, {}), ('fort_stone', 'Fort stone', 'brick', '9a968a', 3, {'mine_time': 1.2}), ('harbour_brick', 'Harbour brick', 'brick', 'a8563a', 2, {}), ('whitewash', 'Whitewashed wall', 'paving', 'f0ece0', 2, {}),
          ('ship_planks', 'Ship planks', 'planks', '9a6a3a', 2, {'tool': 'axe', 'burns': True}), ('tarred_planks', 'Tarred planks', 'planks', '3a2e28', 2, {'tool': 'axe'}), ('pier_planks', 'Pier planks', 'planks', '8a7a5a', 2, {'tool': 'axe'}),
          ('palm_thatch', 'Palm thatch', 'roof', 'c8a850', 1, {'tool': 'axe', 'burns': True}), ('red_tile_roof', 'Red tile roof', 'roof', 'b8452a', 2, {}), ('carved_idol_stone', 'Carved stone', 'brick', '6a7a6a', 3, {'light': 3}),
          ('coral_block', 'Coral block', 'rock', 'f08a80', 2, {}), ('driftwood_block', 'Driftwood', 'planks', 'b0a890', 1, {'tool': 'axe', 'burns': True})]
COL = {}
for n, lab, kind, col, tier, kw in BLOCKS: COL[n] = c(col); block(block_tex(kind, n, c(col)), n, lab, tier, 0.04, **kw)
t = rock('jungle_grass', COL['jungle_soil']); t.r(0, 0, 15, 3, c('2a8a3a')); t.r(0, 0, 15, 0, c('60c050')); [t.p(x, 4, c('2a8a3a')) for x in (1, 4, 6, 9, 12, 14)]
block(t, 'jungle_grass', 'Jungle grass', 2, 0.04, tool='shovel', mine_time=0.3, drop='jungle_soil')
# Buried treasure: a mound under the sand, and a sea chest in it.
t = rock('treasure_mound', c('e8dca8')); t.r(4, 6, 11, 12, c('7a4a2a')); t.r(4, 6, 11, 7, c('c8a040')); t.r(7, 8, 8, 10, c('c8a040'))
t.save('tiles', 'treasure_mound'); tile('treasure_mound', 'Buried chest', True, tool='shovel', mine_time=1.6, drop=None, buried_treasure=True, drops=[{'item': 'sea_chest', 'count': 1}, {'item': 'gold_coin', 'count': 6}, {'item': 'treasure_map', 'count': 1, 'chance': 0.25}])
comp(chest_pic(c('7a4a2a'), c('c8a040')).outline(), 'sea_chest', 'Sea chest', 4, 4.0, 'special', max_stack=8)
q = P().r(2, 3, 13, 12, c('e2d0a4')); q.r(2, 3, 13, 3, c('c8b080')); q.line(4, 9, 7, 6, c('8a6a3a')); q.line(7, 6, 10, 8, c('8a6a3a')); q.line(9, 9, 11, 11, RED); q.line(11, 9, 9, 11, RED); q.outline().save('items', 'treasure_map')
item('treasure_map', 'Treasure map', 3, 'special', 2.0, max_stack=1, treasure_map=True)

# ---- cargo: what the islands make and want ------------------------------------------------------------------------------------------------
# name, label, drawn as, colour, price (copper)
CARGO = [('sugar_loaf', 'Sugar', 'sack', 'f0f0e8', 120), ('molasses', 'Molasses', 'barrel', '5a3018', 90), ('tobacco_bale', 'Tobacco', 'sack', '8a5a2a', 260), ('cotton_bale', 'Cotton bale', 'sack', 'e8e8f0', 180), ('indigo_cake', 'Indigo', 'sack', '2a3a9a', 420),
         ('spice_chest', 'Spice', 'barrel', 'c05020', 800), ('salt_fish_barrel', 'Salt fish', 'barrel', 'a8a090', 110), ('timber_load', 'Ship timber', 'barrel', '8a5a30', 150), ('cloth_bolt', 'Cloth', 'sack', 'c06080', 340), ('tools_crate', 'Tools', 'barrel', '7a7e8a', 500),
         ('powder_keg', 'Powder keg', 'barrel', '2a2a2a', 600), ('silver_bar', 'Silver bar', 'ingot', 'd8dce8', 2400), ('coffee_sack', 'Coffee', 'sack', '5a3a22', 300), ('pearl_string', 'Pearls', 'gem', 'f0f0ff', 3000), ('tortoiseshell', 'Tortoiseshell', 'plate', 'a06a2a', 900), ('ambergris', 'Ambergris', 'orb', 'c8c0a0', 5000)]
for n, lab, kind, col, price in CARGO:
    pic = sack(c(col)) if kind == 'sack' else barrel(c(col), c('5a5560'), RED if n == 'powder_keg' else None).outline() if kind == 'barrel' else ingot(c(col)) if kind == 'ingot' else gemstone(c(col)) if kind == 'gem' else trinket('plate', c(col)) if kind == 'plate' else orb(c(col), spark=False)
    pic.save('items', n); items.append({'name': n, 'label': lab, 'texture': 'items/%s.png' % n, 'category': 'cargo', 'price': price, 'tier': 2 if price < 300 else 3 if price < 700 else 4 if price < 2600 else 5, 'speciality': True, 'max_stack': 20})

# ---- crops, palms and what is made of them -------------------------------------------------------------------------------------------------
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
ISLE_SOIL = ['dirt', 'grass', 'compost', 'jungle_soil', 'jungle_grass', 'volcanic_ash', 'mangrove_mud']
CROPS = [('sugar_cane', 'Sugar cane', 2, 'cane', 'a8c860', 0.05), ('tobacco_leaf', 'Tobacco leaf', 2, 'leaf', '6a9a3a', 0), ('sea_island_cotton', 'Sea-island cotton', 2, 'boll', 'f0f0f8', 0), ('indigo_plant', 'Indigo', 3, 'leaf', '3a4ab0', 0),
         ('pepper_vine', 'Pepper vine', 3, 'vine', 'c03020', 0), ('cassava_root', 'Cassava', 1, 'bush', 'c8b080', 0.15), ('pineapple', 'Pineapple', 2, 'bush', 'e0b030', 0.2), ('yam', 'Yam', 1, 'vine', 'a06a4a', 0.15)]
for n, lab, tier, arch, col, food in CROPS:
    for i in range(4):
        plant(arch, c(col), i).save('tiles', '%s_%d' % (n, i))
        tile('%s_%d' % (n, i), lab, False, mine_time=0.1, tool='machete', drop=None, drops=[{'item': n, 'count': 1 if i < 3 else 3}], soil=ISLE_SOIL, grows_into='%s_%d' % (n, i + 1) if i < 3 else None, model='cross')
    plant(arch, c(col), 3).outline().save('items', n); item(n, lab, tier, 'nature', 0.15, sows=n + '_0', food=food or None)
near = lambda col: ramp('%02x%02x%02x' % dk(c(col), 0.62)[:3], col, '%02x%02x%02x' % lt(c(col), 1.12)[:3])
block(block_tex('planks', 'hardwood', c('6a3a22')), 'hardwood', 'Hardwood', 2, 0.04, tool='axe', burns=True)
TREES = [('key_lime_tree', 'Key lime tree', 'orange', '7a5a3a', '4a9a3a', 'key_lime', 'Key lime', 'a0d030', 0.06, 'wood'), ('plantain_palm', 'Plantain palm', 'coconut', '8a7a4a', '5aa040', 'plantain', 'Plantain', 'f0d040', 0.15, 'wood'),
         ('mahogany', 'Mahogany', 'maple', '5a2a1a', '2a6a2a', 'mahogany_seed', 'Mahogany seed', '8a5a3a', 0, 'hardwood'), ('teak', 'Teak', 'oak', '7a5a2a', '3a7a3a', 'teak_seed', 'Teak seed', 'a08040', 0, 'hardwood'),
         ('mangrove', 'Mangrove', 'cherry', '5a4a3a', '3a6a4a', 'mangrove_pod', 'Mangrove pod', '6a8a4a', 0, 'wood'), ('breadfruit_tree', 'Breadfruit tree', 'apple', '6a5a3a', '3a8a3a', 'breadfruit', 'Breadfruit', 'b0c050', 0.3, 'wood')]
for n, lab, shape, trunk, leaf, fruit, flab, fcol, food, wood in TREES:
    clone_tile('oak_log', n + '_log', lab + ' trunk', near(trunk), drop=wood); clone_tile('oak_leaves', n + '_leaves', lab + ' leaves', near(leaf))
    q = P().disc(7.5, 9, 3.6, c(fcol)); q.p(6, 7, WHITE); q.r(7, 3, 8, 5, c(trunk)); q.outline().save('items', fruit)
    item(fruit, flab, 2, 'nature', 0.15, plants=n, food=food or None, cures=True if fruit == 'key_lime' else None)
    tr = copy.deepcopy(BTR[shape]); tr.update(name=n, log=n + '_log', leaves=n + '_leaves', fruit=fruit, soil=ISLE_SOIL + ['sand', 'white_sand']); tr.pop('needs_water', None); trees.append(tr)

# ---- benches ---------------------------------------------------------------------------------------------------------------------------------
def bench_pic(kind, trim):
    t = P()
    if kind == 'shipwright': t.r(1, 10, 14, 11, OAK); t.r(2, 12, 3, 15, dk(OAK)); t.r(12, 12, 13, 15, dk(OAK)); t.line(3, 9, 12, 5, c('c8a070')); t.line(3, 8, 12, 4, c('a07840')); t.r(1, 10, 14, 10, trim); t.r(10, 7, 12, 9, c('7a7e8a'))
    elif kind == 'sawpit': t.r(1, 8, 14, 9, OAK); t.r(2, 10, 3, 15, dk(OAK)); t.r(12, 10, 13, 15, dk(OAK)); t.r(7, 2, 8, 14, c('c0c8d0')); [t.p(9, y, c('c0c8d0')) for y in range(3, 14, 2)]; t.r(6, 1, 9, 1, trim)
    elif kind == 'ropewalk': t.r(2, 6, 3, 15, OAK); t.r(12, 6, 13, 15, OAK); t.disc(3, 6, 2.2, trim); t.line(3, 6, 12, 7, ROPE); t.line(3, 7, 12, 8, dk(ROPE)); t.line(3, 8, 12, 9, ROPE)
    elif kind == 'sail_loft': t.r(2, 2, 3, 15, OAK); t.r(12, 2, 13, 15, OAK); t.r(2, 2, 13, 3, dk(OAK)); t.r(4, 4, 11, 11, CANVAS); t.line(4, 11, 11, 4, dk(CANVAS, 0.85)); t.r(4, 4, 11, 4, trim)
    elif kind == 'powder_mill': t.r(3, 8, 12, 15, c('5a5560')); t.disc(7.5, 6, 3.5, c('7a7e8a')); t.disc(7.5, 6, 1.5, c('2a2a2a')); t.r(3, 8, 12, 8, trim); t.p(11, 12, RED)
    elif kind == 'gun_foundry': t.r(1, 10, 9, 15, c('5a5560')); t.r(2, 11, 8, 13, c('f06020')); t.r(3, 11, 7, 11, c('ffd060')); t.r(10, 8, 15, 10, c('2a2a2a')); t.r(11, 11, 14, 12, OAK); t.r(1, 10, 9, 10, trim)
    elif kind == 'cooperage': t.r(4, 6, 11, 14, OAK); t.r(3, 8, 12, 12, OAK); t.r(3, 8, 12, 8, trim); t.r(3, 12, 12, 12, trim); t.r(6, 2, 9, 5, c('7a7e8a')); t.r(7, 5, 8, 7, dk(OAK))
    elif kind == 'still': t.r(4, 7, 9, 14, BRASS); t.r(5, 4, 8, 6, BRASS); t.line(8, 4, 13, 6, BRASS); t.r(12, 6, 13, 12, BRASS); t.r(11, 12, 14, 14, c('3a6a4a')); t.r(5, 14, 8, 15, c('f06020')); t.r(4, 7, 9, 7, trim)
    elif kind == 'sugar_mill': t.r(2, 10, 13, 15, c('8a8a84')); t.disc(5, 7, 3, c('7a7e8a')); t.disc(10, 7, 3, c('7a7e8a')); t.r(4, 7, 11, 7, dk(OAK)); t.r(2, 10, 13, 10, trim)
    elif kind == 'smokehouse': t.r(3, 5, 12, 15, c('5a3a22')); t.r(2, 3, 13, 5, c('3a2e28')); t.r(5, 8, 10, 14, c('2a1a12')); t.r(6, 9, 6, 12, c('c08050')); t.r(9, 9, 9, 12, c('c08050')); t.p(7, 1, WHITE).p(9, 0, WHITE); t.r(3, 5, 12, 5, trim)
    elif kind == 'chart_table': t.r(1, 9, 14, 10, OAK); t.r(2, 11, 3, 15, dk(OAK)); t.r(12, 11, 13, 15, dk(OAK)); t.r(3, 5, 11, 8, PAPER); t.line(4, 7, 7, 6, SEA); t.line(7, 6, 10, 7, SEA); t.p(9, 6, RED); t.r(12, 6, 13, 8, BRASS); t.r(1, 9, 14, 9, trim)
    elif kind == 'captains_desk': t.r(1, 8, 14, 10, c('6a2a1a')); t.r(2, 11, 4, 15, c('4a1a10')); t.r(11, 11, 13, 15, c('4a1a10')); t.r(4, 5, 8, 7, PAPER); t.line(10, 7, 12, 3, WHITE); t.r(1, 8, 14, 8, trim); t.r(11, 6, 12, 7, c('2a2a2a'))
    elif kind == 'ships_galley': t.r(2, 8, 13, 15, c('3a3a44')); t.r(3, 10, 12, 13, c('2a1a12')); t.r(5, 11, 10, 12, c('f07020')); t.r(4, 5, 11, 7, c('7a7e8a')); t.p(6, 3, WHITE).p(9, 2, WHITE); t.r(2, 8, 13, 8, trim)
    elif kind == 'tavern_bar': t.r(1, 8, 14, 15, c('6a3a22')); t.r(1, 8, 14, 9, c('8a5a30')); t.r(3, 4, 5, 7, c('3a6a4a')); t.r(7, 5, 8, 7, c('c8a040')); t.r(10, 4, 12, 7, c('7a4a2a')); t.r(1, 8, 14, 8, trim)
    return t
GRADE = [c('8a8a8a'), c('c8a040'), c('d0d8e0'), c('60d0f0'), c('f05030')]
BENCHES = [('shipwright', "Shipwright's yard", 5), ('sawpit', 'Sawpit', 1), ('ropewalk', 'Ropewalk', 1), ('sail_loft', 'Sail loft', 1), ('powder_mill', 'Powder mill', 1), ('gun_foundry', 'Gun foundry', 3), ('cooperage', 'Cooperage', 1), ('still', 'Still', 1),
           ('sugar_mill', 'Sugar mill', 1), ('smokehouse', 'Smokehouse', 1), ('chart_table', 'Chart table', 1), ('captains_desk', "Captain's desk", 1), ('ships_galley', "Ship's galley", 1), ('tavern_bar', 'Tavern bar', 1)]
UPG = {2: [('keel_timber', 4), ('gold_coin', 20)], 3: [('ship_frame', 6), ('gold_coin', 60)], 4: [('copper_sheathing', 8), ('platinum_coin', 2)], 5: [('great_keel', 2), ('platinum_coin', 8)]}
for kind, lab, grades in BENCHES:
    for g in range(1, grades + 1):
        n = kind if g == 1 else '%s_%d' % (kind, g)
        bench_pic(kind, GRADE[g - 1]).save('tiles', n); icon_of(n)
        fuel = kind in ('gun_foundry', 'still', 'smokehouse', 'ships_galley')
        tile(n, lab, False, tool='axe', mine_time=0.4, bench=True, bench_kind=kind, bench_level=g, fuel_capacity=3000 if fuel else None,
             upgrade={'to': '%s_%d' % (kind, g + 1), 'cost': [{'item': i, 'count': k} for i, k in (UPG[g + 1] if kind == 'shipwright' else [('bronze_ingot', 6 * g), ('iron_ingot', 4 * g)])]} if g < grades else None)
        item(n, lab + (' ' + NUM[g] if grades > 1 else ''), min(6, g + 1), 'benches', 3.0, places=n)
for n, ins, lvl in (('shipwright', [('wood', 20), ('iron_ingot', 4), ('string', 8)], 2), ('sawpit', [('wood', 10), ('iron_ingot', 3)], 1), ('ropewalk', [('wood', 8), ('string', 10)], 1), ('sail_loft', [('wood', 8), ('linen', 6)], 2),
                    ('powder_mill', [('stone', 16), ('iron_ingot', 4)], 2), ('gun_foundry', [('stone', 20), ('clay', 10), ('iron_ingot', 6)], 3), ('cooperage', [('wood', 12), ('iron_ingot', 2)], 1), ('still', [('copper_ingot', 8), ('clay', 6)], 2),
                    ('sugar_mill', [('stone', 12), ('iron_ingot', 4)], 2), ('smokehouse', [('wood', 12), ('clay', 6)], 1), ('chart_table', [('wood', 8), ('linen', 4), ('glass', 2)], 2), ('captains_desk', [('wood', 10), ('linen', 4), ('gold_ingot', 1)], 2),
                    ('ships_galley', [('iron_ingot', 6), ('clay', 6)], 2), ('tavern_bar', [('wood', 16), ('glass', 4), ('bronze_ingot', 2)], 2)):
    rec('workbench', n, ins, level=lvl)

# ---- what a ship is made of: a long making ---------------------------------------------------------------------------------------------------
MAT = [('seasoned_timber', 'Seasoned timber', 2, lambda: ingot(c('a07840'))), ('sawn_planking', 'Sawn planking', 2, lambda: trinket('plate', c('b08850'))), ('pitch', 'Pitch', 2, lambda: barrel(TAR).outline()), ('oakum', 'Oakum', 2, lambda: trinket('thread', c('8a7a5a'))),
       ('tarred_rope', 'Tarred rope', 2, lambda: trinket('thread', c('4a3a2a'))), ('cordage', 'Cordage', 2, lambda: trinket('thread', ROPE)), ('sailcloth', 'Sailcloth', 2, lambda: trinket('cloth', CANVAS)), ('treenails', 'Treenails', 2, lambda: trinket('needle', c('b08850'), OAK)),
       ('iron_fittings', 'Iron fittings', 3, lambda: trinket('ring', c('7a7e8a'), c('5a5560'))), ('ship_frame', 'Ship frame', 3, lambda: P().line(3, 3, 5, 13, OAK).line(12, 3, 10, 13, OAK).r(5, 13, 10, 13, OAK).r(4, 7, 11, 7, dk(OAK)).outline()),
       ('keel_timber', 'Keel timber', 3, lambda: ingot(c('6a4a22'))), ('copper_sheathing', 'Copper sheathing', 4, lambda: trinket('plate', c('c87840'))), ('great_keel', 'Great keel', 5, lambda: ingot(c('4a2a12'))),
       ('gunpowder', 'Gunpowder', 3, lambda: trinket('dust', c('2a2a2a'))), ('nitre', 'Nitre', 2, lambda: trinket('dust', c('e8e8e0'))), ('brimstone', 'Brimstone', 3, lambda: trinket('dust', c('e0d040'))),
       ('round_shot', 'Round shot', 3, lambda: orb(c('3a3a44'), c('5a5a66'), False)), ('chain_shot', 'Chain shot', 3, lambda: P().disc(4, 8, 2.5, c('3a3a44')).disc(11, 8, 2.5, c('3a3a44')).line(6, 8, 9, 8, c('8a8e9a')).outline()),
       ('grape_shot', 'Grape shot', 3, lambda: P().disc(5, 6, 1.8, c('3a3a44')).disc(9, 6, 1.8, c('3a3a44')).disc(7, 10, 1.8, c('3a3a44')).disc(11, 10, 1.8, c('3a3a44')).outline())]
for n, lab, tier, pic in MAT: comp(pic(), n, lab, tier, 0.6)
for n in ('nitre', 'brimstone'):
    rock(n + '_ore', c('8a8a8a') if n == 'nitre' else COL['volcanic_ash'], vein=c('e8e8e0') if n == 'nitre' else c('e0d040')).save('tiles', n + '_ore'); tile(n + '_ore', next(m[1] for m in MAT if m[0] == n) + ' deposit', ore=True, mine_time=0.9, drop=n)
make('sawpit', 'seasoned_timber', [('wood', 4)], 1, 2, 240); make('sawpit', 'seasoned_timber', [('hardwood', 2)], 1, 2, 160); make('sawpit', 'sawn_planking', [('seasoned_timber', 2)], 1, 3, 120); make('sawpit', 'treenails', [('seasoned_timber', 1)], 1, 8, 80)
make('smokehouse', 'pitch', [('raw_resin', 3), ('charcoal', 1)], 1, 2, 160); make('smokehouse', 'pitch', [('pine_cone', 6), ('charcoal', 1)], 1, 1, 160); make('ropewalk', 'cordage', [('string', 4)], 1, 2, 80); make('ropewalk', 'cordage', [('sea_island_cotton', 3)], 1, 2, 80)
make('ropewalk', 'oakum', [('cordage', 1), ('pitch', 1)], 1, 4, 80); make('ropewalk', 'tarred_rope', [('cordage', 2), ('pitch', 1)], 1, 2, 120); make('sail_loft', 'sailcloth', [('linen', 3), ('cordage', 1)], 1, 2, 160); make('sail_loft', 'sailcloth', [('cotton_bale', 1), ('cordage', 1)], 1, 3, 160)
make('gun_foundry', 'iron_fittings', [('iron_ingot', 2), ('charcoal', 1)], 1, 4, 120); make('shipwright', 'ship_frame', [('sawn_planking', 4), ('treenails', 8), ('iron_fittings', 1)], 1, 1, 240); make('shipwright', 'keel_timber', [('seasoned_timber', 6), ('pitch', 2), ('iron_fittings', 2)], 1, 1, 300)
make('gun_foundry', 'copper_sheathing', [('copper_ingot', 3), ('pitch', 1)], 2, 2, 200); make('shipwright', 'great_keel', [('keel_timber', 4), ('copper_sheathing', 4), ('hardwood', 12)], 4, 1, 600)
make('powder_mill', 'gunpowder', [('nitre', 3), ('brimstone', 1), ('charcoal', 2)], 1, 4, 160); make('gun_foundry', 'round_shot', [('iron_ingot', 1)], 1, 6, 80); make('gun_foundry', 'chain_shot', [('round_shot', 2), ('iron_fittings', 1)], 1, 2, 100); make('gun_foundry', 'grape_shot', [('round_shot', 1), ('sailcloth', 1)], 1, 2, 100)
make('sawpit', 'ship_planks', [('sawn_planking', 1)], 1, 4, 60); make('sawpit', 'tarred_planks', [('sawn_planking', 1), ('pitch', 1)], 1, 4, 60); make('sawpit', 'pier_planks', [('seasoned_timber', 1)], 1, 4, 60); make('cooperage', 'powder_keg', [('gunpowder', 6), ('sawn_planking', 1)], 1, 1, 120)
# Guns, each a bigger casting than the last.
GUNS = [('swivel_gun', 'Swivel gun', 3, 1, [('bronze_ingot', 4), ('iron_fittings', 1)]), ('saker', 'Saker', 4, 1, [('swivel_gun', 1), ('iron_ingot', 8), ('seasoned_timber', 2)]), ('long_nine', 'Long nine', 5, 2, [('saker', 1), ('steel_ingot', 8), ('iron_fittings', 4)]),
        ('carronade', 'Carronade', 6, 3, [('long_nine', 1), ('steel_ingot', 12), ('copper_sheathing', 2)])]
for n, lab, tier, g, ins in GUNS:
    comp(P().r(3, 6, 13, 8, c('2a2a2a')).r(2, 5, 4, 9, c('2a2a2a')).r(13, 6, 14, 8, c('5a5a66')).r(5, 9, 10, 10, OAK).disc(6, 11, 1.6, dk(OAK)).disc(10, 11, 1.6, dk(OAK)).outline(), n, lab, tier, 2.0); make('gun_foundry', n, ins, g, 1, 400)
# The parts of a hull, by the size of ship; each size has the one below in it.
SIZES = [('boat', 'Boat', 2, 1, None), ('sloop', 'Sloop', 3, 1, 'boat'), ('brig', 'Brig', 4, 2, 'sloop'), ('frigate', 'Frigate', 5, 3, 'brig'), ('great', 'Great', 6, 5, 'frigate')]
for k, (s_, lab, tier, g, below) in enumerate(SIZES):
    q = lambda col: P().r(2, 7, 13, 11, col).r(3, 12, 12, 12, dk(col)).r(2, 7, 13, 7, lt(col)).p(13, 6, col).p(14, 5, col).outline()
    comp(q(OAK), s_ + '_hull', lab + ' hull', tier, 3.0); comp(P().r(7, 1, 8, 14, c('6a4a22')).r(3, 4, 12, 4, c('6a4a22')).r(5, 14, 10, 14, dk(OAK)).outline(), s_ + '_mast', lab + ' mast', tier, 1.5)
    comp(trinket('cloth', CANVAS).line(4, 12, 11, 4, ROPE).outline(), s_ + '_rig', lab + ' rig', tier, 2.0)
    n = k + 1
    make('shipwright', s_ + '_hull', [('ship_frame', 2 * n), ('keel_timber' if k < 4 else 'great_keel', 1 if k < 4 else 2), ('sawn_planking', 6 * n), ('oakum', 4 * n)] + ([(below + '_hull', 1)] if below else []) + ([('copper_sheathing', 4 * (k - 1))] if k >= 2 else []), g, 1, 600)
    make('shipwright', s_ + '_mast', [('seasoned_timber', 3 * n), ('iron_fittings', n), ('tarred_rope', 2 * n)] + ([(below + '_mast', 1)] if below else []), g, 1, 300)
    make('sail_loft', s_ + '_rig', [('sailcloth', 3 * n), ('tarred_rope', 2 * n), ('cordage', 2 * n)] + ([(below + '_rig', 1)] if below else []), g, 1, 300)

# ---- ships ---------------------------------------------------------------------------------------------------------------------------------------
# name, label, how many blocks across it is drawn, hull, sail, masts, gun ports, health, speed, guns' damage and range, tier, what it is built of, the plan
WRECK_LOOT = lambda rich: [drop('sawn_planking', 2, 6), drop('gold_coin', rich, rich * 4), {'one_of': [{'item': 'round_shot', 'weight': 4}, {'item': 'gunpowder', 'weight': 3}, {'item': 'sailcloth', 'weight': 3}, {'item': 'treasure_map', 'weight': 1}, {'item': 'silver_bar', 'weight': 1}, {'item': 'rum_bottle', 'weight': 3}], 'min': 1, 'max': 3}]
chest_pic(c('5a4a3a'), c('3a6a5a')).r(2, 12, 13, 14, c('3a5a4a')).save('tiles', 'ship_wreckage'); icon_of('ship_wreckage')
tile('ship_wreckage', 'Wreckage', False, tool='axe', mine_time=0.6, storage={'columns': 4, 'rows': 2}, loot=WRECK_LOOT(2), model='box'); item('ship_wreckage', 'Wreckage', 3, 'crafted', 1.0, places='ship_wreckage', container={'slots': 8})
SHIPS = [('raft', 'Raft', 2, '9a7a4a', 'e8dcc0', 1, 0, 40, 1.3, 0, 0, 1, [('wood', 12), ('string', 6)], None, 1),
         ('jolly_boat', 'Jolly boat', 2, '8a5a30', 'e8dcc0', 1, 0, 80, 1.7, 0, 0, 2, [('boat_hull', 1), ('boat_mast', 1), ('boat_rig', 1)], None, 1),
         ('war_canoe', 'Canoe', 2, '6a3a1a', 'c05030', 0, 0, 60, 2.1, 0, 0, 2, [('boat_hull', 1), ('hardwood', 6), ('pitch', 2)], 'canoe_plan', 1),
         ('sloop', 'Sloop', 3, '8a5a30', 'f0e8d0', 1, 2, 220, 2.2, 14, 9, 3, [('sloop_hull', 1), ('sloop_mast', 1), ('sloop_rig', 1), ('swivel_gun', 4)], 'sloop_plan', 1),
         ('schooner', 'Schooner', 3, '5a3a22', 'f0e8d0', 2, 3, 300, 2.5, 20, 10, 4, [('sloop_hull', 2), ('sloop_mast', 2), ('sloop_rig', 2), ('saker', 4), ('copper_sheathing', 4)], 'schooner_plan', 2),
         ('brig', 'Brig', 4, '7a4a22', 'f0e8d0', 2, 5, 520, 2.0, 34, 11, 4, [('brig_hull', 1), ('brig_mast', 2), ('brig_rig', 2), ('saker', 8)], 'brig_plan', 2),
         ('merchantman', 'Merchantman', 4, '9a6a3a', 'e0d8c0', 3, 3, 700, 1.6, 18, 9, 4, [('brig_hull', 2), ('brig_mast', 3), ('brig_rig', 3), ('saker', 4), ('cargo_hold', 2)], 'merchantman_plan', 2),
         ('frigate', 'Frigate', 5, '3a3a44', 'f0f0e8', 3, 8, 1100, 2.4, 60, 13, 5, [('frigate_hull', 1), ('frigate_mast', 3), ('frigate_rig', 3), ('long_nine', 12)], 'frigate_plan', 3),
         ('man_of_war', 'Man-of-war', 6, '2a2a34', 'f0f0e8', 3, 12, 2400, 1.9, 120, 15, 6, [('great_hull', 1), ('great_mast', 3), ('great_rig', 3), ('carronade', 20), ('long_nine', 20)], 'man_of_war_plan', 5)]
def ship_mob(name, label, n, hull, sail, flag, masts, ports, health, tatter=False, oars=False, trim=None, **kw):
    for f in (0, 1): ship_pic(n, c(hull), c(sail), c(flag), masts, ports, c(trim) if trim else None, f, tatter, oars).save('mobs', name + ('_sail' if f else ''))
    m = {'name': name, 'label': label, 'texture': 'mobs/%s.png' % name, 'walk': ['mobs/%s_sail.png' % name, 'mobs/%s.png' % name], 'step_ticks': 3, 'move_chance': 0.9, 'turn_chance': 0.03, 'health': health, 'cap': 2, 'size': [n, n], 'swim': {}}
    m.update(kw); mobs.append(clean(m)); return m
comp(chest_pic(c('8a5a30'), c('7a7e8a')).outline(), 'cargo_hold', 'Cargo hold', 4, 2.0, max_stack=1, container={'slots': 16, 'open_in_inventory': True}); make('cooperage', 'cargo_hold', [('sawn_planking', 8), ('iron_fittings', 4), ('tarred_rope', 2)], 1, 1, 300)
for n, lab, size, hull, sail, masts, ports, hp, speed, dmg, rng_, tier, ins, plan, g in SHIPS:
    ship_pic(1, c(hull), c(sail), RED, 1 if masts else 0, 0, oars=n in ('raft', 'war_canoe')).save('items', n) if False else None
    q = Big(1); src = ship_pic(size, c(hull), c(sail), c('2a2a2a') if n != 'raft' else c(sail), masts, ports, oars=n in ('raft', 'war_canoe')); q.im = src.im.resize((16, 16), Image.NEAREST); q.im.resize((48, 48), Image.NEAREST).save(os.path.join(TEX, 'items', n + '.png'))
    item(n, lab, tier, 'special', 6.0, max_stack=1)
    ship_mob(n + '_afloat', lab, size, hull, sail, '2a2a2a' if n != 'raft' else sail, masts, ports, hp, oars=n in ('raft', 'war_canoe'), mount={'speed': speed}, ship=clean({'guns': dmg or None, 'range': rng_ or 6, 'wreck': 'ship_wreckage' if size >= 3 else None}),
             companion={'bite': dmg, 'sight': rng_}, tame=9, caged=n, drops=[drop('sawn_planking', 1, 3)])
    make('shipwright', n, ins, g, 1, 900, plan)
    if plan:
        comp(P().r(2, 3, 13, 12, c('c8d8e8')).r(2, 3, 13, 3, WHITE).line(4, 9, 11, 9, c('2a4a8a')).line(5, 9, 7, 5, c('2a4a8a')).line(7, 5, 7, 9, c('2a4a8a')).outline(), plan, 'Plan: ' + lab.lower(), tier, 3.0, 'special', max_stack=1)

# ---- weapons, tools of the trade, and what is worn ----------------------------------------------------------------------------------------------
for metal, mlab, col, tier, k in (('iron', 'Iron', '8a8e9a', 3, 1.0), ('steel', 'Steel', 'c0c8d0', 4, 1.6), ('gold', 'Gilded', 'f0c840', 5, 2.4)):
    for kind, lab, dmg, rng_, ins in (('cutlass', 'cutlass', 12, 0, [(metal + '_ingot', 3), ('seasoned_timber', 1)]), ('axe', 'boarding axe', 16, 0, [(metal + '_ingot', 4), ('seasoned_timber', 2)]), ('pike', 'boarding pike', 10, 2, [(metal + '_ingot', 2), ('seasoned_timber', 3)])):
        nm = '%s_%s' % (metal, {'cutlass': 'cutlass', 'axe': 'boarding_axe', 'pike': 'boarding_pike'}[kind])
        thing(weapon(kind, c(col)), nm, '%s %s' % (mlab, lab), tier, 'tools', 1.2, max_stack=1, damage=int(dmg * k), range=rng_ or None); make('gun_foundry', nm, ins, 1, 1, 200)
for n, lab, kind, tier, dmg, rng_, ins, g in (('flintlock_pistol', 'Flintlock pistol', 'pistol', 4, 30, 8, [('iron_fittings', 2), ('seasoned_timber', 1), ('flint', 1), ('gunpowder', 2)], 1), ('musket', 'Musket', 'musket', 5, 48, 13, [('flintlock_pistol', 1), ('steel_ingot', 3), ('seasoned_timber', 2)], 2),
                                         ('blunderbuss', 'Blunderbuss', 'blunderbuss', 5, 70, 5, [('flintlock_pistol', 1), ('bronze_ingot', 4), ('grape_shot', 2)], 2), ('brace_of_pistols', 'Brace of pistols', 'pistol', 6, 62, 9, [('flintlock_pistol', 2), ('gold_ingot', 1), ('tarred_rope', 1)], 3)):
    thing(weapon(kind, c('5a5a66')), n, lab, tier, 'tools', 2.0, max_stack=1, damage=dmg, range=rng_); make('gun_foundry', n, ins, g, 1, 300)
thing(weapon('grenado', INK), 'grenado', 'Grenado', 4, 'tools', 0.6, max_stack=10, uses=1, range=8, cast={'damage': 40, 'splash': 2}); make('powder_mill', 'grenado', [('gunpowder', 2), ('iron_fittings', 1), ('cordage', 1)], 1, 2, 120)
thing(weapon('hook', c('8a8e9a')), 'grappling_hook', 'Grappling hook', 3, 'tools', 1.5, max_stack=1, boards=True); make('gun_foundry', 'grappling_hook', [('iron_ingot', 3), ('tarred_rope', 4)], 1, 1, 200)
thing(weapon('spyglass', c('2a2a2a')), 'spyglass', 'Spyglass', 4, 'special', 2.0, max_stack=1, relic=True, boons={'sight': 1.0}); make('chart_table', 'spyglass', [('bronze_ingot', 2), ('glass', 2)], 1, 1, 240)
for metal, mlab, col, tier, sp in (('iron', 'Iron', '8a8e9a', 3, 6.0), ('steel', 'Steel', 'c0c8d0', 4, 9.0)):
    thing(weapon('shovel', c(col)), metal + '_spade', mlab + ' spade', tier, 'tools', 1.0, max_stack=1, tool={'class': 'shovel', 'speed': sp}); make('gun_foundry', metal + '_spade', [(metal + '_ingot', 2), ('seasoned_timber', 2)], 1, 1, 160)
# Clothes: each power's, and a diver's.
OUTFITS = [('buccaneer', "Buccaneer's", 'a02828', 3, ('bandana', 'bandana'), {'strength': 1}, [{'walk': 1.1}, {'strength': 3, 'luck': 0.1}]), ('captain', "Captain's", '2a2a4a', 5, ('tricorn', 'tricorn'), {'luck': 0.05}, [{'craft': 1.2}, {'strength': 2, 'luck': 0.2, 'regen': 1.5}]),
           ('navy', 'Navy', '2a4a9a', 4, ('tricorn', 'hat'), {'armor': 0.04}, [{'armor': 0.1}, {'armor': 0.2, 'stamina': 1.4}]), ('company', 'Company', '6a4a2a', 4, ('tricorn', 'hat'), {'craft': 1.05}, [{'craft': 1.2}, {'luck': 0.25, 'magnet': 1.0}]),
           ('islander', "Islander's", 'c08030', 3, ('bandana', 'headband'), {'walk': 1.04}, [{'lung': 2.0}, {'lung': 4.0, 'walk': 1.25}]), ('diver', "Diver's", 'b08850', 5, ('helm', 'helmet'), {'lung': 2.2}, [{'lung': 2.0}, {'lung': 4.0, 'sight': 1.0}])]
for n, lab, col, tier, (hk, hlab), boon, bonus in OUTFITS:
    for slot, kind, plab in (('head', hk, hlab), ('chest', 'coat', 'coat'), ('legs', 'breeches', 'breeches'), ('boots', 'boots', 'boots')):
        nm = '%s_%s' % (n, plab); hatpic(kind, c(col)).save('items', nm)
        item(nm, '%s %s' % (lab, plab), tier, 'clothing', 1.0, equip=slot, max_stack=1, armor=round(0.02 * tier, 2), warmth=0.3, set=n + '_outfit', boons=boon)
        make('sail_loft', nm, [('sailcloth', 2 if slot != 'chest' else 4), ('cordage', 1)] + ([('glass', 2), ('bronze_ingot', 2)] if n == 'diver' and slot == 'head' else [('leather', 1)] if slot == 'boots' else []), 1, 1, 240)
    sets.append({'name': n + '_outfit', 'label': lab + ' rig', 'bonuses': [{'pieces': 2, 'boons': bonus[0]}, {'pieces': 4, 'boons': bonus[1]}]})
hatpic('patch', INK).save('items', 'eyepatch'); item('eyepatch', 'Eyepatch', 2, 'special', 2.0, max_stack=1, relic=True, boons={'sight': 1.0}); make('sail_loft', 'eyepatch', [('leather', 1), ('cordage', 1)], 1, 1, 80)
# Figureheads and flags: carried, each does a little.
for n, lab, col, boons in (('mermaid_figurehead', 'Mermaid figurehead', '60c0a0', {'regen': 1.5}), ('lion_figurehead', 'Lion figurehead', 'e0b030', {'strength': 2}), ('saint_figurehead', 'Saint figurehead', 'f0f0f0', {'ward': 0.15, 'armor': 0.05}), ('serpent_figurehead', 'Serpent figurehead', '3a8a4a', {'walk': 1.15}),
                           ('black_flag', 'Black flag', '1a1a1a', {'strength': 1, 'luck': 0.05}), ('kings_colours', "King's colours", '2a4a9a', {'armor': 0.08}), ('company_ensign', 'Company ensign', 'a02020', {'craft': 1.15}), ('shell_pennant', 'Shell pennant', 'f0d0a0', {'lung': 2.0})):
    thing(trinket('seal', c(col), OAK) if 'figurehead' in n else flag(c(col), OAK, WHITE), n, lab, 4, 'special', 2.0, max_stack=1, relic=True, boons=boons, set='ships_luck')
    make('shipwright' if 'figurehead' in n else 'sail_loft', n, [('hardwood', 4), ('gold_ingot', 1)] if 'figurehead' in n else [('sailcloth', 2), ('seasoned_timber', 1)], 1, 1, 300)
sets.append({'name': 'ships_luck', 'label': "Ship's luck", 'bonuses': [{'pieces': 2, 'boons': {'luck': 0.1}}, {'pieces': 4, 'boons': {'luck': 0.25, 'walk': 1.2}}]})

# ---- victuals and rum --------------------------------------------------------------------------------------------------------------------------
def meal(name, label, tier, col, ins, bench='ships_galley', cupped=False, **kw): thing(cup(c(col)) if cupped else bowl(c(col)), name, label, tier, 'nature', 1.0, cooked=True, **kw); rec(bench, name, ins, time=120)
MEALS = [('hardtack', 'Hardtack', 1, 'd8c8a0', [('flour', 2), ('salt', 1)], {'food': 0.3}), ('salt_pork', 'Salt pork', 2, 'c07a6a', [('dodo_meat', 2), ('salt', 2)], {'food': 0.5}), ('lime_water', 'Lime water', 2, 'c0e060', [('key_lime', 2)], {'cupped': True, 'cures': True, 'effect': lasts(6000, regen=2.0)}),
         ('fish_stew', 'Fish stew', 2, 'c0a070', [('fish', 2), ('cassava_root', 1)], {'food': 0.7}), ('turtle_soup', 'Turtle soup', 3, '6a8a4a', [('turtle_meat', 2), ('yam', 1)], {'food': 0.9, 'effect': lasts(3600, armor=0.1)}), ('crab_boil', 'Crab boil', 3, 'e06a4a', [('crab_meat', 3), ('salt', 1)], {'food': 0.8, 'effect': lasts(2400, strength=1)}),
         ('fried_plantain', 'Fried banana', 2, 'f0d040', [('plantain', 3)], {'food': 0.5, 'effect': lasts(2400, walk=1.15)}), ('roast_breadfruit', 'Roast breadfruit', 2, 'b0c050', [('breadfruit', 2)], {'food': 0.7}), ('pineapple_slices', 'Pineapple slices', 2, 'e0b030', [('pineapple', 2)], {'food': 0.4, 'cures': True}),
         ('cassava_bread', 'Cassava bread', 1, 'e0d0a0', [('cassava_root', 3)], {'food': 0.6}), ('pepper_pot', 'Pepper pot', 4, 'a03020', [('pepper_vine', 2), ('dodo_meat', 2), ('yam', 2)], {'food': 1.0, 'effect': lasts(6000, warmth=0.4, strength=2)}), ('boar_roast', 'Roast boar', 3, '9a5a3a', [('boar_haunch', 2), ('pineapple', 1)], {'food': 1.0}),
         ('salmagundi', 'Salmagundi', 4, 'c8a060', [('salt_pork', 1), ('fish', 1), ('key_lime', 1), ('pepper_vine', 1)], {'food': 1.0, 'effect': lasts(6000, regen=3.0, walk=1.15)}), ('shark_steak', 'Shark steak', 3, 'a0a8b0', [('fish', 4), ('key_lime', 1)], {'food': 0.8, 'effect': lasts(3600, lung=2.0)}),
         ('coffee_cup', 'Sea coffee', 2, '5a3a22', [('coffee_sack', 1)], {'cupped': True, 'effect': lasts(3600, craft=1.2)}), ('captains_table', "Captain's table", 5, 'f0c080', [('salmagundi', 1), ('turtle_soup', 1), ('aged_rum', 1)], {'food': 1.0, 'effect': lasts(12000, regen=4.0, strength=2, luck=0.1)})]
for n, lab, tier, col, ins, kw in MEALS: meal(n, lab, tier, col, ins, 'ships_galley', kw.pop('cupped', False), **kw)
make('sugar_mill', 'sugar_loaf', [('sugar_cane', 6)], 1, 1, 120); make('sugar_mill', 'molasses', [('sugar_cane', 4)], 1, 1, 120); make('smokehouse', 'tobacco_bale', [('tobacco_leaf', 8)], 1, 1, 200); make('sail_loft', 'cotton_bale', [('sea_island_cotton', 8)], 1, 1, 120)
make('sugar_mill', 'indigo_cake', [('indigo_plant', 8)], 1, 1, 200); make('cooperage', 'spice_chest', [('pepper_vine', 10), ('sawn_planking', 1)], 1, 1, 200); make('smokehouse', 'salt_fish_barrel', [('fish', 6), ('salt', 3)], 1, 1, 200); make('sawpit', 'timber_load', [('seasoned_timber', 6)], 1, 1, 120)
make('sail_loft', 'cloth_bolt', [('cotton_bale', 2), ('indigo_cake', 1)], 1, 1, 240); make('gun_foundry', 'tools_crate', [('iron_ingot', 4), ('sawn_planking', 1)], 1, 1, 240); make('smokehouse', 'coffee_sack', [('coffee_bean', 12)], 1, 1, 160)
# Rum: distilled, bottled - or put down in a cask and left. A cask in a cellar gets better by the year.
RUM = [('rum_bottle', 'Rum', 2, 'c88a40', lasts(2400, strength=1, warmth=0.3)), ('aged_rum', 'Aged rum', 3, 'a86a28', lasts(3600, strength=2, warmth=0.4)), ('old_rum', 'Old rum', 4, '8a4a18', lasts(4800, strength=3, luck=0.05, warmth=0.5)), ('captains_reserve', "Captain's reserve", 5, '6a2a10', lasts(6000, strength=4, luck=0.1, regen=2.0))]
for n, lab, tier, col, eff in RUM: thing(bottle(c(col)), n, lab, tier, 'nature', 1.0 + tier * 0.5, max_stack=10, effect=eff)
make('still', 'rum_bottle', [('molasses', 2), ('glass', 1)], 1, 2, 240)
for k, ((n, lab, tier, col, eff), nxt) in enumerate(zip(RUM, RUM[1:] + [None])):
    barrel(OAK, c('5a5560'), c(col)).save('tiles', 'cask_of_' + n); 
    tile('cask_of_' + n, 'Cask of ' + lab.lower(), False, tool='axe', mine_time=0.5, drop=None, drops=[{'item': n, 'count': 8}, {'item': 'empty_cask', 'count': 1}], grows_into='cask_of_' + nxt[0] if nxt else None, grow_one_in=40 if nxt else None, model='box')
comp(barrel(OAK).outline(), 'empty_cask', 'Empty cask', 2, 1.0); make('cooperage', 'empty_cask', [('sawn_planking', 3), ('iron_fittings', 1)], 1, 1, 160)
icon_of('cask_of_rum_bottle'); item('cask_of_rum_bottle', 'Cask of rum', 3, 'crafted', 4.0, places='cask_of_rum_bottle'); make('still', 'cask_of_rum_bottle', [('rum_bottle', 8), ('empty_cask', 1)], 1, 1, 120)
thing(cup(c('c8a060'), c('7a7e8a')), 'grog', 'Grog', 2, 'nature', 0.6, cooked=True, max_stack=10, effect=lasts(2400, stamina=1.3, warmth=0.2)); make('ships_galley', 'grog', [('rum_bottle', 1), ('key_lime', 1), ('sugar_loaf', 1)], 1, 4, 80)

# ---- the people of the sea -----------------------------------------------------------------------------------------------------------------------
def folk(name, label, coat, hat, hatkind, health, drops_, spawn=None, mean=False, blade=True, skin='e0b890', **kw):
    for f in (0, 1): person(c(coat), c(hat), f, c(skin), blade, hatkind).save('mobs', name + ('_walk' if f else ''))
    m = {'name': name, 'label': label, 'texture': 'mobs/%s.png' % name, 'walk': ['mobs/%s_walk.png' % name, 'mobs/%s.png' % name], 'hop': 0.05, 'step_ticks': 3 if mean else 5, 'move_chance': 0.25 if mean else 0.12, 'turn_chance': 0.3,
         'health': health, 'cap': 4, 'spawn': spawn, 'drops': drops_, 'sting': {'ticks': 30, 'rest_ticks': 40, 'sight': 9} if mean else None}
    m.update(kw); mobs.append(clean(m))
def above(tiles_, chance, biome=None): return clean({'above': tiles_, 'chance': chance, 'biome': biome})
# Marks of each power: what its enemies carry, and what it pays in.
POWERS = [('crown', 'Crown', '2a4a9a', "King's shilling"), ('company', 'Company', '8a5a2a', 'Company scrip'), ('brethren', 'Brethren', '1a1a1a', 'Piece of eight'), ('islander', 'Islanders', 'c08030', 'Trade shell')]
for p, lab, col, coin in POWERS: comp(P().disc(7.5, 8, 4.5, c(col)).disc(7.5, 8, 3.2, lt(c(col), 1.4)).p(7, 7, WHITE).r(7, 6, 8, 10, c(col)).outline(), p + '_mark', coin, 2, 1.0, 'special')
TOKENS = [('pirate_flag_taken', 'Captured black flag', flag(INK, OAK, WHITE)), ('navy_insignia', 'Navy insignia', trinket('seal', c('2a4a9a'), GOLD)), ('cargo_manifest', 'Cargo manifest', trinket('paper', PAPER)), ('old_carving', 'Old carving', trinket('rune', c('6a7a6a'), c('c08030'))),
          ('ghost_lantern', 'Ghost lantern', lantern_pic(c('3a5a5a'), c('60ffd0')).outline()), ('kraken_beak', 'Kraken beak', fang(c('3a2a3a'))), ('serpent_scale', 'Sea serpent scale', trinket('plate', c('2a8a6a'))), ('whale_oil', 'Whale oil', barrel(c('e8e0c0')).outline())]
for n, lab, pic in TOKENS: pic.save('items', n); item(n, lab, 4, 'special', 2.0)
# Townsfolk, who do nobody any harm.
folk('townsman', 'Townsman', '7a5a3a', '3a2a1a', 2, 20, [drop('copper_coin', 0, 4)], above(['quay_stone', 'harbour_brick', 'pier_planks'], 0.4), blade=False)
folk('fishwife', 'Fishwife', '5a7a8a', 'e8e8f0', 1, 20, [drop('fish', 0, 2)], above(['pier_planks', 'quay_stone'], 0.3), blade=False)
folk('islander', 'Islander', 'c08030', 'f0d0a0', 3, 24, [drop('islander_mark', 0, 1)], above(['pier_planks', 'white_sand'], 0.2, ['palm_cay', 'jungle_isle']), blade=False, skin='a87850')
# Hands to hire: their articles signed at a tavern, they follow and fight.
for n, lab, coat, hat, hk, hp, bite, cost in (('deckhand', 'Deckhand', '6a7a8a', 'a02828', 1, 40, 6, 6), ('buccaneer', 'Buccaneer', 'a02828', '1a1a1a', 0, 80, 14, 20), ('master_gunner_hand', 'Sea-dog', '2a2a4a', '1a1a1a', 0, 140, 26, 60)):
    folk(n, lab, coat, hat, hk, hp, [], None, tame=9, companion={'bite': bite, 'sight': 9}, caged=n + '_articles')
    comp(trinket('paper', PAPER).r(6, 5, 9, 5, INK).r(6, 7, 10, 7, INK).r(8, 10, 10, 11, RED).outline(), n + '_articles', lab + "'s articles", 3, cost / 4.0, 'special', max_stack=1); rec('tavern_bar', n + '_articles', [('gold_coin', cost)], time=60)
# Officers: named posts, carried with the captain's things.
for n, lab, col, boons, cost in (('gunner_warrant', "Gunner's warrant", 'a02020', {'strength': 3}, 40), ('bosun_warrant', "Bosun's warrant", '2a4a9a', {'walk': 1.2, 'stamina': 1.3}, 40), ('carpenter_warrant', "Carpenter's warrant", '8a5a30', {'craft': 1.4}, 40),
                                 ('surgeon_warrant', "Surgeon's warrant", 'f0f0f0', {'regen': 2.5}, 60), ('cook_warrant', "Cook's warrant", 'c8a060', {'appetite': 0.5}, 30), ('navigator_warrant', "Navigator's warrant", '60d0f0', {'sight': 1.0, 'luck': 0.1}, 80),
                                 ('quartermaster_warrant', "Quartermaster's warrant", 'f0c840', {'luck': 0.2, 'magnet': 1.0}, 80), ('lookout_warrant', "Lookout's warrant", '3a8a4a', {'sight': 1.0, 'unseen': 0.5}, 40)):
    thing(trinket('paper', PAPER).r(6, 5, 9, 9, c(col)).outline(), n, lab, 4, 'special', cost / 10.0, max_stack=1, relic=True, boons=boons, set='ships_company'); rec('tavern_bar', n, [('gold_coin', cost)], time=60)
sets.append({'name': 'ships_company', 'label': "Ship's company", 'bonuses': [{'pieces': 2, 'boons': {'regen': 1.5}}, {'pieces': 4, 'boons': {'strength': 3, 'craft': 1.3, 'luck': 0.15}}]})
rec('tavern_bar', 'treasure_map', [('gold_coin', 30)], time=60); rec('tavern_bar', 'rum_bottle', [('copper_coin', 60)], 2, time=30); rec('tavern_bar', 'grog', [('copper_coin', 30)], 2, time=30)
# Crews met at sea, and the captains over them. Each captain carries his ship's papers: beat him and she is a prize.
CREWS = [('merchant_sailor', 'Merchant sailor', '8a6a4a', 'e8e8f0', 1, 30, [drop('copper_coin', 2, 8), drop('company_mark', 0, 1)]), ('navy_marine', 'Marine', 'a02020', '1a1a1a', 2, 60, [drop('navy_insignia', 0, 1), drop('round_shot', 0, 2)]),
         ('pirate_cutthroat', 'Cutthroat', '3a3a44', 'a02828', 1, 50, [drop('gold_coin', 0, 2), drop('rum_bottle', 0, 1)]), ('islander_warrior', 'Islander warrior', 'c08030', 'f0d0a0', 3, 44, [drop('islander_mark', 0, 1)]), ('drowned_sailor', 'Drowned sailor', '3a6a5a', '2a4a4a', 1, 70, [drop('old_carving', 0, 1)])]
for n, lab, coat, hat, hk, hp, dr in CREWS: folk(n, lab, coat, hat, hk, hp, dr, None, True, skin='a87850' if 'islander' in n else '9ad0c0' if 'drowned' in n else 'e0b890')
# name, label, drawn across, hull, sail, flag, masts, ports, health, the crew, how many, what the captain yields besides, how hard they hit and from how far, where
FOES = [('merchant_sloop', 'Merchant sloop', 3, '9a6a3a', 'e8dcc0', 'c8a040', 1, 1, 160, 'merchant_sailor', 3, 'sloop', [('cargo_manifest', 1, 2), ('sugar_loaf', 2, 6), ('cloth_bolt', 0, 3)], 0.0, 0, 0.012),
        ('merchant_brig', 'Merchantman', 4, '9a6a3a', 'e0d8c0', 'c8a040', 3, 3, 420, 'merchant_sailor', 5, 'merchantman', [('cargo_manifest', 1, 3), ('spice_chest', 1, 3), ('silver_bar', 0, 2), ('tobacco_bale', 2, 5)], 0.04, 7, 0.006),
        ('treasure_galleon', 'Treasure galleon', 5, '7a3a1a', 'f0e0b0', 'f0c840', 3, 8, 1400, 'navy_marine', 8, 'frigate', [('silver_bar', 6, 12), ('pearl_string', 1, 3), ('platinum_coin', 1, 3), ('treasure_map', 1, 1)], 0.1, 10, 0.0012),
        ('navy_cutter', 'Navy cutter', 3, '2a3a5a', 'f0f0e8', '2a4a9a', 1, 3, 260, 'navy_marine', 4, 'sloop', [('navy_insignia', 1, 2), ('gunpowder', 2, 6)], 0.05, 9, 0.006),
        ('navy_frigate', 'Navy frigate', 5, '2a3a5a', 'f0f0e8', '2a4a9a', 3, 8, 1000, 'navy_marine', 8, 'frigate', [('navy_insignia', 2, 4), ('long_nine', 0, 2), ('frigate_plan', 0, 1)], 0.1, 12, 0.002),
        ('royal_man_of_war', "King's man-of-war", 6, '1a2a4a', 'f0f0e8', '2a4a9a', 3, 12, 2600, 'navy_marine', 12, 'man_of_war', [('navy_insignia', 4, 8), ('carronade', 1, 3), ('man_of_war_plan', 1, 1), ('platinum_coin', 2, 5)], 0.16, 14, 0.0004),
        ('pirate_sloop', 'Pirate sloop', 3, '3a2a22', '3a3a3a', '1a1a1a', 1, 2, 220, 'pirate_cutthroat', 4, 'sloop', [('pirate_flag_taken', 1, 1), ('gold_coin', 4, 12), ('treasure_map', 0, 1)], 0.05, 9, 0.008),
        ('pirate_brig', 'Pirate brig', 4, '3a2a22', '2a2a2a', '1a1a1a', 2, 5, 560, 'pirate_cutthroat', 7, 'brig', [('pirate_flag_taken', 1, 2), ('gold_coin', 10, 30), ('treasure_map', 1, 1), ('brig_plan', 0, 1)], 0.08, 11, 0.004),
        ('corsair_schooner', 'Corsair schooner', 3, '5a1a1a', 'c03030', '1a1a1a', 2, 3, 320, 'pirate_cutthroat', 5, 'schooner', [('pirate_flag_taken', 1, 1), ('schooner_plan', 0, 1), ('aged_rum', 1, 3)], 0.07, 10, 0.004),
        ('islander_canoe', 'War canoe', 2, '6a3a1a', 'c05030', 'f0d0a0', 0, 0, 90, 'islander_warrior', 3, 'war_canoe', [('islander_mark', 1, 3), ('canoe_plan', 0, 1), ('old_carving', 0, 1)], 0.03, 5, 0.006),
        ('ghost_ship', 'Ghost ship', 5, '2a4a44', '8ad0c0', '60ffd0', 3, 6, 1800, 'drowned_sailor', 9, 'frigate', [('ghost_lantern', 1, 1), ('old_carving', 2, 5), ('captains_reserve', 1, 3), ('treasure_map', 1, 2)], 0.12, 12, 0.0006)]
for n, lab, size, hull, sail, flg, masts, ports, hp, crew, count, prize, loot, hit, far, chance in FOES:
    cap = n + '_captain'
    folk(cap, 'Captain of the ' + lab.lower(), {'merchant_sailor': '6a4a2a', 'navy_marine': '2a4a9a', 'pirate_cutthroat': '1a1a1a', 'islander_warrior': 'c08030', 'drowned_sailor': '2a4a44'}[crew], 'f0c840', 0, 60 + hp // 6, [drop(prize, 1, 1)] + [drop(i, a, b) for i, a, b in loot], None, True,
         skin='a87850' if 'islander' in crew else '9ad0c0' if 'drowned' in crew else 'e0b890', cap=1)
    ship_mob(n, lab, size, hull, sail, flg, masts, ports, hp, tatter=n == 'ghost_ship', oars=n == 'islander_canoe', cap=2 if size < 5 else 1, spawn={'water': True, 'biome': 'ocean', 'chance': chance},
             swim={'hunts': True, 'sight': 20, 'bite': 0.0, 'rest_ticks': 40} if hit else {}, strike={'share': hit, 'range': far, 'rest_ticks': 80} if hit else None, ship={'wreck': 'ship_wreckage'}, boarding={'crew': crew, 'count': count, 'captain': cap, 'deck': 'ship_planks'},
             drops=[drop('sawn_planking', 1, 4)] + [drop(i, a // 2, max(1, b // 2)) for i, a, b in loot[:2]])

# ---- beasts of the islands and the deep -------------------------------------------------------------------------------------------------------------
def creature(kind, name, label, body, accent, health, part, spawn, mean=False, **kw):
    for f in (0, 1): beast(kind, c(body), c(accent), f).save('mobs', name + ('_walk' if f else ''))
    flier = kind in ('bird',)
    m = {'name': name, 'label': label, 'texture': 'mobs/%s.png' % name, 'walk': ['mobs/%s_walk.png' % name, 'mobs/%s.png' % name], 'hop': 0.0 if kind in ('serpent', 'tortoise') else 0.06, 'step_ticks': 3 if mean else 4,
         'move_chance': 0.5 if flier else 0.2, 'turn_chance': 0.25, 'health': health, 'cap': 3, 'spawn': spawn, 'flies': True if flier else None, 'drops': part, 'sting': {'ticks': 40, 'rest_ticks': 60, 'sight': 7} if mean and not flier and 'swim' not in kw else None}
    m.update(kw); mobs.append(clean(m))
for n, lab, pic in (('turtle_meat', 'Turtle meat', meat(c('6a8a4a'))), ('crab_meat', 'Crab meat', meat(c('e06a4a'))), ('boar_haunch', 'Boar haunch', meat(c('9a5a3a'))), ('parrot_feather', 'Parrot feather', feather(c('e03030'))), ('snake_skin', 'Snake skin', hide(c('5a8a3a'))),
                    ('caiman_hide', 'Caiman hide', hide(c('3a5a3a'))), ('octopus_ink', 'Octopus ink', barrel(c('2a2a3a')).outline()), ('monkey_paw', "Monkey's charm", trinket('pendant', c('8a5a30'))), ('gull_egg', 'Gull egg', trinket('egg', c('f0f0e8'), c('c0c0b0')))):
    pic.save('items', n); item(n, lab, 2, 'nature', 0.4, food=0.25 if 'meat' in n or 'haunch' in n or 'egg' in n else None)
ISLES = ['palm_cay', 'jungle_isle', 'volcanic_isle', 'mangrove_isle', 'skull_coast', 'spice_isle']
creature('tortoise', 'sea_turtle', 'Sea turtle', '4a8a5a', 'c0d070', 20, [drop('turtle_meat', 1, 2), drop('tortoiseshell', 0, 1)], above(['sand', 'white_sand'], 0.12, ['palm_cay', 'beach']))
creature('tortoise', 'sand_crab', 'Sand crab', 'e06a4a', 'f0a080', 12, [drop('crab_meat', 1, 1)], above(['sand', 'white_sand', 'mangrove_mud'], 0.2), True)
creature('tortoise', 'giant_crab', 'Giant crab', 'a03a2a', 'f08060', 90, [drop('crab_meat', 3, 5), drop('pearl_string', 0, 1)], above(['white_sand', 'skull_rock'], 0.04, ['skull_coast', 'palm_cay']), True, size=[2, 2], cap=1)
creature('boar', 'island_boar', 'Island boar', '5a4a3a', '2a2018', 30, [drop('boar_haunch', 1, 2), drop('leather', 0, 1)], above(['jungle_grass', 'grass'], 0.12, ['jungle_isle', 'spice_isle', 'tropics']))
creature('serpent', 'jungle_viper', 'Jungle viper', '5a8a3a', 'f0e060', 24, [drop('snake_skin', 1, 1)], above(['jungle_grass'], 0.1, ['jungle_isle']), True)
creature('quad', 'caiman', 'Caiman', '3a5a3a', '8aa870', 60, [drop('caiman_hide', 1, 2)], above(['mangrove_mud'], 0.12, ['mangrove_isle']), True)
creature('bird', 'parrot', 'Parrot', 'e03030', '30a0e0', 10, [drop('parrot_feather', 1, 2)], {'sky': True, 'biome': ['jungle_isle', 'spice_isle', 'palm_cay'], 'time': 'day', 'chance': 0.004}, tame=1, companion={'bite': 2, 'sight': 8})
creature('bird', 'sea_gull', 'Gull', 'f0f0f0', '8a8e9a', 8, [drop('feather', 1, 2), drop('gull_egg', 0, 1)], {'sky': True, 'biome': ['ocean', 'beach', 'palm_cay'], 'time': 'day', 'chance': 0.004})
creature('ape', 'ships_monkey', 'Monkey', '8a5a30', 'e0c0a0', 16, [drop('monkey_paw', 0, 1)], above(['jungle_grass'], 0.1, ['jungle_isle', 'spice_isle']), tame=1, companion={'bite': 3, 'sight': 8})
creature('quad', 'ships_cat', "Ship's cat", '3a3a3a', 'f0f0f0', 12, [], above(['pier_planks', 'quay_stone'], 0.1), tame=1, companion={'bite': 4, 'sight': 8})
creature('quad', 'sea_dog', 'Dog', 'b08850', 'f0e0c0', 30, [], above(['harbour_brick', 'quay_stone'], 0.06), tame=1, companion={'bite': 10, 'sight': 10})
creature('fish', 'barracuda', 'Barracuda', '8a9aa8', 'e0e8f0', 30, [drop('fish', 2, 3)], {'water': True, 'biome': 'ocean', 'chance': 0.006}, swim={'hunts': True, 'sight': 14, 'bite': 0.12, 'rest_ticks': 20}, move_chance=0.96, turn_chance=0.05)
creature('wyrm', 'sea_serpent', 'Sea serpent', '2a8a6a', 'c0f0d0', 700, [drop('serpent_scale', 2, 4), drop('pearl_string', 1, 2)], {'water': True, 'biome': 'ocean', 'chance': 0.0008}, True, swim={'hunts': True, 'sight': 24, 'bite': 0.3, 'rest_ticks': 24}, size=[3, 3], cap=1, move_chance=0.96, turn_chance=0.04,
         strike={'share': 0.1, 'range': 5, 'rest_ticks': 60}, ship={'wreck': 'ship_wreckage'})
creature('wyrm', 'white_whale', 'White whale', 'f0f0f0', 'c0d0e0', 1200, [drop('whale_oil', 3, 6), drop('ambergris', 1, 2)], {'water': True, 'biome': 'ocean', 'chance': 0.0005}, swim={'hunts': False, 'sight': 10}, size=[4, 4], cap=1, move_chance=0.9, turn_chance=0.03)
creature('wraith', 'giant_octopus', 'Giant octopus', '8a3a6a', 'f0a0c0', 400, [drop('octopus_ink', 2, 4), drop('pearl_string', 0, 2)], {'water': True, 'biome': 'ocean', 'chance': 0.0012}, True, swim={'hunts': True, 'sight': 14, 'bite': 0.25, 'rest_ticks': 30}, size=[2, 2], cap=1)
creature('wraith', 'kraken', 'The Kraken', '4a2a4a', 'f06080', 4000, [drop('kraken_beak', 1, 1), drop('platinum_coin', 6, 12), drop('pearl_string', 4, 8), drop('silver_bar', 8, 16), drop('man_of_war_plan', 1, 1)], {'water': True, 'biome': 'ocean', 'chance': 0.0002}, True,
         swim={'hunts': True, 'sight': 30, 'bite': 0.5, 'rest_ticks': 30}, size=[6, 6], cap=1, strike={'share': 0.2, 'range': 8, 'rest_ticks': 50})

# ---- the four powers' boards: what is handed in, and what is paid out --------------------------------------------------------------------------------
RANKS = {'crown': ['Midshipman', 'Lieutenant', 'Commander', 'Post-captain', 'Admiral'], 'company': ['Factor', 'Supercargo', 'Merchant', 'Director', 'Governor'], 'brethren': ['Swab', 'Hand', 'Mate', 'Captain', 'Pirate lord'], 'islander': ['Stranger', 'Guest', 'Friend', 'Kin', 'Elder']}
for p, lab, col, coin in POWERS:
    q = P().r(2, 2, 13, 11, c('8a6a3a')); q.r(3, 3, 12, 10, c('c8b890')); q.r(4, 4, 11, 5, c(col)); q.r(4, 7, 7, 9, PAPER); q.r(9, 7, 11, 9, PAPER); q.r(3, 12, 4, 15, c('5a3a1a')); q.r(11, 12, 12, 15, c('5a3a1a'))
    for g in range(1, 6):
        n = p + '_board' if g == 1 else '%s_board_%d' % (p, g)
        q.save('tiles', n); icon_of(n); tile(n, lab + ' board', False, tool='axe', mine_time=0.4, bench=True, bench_kind=p + '_board', bench_level=g, upgrade={'to': '%s_board_%d' % (p, g + 1), 'cost': [{'item': p + '_mark', 'count': [0, 10, 30, 80, 200][g]}]} if g < 5 else None)
        item(n, '%s board (%s)' % (lab, RANKS[p][g - 1].lower()), 2, 'benches', 2.0, places=n)
    rec('workbench', p + '_board', [('wood', 8), ('linen', 2), (p + '_mark', 5)], level=2)
def pay(p, ins, marks, g=1): rec(p + '_board', p + '_mark', ins, marks, level=g, time=40)
def buy(p, out, marks, g=1, count=1): rec(p + '_board', out, [(p + '_mark', marks)], count, level=g, time=40)
pay('crown', [('pirate_flag_taken', 1)], 4); pay('crown', [('ghost_lantern', 1)], 30, 3); pay('crown', [('kraken_beak', 1)], 80, 4); pay('crown', [('serpent_scale', 2)], 10, 2); pay('crown', [('powder_keg', 2)], 3)
pay('company', [('cargo_manifest', 2)], 2); pay('company', [('whale_oil', 1)], 6, 2)
for n, lab, kind, col, price in CARGO: pay('company', [(n, 4)], max(1, price // 80), 1 if price < 700 else 2 if price < 2600 else 3)
pay('brethren', [('navy_insignia', 1)], 4); pay('brethren', [('cargo_manifest', 1)], 2); pay('brethren', [('silver_bar', 1)], 6, 2); pay('brethren', [('sea_chest', 1)], 8); pay('brethren', [('aged_rum', 2)], 3)
pay('islander', [('old_carving', 1)], 4); pay('islander', [('pearl_string', 1)], 6, 2); pay('islander', [('parrot_feather', 4)], 2); pay('islander', [('tortoiseshell', 1)], 3); pay('islander', [('ghost_lantern', 1)], 30, 3)
for g, (plan, marks) in enumerate((('sloop_plan', 8), ('brig_plan', 30), ('frigate_plan', 80), (None, 0), ('man_of_war_plan', 300)), 1):
    if plan: buy('crown', plan, marks, g)
buy('crown', 'navy_coat', 12, 2); buy('crown', 'kings_colours', 10, 2); buy('crown', 'long_nine', 30, 3); buy('crown', 'gunner_warrant', 12, 2); buy('crown', 'gunpowder', 2, 1, 4); buy('crown', 'musket', 20, 3)
buy('company', 'merchantman_plan', 30, 2); buy('company', 'cargo_hold', 10, 1); buy('company', 'company_coat', 12, 2); buy('company', 'company_ensign', 10, 2); buy('company', 'gold_coin', 2, 1, 3); buy('company', 'platinum_coin', 150, 4); buy('company', 'quartermaster_warrant', 20, 3)
buy('brethren', 'schooner_plan', 20, 2); buy('brethren', 'brig_plan', 30, 3); buy('brethren', 'black_flag', 10, 1); buy('brethren', 'treasure_map', 6, 1); buy('brethren', 'buccaneer_articles', 4, 2); buy('brethren', 'brace_of_pistols', 40, 4); buy('brethren', 'captains_reserve', 12, 3); buy('brethren', 'grappling_hook', 4)
buy('islander', 'canoe_plan', 5, 1); buy('islander', 'shell_pennant', 10, 2); buy('islander', 'treasure_map', 5, 2); buy('islander', 'diver_helmet', 20, 3); buy('islander', 'navigator_warrant', 20, 3); buy('islander', 'serpent_figurehead', 16, 3); buy('islander', 'spice_chest', 8, 2)
# A sea chest is broken open: what is in it is anybody's guess.
rec('captains_desk', 'gold_coin', [('sea_chest', 1)], 1, time=60, output={'count': 1, 'one_of': [{'item': 'gold_coin', 'weight': 6}, {'item': 'silver_bar', 'weight': 4}, {'item': 'pearl_string', 'weight': 3}, {'item': 'platinum_coin', 'weight': 1}, {'item': 'treasure_map', 'weight': 2},
     {'item': 'old_carving', 'weight': 3}, {'item': 'captains_reserve', 'weight': 2}, {'item': 'spice_chest', 'weight': 2}, {'item': 'brig_plan', 'weight': 1}, {'item': 'gilded_idol', 'weight': 1}, {'item': 'emerald', 'weight': 2}, {'item': 'ruby', 'weight': 1}]})
thing(trinket('seal', c('f0c840'), c('3a8a4a')), 'gilded_idol', 'Gilded idol', 5, 'special', 6.0, max_stack=1, relic=True, boons={'luck': 0.25})
rec('chart_table', 'treasure_map', [('old_carving', 3), ('linen', 1)], time=200); rec('captains_desk', 'sloop_plan', [('linen', 4), ('seasoned_timber', 2), ('gold_coin', 10)], time=300); rec('captains_desk', 'canoe_plan', [('linen', 2), ('hardwood', 2)], time=200)

# ---- a harbour's furniture, and what is left lying --------------------------------------------------------------------------------------------------
def vault(name, label, col, trim, tier, cols, rows, loot):
    chest_pic(c(col), c(trim)).save('tiles', name); icon_of(name)
    tile(name, label, False, tool='axe', mine_time=0.6, storage={'columns': cols, 'rows': rows}, loot=loot, model='box'); item(name, label, tier, 'crafted', 1.0, places=name, container={'slots': cols * rows})
def some(*names, most=2): return {'one_of': [{'item': n, 'weight': w} for n, w in names], 'min': 1, 'max': most}
vault('town_strongbox', 'Strongbox', '5a5560', 'f0c840', 3, 4, 2, [drop('gold_coin', 2, 8), some(('rum_bottle', 4), ('gunpowder', 3), ('sailcloth', 3), ('sugar_loaf', 4), ('tobacco_bale', 3), ('cargo_manifest', 2), ('treasure_map', 1))])
vault('wreck_chest', 'Sunken chest', '4a5a4a', '6ab0a0', 4, 4, 2, [drop('gold_coin', 4, 14), drop('old_carving', 0, 2), some(('silver_bar', 4), ('pearl_string', 3), ('treasure_map', 3), ('aged_rum', 3), ('long_nine', 1), ('spice_chest', 2), ('sloop_plan', 1), ('schooner_plan', 1), ('gilded_idol', 1))])
vault('pirate_cache', 'Pirate cache', '2a2a2a', 'a02828', 4, 4, 2, [drop('gold_coin', 6, 20), some(('treasure_map', 4), ('black_flag', 1), ('brace_of_pistols', 1), ('captains_reserve', 2), ('grenado', 4), ('brig_plan', 1), ('silver_bar', 3))])
q = P().r(6, 9, 9, 15, OAK); q.r(5, 1, 10, 3, OAK); q.r(5, 3, 10, 8, BRASS); q.r(5, 3, 5, 8, lt(BRASS)); q.r(7, 9, 8, 10, c('7a7e8a'))
q.save('tiles', 'town_bell'); tile('town_bell', 'Harbour bell', False, removable=False, mine_time=0.3, village=60, light=8, drop=None)
q = P().r(1, 9, 14, 10, OAK); q.r(2, 11, 3, 15, dk(OAK)); q.r(12, 11, 13, 15, dk(OAK)); q.r(1, 2, 14, 4, c('c03030')); [q.r(x, 2, x + 1, 4, WHITE) for x in (3, 7, 11)]; q.r(2, 5, 2, 8, OAK); q.r(13, 5, 13, 8, OAK); q.r(4, 7, 6, 8, c('f0d040')); q.r(8, 6, 10, 8, c('c05020'))
q.save('tiles', 'harbour_market'); icon_of('harbour_market'); tile('harbour_market', 'Harbour market', False, tool='axe', mine_time=0.6, trade=True, trade_level=6, light=6); item('harbour_market', 'Harbour market', 3, 'benches', 3.0, places='harbour_market')
rec('workbench', 'harbour_market', [('wood', 12), ('sailcloth', 4), ('gold_coin', 20)], level=2)
deco(lantern_pic(c('3a2a22'), c('ffd070')), 'harbour_lantern', 'Harbour lantern', 2, [('iron_fittings', 1), ('glass', 1)], 'gun_foundry', 1, 2, light=13)
deco(barrel(OAK), 'stowage_barrel', 'Barrel', 2, [('empty_cask', 1)], 'cooperage', storage={'columns': 4, 'rows': 1}, model='box'); items[-1]['container'] = {'slots': 4}
deco(P().r(2, 5, 13, 14, c('a07840')).r(2, 5, 13, 5, c('c8a070')).r(2, 9, 13, 9, dk(OAK)).r(7, 5, 8, 14, dk(OAK)), 'cargo_crate', 'Crate', 2, [('sawn_planking', 2), ('treenails', 4)], 'cooperage', storage={'columns': 4, 'rows': 2}, model='box'); items[-1]['container'] = {'slots': 8}
deco(P().line(1, 5, 4, 9, ROPE).line(11, 9, 14, 5, ROPE).r(4, 9, 11, 10, CANVAS).r(5, 11, 10, 11, dk(CANVAS, 0.8)).r(1, 2, 1, 6, OAK).r(14, 2, 14, 6, OAK), 'hammock', 'Hammock', 2, [('sailcloth', 2), ('cordage', 2)], 'sail_loft', bed={'speed': 2.2})
deco(P().r(7, 2, 8, 12, c('5a5560')).disc(7.5, 2.5, 1.6, c('5a5560')).line(3, 10, 7, 13, c('5a5560')).line(12, 10, 8, 13, c('5a5560')).r(5, 5, 10, 5, c('5a5560')), 'anchor_stand', 'Anchor', 3, [('iron_ingot', 6), ('tarred_rope', 2)], 'gun_foundry')
deco(P().disc(7.5, 7.5, 5.5, OAK).disc(7.5, 7.5, 3.8, (0, 0, 0, 0)).r(7, 1, 8, 14, dk(OAK)).r(1, 7, 14, 8, dk(OAK)).disc(7.5, 7.5, 1.4, BRASS), 'ships_wheel', "Ship's wheel", 3, [('hardwood', 4), ('bronze_ingot', 1)])
deco(P().r(3, 7, 13, 9, c('2a2a2a')).r(2, 6, 4, 10, c('2a2a2a')).r(4, 10, 10, 12, OAK).disc(5, 13, 1.8, dk(OAK)).disc(10, 13, 1.8, dk(OAK)), 'shore_gun', 'Shore gun', 4, [('saker', 1), ('seasoned_timber', 4)], 'gun_foundry')
deco(P().r(3, 1, 4, 15, OAK).r(3, 1, 12, 2, OAK).r(11, 2, 11, 6, ROPE).disc(11, 7, 1.5, ROPE).r(1, 14, 8, 15, dk(OAK)), 'gallows', 'Gallows', 3, [('seasoned_timber', 6), ('cordage', 2)])
deco(P().r(7, 4, 8, 15, OAK).r(3, 2, 12, 6, c('c8b890')).r(4, 3, 11, 3, INK).r(4, 5, 9, 5, INK), 'harbour_sign', 'Signpost', 1, [('sawn_planking', 1), ('seasoned_timber', 1)])
deco(P().r(1, 2, 1, 15, OAK).r(14, 2, 14, 15, OAK).line(1, 3, 14, 5, ROPE).line(1, 7, 14, 9, ROPE).line(1, 11, 14, 13, ROPE).line(4, 2, 5, 14, ROPE).line(9, 2, 10, 14, ROPE), 'drying_net', 'Drying net', 1, [('cordage', 6), ('seasoned_timber', 2)], 'ropewalk')
deco(P().r(6, 6, 9, 15, OAK).r(5, 4, 10, 6, dk(OAK)).line(7, 6, 3, 9, ROPE), 'mooring_post', 'Mooring post', 1, [('seasoned_timber', 2), ('tarred_rope', 1)], floats=True)
deco(P().r(3, 0, 4, 15, OAK).r(4, 2, 13, 3, OAK).line(4, 8, 10, 3, OAK).r(12, 3, 12, 9, ROPE).r(10, 10, 14, 13, c('a07840')), 'dock_crane', 'Dock crane', 3, [('seasoned_timber', 6), ('iron_fittings', 2), ('tarred_rope', 4)])
deco(P().r(3, 3, 12, 13, PAPER).r(3, 3, 12, 3, dk(PAPER, 0.85)).disc(7.5, 7, 2.2, c('e0b890')).r(5, 10, 10, 10, INK).r(6, 12, 9, 12, RED), 'wanted_poster', 'Wanted poster', 1, [('linen', 1), ('octopus_ink', 1)], 'captains_desk')
t = P(); t.r(6, 0, 9, 15, c('7a6a4a')); t.r(6, 0, 6, 15, c('9a8a6a')); t.r(5, 5, 10, 6, c('3a2e28')); t.r(5, 11, 10, 12, c('3a2e28')); t.save('tiles', 'piling'); icon_of('piling')
tile('piling', 'Piling', False, tool='axe', mine_time=0.3, climbable=True); item('piling', 'Piling', 1, 'crafted', 0.3, places='piling'); make('sawpit', 'piling', [('seasoned_timber', 1), ('pitch', 1)], 1, 4, 60)
t = P(); t.r(4, 0, 4, 15, ROPE); t.r(11, 0, 11, 15, ROPE); [t.r(4, y, 11, y, OAK) for y in (2, 6, 10, 14)]; t.save('tiles', 'rope_ladder'); icon_of('rope_ladder')
tile('rope_ladder', 'Rope ladder', False, tool='axe', mine_time=0.2, climbable=True); item('rope_ladder', 'Rope ladder', 1, 'crafted', 0.2, places='rope_ladder'); make('ropewalk', 'rope_ladder', [('cordage', 2), ('treenails', 2)], 1, 4, 60)
for n, lab, ins, bench_ in (('quay_stone', 'q', [('stone', 4)], 'sawpit'), ('fort_stone', 'f', [('stone', 4), ('clay', 1)], 'gun_foundry'), ('harbour_brick', 'h', [('clay', 4)], 'gun_foundry'), ('whitewash', 'w', [('limestone', 2), ('clay', 2)], 'sawpit'),
                            ('palm_thatch', 'p', [('stick', 6)], 'ropewalk'), ('red_tile_roof', 'r', [('clay', 4)], 'gun_foundry'), ('carved_idol_stone', 'c', [('stone', 4), ('old_carving', 1)], 'sawpit'), ('coral_block', 'c', [('coral', 4)], 'sawpit'), ('driftwood_block', 'd', [('stick', 8)], 'sawpit')):
    make(bench_, n, ins, 1, 4, 60)

# ---- towns, and what is on the bottom -------------------------------------------------------------------------------------------------------------
def structure(name, rows, legend): assert len({len(r) for r in rows}) == 1, (name, [len(r) for r in rows]); structures.append({'name': name, 'rows': rows, 'legend': legend})
def L(front=None, wall=None, back=None): return clean({'front': front, 'wall': wall, 'back': back})
def style(wall, floor, roof, door='wooden_door'):
    return {'#': L(wall, wall), '.': L(None, wall), 'R': L(roof), 'F': L(floor, floor), 'D': L(door, wall), 'W': L(None, wall, 'window'), 'l': L(None, wall, 'harbour_lantern'), 'L': L(None, None, 'harbour_lantern'), 'b': L(None, wall, 'stowage_barrel'),
            'c': L(None, wall, 'cargo_crate'), 'h': L(None, wall, 'hammock'), 's': L(None, wall, 'town_strongbox'), 'B': L(None, wall, 'town_bell'), 'M': L(None, wall, 'harbour_market'), 'T': L(None, wall, 'tavern_bar'), 'Y': L(None, wall, 'shipwright'),
            'G': L(None, None, 'shore_gun'), 'g': L(None, None, 'gallows'), 'S': L(None, None, 'harbour_sign'), 'n': L(None, None, 'drying_net'), 'm': L(None, None, 'mooring_post'), 'k': L(None, None, 'dock_crane'), 'a': L(None, wall, 'anchor_stand'),
            'w': L(None, wall, 'ships_wheel'), 'p': L(None, wall, 'wanted_poster'), 'P': L(None, None, 'piling'), 'C': L(None, wall, 'chart_table'), 'd': L(None, wall, 'captains_desk'), 'x': L(None, wall, 'pirate_cache'), 'q': L(None, wall, 'ships_galley'),
            '1': L(None, wall, 'crown_board'), '2': L(None, wall, 'company_board'), '3': L(None, wall, 'brethren_board'), '4': L(None, wall, 'islander_board'), 'o': L(None, wall, 'cooperage'), 'e': L(None, wall, 'still'), 'r': L(None, wall, 'ropewalk'), 'i': L(None, wall, 'sail_loft')}
BRICKS, FORT, SHACK, STILT = style('harbour_brick', 'quay_stone', 'red_tile_roof'), style('fort_stone', 'fort_stone', 'fort_stone', 'iron_door'), style('tarred_planks', 'pier_planks', 'driftwood_block'), style('ship_planks', 'pier_planks', 'palm_thatch')
WHITE_ = style('whitewash', 'quay_stone', 'red_tile_roof')
# The Company's harbours.
structure('harbour_square', ['~~~RRRRRRRRRR~~~', '~RRRRRRRRRRRRRR~', '~#.W..l..B..W.#~', '~D............D~', '~D.2..M..s..b.D~', '~FFFFFFFFFFFFFF~'], BRICKS)
structure('harbour_warehouse', ['~RRRRRRRRRR~', '~#.l....l.#~', '~D.c.c..b.D~', '~D.c.c.bb.D~', '~FFFFFFFFFF~'], BRICKS)
structure('harbour_tavern', ['~~RRRRRRRR~~', '~RRRRRRRRRR~', '~#.W.l..W.#~', '~D........D~', '~D.T..b.h.D~', '~FFFFFFFFFF~'], WHITE_)
structure('harbour_shipyard', ['~k~~~~~~~~~~', '~~~RRRRRRRR~', '~~~#.l....#~', '~m~D.Y.r.i.~', '~FFFFFFFFFF~'], BRICKS)
structure('harbour_cottage', ['~~RRRR~~', '~RRRRRR~', '~#.W.l#~', '~D.h.bD~', '~FFFFFF~'], WHITE_)
structure('harbour_chandlery', ['~RRRRRRRR~', '~#.l..W.#~', '~D.C.a.wD~', '~FFFFFFFF~'], WHITE_)
structure('harbour_quay', ['~L~~~~~~L~', '~m~S~k~~m~', '~FFFFFFFF~'], BRICKS)
# The Crown's forts.
structure('fort_keep', ['#~#~#~#~#~#~#~#~', '################', '~#.l..B....l..#~', '~D............D~', '~D.1..M..s..p.D~', '~FFFFFFFFFFFFFF~'], FORT)
structure('fort_battery', ['~G~~G~~G~', '#########', '#.l...l.#', 'D..b.b..D', 'FFFFFFFFF'], FORT)
structure('fort_barracks', ['~#########~', '~#.l...l.#~', '~D.h.h.h.D~', '~D.h.h.b.D~', '~FFFFFFFFF~'], FORT)
structure('fort_gaol', ['~#######~', '~#.l.#.#~', '~D...D.#~', '~D.p.#s#~', '~FFFFFFF~'], FORT)
structure('fort_parade', ['~g~~~S~~L~', '~FFFFFFFF~'], FORT)
structure('fort_powder_house', ['~#####~', '~#.l.#~', '~D.bbD~', '~FFFFF~'], FORT)
# The Brethren's free ports.
structure('pirate_hall', ['~~~RRRRRRRRR~~~', '~RRRRRRRRRRRRR~', '~#.l..B..p.l.#~', '~D...........D~', '~D.3..M..x.T.D~', '~FFFFFFFFFFFFF~'], SHACK)
structure('pirate_tavern', ['~RRRRRRRRR~', '~#.l...l.#~', '~D.T.e.b.D~', '~D.h...b.D~', '~FFFFFFFFF~'], SHACK)
structure('pirate_shack', ['~RRRRR~', '~#.l.#~', '~D.h.D~', '~FFFFF~'], SHACK)
structure('pirate_lookout', ['~~L~~', '~###~', '~#.#~', '~#.#~', '~#.#~', '~D.D~', '~FFF~'], SHACK)
structure('fence_den', ['~RRRRRRR~', '~#.l.p.#~', '~D.x.c.D~', '~FFFFFFF~'], SHACK)
structure('careening_yard', ['~k~~~~~~~', '~~~Y~o~m~', '~FFFFFFF~'], SHACK)
# The Islanders' stilt towns.
structure('islander_longhouse', ['~~RRRRRRRRRR~~', '~RRRRRRRRRRRR~', '~#.l..B...l.#~', '~D..........D~', '~D.4..M..C..D~', '~FFFFFFFFFFFF~', '~P~~P~~~P~~P~~'][:6], STILT)
structure('islander_hut', ['~RRRRR~', '~#.l.#~', '~D.h.D~', '~FFFFF~'], STILT)
structure('islander_canoe_shed', ['~RRRRRRR~', '~.l...m.~', '~.n.....~', '~FFFFFFF~'], STILT)
structure('islander_shrine', ['~~#~~', '~###~', '~#l#~', '~FFF~'], style('carved_idol_stone', 'carved_idol_stone', 'carved_idol_stone'))
# Fishing hamlets.
structure('fish_market', ['~RRRRRRRRRR~', '~#.l..B.l.#~', '~D.M..b.s.D~', '~FFFFFFFFFF~'], SHACK)
structure('fisher_hut', ['~RRRRR~', '~#.l.#~', '~D.h.D~', '~FFFFF~'], STILT)
structure('fish_racks', ['~n~n~n~', '~FFFFF~'], SHACK)
structure('fish_smokery', ['~RRRRR~', '~#.l.#~', '~D.q.D~', '~FFFFF~'], SHACK)
def town(name, centre, buildings, per, houses, street, at_least=1): villages.append({'name': name, 'where': 'ground', 'at_least': at_least, 'per_1000_columns': per, 'houses': houses, 'centre': centre, 'street': street, 'ground': 'dirt', 'buildings': buildings})
town('company_harbour', 'harbour_square', ['harbour_warehouse', 'harbour_tavern', 'harbour_shipyard', 'harbour_cottage', 'harbour_chandlery', 'harbour_quay', 'harbour_cottage'], 1.0, [3, 5], 'quay_stone')
town('crown_fort', 'fort_keep', ['fort_battery', 'fort_barracks', 'fort_gaol', 'fort_parade', 'fort_powder_house', 'harbour_tavern'], 0.7, [3, 5], 'fort_stone')
town('free_port', 'pirate_hall', ['pirate_tavern', 'pirate_shack', 'pirate_lookout', 'fence_den', 'careening_yard', 'pirate_shack'], 0.8, [3, 5], 'pier_planks')
town('stilt_town', 'islander_longhouse', ['islander_hut', 'islander_canoe_shed', 'islander_shrine', 'islander_hut'], 0.8, [2, 4], 'pier_planks')
town('fishing_hamlet', 'fish_market', ['fisher_hut', 'fish_racks', 'fish_smokery', 'fisher_hut'], 1.2, [2, 3], 'pier_planks')
# On the bottom.
SUNK = {'#': L('tarred_planks', 'tarred_planks'), '.': L(None, 'tarred_planks'), 'c': L(None, 'tarred_planks', 'wreck_chest'), 'b': L(None, 'tarred_planks', 'stowage_barrel'), 'm': L('ship_planks'), 'a': L(None, None, 'anchor_stand'), 'G': L(None, None, 'shore_gun'),
        'I': L('carved_idol_stone', 'carved_idol_stone'), 'i': L(None, 'carved_idol_stone'), 'x': L(None, 'carved_idol_stone', 'pirate_cache'), 'C': L('coral_block')}
structure('sunken_sloop', ['~~~~m~~~~', '~~~~m~~~~', '~##.m.##~', '~~#c.b#~~', '~~~###~~~'], SUNK)
structure('sunken_galleon', ['~~~m~~~~m~~~~', '~~~m~~~~m~~~~', '####.##.#####', '~#c..G..b.c#~', '~~#########~~'], SUNK)
structure('broken_hull', ['~#~~~~~#~', '~#.~a~.#~', '~##b~c##~'], SUNK)
structure('lost_cargo', ['~b~~b~', '~bcbb~'], SUNK)
structure('drowned_shrine', ['~~I~I~~', '~IiiiI~', '~IixiI~', 'CIIIIIC'], SUNK)

# ---- the islands -----------------------------------------------------------------------------------------------------------------------------------
def isle(name, label, heat, dry, surface, soil, trees_, one_in, wild, **kw):
    b = {'name': name, 'label': label, 'heat': heat, 'dryness': dry, 'surface': surface, 'soil': soil, 'tree_one_in': one_in, 'trees': [{'tree': t, 'weight': w} for t, w in trees_], 'plants': [{'tile': t + '_0', 'one_in': n} for t, n in wild]}
    b.update(kw); biomes.append(b)
isle('palm_cay', 'Palm cay', [0.1, 1], [0.05, 0.5], 'white_sand', 'white_sand', [('coconut', 4), ('plantain_palm', 2)], 5, [('pineapple', 14), ('yam', 22)])
isle('jungle_isle', 'Jungle isle', [0.3, 1], [-1, -0.3], 'jungle_grass', 'jungle_soil', [('mahogany', 3), ('teak', 3), ('plantain_palm', 2), ('breadfruit_tree', 2)], 3, [('yam', 12), ('cassava_root', 12), ('pepper_vine', 16)])
isle('volcanic_isle', 'Volcanic isle', [0.5, 1], [0.5, 1], 'volcanic_ash', 'volcanic_ash', [('teak', 1)], 16, [('pepper_vine', 30)], under={'tile': 'pumice_rock', 'depth': 30}, hot=True)
isle('mangrove_isle', 'Mangrove isle', [-0.1, 0.3], [-1, -0.5], 'mangrove_mud', 'mangrove_mud', [('mangrove', 1)], 2, [('cassava_root', 20)])
isle('skull_coast', 'Skull coast', [-1, -0.5], [0.4, 1], 'skull_rock', 'skull_rock', [('mangrove', 1)], 20, [])
isle('spice_isle', 'Spice isle', [0.1, 0.5], [-0.5, 0.05], 'grass', 'dirt', [('key_lime_tree', 3), ('breadfruit_tree', 1), ('plantain_palm', 1)], 7, [('sugar_cane', 8), ('tobacco_leaf', 12), ('sea_island_cotton', 12), ('indigo_plant', 16), ('pepper_vine', 16), ('pineapple', 20)])
veins.append({'name': 'pt_nitre', 'tile': 'nitre_ore', 'in': ['stone'], 'size': [2, 6], 'per_100_columns': 10, 'depth': [6, 160]})
veins.append({'name': 'pt_brimstone', 'tile': 'brimstone_ore', 'in': ['volcanic_ash', 'pumice_rock', 'stone'], 'size': [2, 6], 'per_100_columns': 8, 'depth': [4, 200]})
ocean = {'gap': [80, 220], 'length': [22, 100], 'peak': 14, 'great_every': 7, 'great_length': [170, 280], 'great_peak': 44, 'depth': [12, 36], 'wrecks': ['sunken_sloop', 'sunken_galleon', 'broken_hull', 'lost_cargo', 'drowned_shrine'], 'wrecks_per_1000_columns': 7, 'buried': 'treasure_mound', 'buried_per_1000_columns': 14}

# ==== more of everything, and longer makings ============================================================================================
def swap(out, old, new, count=None):
    for r_ in recipes:
        if r_['output'].get('item') != out: continue
        for i in r_['inputs']:
            if i['item'] == old: i['item'] = new; i['count'] = count or i['count']
def also(out, name, count=1):
    for r_ in recipes:
        if r_['output'].get('item') == out and all(i['item'] != name for i in r_['inputs']): r_['inputs'].append({'item': name, 'count': count})
# A hull is laid, then decked, then fitted out - and each size of ship is begun from the fitted hull of the size below.
EXTRA = [('steamed_strake', 'Steamed strake', 2, trinket('plate', c('c8a070')), 'sawpit', [('sawn_planking', 2), ('pitch', 1)], 2), ('deck_beam', 'Deck beam', 3, ingot(c('8a5a30')), 'sawpit', [('seasoned_timber', 2), ('treenails', 4)], 1),
         ('wrought_iron', 'Wrought iron', 3, ingot(c('5a5a66')), 'gun_foundry', [('iron_ingot', 2), ('charcoal', 2)], 2), ('tallow', 'Tallow', 2, barrel(c('e8e0c0')).outline(), 'smokehouse', [('dodo_meat', 2)], 2),
         ('deadeye', 'Deadeyes', 3, trinket('ring', OAK, ROPE), 'ropewalk', [('seasoned_timber', 1), ('tarred_rope', 1)], 4), ('capstan', 'Capstan', 4, P().r(5, 6, 10, 13, OAK).r(3, 4, 12, 5, dk(OAK)).r(6, 2, 9, 3, OAK).r(4, 13, 11, 14, c('5a5560')).outline(), 'shipwright', [('seasoned_timber', 4), ('wrought_iron', 2), ('tarred_rope', 2)], 1),
         ('ships_pump', "Ship's pump", 4, P().r(6, 3, 9, 13, c('7a7e8a')).r(4, 2, 11, 3, c('5a5560')).r(10, 8, 13, 9, c('7a7e8a')).r(5, 13, 10, 14, OAK).outline(), 'gun_foundry', [('wrought_iron', 3), ('copper_ingot', 2)], 1),
         ('rudder', 'Rudder', 3, P().r(7, 2, 8, 9, OAK).r(5, 9, 11, 14, OAK).r(5, 9, 5, 14, lt(OAK)).r(6, 5, 9, 5, c('5a5560')).outline(), 'shipwright', [('sawn_planking', 3), ('wrought_iron', 1)], 1)]
for n, lab, tier, pic, bench_, ins, count in EXTRA: comp(pic, n, lab, tier, 0.8); make(bench_, n, ins, 1, count, 160)
swap('ship_frame', 'sawn_planking', 'steamed_strake'); swap('iron_fittings', 'iron_ingot', 'wrought_iron', 1); also('keel_timber', 'steamed_strake', 2)
for k, (s_, lab, tier, g, below) in enumerate(SIZES):
    if k == 0: continue
    n = k + 1; q = lambda col, deck: P().r(2, 7, 13, 11, col).r(3, 12, 12, 12, dk(col)).r(2, 7, 13, 7, deck).p(13, 6, col).p(14, 5, col)
    comp(q(OAK, c('c8a070')).outline(), 'decked_%s_hull' % s_, 'Decked %s hull' % lab.lower(), tier, 4.0); make('shipwright', 'decked_%s_hull' % s_, [(s_ + '_hull', 1), ('deck_beam', 3 * n), ('sawn_planking', 4 * n), ('oakum', 3 * n), ('tallow', n)], g, 1, 600)
    comp(q(OAK, c('c8a070')).r(7, 2, 7, 6, c('5a3a22')).r(5, 3, 9, 3, CANVAS).outline(), 'fitted_%s_hull' % s_, 'Fitted %s hull' % lab.lower(), tier, 5.0)
    make('shipwright', 'fitted_%s_hull' % s_, [('decked_%s_hull' % s_, 1), ('rudder', 1), ('capstan', 1 if k < 3 else 2), ('ships_pump', 1), ('deadeye', 4 * n), ('harbour_lantern', n)], g, 1, 600)
    for ship in SHIPS: swap(ship[0], s_ + '_hull', 'fitted_%s_hull' % s_)
    if k + 1 < len(SIZES): swap(SIZES[k + 1][0] + '_hull', s_ + '_hull', 'fitted_%s_hull' % s_)
# Guns want boring, carriages and proving.
comp(P().r(3, 9, 12, 11, OAK).disc(5, 12, 1.8, dk(OAK)).disc(10, 12, 1.8, dk(OAK)).r(4, 7, 11, 8, dk(OAK)).outline(), 'gun_carriage', 'Gun carriage', 3, 1.0); make('shipwright', 'gun_carriage', [('seasoned_timber', 3), ('wrought_iron', 1), ('treenails', 4)], 1, 1, 200)
comp(P().r(2, 6, 13, 9, c('3a3a44')).r(2, 6, 13, 6, c('5a5a66')).r(13, 7, 14, 8, INK).outline(), 'bored_barrel', 'Bored gun barrel', 4, 1.5); make('gun_foundry', 'bored_barrel', [('wrought_iron', 4), ('bronze_ingot', 2), ('charcoal', 4)], 1, 1, 300)
for n, lab, tier, g, ins in GUNS: also(n, 'gun_carriage'); also(n, 'bored_barrel', 1 if n != 'carronade' else 2)

# ---- more islands -------------------------------------------------------------------------------------------------------------------------------
BLOCKS2 = [('reef_rock', 'Reef rock', 'rock', 'c8b8a0', 2, {}), ('cliff_stone', 'Cliff stone', 'rock', '6a6a70', 2, {'mine_time': 1.0}), ('guano_rock', 'Guano rock', 'rock', 'e0dcc8', 2, {}), ('black_beach', 'Black sand beach', 'rock', '2a2a30', 2, {'tool': 'shovel', 'mine_time': 0.2}),
           ('pink_sand', 'Pink sand', 'rock', 'f0c8c0', 2, {'tool': 'shovel', 'mine_time': 0.2}), ('peat_bog', 'Peat', 'rock', '3a2a1a', 2, {'tool': 'shovel', 'mine_time': 0.3}), ('plantation_soil', 'Plantation soil', 'rock', '6a3a22', 2, {'tool': 'shovel', 'mine_time': 0.3}),
           ('adobe_wall', 'Adobe wall', 'paving', 'd8b888', 2, {}), ('bamboo_wall', 'Bamboo wall', 'planks', 'c8c070', 1, {'tool': 'axe', 'burns': True}), ('grey_slate_roof', 'Slate roof', 'roof', '4a5060', 3, {}), ('ballast_stone', 'Ballast stone', 'brick', '5a5a5a', 2, {}), ('lighthouse_white', 'Lighthouse wall', 'paving', 'f4f4f0', 3, {})]
for n, lab, kind, col, tier, kw in BLOCKS2: COL[n] = c(col); block(block_tex(kind, n, c(col)), n, lab, tier, 0.04, **kw)
ISLE_SOIL += ['plantation_soil', 'peat_bog']
for t_ in tiles:
    if t_.get('soil') and 'jungle_soil' in t_['soil']: t_['soil'] = sorted(set(t_['soil']) | {'plantation_soil', 'peat_bog'})
for tr in trees: tr['soil'] = sorted(set(tr['soil']) | {'plantation_soil', 'pink_sand', 'black_beach'})
isle('coral_atoll', 'Coral atoll', [0.6, 1], [-0.3, 0.05], 'pink_sand', 'reef_rock', [('coconut', 1)], 9, [('pineapple', 30)])
isle('sea_cliffs', 'Sea cliffs', [-0.5, -0.1], [0.4, 1], 'cliff_stone', 'cliff_stone', [('teak', 1)], 30, [])
isle('guano_rocks', 'Bird rocks', [-0.1, 0.1], [0.5, 1], 'guano_rock', 'guano_rock', [('mangrove', 1)], 40, [])
isle('black_shore', 'Black shore', [0.5, 1], [0.05, 0.5], 'black_beach', 'volcanic_ash', [('plantain_palm', 1), ('coconut', 1)], 8, [('yam', 20)])
isle('plantation_isle', 'Plantation isle', [0.1, 0.3], [-0.3, 0.05], 'plantation_soil', 'plantation_soil', [('key_lime_tree', 1), ('breadfruit_tree', 1)], 14, [('sugar_cane', 4), ('tobacco_leaf', 6), ('sea_island_cotton', 7), ('indigo_plant', 10)])
isle('misty_moor', 'Misty isle', [-1, -0.5], [-1, 0.4], 'peat_bog', 'peat_bog', [('mangrove', 1), ('teak', 1)], 9, [('cassava_root', 18), ('yam', 18)])

# ---- the sea bed: what grows and what lies there ------------------------------------------------------------------------------------------------
def sea_thing(name, label, pic, drops_, one_in, tier=2, light=None):
    pic.save('tiles', name); tile(name, label, False, tool='machete', mine_time=0.3, drop=None, drops=drops_, soil=['sand', 'white_sand', 'pink_sand', 'reef_rock'], light=light, model='cross')
    plants.append({'biome': 'ocean', 'tile': name, 'one_in': one_in})
for n, lab, pic, tier, food in (('oyster', 'Oyster', P().disc(7.5, 9, 4, c('8a8a84')).disc(7.5, 8, 2.6, c('e8e8f0')).p(7, 8, WHITE).outline(), 2, 0.1), ('sea_sponge', 'Sea sponge', P().disc(7.5, 8, 4.5, c('e0b040')).p(6, 6, c('a07820')).p(9, 8, c('a07820')).p(7, 10, c('a07820')).outline(), 2, 0),
                               ('giant_clam_shell', 'Clam shell', trinket('plate', c('f0e0f0')), 3, 0), ('sea_urchin', 'Sea urchin', P().disc(7.5, 8, 3, c('3a2a4a')).line(2, 8, 13, 8, c('3a2a4a')).line(7, 2, 7, 14, c('3a2a4a')).line(3, 4, 12, 12, c('3a2a4a')).line(12, 4, 3, 12, c('3a2a4a')).outline(), 2, 0.1),
                               ('amphora', 'Amphora', trinket('pot', c('c07a4a'), c('8a4a2a')), 3, 0), ('sea_glass', 'Sea glass', gemstone(c('70d0b0')), 2, 0)):
    pic.save('items', n); item(n, lab, tier, 'nature', 0.4, food=food or None)
sea_thing('oyster_bed', 'Oyster bed', P().r(2, 12, 13, 15, c('6a6a64')).disc(5, 11, 2, c('8a8a84')).disc(10, 12, 2, c('9a9a94')).p(5, 10, WHITE), [{'item': 'oyster', 'count': 2}, {'item': 'pearl_string', 'count': 1, 'chance': 0.06}], 40)
sea_thing('sponge_bed', 'Sponge', P().disc(7.5, 11, 4, c('e0b040')).r(6, 6, 9, 9, c('e0b040')).p(6, 9, c('a07820')).p(9, 12, c('a07820')), [{'item': 'sea_sponge', 'count': 1}], 55)
sea_thing('giant_clam', 'Giant clam', P().r(2, 10, 13, 14, c('d0c0d8')).r(3, 8, 12, 9, c('f0e0f0')).r(4, 11, 11, 11, c('6040a0')).p(7, 11, c('60d0f0')), [{'item': 'giant_clam_shell', 'count': 1}, {'item': 'pearl_string', 'count': 1, 'chance': 0.3}], 160, 3, 5)
sea_thing('urchin_bed', 'Sea urchins', P().disc(5, 12, 2, c('3a2a4a')).disc(10, 13, 2, c('3a2a4a')).line(3, 9, 7, 14, c('3a2a4a')).line(8, 10, 12, 15, c('3a2a4a')), [{'item': 'sea_urchin', 'count': 2}], 70)
sea_thing('lost_amphora', 'Lost amphora', P().r(6, 4, 9, 6, c('c07a4a')).disc(7.5, 10, 3.6, c('c07a4a')).r(6, 14, 9, 15, c('8a4a2a')).p(6, 8, c('e0a070')), [{'item': 'amphora', 'count': 1}, {'item': 'gold_coin', 'count': 2, 'chance': 0.5}, {'item': 'old_carving', 'count': 1, 'chance': 0.15}], 220, 3)
sea_thing('sea_glass_bed', 'Sea glass', P().p(4, 14, c('70d0b0')).p(7, 13, c('a0e0f0')).p(10, 14, c('70d0b0')).p(6, 15, c('50b090')).p(12, 15, c('a0e0f0')), [{'item': 'sea_glass', 'count': 2}], 90)
make('chart_table', 'glass', [('sea_glass', 2)], 1, 1, 60); pay('islander', [('giant_clam_shell', 1)], 3); pay('company', [('sea_sponge', 6)], 2); pay('islander', [('amphora', 1)], 4, 2)
# Fish of the open sea, and what takes them.
for n, lab, body, acc, hp, size, chance, extra in (('snapper', 'Red snapper', 'e05040', 'f0a090', 8, [1, 1], 0.02, []), ('tuna', 'Tuna', '3a5a8a', 'c0d0e0', 20, [1, 2], 0.01, []), ('flying_fish', 'Flying fish', '60a0e0', 'e0f0ff', 6, [1, 1], 0.02, []),
                                                  ('grouper', 'Grouper', '6a5a3a', 'c0b080', 30, [1, 2], 0.008, []), ('marlin', 'Marlin', '2a4a9a', 'e0e8f0', 80, [2, 3], 0.003, [drop('marlin_bill', 1, 1)]), ('manta_ray', 'Manta ray', '2a2a3a', 'c0c0d0', 60, [2, 2], 0.003, [])):
    creature('fish', n, lab, body, acc, hp, [drop(n + '_fillet', 1, 2, None)] + extra, {'water': True, 'biome': 'ocean', 'chance': chance}, swim={}, size=size, move_chance=0.96, turn_chance=0.06, cap=6)
    meat(c(body)).save('items', n + '_fillet'); item(n + '_fillet', lab + ' fillet', 2, 'nature', 0.4, food=0.25)
fang(c('e0e8f0')).save('items', 'marlin_bill'); item('marlin_bill', 'Marlin bill', 4, 'nature', 2.0)
for metal, mlab, col, tier, dmg in (('iron', 'Iron', '8a8e9a', 3, 14), ('steel', 'Steel', 'c0c8d0', 4, 24), ('bronze', 'Bronze', 'c8a040', 2, 9)):
    thing(weapon('pike', c(col), ROPE), metal + '_harpoon', mlab + ' harpoon', tier, 'tools', 1.2, max_stack=1, damage=dmg, range=7); make('gun_foundry', metal + '_harpoon', [(metal + '_ingot', 2), ('seasoned_timber', 2), ('tarred_rope', 2)], 1, 1, 200)
for metal, mlab, col, tier, k in (('bronze', 'Bronze', 'c8a040', 2, 0.7), ('titanium', 'Titanium', 'd0e0f0', 6, 3.2)):
    for kind, lab, dmg, rng_, nm, cnt in (('cutlass', 'cutlass', 12, 0, 'cutlass', 3), ('axe', 'boarding axe', 16, 0, 'boarding_axe', 4), ('pike', 'boarding pike', 10, 2, 'boarding_pike', 2)):
        thing(weapon(kind, c(col)), '%s_%s' % (metal, nm), '%s %s' % (mlab, lab), tier, 'tools', 1.2, max_stack=1, damage=int(dmg * k), range=rng_ or None); make('gun_foundry', '%s_%s' % (metal, nm), [(metal + '_ingot', cnt), ('seasoned_timber', 2)], 1, 1, 200)
MEALS2 = [('grilled_snapper', 'Grilled snapper', 2, 'e05040', [('snapper_fillet', 2), ('key_lime', 1)], {'food': 0.6}), ('tuna_steak', 'Tuna steak', 3, '8a4a5a', [('tuna_fillet', 2), ('pepper_vine', 1)], {'food': 0.8, 'effect': lasts(3600, strength=2)}),
          ('oysters_on_ice', 'Fresh oysters', 2, 'e8e8f0', [('oyster', 4), ('key_lime', 1)], {'food': 0.4, 'effect': lasts(3600, luck=0.05)}), ('urchin_roe', 'Urchin roe', 3, 'f0a030', [('sea_urchin', 3)], {'food': 0.4, 'effect': lasts(3600, lung=2.0)}),
          ('marlin_feast', 'Marlin feast', 4, '5a7ab0', [('marlin_fillet', 2), ('plantain', 2), ('key_lime', 1)], {'food': 1.0, 'effect': lasts(6000, strength=3, walk=1.15)}), ('grouper_chowder', 'Grouper chowder', 3, 'e0d0a0', [('grouper_fillet', 2), ('yam', 2)], {'food': 0.9}),
          ('ray_wing', 'Ray wing', 3, '8a8a9a', [('manta_ray_fillet', 2), ('pineapple', 1)], {'food': 0.8, 'effect': lasts(6000, glide=1.0)}), ('flying_fish_fry', 'Flying-fish fry', 2, '60a0e0', [('flying_fish_fillet', 3)], {'food': 0.6, 'effect': lasts(2400, walk=1.2)}),
          ('sugar_candy', 'Rock candy', 2, 'f0f0e8', [('sugar_loaf', 1)], {'food': 0.2, 'effect': lasts(2400, stamina=1.4)}), ('rum_cake', 'Rum cake', 3, 'c8a060', [('flour', 2), ('sugar_loaf', 1), ('rum_bottle', 1)], {'food': 0.7, 'effect': lasts(3600, warmth=0.4)}),
          ('spiced_rum', 'Spiced rum', 4, 'a85a20', [('aged_rum', 1), ('pepper_vine', 1), ('sugar_loaf', 1)], {'cupped': True, 'effect': lasts(4800, strength=3, warmth=0.6)}), ('bumbo', 'Bumbo', 3, 'd0a060', [('rum_bottle', 1), ('sugar_loaf', 1), ('pepper_vine', 1)], {'cupped': True, 'effect': lasts(3600, luck=0.1, stamina=1.3)})]
for n, lab, tier, col, ins, kw in MEALS2: meal(n, lab, tier, col, ins, 'ships_galley', kw.pop('cupped', False), **kw)

# ---- more for a harbour ----------------------------------------------------------------------------------------------------------------------------
MORE_DECO = [('tavern_table', 'Tavern table', P().r(2, 9, 13, 10, OAK).r(3, 11, 4, 15, dk(OAK)).r(11, 11, 12, 15, dk(OAK)).r(5, 6, 6, 8, c('3a6a4a')).r(9, 7, 10, 8, BRASS), [('sawn_planking', 2)], 'sawpit', {}),
             ('tavern_stool', 'Stool', P().r(5, 10, 10, 11, OAK).r(5, 12, 6, 15, dk(OAK)).r(9, 12, 10, 15, dk(OAK)), [('sawn_planking', 1)], 'sawpit', {}), ('rum_rack', 'Rum rack', P().r(2, 3, 13, 14, dk(OAK)).disc(5, 6, 1.8, OAK).disc(10, 6, 1.8, OAK).disc(5, 11, 1.8, OAK).disc(10, 11, 1.8, OAK), [('empty_cask', 2), ('sawn_planking', 2)], 'cooperage', {}),
             ('ship_in_bottle', 'Ship in a bottle', P().r(3, 7, 12, 12, c('a0d0c0')).r(13, 9, 14, 10, c('a0d0c0')).r(6, 10, 9, 10, OAK).r(7, 8, 7, 9, CANVAS).r(4, 13, 11, 14, dk(OAK)), [('glass', 2), ('treenails', 2), ('sailcloth', 1)], 'chart_table', {}),
             ('parrot_perch', 'Parrot perch', P().r(7, 5, 8, 15, OAK).r(4, 5, 11, 5, OAK).r(5, 2, 6, 4, c('e03030')).p(7, 3, c('f0d040')), [('seasoned_timber', 2), ('parrot_feather', 2)], 'sawpit', {}),
             ('treasure_pile', 'Treasure pile', P().r(2, 11, 13, 15, c('f0c840')).r(4, 9, 11, 10, c('f0c840')).p(5, 9, WHITE).p(9, 12, c('e03030')).p(11, 13, c('30a0e0')).p(4, 13, c('40c060')), [('gold_coin', 60), ('silver_bar', 2)], 'captains_desk', {'light': 7}),
             ('sea_chart_wall', 'Wall chart', P().r(2, 3, 13, 12, PAPER).r(2, 3, 13, 3, dk(PAPER, 0.85)).line(4, 9, 7, 6, SEA).line(7, 6, 11, 8, SEA).p(9, 5, RED).r(11, 10, 12, 11, c('8a5a30')), [('linen', 2), ('octopus_ink', 1)], 'chart_table', {}),
             ('lobster_pot', 'Lobster pot', P().r(3, 7, 12, 14, c('a07840')).line(3, 7, 12, 14, dk(OAK)).line(12, 7, 3, 14, dk(OAK)).r(6, 5, 9, 6, ROPE), [('cordage', 3), ('seasoned_timber', 1)], 'ropewalk', {}),
             ('rope_coil', 'Rope coil', P().disc(7.5, 11, 4.5, ROPE).disc(7.5, 11, 2.5, dk(ROPE)).disc(7.5, 11, 1.0, ROPE), [('tarred_rope', 3)], 'ropewalk', {}), ('ships_bell', "Ship's bell", P().r(3, 2, 12, 3, OAK).r(7, 3, 8, 5, ROPE).r(5, 5, 10, 11, BRASS).r(4, 11, 11, 12, BRASS).p(7, 13, c('5a5560')), [('bronze_ingot', 3), ('seasoned_timber', 1)], 'gun_foundry', {}),
             ('powder_magazine', 'Powder store', P().r(3, 5, 12, 14, c('3a3a44')).r(3, 5, 12, 5, c('5a5a66')).r(6, 8, 9, 11, RED), [('wrought_iron', 4), ('powder_keg', 1)], 'gun_foundry', {'storage': {'columns': 4, 'rows': 2}, 'model': 'box'}),
             ('lighthouse_lamp', 'Lighthouse lamp', P().r(4, 12, 11, 15, c('5a5560')).r(5, 4, 10, 11, c('ffe890')).r(5, 4, 5, 11, BRASS).r(10, 4, 10, 11, BRASS).r(4, 2, 11, 3, BRASS), [('bronze_ingot', 4), ('glass', 6), ('whale_oil', 1)], 'gun_foundry', {'light': 15, 'pulse': 0.2}),
             ('signal_fire', 'Signal fire', P().r(3, 12, 12, 15, c('5a5a5a')).r(5, 7, 10, 11, c('f06020')).r(6, 4, 9, 7, c('ffc040')).p(7, 2, c('ffe890')), [('stone', 6), ('wood', 4), ('pitch', 1)], 'sawpit', {'light': 14, 'hot': True}),
             ('figurehead_stand', 'Figurehead', P().r(6, 9, 9, 15, OAK).r(5, 4, 10, 9, c('e0b030')).r(6, 2, 9, 4, c('e0b890')).p(7, 3, INK), [('hardwood', 4), ('gold_ingot', 1)], 'shipwright', {}),
             ('fish_barrel', 'Fish barrel', barrel(c('7a8a8a')).r(5, 2, 10, 2, c('a0b0c0')), [('empty_cask', 1), ('fish', 4)], 'cooperage', {}), ('sail_rack', 'Drying sail', P().r(1, 2, 14, 3, OAK).r(2, 4, 13, 12, CANVAS).line(2, 12, 13, 4, dk(CANVAS, 0.85)), [('sailcloth', 2), ('seasoned_timber', 1)], 'sail_loft', {}),
             ('hermit_fire', 'Camp fire', P().r(4, 13, 11, 15, c('5a5a5a')).r(6, 9, 9, 12, c('f06020')).p(7, 7, c('ffc040')), [('stone', 3), ('stick', 4)], 'sawpit', {'light': 12}), ('skull_pile', 'Skull pile', P().disc(5, 12, 2.5, c('e8e0d0')).disc(10, 12, 2.5, c('e8e0d0')).disc(7.5, 8, 2.5, c('e8e0d0')).p(4, 12, INK).p(6, 12, INK).p(7, 8, INK).p(9, 8, INK), [('old_carving', 1)], 'captains_desk', {})]
for n, lab, pic, ins, bench_, kw in MORE_DECO:
    deco(pic, n, lab, 3, ins, bench_, **kw)
    if kw.get('storage'): items[-1]['container'] = {'slots': 8}
for p_, lab, col, coin in POWERS:
    deco(P().r(7, 1, 8, 15, OAK).r(9, 2, 14, 7, c(col)).r(9, 2, 14, 2, lt(c(col), 1.3)).r(10, 4, 12, 5, WHITE if p_ != 'islander' else c('2a2a2a')), p_ + '_flagpole', lab + ' flag', 2, [('sailcloth', 2), ('seasoned_timber', 2), (p_ + '_mark', 2)], 'sail_loft', light=3)
STAIRED = ['quay_stone', 'fort_stone', 'harbour_brick', 'whitewash', 'ship_planks', 'tarred_planks', 'cliff_stone', 'adobe_wall', 'ballast_stone', 'hardwood']
for n in STAIRED:
    lab, tier = next(i['label'] for i in items if i['name'] == n), next(i['tier'] for i in items if i['name'] == n)
    for what, base in (('column', 'wood_column'), ('stairs', 'wood_stairs')):
        d = copy.deepcopy(BT[base]); d.update(name='%s_%s' % (n, what), label='%s %s' % (lab, what), drop='%s_%s' % (n, what), texture='tiles/%s_%s.png' % (n, what))
        Image.open(os.path.join(TEX, 'tiles', n + '.png')).save(os.path.join(TEX, 'tiles', '%s_%s.png' % (n, what)))
        tiles.append(d); item('%s_%s' % (n, what), '%s %s' % (lab, what), tier, 'blocks', 0.04, places='%s_%s' % (n, what)); make('sawpit', '%s_%s' % (n, what), [(n, 1)], 1, 2, 60)
for n, ins, bench_ in (('adobe_wall', [('clay', 3), ('stick', 2)], 'sawpit'), ('bamboo_wall', [('sugar_cane', 6)], 'sawpit'), ('grey_slate_roof', [('stone', 4)], 'sawpit'), ('ballast_stone', [('stone', 4), ('gravel', 1)], 'sawpit'), ('lighthouse_white', [('limestone', 3), ('clay', 1)], 'sawpit'), ('reef_rock', [('coral', 2), ('sand', 2)], 'sawpit')):
    make(bench_, n, ins, 1, 4, 60)

# ---- more places ashore ----------------------------------------------------------------------------------------------------------------------------
LIGHT = style('lighthouse_white', 'ballast_stone', 'grey_slate_roof'); LIGHT['*'] = L(None, None, 'lighthouse_lamp'); LIGHT['f'] = L(None, None, 'signal_fire')
PLANT = style('adobe_wall', 'quay_stone', 'red_tile_roof'); PLANT['u'] = L(None, 'adobe_wall', 'sugar_mill'); PLANT['t'] = L(None, 'adobe_wall', 'tavern_table'); PLANT['v'] = L(None, 'adobe_wall', 'rum_rack'); PLANT['f'] = L(None, None, 'company_flagpole')
RUIN = style('ballast_stone', 'ballast_stone', 'ballast_stone'); RUIN['z'] = L(None, 'ballast_stone', 'skull_pile'); RUIN['t'] = L(None, 'ballast_stone', 'treasure_pile'); RUIN['f'] = L(None, None, 'hermit_fire'); RUIN['j'] = L(None, None, 'brethren_flagpole')
structure('lighthouse_tower', ['~~*~~', '~###~', '~#.#~', '~#W#~', '~#.#~', '~#W#~', '~#.#~', '~#l#~', '~D.D~', '~FFF~'], LIGHT)
structure('keepers_cottage', ['~RRRRR~', '~#.l.#~', '~D.hsD~', '~FFFFF~'], LIGHT)
structure('lighthouse_yard', ['~f~~~S~', '~FFFFF~'], LIGHT)
structure('great_house', ['~~~RRRRRRRRRR~~~', '~RRRRRRRRRRRRRR~', '~#.W..l..l..W.#~', '~D............D~', '~D.t.v..s..d.BD~', '~FFFFFFFFFFFFFF~'], PLANT)
structure('boiling_house', ['~RRRRRRRR~', '~#.l..l.#~', '~D.u.e.bD~', '~FFFFFFFF~'], PLANT)
structure('plantation_store', ['~RRRRRR~', '~#.l..#~', '~D.c.cD~', '~FFFFFF~'], PLANT)
structure('plantation_gate', ['~f~~~~f~', '~#~~~~#~', '~FFFFFF~'], PLANT)
structure('ruined_keep', ['#~#~~~~#~#', '##.~~~~.##', '~#.z~~t.#~', '~~......~~', '~FFFFFFFF~'], RUIN)
structure('ruined_wall', ['#~~#~~#', '##~#~##', 'FFFFFFF'], RUIN)
structure('marooners_camp', ['~~~~~j~', '~f~h~~~', '~FFFFF~'], RUIN)
structure('smugglers_store', ['#########', '#.l.x.l.#', 'D..c.b..D', 'FFFFFFFFF'], SHACK)
structure('smugglers_bunks', ['#######', '#.l.h.#', 'D.h.b.D', 'FFFFFFF'], SHACK)
structure('whalers_shed', ['~RRRRRRR~', '~#.l...#~', '~D.o.bbD~', '~FFFFFFF~'], SHACK)
structure('whalers_tryworks', ['~~f~~~f~~', '~FFFFFFF~'], dict(SHACK, f=L(None, None, 'signal_fire')))
town('lighthouse_rock', 'lighthouse_tower', ['keepers_cottage', 'lighthouse_yard'], 0.8, [1, 2], 'ballast_stone')
town('plantation', 'great_house', ['boiling_house', 'plantation_store', 'plantation_gate', 'plantation_store'], 0.7, [2, 4], 'quay_stone')
town('old_ruin', 'ruined_keep', ['ruined_wall', 'marooners_camp', 'ruined_wall'], 0.9, [1, 3], 'ballast_stone')
town('whaling_station', 'fish_market', ['whalers_shed', 'whalers_tryworks', 'fisher_hut'], 0.5, [2, 3], 'pier_planks', 0)
villages.append({'name': 'smugglers_den', 'where': 'cave', 'at_least': 1, 'per_1000_columns': 0.8, 'houses': [1, 2], 'centre': 'smugglers_store', 'street': 'pier_planks', 'ground': 'stone', 'buildings': ['smugglers_bunks']})
# Letters and papers.
for n, lab, col, boons, p_, marks, g in (('letter_of_marque', 'Letter of marque', '2a4a9a', {'luck': 0.1, 'armor': 0.05}, 'crown', 20, 2), ('company_charter', 'Company charter', '8a5a2a', {'craft': 1.3, 'luck': 0.1}, 'company', 40, 3),
                                        ('black_spot', 'The black spot', '1a1a1a', {'strength': 4, 'unseen': 0.5}, 'brethren', 30, 3), ('kinship_token', 'Token of kinship', 'c08030', {'lung': 3.0, 'regen': 2.0}, 'islander', 30, 3), ('admirals_commission', "Admiral's commission", 'f0c840', {'armor': 0.15, 'strength': 3, 'luck': 0.2}, 'crown', 250, 5),
                                        ('pirate_lords_ring', "Pirate lord's ring", 'a02828', {'strength': 5, 'luck': 0.3}, 'brethren', 250, 5), ('governors_seal', "Governor's seal", '6a4a2a', {'craft': 1.8, 'luck': 0.3, 'magnet': 1.0}, 'company', 250, 5), ('elders_carving', "Elder's carving", '6a7a6a', {'lung': 20.0, 'regen': 4.0, 'sight': 1.0}, 'islander', 250, 5)):
    thing(trinket('paper', PAPER).r(6, 5, 9, 9, c(col)).r(7, 11, 8, 12, RED).outline() if 'ring' not in n and 'carving' not in n else trinket('ring' if 'ring' in n else 'rune', c(col)), n, lab, 4 if g < 5 else 6, 'special', 3.0, max_stack=1, relic=True, boons=boons); buy(p_, n, marks, g)

# ==== the second pass: trades ashore, instruments, more ships, more to find ============================================================
def bench2(kind, lab, draw, grades, ins, lvl, fuel=False):
    for g in range(1, grades + 1):
        n = kind if g == 1 else '%s_%d' % (kind, g)
        draw(GRADE[g - 1]).save('tiles', n); icon_of(n)
        tile(n, lab, False, tool='axe', mine_time=0.4, bench=True, bench_kind=kind, bench_level=g, fuel_capacity=3000 if fuel else None,
             upgrade={'to': '%s_%d' % (kind, g + 1), 'cost': [{'item': 'bronze_ingot', 'count': 6 * g}, {'item': 'wrought_iron', 'count': 4 * g}, {'item': 'gold_coin', 'count': 30 * g}]} if g < grades else None)
        item(n, lab + (' ' + NUM[g] if grades > 1 else ''), min(6, g + 1), 'benches', 3.0, places=n)
    rec('workbench', kind, ins, level=lvl)
IVORY = c('f0e8d0')
bench2('dye_works', 'Dye works', lambda t: P().r(1, 2, 14, 2, OAK).r(3, 3, 5, 6, c('2a4a9a')).r(9, 3, 11, 7, c('c03030')).r(1, 9, 4, 14, c('5a5560')).r(6, 9, 9, 14, c('5a5560')).r(11, 9, 14, 14, c('5a5560')).r(1, 9, 4, 9, c('2a4a9a')).r(6, 9, 9, 9, c('c03030')).r(11, 9, 14, 9, c('e0b030')).r(1, 15, 14, 15, t), 2, [('wood', 10), ('clay', 8), ('copper_ingot', 4)], 2)
bench2('weavers_loom', "Weaver's loom", lambda t: P().r(2, 1, 3, 15, OAK).r(12, 1, 13, 15, OAK).r(2, 1, 13, 2, dk(OAK)).r(2, 12, 13, 13, dk(OAK)).r(4, 3, 4, 11, IVORY).r(6, 3, 6, 11, IVORY).r(8, 3, 8, 11, IVORY).r(10, 3, 10, 11, IVORY).r(4, 8, 11, 11, c('e8e8f0')).r(2, 1, 13, 1, t), 1, [('wood', 14), ('string', 12)], 1)
bench2('curing_shed', 'Curing shed', lambda t: P().r(1, 2, 14, 3, c('6a4a2a')).r(2, 4, 2, 15, OAK).r(13, 4, 13, 15, OAK).r(4, 4, 5, 9, c('8a6a2a')).r(7, 4, 8, 10, c('7a5a22')).r(10, 4, 11, 8, c('8a6a2a')).r(1, 2, 14, 2, t), 1, [('wood', 14), ('stick', 8)], 1)
bench2('chandlery', 'Chandlery', lambda t: P().r(1, 10, 14, 11, OAK).r(2, 12, 3, 15, dk(OAK)).r(12, 12, 13, 15, dk(OAK)).r(3, 6, 4, 9, IVORY).r(7, 5, 8, 9, IVORY).r(11, 7, 12, 9, IVORY).p(3, 5, c('ffc040')).p(7, 4, c('ffc040')).p(11, 6, c('ffc040')).r(1, 10, 14, 10, t), 1, [('wood', 10), ('iron_ingot', 2), ('clay', 4)], 1, True)
bench2('sea_jeweller', "Jeweller's bench", lambda t: P().r(1, 9, 14, 10, c('6a2a1a')).r(2, 11, 3, 15, c('4a1a10')).r(12, 11, 13, 15, c('4a1a10')).disc(5, 7, 1.6, WHITE).disc(9, 7, 1.6, c('60d0f0')).r(11, 4, 12, 8, BRASS).disc(11.5, 3.5, 1.6, c('a0d0e0')).r(1, 9, 14, 9, t), 2, [('wood', 10), ('gold_ingot', 2), ('glass', 2)], 3)
bench2('instrument_maker', "Instrument maker's", lambda t: P().r(1, 10, 14, 11, OAK).r(2, 12, 3, 15, dk(OAK)).r(12, 12, 13, 15, dk(OAK)).line(4, 9, 7, 3, BRASS).line(7, 3, 10, 9, BRASS).line(4, 9, 10, 9, BRASS).p(7, 6, c('a0d0e0')).r(12, 7, 13, 9, BRASS).r(1, 10, 14, 10, t), 3, [('wood', 10), ('bronze_ingot', 6), ('glass', 4)], 3)
bench2('gunsmith', "Gunsmith's bench", lambda t: P().r(1, 10, 14, 11, c('5a5560')).r(2, 12, 3, 15, c('3a3a44')).r(12, 12, 13, 15, c('3a3a44')).r(3, 7, 10, 8, c('2a2a2a')).r(9, 8, 11, 9, OAK).r(12, 6, 13, 9, c('7a7e8a')).r(1, 10, 14, 10, t), 3, [('stone', 12), ('iron_ingot', 8), ('wood', 6)], 3, True)
bench2('ships_surgery', "Ship's surgery", lambda t: P().r(2, 7, 13, 14, c('e8e8f0')).r(2, 7, 13, 7, t).r(7, 9, 8, 12, RED).r(6, 10, 9, 11, RED).r(4, 3, 5, 6, c('3a6a4a')).r(10, 4, 11, 6, c('8a4a2a')).r(3, 14, 12, 15, OAK), 1, [('wood', 8), ('linen', 6), ('glass', 2)], 2)
bench2('scrimshaw_bench', 'Carving block', lambda t: P().r(3, 9, 12, 15, OAK).r(3, 9, 12, 9, t).line(4, 8, 10, 4, IVORY).line(5, 8, 11, 4, IVORY).p(11, 3, IVORY).r(12, 6, 13, 8, c('7a7e8a')), 1, [('wood', 10), ('flint', 2)], 1)

# ---- wild things ashore ----------------------------------------------------------------------------------------------------------------------------
BEACH, GROUND = ['white_sand', 'pink_sand', 'black_beach', 'sand'], ['jungle_grass', 'jungle_soil', 'grass', 'dirt', 'plantation_soil', 'peat_bog', 'volcanic_ash', 'mangrove_mud']
def wild(name, label, pic, drops_, soil, where, light=None):
    pic.save('tiles', name); tile(name, label, False, tool='machete', mine_time=0.3, drop=None, drops=drops_, soil=soil, light=light, model='cross')
    for b, n in where: plants.append({'biome': b, 'tile': name, 'one_in': n if b == 'ocean' else max(5, n // 3)})
RAW = [('annatto_seed', 'Annatto seed', P().disc(7.5, 8, 4, c('c03020')).p(6, 6, c('f06040')).p(9, 9, c('801810')).outline()), ('logwood_chips', 'Logwood chips', P().r(3, 8, 8, 10, c('6a2a3a')).r(7, 5, 12, 7, c('8a3a4a')).r(5, 11, 11, 12, c('5a2030')).outline()),
       ('vanilla_pod', 'Vanilla pod', P().line(4, 13, 10, 3, c('3a2a1a')).line(6, 13, 12, 4, c('4a3220')).outline()), ('wild_ginger', 'Wild ginger', P().disc(6, 9, 3, c('d0b070')).disc(10, 8, 2.4, c('d0b070')).r(6, 4, 6, 6, c('4a9a3a')).outline()),
       ('aloe_leaf', 'Aloe leaf', P().line(7, 14, 4, 3, c('4a9a5a')).line(8, 14, 8, 2, c('5ab06a')).line(9, 14, 12, 4, c('4a9a5a')).outline()), ('conch_shell', 'Conch shell', P().disc(8, 9, 4, c('f0c0b0')).r(3, 8, 6, 10, c('f0d8c8')).p(9, 8, c('e08090')).p(11, 6, c('f0d8c8')).outline()),
       ('murex_shell', 'Murex shell', P().disc(7.5, 8, 3.5, c('c0a0c0')).p(4, 5, c('c0a0c0')).p(11, 5, c('c0a0c0')).p(7, 12, c('7a3a8a')).p(8, 13, c('7a3a8a')).outline()), ('sea_grapes', 'Sea grapes', P().disc(6, 7, 2, c('7a3a8a')).disc(9, 8, 2, c('8a4a9a')).disc(7, 10, 2, c('6a2a7a')).r(7, 3, 8, 5, c('4a9a3a')).outline()),
       ('driftwood', 'Driftwood', P().r(2, 8, 13, 10, c('b0a890')).r(9, 5, 10, 8, c('b0a890')).r(2, 8, 13, 8, c('d0c8b0')).outline()), ('message_bottle', 'Message in a bottle', bottle(PAPER, c('70b0a0')))]
for n, lab, pic in RAW: pic.save('items', n); item(n, lab, 2, 'nature', 0.4, food=0.15 if n == 'sea_grapes' else None)
wild('annatto_bush', 'Annatto bush', P().disc(7.5, 8, 5, c('3a8a3a')).disc(5, 7, 1.4, c('c03020')).disc(10, 9, 1.4, c('c03020')).disc(8, 5, 1.4, c('e04030')).r(7, 12, 8, 15, c('6a4a2a')), [{'item': 'annatto_seed', 'count': 2}], GROUND, [('jungle_isle', 30), ('spice_isle', 24), ('plantation_isle', 30)])
wild('logwood_shrub', 'Logwood', P().r(7, 6, 8, 15, c('6a2a3a')).line(7, 9, 3, 5, c('6a2a3a')).line(8, 8, 12, 4, c('6a2a3a')).disc(3, 4, 2, c('5a8a3a')).disc(12, 3, 2, c('5a8a3a')).disc(7.5, 4, 2.4, c('6a9a3a')), [{'item': 'logwood_chips', 'count': 2}], GROUND, [('mangrove_isle', 14), ('jungle_isle', 30), ('misty_moor', 24)])
wild('vanilla_orchid', 'Vanilla orchid', P().line(7, 15, 7, 5, c('4a9a3a')).disc(5, 5, 2, c('f0f0c0')).disc(10, 6, 2, c('f0f0c0')).p(5, 5, c('e0b030')).p(10, 6, c('e0b030')).line(8, 9, 11, 12, c('3a2a1a')), [{'item': 'vanilla_pod', 'count': 1}], GROUND, [('jungle_isle', 50), ('spice_isle', 40)])
wild('ginger_patch', 'Wild ginger', P().line(5, 15, 4, 6, c('4a9a3a')).line(8, 15, 8, 4, c('5ab04a')).line(11, 15, 12, 6, c('4a9a3a')).disc(8, 4, 1.4, c('e04060')), [{'item': 'wild_ginger', 'count': 2}], GROUND, [('jungle_isle', 34), ('spice_isle', 30), ('black_shore', 40)])
wild('aloe_plant', 'Aloe', P().line(7, 15, 3, 6, c('4a9a5a')).line(8, 15, 8, 3, c('5ab06a')).line(9, 15, 13, 6, c('4a9a5a')).line(7, 15, 5, 9, c('5ab06a')).line(9, 15, 11, 9, c('5ab06a')), [{'item': 'aloe_leaf', 'count': 2}], BEACH + GROUND, [('palm_cay', 30), ('black_shore', 30), ('sea_cliffs', 50), ('volcanic_isle', 40)])
wild('conch_on_sand', 'Conch', P().disc(8, 12, 3, c('f0c0b0')).r(4, 12, 6, 13, c('f0d8c8')).p(9, 11, c('e08090')), [{'item': 'conch_shell', 'count': 1}], BEACH, [('palm_cay', 24), ('coral_atoll', 16), ('ocean', 120)])
wild('murex_bed', 'Murex shells', P().disc(5, 13, 2, c('c0a0c0')).disc(10, 12, 2, c('b090b0')).p(5, 15, c('7a3a8a')).p(10, 14, c('7a3a8a')), [{'item': 'murex_shell', 'count': 2}], BEACH + ['reef_rock'], [('coral_atoll', 22), ('ocean', 110)])
wild('sea_grape_bush', 'Sea grape', P().disc(7.5, 8, 5, c('4a9a4a')).disc(5, 9, 1.4, c('7a3a8a')).disc(9, 7, 1.4, c('8a4a9a')).disc(8, 11, 1.4, c('6a2a7a')).r(7, 13, 8, 15, c('6a4a2a')), [{'item': 'sea_grapes', 'count': 2}], BEACH + GROUND, [('palm_cay', 18), ('coral_atoll', 20), ('black_shore', 22)])
wild('driftwood_pile', 'Driftwood', P().r(2, 12, 13, 14, c('b0a890')).line(4, 12, 9, 7, c('b0a890')).line(8, 12, 13, 9, c('c0b8a0')), [{'item': 'driftwood', 'count': 2}, {'item': 'cordage', 'count': 1, 'chance': 0.2}], BEACH, [('palm_cay', 14), ('skull_coast', 10), ('black_shore', 14), ('coral_atoll', 14), ('guano_rocks', 12)])
wild('bottle_on_sand', 'Bottle', P().r(6, 9, 9, 14, c('70b0a0')).r(7, 6, 8, 8, c('70b0a0')).r(7, 5, 8, 5, c('8a5a30')).r(7, 10, 8, 12, PAPER), [{'item': 'message_bottle', 'count': 1}], BEACH, [('palm_cay', 60), ('skull_coast', 40), ('coral_atoll', 50), ('black_shore', 60)], 3)
wild('castaway_bones', 'Castaway', P().disc(4, 12, 2.4, c('e8e0d0')).p(3, 12, INK).p(5, 12, INK).r(7, 12, 13, 13, c('e8e0d0')).r(9, 10, 9, 14, c('e8e0d0')).r(11, 10, 11, 14, c('e8e0d0')), [{'item': 'old_carving', 'count': 1}, {'item': 'gold_coin', 'count': 3, 'chance': 0.6}, {'item': 'treasure_map', 'count': 1, 'chance': 0.2}, {'item': 'iron_cutlass', 'count': 1, 'chance': 0.2}],
     BEACH + ['skull_rock', 'guano_rock'], [('skull_coast', 16), ('guano_rocks', 30), ('palm_cay', 90), ('black_shore', 70)])
rec('chart_table', 'treasure_map', [('message_bottle', 2)], time=120); make('sawpit', 'driftwood_block', [('driftwood', 3)], 1, 2, 60); pay('islander', [('conch_shell', 2)], 2); pay('islander', [('murex_shell', 4)], 2)

# ---- goods: what an island grows, worked up into what a harbour pays for ----------------------------------------------------------------------------
def good(name, label, tier, pic, bench_, ins, g=1, count=1, cat='crafted', mult=0.8, time=200, **kw): thing(pic, name, label, tier, cat, mult, **kw); make(bench_, name, ins, g, count, time)
DYES = [('indigo', 'Indigo', '2a4a9a', [('indigo_plant', 3)]), ('scarlet', 'Scarlet', 'c03020', [('annatto_seed', 3)]), ('logwood', 'Logwood black', '2a1a2a', [('logwood_chips', 3)]), ('royal_purple', 'Royal purple', '7a3a8a', [('murex_shell', 4)]), ('saffron', 'Saffron', 'e0b030', [('wild_ginger', 2), ('annatto_seed', 1)]), ('sea_green', 'Sea green', '3a9a7a', [('kelp', 3), ('indigo_plant', 1)])]
good('cotton_yarn', 'Cotton yarn', 2, trinket('thread', c('f0f0f8')), 'weavers_loom', [('sea_island_cotton', 2)], 1, 2)
good('calico', 'Calico', 2, trinket('cloth', c('f0f0e8')), 'weavers_loom', [('cotton_yarn', 3)])
good('fine_lawn', 'Fine lawn', 3, trinket('cloth', c('f8f8ff'), c('c0d0e0')), 'weavers_loom', [('calico', 2), ('cotton_yarn', 2)])
good('lace_trim', 'Lace', 4, trinket('fan', c('f8f8ff')), 'weavers_loom', [('fine_lawn', 1), ('cotton_yarn', 4)])
good('mordant', 'Mordant', 2, barrel(c('c0c0b0')).outline(), 'dye_works', [('nitre', 1), ('salt', 1)], 1, 2)
for n, lab, col, ins in DYES:
    good(n + '_dye', lab + ' dye', 3 if n != 'royal_purple' else 5, trinket('pot', c(col), c('5a5560')), 'dye_works', ins + [('mordant', 1)], 2 if n == 'royal_purple' else 1, 2)
    good(n + '_calico', lab + ' calico', 3 if n != 'royal_purple' else 5, trinket('cloth', c(col)), 'dye_works', [('calico', 2), (n + '_dye', 1)], 2 if n == 'royal_purple' else 1, 2, 'cargo', 1.2)
    pay('company', [(n + '_calico', 2)], 3 if n != 'royal_purple' else 12, 1 if n != 'royal_purple' else 3)
good('cloth_of_state', 'Cloth of state', 6, trinket('cloth', c('7a3a8a'), GOLD), 'weavers_loom', [('royal_purple_calico', 2), ('lace_trim', 2), ('gold_ingot', 1)], 1, 1, 'cargo', 2.0); pay('crown', [('cloth_of_state', 1)], 40, 4)
# Tobacco.
good('cured_leaf', 'Cured leaf', 2, trinket('fan', c('8a6a2a'), c('6a4a1a')), 'curing_shed', [('tobacco_leaf', 2)], 1, 2, time=400)
good('twist_tobacco', 'Twist tobacco', 3, P().line(3, 12, 12, 4, c('6a4a1a')).line(4, 13, 13, 5, c('8a6a2a')).line(3, 10, 10, 3, c('5a3a12')).outline(), 'curing_shed', [('cured_leaf', 3), ('molasses', 1)], 1, 2, 'cargo')
good('cigar', 'Cigar', 3, P().r(3, 7, 12, 9, c('7a4a1a')).r(3, 7, 12, 7, c('9a6a2a')).r(6, 7, 7, 9, GOLD).p(13, 8, c('f06020')).outline(), 'curing_shed', [('cured_leaf', 2)], 1, 2, 'nature', 1.0, max_stack=10, effect=lasts(2400, luck=0.08))
good('snuff', 'Snuff', 3, trinket('dust', c('8a5a2a')), 'sugar_mill', [('cured_leaf', 2), ('vanilla_pod', 1)], 1, 2)
good('cigar_box', 'Box of cigars', 4, P().r(2, 6, 13, 13, c('8a3a1a')).r(2, 6, 13, 7, c('a04a2a')).r(4, 9, 11, 11, c('e0b030')).outline(), 'cooperage', [('cigar', 8), ('sawn_planking', 1)], 1, 1, 'cargo', 1.6)
# The chandler's.
good('tallow_dip', 'Tallow dips', 2, P().r(4, 5, 5, 13, IVORY).r(7, 4, 8, 13, IVORY).r(10, 6, 11, 13, IVORY).r(3, 3, 12, 3, ROPE).outline(), 'chandlery', [('tallow', 1), ('cordage', 1)], 1, 4)
good('hard_soap', 'Soap', 2, P().r(3, 7, 12, 12, c('e8d8a0')).r(3, 7, 12, 7, c('f8f0c8')).r(5, 9, 10, 10, c('c8b880')).outline(), 'chandlery', [('tallow', 2), ('charcoal', 1), ('salt', 1)], 1, 2, 'cargo')
good('lamp_oil', 'Lamp oil', 3, bottle(c('f0d070'), c('a0b0a0')), 'chandlery', [('whale_oil', 1), ('glass', 2)], 1, 3, 'cargo')
good('spermaceti', 'Spermaceti', 4, barrel(c('f8f8f0')).outline(), 'chandlery', [('whale_oil', 3)], 1, 1)
good('fine_candles', 'Fine candles', 4, P().r(4, 5, 5, 13, WHITE).r(7, 3, 8, 13, WHITE).r(10, 5, 11, 13, WHITE).p(4, 4, c('ffc040')).p(7, 2, c('ffc040')).p(10, 4, c('ffc040')).r(3, 13, 12, 14, BRASS).outline(), 'chandlery', [('spermaceti', 1), ('cotton_yarn', 1)], 1, 4, 'cargo', 1.4)
good('sealing_wax', 'Sealing wax', 3, P().r(4, 6, 11, 10, RED).r(4, 6, 11, 6, c('f06050')).disc(7.5, 12, 1.6, RED).outline(), 'chandlery', [('tallow', 1), ('annatto_seed', 2)], 1, 3)
good('tarpaulin', 'Tarpaulin', 3, trinket('cloth', c('3a3a34'), c('5a5a50')), 'chandlery', [('sailcloth', 2), ('pitch', 1), ('tallow', 1)], 1, 2)
good('ships_stores', "Ship's stores", 4, P().r(2, 5, 13, 14, c('a07840')).r(2, 5, 13, 5, c('c8a070')).r(4, 8, 6, 12, c('3a6a4a')).r(8, 8, 11, 12, IVORY).outline(), 'chandlery', [('tarpaulin', 1), ('tallow_dip', 4), ('hard_soap', 1), ('hardtack', 4), ('salt_pork', 2), ('tarred_rope', 2)], 1, 1, 'cargo', 1.8)
deco(P().r(6, 13, 9, 14, BRASS).r(7, 6, 8, 12, IVORY).p(7, 4, c('ffc040')).p(8, 5, c('ff9020')), 'candle_stand', 'Candle', 2, [('tallow_dip', 1), ('bronze_ingot', 1)], 'chandlery', light=11)
deco(P().r(5, 2, 10, 3, BRASS).r(7, 0, 8, 1, ROPE).r(5, 4, 10, 11, c('ffe890')).r(5, 4, 5, 11, BRASS).r(10, 4, 10, 11, BRASS).r(5, 12, 10, 13, BRASS), 'ships_lamp', "Ship's lamp", 3, [('lamp_oil', 1), ('bronze_ingot', 2), ('glass', 2)], 'chandlery', light=15)
# Sweet things, and strong.
good('refined_sugar', 'Refined sugar', 3, sack(c('ffffff')), 'sugar_mill', [('sugar_loaf', 2), ('limestone', 1)], 1, 2, 'cargo')
good('vanilla_essence', 'Vanilla essence', 4, bottle(c('5a3a1a'), c('c0b090')), 'still', [('vanilla_pod', 3), ('rum_bottle', 1)], 1, 2, 'cargo', 1.4)
good('ginger_beer', 'Ginger beer', 2, bottle(c('e0c060'), c('8a6a3a')), 'still', [('wild_ginger', 2), ('sugar_loaf', 1), ('glass', 1)], 1, 3, 'nature', 1.0, max_stack=10, effect=lasts(2400, stamina=1.3))
good('rum_punch', 'Rum punch', 3, cup(c('e06040')), 'tavern_bar', [('rum_bottle', 1), ('pineapple', 1), ('key_lime', 1), ('refined_sugar', 1)], 1, 4, 'nature', 1.0, cooked=True, max_stack=10, effect=lasts(3600, luck=0.08, stamina=1.3))
good('sea_grape_wine', 'Sea-grape wine', 3, bottle(c('7a2a6a')), 'still', [('sea_grapes', 6), ('sugar_loaf', 1), ('glass', 1)], 1, 2, 'cargo', 1.2, max_stack=10, effect=lasts(3000, regen=1.5))
good('candied_ginger', 'Candied ginger', 3, bowl(c('e0b040'), steam=False), 'ships_galley', [('wild_ginger', 2), ('refined_sugar', 1)], 1, 2, 'nature', 1.0, cooked=True, food=0.3, effect=lasts(3000, warmth=0.5))
good('vanilla_custard', 'Vanilla custard', 4, bowl(c('f8e8a0'), steam=False), 'ships_galley', [('vanilla_essence', 1), ('gull_egg', 2), ('refined_sugar', 1)], 1, 2, 'nature', 1.0, cooked=True, food=0.7, effect=lasts(4800, regen=2.0, luck=0.05))
for n, marks, g in (('twist_tobacco', 3, 1), ('cigar_box', 10, 2), ('hard_soap', 2, 1), ('lamp_oil', 4, 2), ('fine_candles', 6, 2), ('refined_sugar', 3, 1), ('vanilla_essence', 8, 2), ('ships_stores', 14, 3), ('sea_grape_wine', 4, 2)): pay('company', [(n, 2)], marks, g)
pay('crown', [('ships_stores', 1)], 8, 2); pay('brethren', [('cigar_box', 1)], 6, 2); pay('brethren', [('rum_punch', 4)], 3); pay('islander', [('fine_candles', 2)], 5, 2); pay('islander', [('indigo_calico', 2)], 3)
# The surgeon's chest.
good('lint_bandage', 'Bandage', 2, trinket('cloth', c('f8f8f0'), RED), 'ships_surgery', [('calico', 1)], 1, 2, 'nature', 1.0, max_stack=10, effect=lasts(600, regen=3.0))
good('aloe_salve', 'Aloe salve', 2, trinket('pot', c('7ac08a'), c('e8e8f0')), 'ships_surgery', [('aloe_leaf', 3), ('tallow', 1)], 1, 2, 'nature', 1.0, max_stack=10, effect=lasts(2400, regen=2.0))
good('sponge_dressing', 'Sponge dressing', 3, P().disc(7.5, 8, 4.5, c('e0b040')).r(3, 7, 12, 9, c('f8f8f0')).outline(), 'ships_surgery', [('sea_sponge', 1), ('lint_bandage', 1), ('rum_bottle', 1)], 1, 2, 'nature', 1.0, max_stack=10, effect=lasts(1200, regen=5.0))
good('antivenom', 'Antivenom', 4, bottle(c('60c060'), c('c0d0c0')), 'ships_surgery', [('snake_skin', 1), ('aloe_leaf', 2), ('rum_bottle', 1)], 1, 2, 'nature', 1.0, max_stack=10, effect=lasts(6000, ward=0.4))
good('smelling_salts', 'Smelling salts', 3, bottle(c('f0f0f0'), c('6a8ab0')), 'ships_surgery', [('nitre', 1), ('salt', 2), ('glass', 1)], 1, 2, 'nature', 1.0, max_stack=10, effect=lasts(1800, stamina=1.8, walk=1.15))
good('scurvy_cure', 'Scurvy cure', 3, bottle(c('c0e040'), c('a0b0a0')), 'ships_surgery', [('key_lime', 4), ('wild_ginger', 1), ('glass', 1)], 1, 3, 'nature', 1.0, max_stack=10, effect=lasts(9000, regen=1.5, stamina=1.2))
good('divers_draught', "Diver's draught", 4, bottle(c('40a0c0'), c('a0c0d0')), 'ships_surgery', [('sea_urchin', 2), ('sea_sponge', 1), ('kelp', 2), ('glass', 1)], 1, 2, 'nature', 1.0, max_stack=10, effect=lasts(4800, lung=6.0, sight=1.0))
good('surgeons_kit', "Surgeon's kit", 5, P().r(2, 6, 13, 13, c('3a2a22')).r(2, 6, 13, 6, c('5a4a3a')).r(7, 8, 8, 11, RED).r(6, 9, 9, 10, RED).r(6, 4, 9, 5, BRASS).outline(), 'ships_surgery', [('lint_bandage', 6), ('aloe_salve', 4), ('antivenom', 2), ('steel_ingot', 2), ('leather', 2)], 1, 1, 'special', 3.0, max_stack=1, relic=True, boons={'regen': 3.0, 'ward': 0.1})

# ---- instruments: brass, glass, and a great deal of patience -------------------------------------------------------------------------------------------
good('brass_sheet', 'Brass sheet', 3, trinket('plate', BRASS), 'gun_foundry', [('copper_ingot', 2), ('tin_ingot', 1)], 1, 2)
good('brass_wire', 'Brass wire', 3, trinket('thread', BRASS), 'instrument_maker', [('brass_sheet', 1)], 1, 3)
good('brass_gears', 'Brass gears', 4, P().disc(6, 7, 3.2, BRASS).disc(6, 7, 1.2, dk(BRASS)).disc(11, 11, 2.4, lt(BRASS)).disc(11, 11, 0.8, dk(BRASS)).outline(), 'instrument_maker', [('brass_sheet', 2), ('brass_wire', 1)], 1, 2)
good('ground_lens', 'Ground lens', 3, P().disc(7.5, 7.5, 4.5, c('a0d0e0')).disc(6.5, 6.5, 1.6, WHITE).outline(), 'instrument_maker', [('glass', 2), ('sand', 2)])
good('fine_lens', 'Fine lens', 5, P().disc(7.5, 7.5, 5, c('c0e8f8')).disc(6, 6, 2, WHITE).disc(7.5, 7.5, 5.4, BRASS).disc(7.5, 7.5, 4.4, c('c0e8f8')).disc(6, 6, 1.6, WHITE).outline(), 'instrument_maker', [('ground_lens', 2), ('sea_glass', 2), ('brass_sheet', 1)], 2)
good('mainspring', 'Mainspring', 5, P().disc(7.5, 7.5, 5, c('8a9aaa')).disc(7.5, 7.5, 3.6, (0, 0, 0, 0)).disc(7.5, 7.5, 2.4, c('8a9aaa')).disc(7.5, 7.5, 1.2, (0, 0, 0, 0)).outline(), 'instrument_maker', [('steel_ingot', 2), ('brass_wire', 2)], 2)
good('gimbal_mount', 'Gimbal mount', 4, P().disc(7.5, 7.5, 5.5, BRASS).disc(7.5, 7.5, 4.2, (0, 0, 0, 0)).disc(7.5, 7.5, 2.4, lt(BRASS)).r(1, 7, 3, 8, BRASS).r(12, 7, 14, 8, BRASS).outline(), 'instrument_maker', [('brass_sheet', 2), ('brass_gears', 1), ('hardwood', 1)], 2)
good('lodestone_needle', 'Lodestone needle', 3, trinket('needle', c('5a5a66'), RED), 'instrument_maker', [('wrought_iron', 1), ('brass_wire', 1)])
good('vellum', 'Vellum', 3, trinket('paper', c('f0e8c8')), 'captains_desk', [('leather', 1), ('limestone', 1)], 1, 2)
good('graduated_arc', 'Graduated arc', 4, P().line(3, 12, 7, 3, BRASS).line(7, 3, 12, 12, BRASS).line(3, 12, 12, 12, BRASS).line(4, 13, 11, 13, dk(BRASS)).outline(), 'instrument_maker', [('brass_sheet', 2), ('brass_wire', 2)], 2)
KIT = [('ships_compass', "Ship's compass", 3, P().disc(7.5, 8, 5.5, BRASS).disc(7.5, 8, 4.2, IVORY).line(7, 5, 8, 11, RED).p(7, 8, INK), 1, [('lodestone_needle', 1), ('brass_sheet', 2), ('glass', 1)], {'sight': 0.5, 'walk': 1.05}),
       ('sounding_lead', 'Sounding lead', 3, P().line(7, 1, 8, 8, ROPE).r(6, 8, 9, 13, c('5a5a66')).r(6, 8, 6, 13, c('7a7a86')), 1, [('wrought_iron', 2), ('tarred_rope', 2), ('tallow', 1)], {'lung': 1.5}),
       ('log_line', 'Log line', 3, P().disc(7.5, 6, 4, OAK).disc(7.5, 6, 2.2, ROPE).line(8, 9, 11, 14, ROPE).r(10, 13, 13, 14, OAK), 1, [('cordage', 4), ('seasoned_timber', 1), ('deadeye', 1)], {'walk': 1.1}),
       ('brass_dividers', 'Dividers', 3, P().line(7, 2, 4, 14, BRASS).line(8, 2, 11, 14, BRASS).disc(7.5, 2.5, 1.4, lt(BRASS)), 1, [('brass_wire', 2), ('brass_sheet', 1)], {'craft': 1.1}),
       ('sand_glass', 'Sand-glass', 3, P().r(4, 2, 11, 3, OAK).r(4, 12, 11, 13, OAK).line(5, 4, 7, 7, c('a0d0e0')).line(10, 4, 8, 7, c('a0d0e0')).line(7, 8, 5, 11, c('e0c080')).line(8, 8, 10, 11, c('e0c080')).r(6, 10, 9, 11, c('e0c080')), 1, [('glass', 2), ('sand', 1), ('seasoned_timber', 1)], {'stamina': 1.15}),
       ('astrolabe', 'Astrolabe', 4, P().disc(7.5, 8.5, 5.5, BRASS).disc(7.5, 8.5, 3.6, dk(BRASS)).line(3, 8, 12, 9, lt(BRASS)).r(7, 1, 8, 2, BRASS), 2, [('graduated_arc', 1), ('brass_sheet', 3), ('brass_gears', 1)], {'sight': 1.0}),
       ('back_staff', 'Back-staff', 4, P().line(2, 13, 13, 3, OAK).line(4, 13, 9, 5, BRASS).line(9, 5, 13, 9, BRASS).p(13, 3, c('a0d0e0')), 2, [('graduated_arc', 1), ('hardwood', 2), ('ground_lens', 1)], {'luck': 0.08}),
       ('barometer', 'Weather-glass', 4, P().r(6, 1, 9, 14, OAK).r(7, 2, 8, 11, c('a0d0e0')).r(7, 7, 8, 11, c('c0c8d0')).disc(7.5, 12.5, 1.6, c('c0c8d0')), 2, [('glass', 3), ('brass_sheet', 1), ('hardwood', 1), ('tin_ingot', 1)], {'warmth': 0.4, 'ward': 0.1}),
       ('sextant', 'Sextant', 5, P().line(3, 12, 8, 2, BRASS).line(8, 2, 12, 12, BRASS).line(3, 12, 12, 12, lt(BRASS)).line(8, 2, 6, 12, dk(BRASS)).r(10, 4, 13, 5, BRASS).p(13, 4, c('a0d0e0')), 3, [('back_staff', 1), ('astrolabe', 1), ('fine_lens', 2), ('brass_gears', 2)], {'sight': 1.0, 'luck': 0.12, 'walk': 1.1}),
       ('great_telescope', 'Great telescope', 5, P().r(1, 7, 6, 9, BRASS).r(6, 6, 11, 10, OAK).r(11, 5, 14, 11, BRASS).p(14, 8, c('a0d0e0')).r(1, 7, 1, 9, lt(BRASS)), 3, [('spyglass', 1), ('fine_lens', 3), ('brass_sheet', 3), ('gimbal_mount', 1)], {'sight': 2.0}),
       ('chronometer', 'Chronometer', 6, P().r(2, 4, 13, 13, c('6a2a1a')).disc(7.5, 8.5, 4, IVORY).line(7, 8, 7, 6, INK).line(7, 8, 10, 9, INK).disc(7.5, 8.5, 4.4, BRASS).disc(7.5, 8.5, 3.6, IVORY).line(7, 8, 7, 6, INK).line(7, 8, 10, 9, INK), 3, [('mainspring', 2), ('brass_gears', 4), ('gimbal_mount', 1), ('fine_lens', 1), ('hardwood', 2), ('sand_glass', 1)], {'luck': 0.2, 'craft': 1.3, 'stamina': 1.3})]
for n, lab, tier, pic, g, ins, boons in KIT: good(n, lab, tier, pic.outline(), 'instrument_maker', ins, g, 1, 'special', 2.5, 400, max_stack=1, relic=True, boons=boons, set='navigators_kit')
sets.append({'name': 'navigators_kit', 'label': "Navigator's kit", 'bonuses': [{'pieces': 2, 'boons': {'sight': 1.0}}, {'pieces': 4, 'boons': {'luck': 0.2, 'walk': 1.2, 'sight': 1.0}}]})
good('pilots_rutter', "Pilot's rutter", 4, trinket('paper', c('f0e8c8'), c('2a4a9a')), 'chart_table', [('vellum', 4), ('octopus_ink', 2), ('ships_compass', 1), ('sounding_lead', 1)], 1, 1, 'special', 3.0, 400, max_stack=1, relic=True, boons={'lung': 2.0, 'walk': 1.15})
good('great_atlas', 'Great atlas', 6, P().r(3, 3, 12, 13, c('6a2a1a')).r(3, 3, 4, 13, GOLD).r(6, 6, 10, 9, c('2a6a9a')).p(8, 7, c('4a9a3a')).outline(), 'chart_table', [('pilots_rutter', 1), ('vellum', 8), ('sextant', 1), ('chronometer', 1), ('treasure_map', 3), ('royal_purple_dye', 1)], 1, 1, 'special', 6.0, 900, max_stack=1, relic=True, boons={'sight': 2.0, 'luck': 0.3, 'walk': 1.25, 'lung': 3.0})
also('frigate', 'ships_compass'); also('frigate', 'sextant'); also('man_of_war', 'chronometer'); also('man_of_war', 'great_telescope'); also('brig', 'ships_compass'); also('merchantman', 'log_line'); also('schooner', 'log_line'); also('sloop', 'sounding_lead')
buy('islander', 'ships_compass', 14, 2); buy('company', 'barometer', 30, 3); buy('crown', 'sextant', 120, 4)

# ---- the gunsmith ----------------------------------------------------------------------------------------------------------------------------------------
good('gun_lock', 'Gun lock', 4, P().r(4, 6, 11, 10, c('7a7e8a')).line(8, 6, 11, 3, c('5a5a66')).p(11, 3, c('3a3a3a')).r(5, 11, 6, 13, c('5a5a66')).outline(), 'gunsmith', [('wrought_iron', 2), ('brass_wire', 1), ('flint', 1)])
good('gun_stock', 'Gun stock', 3, P().r(2, 7, 9, 9, c('6a3a1a')).r(9, 8, 13, 12, c('6a3a1a')).r(2, 7, 9, 7, c('8a5a30')).outline(), 'gunsmith', [('hardwood', 2), ('tallow', 1)])
good('rifled_barrel', 'Rifled barrel', 5, P().r(1, 7, 14, 9, c('3a3a44')).r(1, 7, 14, 7, c('6a6a76')).p(14, 8, INK).r(4, 6, 5, 6, BRASS).outline(), 'gunsmith', [('bored_barrel', 1), ('steel_ingot', 2), ('tallow', 1)], 2)
good('cartridge_box', 'Cartridges', 3, P().r(3, 6, 12, 13, c('3a2a22')).r(3, 6, 12, 7, c('5a4a3a')).r(5, 3, 6, 6, PAPER).r(8, 3, 9, 6, PAPER).outline(), 'gunsmith', [('gunpowder', 2), ('vellum', 1), ('round_shot', 2)], 1, 4)
for n, lab, kind, tier, dmg, rng_, g, ins in (('boarding_pistol', 'Boarding pistol', 'pistol', 3, 20, 6, 1, [('gun_lock', 1), ('gun_stock', 1), ('wrought_iron', 2)]), ('duelling_pistol', 'Duelling pistol', 'pistol', 5, 55, 10, 2, [('flintlock_pistol', 1), ('rifled_barrel', 1), ('gun_lock', 1), ('silver_bar', 1)]),
                                             ('long_rifle', 'Long rifle', 'musket', 6, 90, 18, 3, [('musket', 1), ('rifled_barrel', 2), ('gun_lock', 1), ('ground_lens', 1), ('gun_stock', 1)]), ('volley_gun', 'Volley gun', 'blunderbuss', 6, 130, 7, 3, [('blunderbuss', 2), ('rifled_barrel', 3), ('gun_lock', 3), ('gun_stock', 1)]),
                                             ('hand_mortar', 'Hand mortar', 'blunderbuss', 5, 100, 9, 2, [('bored_barrel', 1), ('gun_lock', 1), ('gun_stock', 1), ('grenado', 4)]), ('swivel_musket', 'Wall gun', 'musket', 4, 48, 13, 2, [('gun_lock', 1), ('gun_stock', 1), ('bored_barrel', 1)])):
    thing(weapon(kind, c('c0c8d0') if tier > 4 else c('8a8e9a'), c('6a3a1a')), n, lab, tier, 'tools', 1.6, max_stack=1, damage=dmg, range=rng_); make('gunsmith', n, ins + [('cartridge_box', 2)], g, 1, 300)
good('sea_mortar', 'Sea mortar', 5, P().r(4, 5, 11, 11, c('2a2a2a')).r(4, 5, 11, 5, c('5a5a66')).r(2, 11, 13, 14, OAK).r(6, 3, 9, 4, INK).outline(), 'gun_foundry', [('bored_barrel', 3), ('wrought_iron', 8), ('gun_carriage', 2), ('bronze_ingot', 6)], 3, 1, time=500)
good('bomb_shell', 'Bomb shells', 4, P().disc(7.5, 9, 4.5, c('2a2a2a')).disc(6, 7.5, 1.4, c('5a5a66')).line(8, 4, 11, 1, ROPE).p(11, 1, c('f06020')).outline(), 'powder_mill', [('wrought_iron', 2), ('gunpowder', 3), ('cordage', 1)], 1, 4)

# ---- finery, carving, and the great treasures -------------------------------------------------------------------------------------------------------------
FINE = [('pearl_earring', 'Pearl earring', 3, trinket('bead', WHITE), [('pearl_string', 1), ('gold_ingot', 1)], {'luck': 0.06}, 1), ('pearl_rope', 'Rope of pearls', 5, trinket('pendant', WHITE, c('e0e0f0')), [('pearl_string', 6), ('cotton_yarn', 1)], {'luck': 0.15, 'regen': 1.2}, 2),
        ('coral_brooch', 'Coral brooch', 3, trinket('seal', c('f06050')), [('coral', 3), ('gold_ingot', 1)], {'lung': 1.5}, 1), ('shell_comb', 'Tortoiseshell comb', 3, trinket('fan', c('8a5a2a'), c('c08a40')), [('tortoiseshell', 2)], {'craft': 1.08}, 1),
        ('gold_tooth', 'Gold tooth', 3, P().r(5, 4, 10, 9, GOLD).r(5, 9, 6, 12, GOLD).r(9, 9, 10, 12, GOLD).p(6, 5, WHITE).outline(), [('gold_ingot', 1)], {'strength': 1}, 1), ('signet_of_trade', 'Merchant signet', 4, trinket('ring', GOLD, c('8a5a2a')), [('gold_ingot', 2), ('sealing_wax', 1)], {'magnet': 0.6, 'luck': 0.06}, 1),
        ('doubloon_pendant', 'Doubloon pendant', 4, trinket('pendant', GOLD), [('gold_coin', 30), ('brass_wire', 1)], {'luck': 0.12}, 1), ('emerald_cross', 'Emerald cross', 5, P().r(7, 2, 8, 13, GOLD).r(4, 5, 11, 6, GOLD).disc(7.5, 5.5, 1.4, c('30c060')).outline(), [('gold_ingot', 3), ('emerald', 2)], {'ward': 0.2, 'regen': 1.5}, 2),
        ('ruby_buckle', 'Ruby buckle', 5, P().r(3, 5, 12, 11, GOLD).r(5, 7, 10, 9, (0, 0, 0, 0)).disc(7.5, 8, 1.2, c('e03030')).outline(), [('gold_ingot', 2), ('ruby', 2)], {'strength': 3, 'walk': 1.08}, 2), ('sea_glass_charm', 'Sea-glass charm', 2, trinket('bead', c('70d0b0'), ROPE), [('sea_glass', 3), ('cordage', 1)], {'lung': 1.2}, 1),
        ('admirals_star', "Admiral's star", 6, P().line(7, 1, 8, 14, GOLD).line(1, 7, 14, 8, GOLD).line(3, 3, 12, 12, GOLD).line(12, 3, 3, 12, GOLD).disc(7.5, 7.5, 2.4, c('2a4a9a')).outline(), [('gold_ingot', 4), ('sapphire', 2), ('diamond', 1), ('cloth_of_state', 1)], {'armor': 0.12, 'strength': 3, 'luck': 0.15}, 2)]
for n, lab, tier, pic, ins, boons, g in FINE: good(n, lab, tier, pic, 'sea_jeweller', ins, g, 1, 'special', 2.5, 300, max_stack=1, relic=True, boons=boons, set='sea_finery')
sets.append({'name': 'sea_finery', 'label': 'Finery', 'bonuses': [{'pieces': 2, 'boons': {'luck': 0.1}}, {'pieces': 4, 'boons': {'luck': 0.25, 'magnet': 1.0}}]})
good('jewel_casket', 'Jewel casket', 6, P().r(2, 6, 13, 13, c('6a2a1a')).r(2, 6, 13, 7, GOLD).r(7, 8, 8, 10, GOLD).p(4, 10, c('e03030')).p(11, 10, c('30c060')).outline(), 'sea_jeweller', [('pearl_rope', 1), ('emerald_cross', 1), ('ruby_buckle', 1), ('hardwood', 2), ('gold_ingot', 2)], 2, 1, 'cargo', 3.0, 500); pay('company', [('jewel_casket', 1)], 90, 4); pay('crown', [('jewel_casket', 1)], 90, 4)
CARVED = [('scrimshaw_tooth', 'Scrimshaw', 3, fang(IVORY), [('marlin_bill', 1), ('octopus_ink', 1)], None), ('scrimshaw_ship', 'Scrimshaw ship', 5, P().r(3, 9, 12, 11, IVORY).r(7, 3, 7, 8, IVORY).r(8, 4, 11, 7, IVORY).r(4, 12, 11, 12, dk(IVORY, 0.8)).outline(), [('kraken_beak', 1), ('scrimshaw_tooth', 2), ('octopus_ink', 2)], {'luck': 0.15}),
          ('shell_cameo', 'Shell cameo', 3, P().disc(7.5, 8, 5, c('f0c0b0')).disc(7.5, 8, 3, IVORY).p(7, 7, c('e0a090')).outline(), [('conch_shell', 2)], None), ('conch_horn', 'Conch horn', 3, P().disc(9, 8, 4, c('f0c0b0')).r(2, 7, 6, 9, c('f0d8c8')).r(1, 7, 1, 9, BRASS).p(10, 7, c('e08090')).outline(), [('conch_shell', 1), ('brass_sheet', 1)], {'stamina': 1.2}),
          ('clam_basin', 'Clam-shell basin', 4, P().r(2, 8, 13, 12, c('f0e0f0')).r(3, 13, 12, 13, c('d0c0d8')).r(4, 9, 11, 10, c('a0d8f0')).outline(), [('giant_clam_shell', 1)], None), ('amphora_lamp', 'Amphora lamp', 4, trinket('pot', c('c07a4a'), c('ffc040')), [('amphora', 1), ('lamp_oil', 1)], {'sight': 1.0})]
for n, lab, tier, pic, ins, boons in CARVED: good(n, lab, tier, pic, 'scrimshaw_bench', ins, 1, 1, 'special' if boons else 'cargo', 2.0, 300, **(dict(max_stack=1, relic=True, boons=boons) if boons else {}))
pay('islander', [('scrimshaw_tooth', 1)], 5, 2); pay('company', [('shell_cameo', 1)], 4, 2); pay('company', [('clam_basin', 1)], 8, 2)
for n, lab, face, top, light in (('tiki_of_rain', 'Tiki of rain', '3a6a9a', '5a8ab0', 5), ('tiki_of_fire', 'Tiki of fire', 'c04020', 'f09030', 12), ('tiki_of_plenty', 'Tiki of plenty', '4a8a3a', 'e0c040', 5), ('tiki_of_the_deep', 'Tiki of the deep', '2a4a44', '60ffd0', 9)):
    q = P().r(5, 3, 10, 15, c('6a4a2a')).r(5, 3, 5, 15, c('8a6a3a')).r(4, 1, 11, 3, c(top)).r(6, 5, 7, 6, c(face)).r(9, 5, 9, 6, c(face)).r(6, 9, 9, 10, c(face))
    if 'fire' in n: q.r(6, 0, 9, 0, c('ffe060'))
    elif 'rain' in n: q.p(3, 6, c(top)).p(12, 8, c(top)).p(3, 11, c(top))
    elif 'plenty' in n: q.r(3, 12, 4, 13, c(top)).r(11, 12, 12, 13, c(top))
    else: q.r(6, 12, 9, 13, c(top))
    deco(q, n, lab, 4, [('hardwood', 4), ('old_carving', 1)], 'scrimshaw_bench', light=light)
deco(P().r(3, 12, 12, 13, OAK).r(4, 9, 11, 11, c('8a5a30')).r(7, 3, 7, 8, c('5a3a22')).r(8, 4, 11, 7, CANVAS).r(4, 5, 6, 7, CANVAS).p(12, 9, c('8a5a30')), 'ship_model', 'Ship model', 4, [('seasoned_timber', 2), ('sailcloth', 1), ('cordage', 1), ('treenails', 4)], 'scrimshaw_bench')
deco(P().r(5, 12, 10, 15, c('8a8a84')).r(6, 6, 9, 11, c('60c0a0')).disc(7.5, 4.5, 1.8, c('e0b890')).r(5, 3, 5, 7, c('e0b030')).line(9, 11, 12, 13, c('3a9a8a')), 'mermaid_statue', 'Mermaid statue', 5, [('reef_rock', 6), ('pearl_string', 1), ('gold_ingot', 1)], 'scrimshaw_bench', light=4)
# The great treasures are not made. They are in a sealed coffer, or they are nowhere.
GREAT = [('black_pearl', 'The Black Pearl', trinket('bead', c('2a2a3a'), c('7a3a8a')), {'luck': 0.4, 'unseen': 0.4}), ('mermaids_tear', "Mermaid's tear", gemstone(c('60d0f0'), True), {'lung': 30.0, 'regen': 3.0}), ('kraken_eye', "Kraken's eye", P().disc(7.5, 7.5, 5.5, c('e0c040')).disc(7.5, 7.5, 2.6, INK).p(6, 6, WHITE).outline(), {'sight': 3.0, 'strength': 4}),
         ('davy_jones_key', "Davy Jones' key", trinket('key', c('3a8a7a'), c('60ffd0')), {'ward': 0.4, 'lung': 10.0}), ('golden_compass', 'Golden compass', P().disc(7.5, 8, 5.5, GOLD).disc(7.5, 8, 4, c('2a2a3a')).line(7, 5, 8, 11, c('60ffd0')).outline(), {'luck': 0.3, 'walk': 1.3, 'magnet': 2.0}),
         ('sirens_conch', "Siren's conch", P().disc(9, 8, 4, c('c080e0')).r(2, 7, 6, 9, c('e0b0f0')).p(10, 7, WHITE).outline(), {'stamina': 2.0, 'regen': 2.0}), ('sea_kings_signet', "Sea king's signet", trinket('ring', GOLD, c('60d0f0')), {'armor': 0.2, 'strength': 5}), ('dutchmans_log', "Dutchman's log", trinket('paper', c('9ad0c0'), c('2a4a44')), {'unseen': 0.6, 'walk': 1.25}),
         ('fountain_flask', 'Flask from the fountain', bottle(c('c0f8ff'), GOLD), {'regen': 8.0}), ('tempest_bell', 'Tempest bell', trinket('bell', c('5a7a9a'), c('c0e8f8')), {'ward': 0.3, 'glide': 1.0, 'warmth': 0.6}), ('lost_crown', 'The lost crown', trinket('crown', GOLD, c('e03030')), {'luck': 0.5, 'craft': 1.5}),
         ('cursed_doubloon', 'Cursed doubloon', P().disc(7.5, 7.5, 5, c('a0c040')).disc(7.5, 7.5, 3.4, c('6a8a20')).p(6, 6, INK).p(9, 6, INK).r(6, 9, 9, 9, INK).outline(), {'strength': 8, 'regen': 0.5}), ('idol_of_storms', 'Idol of storms', trinket('rune', c('4a5a7a'), c('f0f060')), {'strength': 6, 'ward': 0.25}), ('first_chart', 'The first chart', trinket('paper', c('e8d8a0'), c('c03020')), {'sight': 3.0, 'luck': 0.35})]
for n, lab, pic, boons in GREAT: thing(pic, n, lab, 6, 'special', 12.0, max_stack=1, relic=True, boons=boons, set='great_treasures')
sets.append({'name': 'great_treasures', 'label': 'The great treasures', 'bonuses': [{'pieces': 2, 'boons': {'luck': 0.2}}, {'pieces': 4, 'boons': {'luck': 0.5, 'strength': 6, 'regen': 3.0}}]})
thing(P().r(2, 6, 13, 13, c('2a2a3a')).r(2, 6, 13, 7, c('60ffd0')).r(7, 8, 8, 11, c('60ffd0')).r(2, 10, 13, 10, c('4a4a5a')).outline(), 'sealed_coffer', 'Sealed coffer', 6, 'special', 8.0)
rec('captains_desk', 'black_pearl', [('sealed_coffer', 1)], 1, time=200, output={'count': 1, 'one_of': [{'item': n, 'weight': 2 if n in ('cursed_doubloon', 'first_chart', 'sirens_conch') else 1} for n, _, _, _ in GREAT]})
vault('sunken_reliquary', 'Drowned reliquary', '2a4a44', '60ffd0', 6, 4, 2, [drop('sealed_coffer', 1, 1), drop('platinum_coin', 1, 4), some(('pearl_rope', 2), ('emerald_cross', 2), ('old_rum', 3), ('amphora', 3), ('silver_bar', 4))])
for m in mobs:
    if m['name'] in ('kraken', 'ghost_ship_captain', 'white_whale', 'royal_man_of_war_captain', 'treasure_galleon_captain'): m['drops'] = m.get('drops', []) + [drop('sealed_coffer', 1, 1, 1.0 if m['name'] == 'kraken' else 0.5)]
buy('brethren', 'sealed_coffer', 400, 5); buy('islander', 'sealed_coffer', 400, 5)

# ---- more to wear ----------------------------------------------------------------------------------------------------------------------------------------
OUTFITS2 = [('privateer', "Privateer's", '3a5a3a', 4, ('tricorn', 'tricorn'), {'strength': 1, 'luck': 0.03}, [{'strength': 2}, {'strength': 4, 'armor': 0.12}], 'sea_green_calico'), ('corsair', "Corsair's", 'c03020', 4, ('bandana', 'bandana'), {'walk': 1.05}, [{'walk': 1.15}, {'strength': 4, 'walk': 1.3}], 'scarlet_calico'),
            ('admiral', "Admiral's", '1a2a5a', 6, ('tricorn', 'hat'), {'armor': 0.06, 'luck': 0.04}, [{'armor': 0.15}, {'armor': 0.3, 'strength': 4, 'luck': 0.2}], 'cloth_of_state'), ('smuggler', "Smuggler's", '2a1a2a', 4, ('bandana', 'headband'), {'unseen': 0.1}, [{'unseen': 0.3}, {'unseen': 0.6, 'walk': 1.2, 'luck': 0.1}], 'logwood_calico'),
            ('whaler', "Whaler's", '5a5a44', 4, ('helm', 'helmet'), {'warmth': 0.3}, [{'warmth': 0.6}, {'strength': 4, 'warmth': 1.0, 'lung': 2.0}], 'tarpaulin'), ('governor', "Governor's", '7a3a8a', 6, ('tricorn', 'tricorn'), {'craft': 1.1, 'luck': 0.05}, [{'craft': 1.3}, {'craft': 1.6, 'luck': 0.3, 'magnet': 1.5}], 'royal_purple_calico'),
            ('planter', "Planter's", 'f0f0e8', 3, ('tricorn', 'hat'), {'craft': 1.04}, [{'craft': 1.15}, {'craft': 1.3, 'regen': 1.5}], 'calico'), ('surgeon', "Surgeon's", '8a2a2a', 4, ('bandana', 'headband'), {'regen': 1.2}, [{'regen': 1.8}, {'regen': 3.0, 'ward': 0.2}], 'saffron_calico')]
for n, lab, col, tier, (hk, hlab), boon, bonus, cloth in OUTFITS2:
    for slot, kind, plab in (('head', hk, hlab), ('chest', 'coat', 'coat'), ('legs', 'breeches', 'breeches'), ('boots', 'boots', 'boots')):
        nm = '%s_%s' % (n, plab); hatpic(kind, c(col), IVORY if tier < 6 else GOLD).save('items', nm)
        item(nm, '%s %s' % (lab, plab), tier, 'clothing', 1.0, equip=slot, max_stack=1, armor=round(0.02 * tier, 2), warmth=0.3, set=n + '_outfit', boons=boon)
        make('sail_loft', nm, [(cloth, 2 if slot != 'chest' else 3), ('cotton_yarn', 2)] + ([('leather', 2), ('tallow', 1)] if slot == 'boots' else [('lace_trim', 1)] if tier == 6 else []), 1, 1, 240)
    sets.append({'name': n + '_outfit', 'label': lab + ' rig', 'bonuses': [{'pieces': 2, 'boons': bonus[0]}, {'pieces': 4, 'boons': bonus[1]}]})

# ---- more ships -------------------------------------------------------------------------------------------------------------------------------------------
def player_ship(n, lab, size, hull, sail, masts, ports, hp, speed, dmg, rng_, tier, ins, plan, g, oars=False):
    q = Big(1); src = ship_pic(size, c(hull), c(sail), c('2a2a2a'), masts, ports, oars=oars); q.im = src.im.resize((16, 16), Image.NEAREST); q.im.resize((48, 48), Image.NEAREST).save(os.path.join(TEX, 'items', n + '.png'))
    item(n, lab, tier, 'special', 6.0, max_stack=1)
    ship_mob(n + '_afloat', lab, size, hull, sail, '2a2a2a', masts, ports, hp, oars=oars, mount={'speed': speed}, ship=clean({'guns': dmg or None, 'range': rng_ or 6, 'wreck': 'ship_wreckage' if size >= 3 else None}), companion={'bite': dmg, 'sight': rng_}, tame=9, caged=n, drops=[drop('sawn_planking', 1, 3)])
    make('shipwright', n, ins, g, 1, 900, plan)
    comp(P().r(2, 3, 13, 12, c('c8d8e8')).r(2, 3, 13, 3, WHITE).line(4, 9, 11, 9, c('2a4a8a')).line(5, 9, 7, 5, c('2a4a8a')).line(7, 5, 7, 9, c('2a4a8a')).outline(), plan, 'Plan: ' + lab.lower(), tier, 3.0, 'special', max_stack=1)
player_ship('pinnace', 'Pinnace', 2, 'a06a3a', 'f0e8d0', 1, 1, 130, 2.0, 8, 8, 3, [('boat_hull', 1), ('boat_mast', 1), ('boat_rig', 1), ('swivel_gun', 2), ('rudder', 1)], 'pinnace_plan', 1)
player_ship('cutter', 'Cutter', 3, '4a5a6a', 'f8f8f0', 1, 3, 260, 2.9, 16, 10, 4, [('fitted_sloop_hull', 1), ('sloop_mast', 1), ('sloop_rig', 2), ('swivel_gun', 6), ('copper_sheathing', 2), ('log_line', 1)], 'cutter_plan', 2)
player_ship('whaler', 'Whaler', 4, '3a3a34', 'd8d0b8', 3, 1, 680, 1.7, 26, 8, 4, [('fitted_brig_hull', 1), ('brig_mast', 3), ('brig_rig', 2), ('steel_harpoon', 4), ('cargo_hold', 2), ('tarpaulin', 4)], 'whaler_plan', 2)
player_ship('bomb_ketch', 'Bomb ketch', 4, '4a2a22', 'e8dcc0', 2, 4, 500, 1.6, 80, 17, 5, [('fitted_brig_hull', 1), ('brig_mast', 2), ('brig_rig', 2), ('sea_mortar', 2), ('bomb_shell', 16), ('ships_compass', 1)], 'bomb_ketch_plan', 3)
player_ship('xebec', 'Xebec', 4, 'a04a2a', 'f0e0c0', 3, 6, 560, 3.0, 40, 11, 5, [('fitted_brig_hull', 1), ('brig_mast', 3), ('brig_rig', 3), ('saker', 10), ('copper_sheathing', 6), ('log_line', 1)], 'xebec_plan', 3, True)
player_ship('corvette', 'Corvette', 4, '2a3a4a', 'f0f0e8', 3, 7, 820, 2.7, 52, 12, 5, [('fitted_brig_hull', 2), ('frigate_mast', 2), ('brig_rig', 3), ('long_nine', 8), ('copper_sheathing', 8), ('ships_compass', 1), ('back_staff', 1)], 'corvette_plan', 3)
player_ship('galleon', 'Galleon', 5, '7a3a1a', 'f0e0b0', 3, 9, 1700, 1.5, 72, 12, 5, [('fitted_frigate_hull', 1), ('frigate_mast', 3), ('frigate_rig', 3), ('long_nine', 16), ('cargo_hold', 4), ('ships_stores', 2), ('astrolabe', 1)], 'galleon_plan', 4)
player_ship('first_rate', 'First-rate', 6, '1a1a24', 'f8f8f0', 3, 12, 3800, 1.8, 180, 17, 6, [('man_of_war', 1), ('fitted_great_hull', 1), ('great_mast', 2), ('great_rig', 2), ('carronade', 20), ('sea_mortar', 4), ('great_atlas', 1), ('cloth_of_state', 2), ('ships_stores', 6)], 'first_rate_plan', 5)
for p_, plan, marks, g in (('islander', 'pinnace_plan', 8, 1), ('crown', 'cutter_plan', 20, 2), ('company', 'whaler_plan', 30, 2), ('crown', 'bomb_ketch_plan', 80, 3), ('brethren', 'xebec_plan', 70, 3), ('crown', 'corvette_plan', 90, 3), ('company', 'galleon_plan', 160, 4), ('crown', 'first_rate_plan', 600, 5)): buy(p_, plan, marks, g)
CAPCOAT = {'merchant_sailor': '6a4a2a', 'navy_marine': '2a4a9a', 'pirate_cutthroat': '1a1a1a', 'islander_warrior': 'c08030', 'drowned_sailor': '2a4a44'}
def foe(n, lab, size, hull, sail, flg, masts, ports, hp, crew, count, prize, loot, hit, far, chance, tatter=False, oars=False):
    cap = n + '_captain'
    folk(cap, 'Captain of the ' + lab.lower(), CAPCOAT[crew], 'f0c840', 0, 60 + hp // 6, [drop(prize, 1, 1)] + [drop(i, a, b) for i, a, b in loot], None, True, skin='a87850' if 'islander' in crew else '9ad0c0' if 'drowned' in crew else 'e0b890', cap=1)
    ship_mob(n, lab, size, hull, sail, flg, masts, ports, hp, tatter=tatter, oars=oars, cap=2 if size < 5 else 1, spawn={'water': True, 'biome': 'ocean', 'chance': chance}, swim={'hunts': True, 'sight': 20, 'bite': 0.0, 'rest_ticks': 40} if hit else {},
             strike={'share': hit, 'range': far, 'rest_ticks': 80} if hit else None, ship={'wreck': 'ship_wreckage'}, boarding={'crew': crew, 'count': count, 'captain': cap, 'deck': 'ship_planks'}, drops=[drop('sawn_planking', 1, 4)] + [drop(i, a // 2, max(1, b // 2)) for i, a, b in loot[:2]])
foe('fishing_smack', 'Fishing smack', 2, '7a6a4a', 'c8b890', 'c8b890', 1, 0, 70, 'merchant_sailor', 2, 'jolly_boat', [('salt_fish_barrel', 1, 3), ('snapper_fillet', 2, 6)], 0.0, 0, 0.012)
foe('smuggler_lugger', "Smuggler's lugger", 3, '2a2a2a', '5a4a3a', '2a1a2a', 2, 1, 210, 'pirate_cutthroat', 3, 'cutter', [('twist_tobacco', 2, 5), ('rum_bottle', 2, 6), ('cutter_plan', 0, 1)], 0.03, 7, 0.006)
foe('whaling_ship', 'Whaling ship', 4, '3a3a34', 'd8d0b8', 'c8a040', 3, 1, 520, 'merchant_sailor', 6, 'whaler', [('whale_oil', 2, 6), ('whaler_plan', 0, 1), ('scrimshaw_tooth', 0, 2)], 0.0, 0, 0.004)
foe('privateer_brig', 'Privateer', 4, '3a5a3a', 'f0e8d0', '3a5a3a', 2, 5, 600, 'navy_marine', 7, 'brig', [('letter_of_marque', 0, 1), ('gold_coin', 8, 24), ('cartridge_box', 2, 6)], 0.08, 11, 0.004)
foe('navy_bomb_vessel', 'Navy bomb vessel', 4, '2a3a5a', 'f0f0e8', '2a4a9a', 2, 4, 480, 'navy_marine', 6, 'bomb_ketch', [('bomb_ketch_plan', 0, 1), ('bomb_shell', 4, 10), ('navy_insignia', 1, 3)], 0.15, 17, 0.002)
foe('corsair_xebec', 'Corsair xebec', 4, 'a04a2a', 'c03030', '1a1a1a', 3, 6, 640, 'pirate_cutthroat', 8, 'xebec', [('xebec_plan', 0, 1), ('spice_chest', 1, 3), ('scarlet_calico', 1, 4)], 0.09, 11, 0.003, oars=True)
foe('navy_corvette', 'Navy corvette', 4, '2a3a4a', 'f0f0e8', '2a4a9a', 3, 7, 820, 'navy_marine', 7, 'corvette', [('corvette_plan', 0, 1), ('navy_insignia', 2, 4), ('ships_compass', 0, 1)], 0.09, 12, 0.003)
foe('silver_galleon', 'Silver-fleet galleon', 5, '7a3a1a', 'f0e0b0', 'f0c840', 3, 9, 1800, 'navy_marine', 10, 'galleon', [('silver_bar', 10, 20), ('galleon_plan', 1, 1), ('jewel_casket', 0, 1)], 0.11, 12, 0.001)
foe('pirate_frigate', 'Pirate frigate', 5, '2a1a1a', '2a2a2a', 'a02828', 3, 8, 1300, 'pirate_cutthroat', 10, 'frigate', [('pirate_flag_taken', 2, 4), ('treasure_map', 1, 2), ('old_rum', 1, 4)], 0.12, 12, 0.0015)
foe('plague_ship', 'Plague ship', 4, '4a4a3a', 'a0a080', '6a8a20', 3, 3, 700, 'drowned_sailor', 8, 'merchantman', [('old_carving', 1, 4), ('cursed_doubloon', 0, 1), ('amphora', 1, 3)], 0.06, 8, 0.0015, tatter=True)
foe('dread_sovereign', 'Dread Sovereign', 6, '1a0a0a', '1a1a1a', 'e03030', 3, 12, 4400, 'pirate_cutthroat', 14, 'first_rate', [('first_rate_plan', 1, 1), ('sealed_coffer', 1, 2), ('pirate_lords_ring', 0, 1), ('platinum_coin', 4, 10)], 0.18, 16, 0.0003)

# ---- more to build with -------------------------------------------------------------------------------------------------------------------------------------
BLOCKS3 = [('coquina_wall', 'Coquina wall', 'brick', 'e0d0b0', 2, {}, [('conch_shell', 1), ('sand', 3), ('limestone', 1)]), ('tabby_wall', 'Tabby wall', 'rock', 'd0c8b8', 2, {}, [('oyster', 2), ('sand', 2), ('limestone', 1)]), ('coral_stone_brick', 'Coral-stone brick', 'brick', 'f0b0a0', 3, {}, [('coral', 3), ('clay', 1)]),
           ('holystoned_deck', 'Holystoned deck', 'planks', 'e0d0a8', 3, {'tool': 'axe'}, [('ship_planks', 4), ('sand', 1)]), ('gun_deck', 'Gun deck', 'planks', '8a2a22', 3, {'tool': 'axe'}, [('ship_planks', 4), ('annatto_seed', 1)]), ('mahogany_panel', 'Mahogany panelling', 'planks', '6a2a1a', 4, {'tool': 'axe'}, [('hardwood', 3), ('tallow', 1)]),
           ('teak_deck', 'Teak deck', 'planks', 'b08040', 4, {'tool': 'axe'}, [('hardwood', 3), ('lamp_oil', 1)]), ('copper_bottom', 'Coppered hull', 'paving', 'c07a4a', 4, {}, [('copper_sheathing', 1), ('tarred_planks', 3)]), ('verdigris_hull', 'Weathered copper', 'paving', '5aa890', 4, {}, [('copper_bottom', 4), ('salt', 1)]),
           ('pink_stucco', 'Pink stucco', 'rock', 'f0b8b0', 2, {}, [('whitewash', 4), ('annatto_seed', 1)]), ('ochre_stucco', 'Ochre stucco', 'rock', 'e0b050', 2, {}, [('whitewash', 4), ('wild_ginger', 1)]), ('sea_blue_stucco', 'Blue stucco', 'rock', '70a8d0', 2, {}, [('whitewash', 4), ('indigo_plant', 1)]),
           ('cobbled_lane', 'Cobbled lane', 'paving', '7a7a74', 2, {}, [('stone', 3), ('gravel', 1)]), ('shell_path', 'Shell path', 'rock', 'f0e8e0', 2, {'tool': 'shovel', 'mine_time': 0.3}, [('oyster', 2), ('sand', 2)]), ('cedar_shingles', 'Shingle roof', 'roof', '7a5a3a', 2, {'tool': 'axe'}, [('sawn_planking', 3)]),
           ('lead_roof', 'Lead roof', 'roof', '7a8a9a', 4, {}, [('tin_ingot', 1), ('sawn_planking', 2)]), ('battlement_stone', 'Battlement', 'brick', '5a5a60', 3, {}, [('fort_stone', 4), ('wrought_iron', 1)]), ('magazine_brick', 'Magazine brick', 'brick', '4a3a3a', 3, {}, [('harbour_brick', 4), ('charcoal', 1)]),
           ('sail_canvas_block', 'Sail', 'paving', 'f0e8d0', 2, {'tool': 'axe', 'burns': True}, [('sailcloth', 2)]), ('black_sail_block', 'Black sail', 'paving', '2a2a2a', 3, {'tool': 'axe', 'burns': True}, [('sailcloth', 2), ('logwood_dye', 1)]), ('red_sail_block', 'Red sail', 'paving', 'c03030', 3, {'tool': 'axe', 'burns': True}, [('sailcloth', 2), ('scarlet_dye', 1)]),
           ('gilded_stern', 'Gilded sternwork', 'brick', 'e0b030', 5, {'tool': 'axe'}, [('mahogany_panel', 2), ('gold_ingot', 1)]), ('obsidian_cobble', 'Obsidian cobble', 'paving', '2a2a3a', 3, {}, [('black_beach', 3), ('pumice_rock', 1)]), ('bone_wall', 'Bone wall', 'brick', 'e8e0d0', 3, {}, [('old_carving', 1), ('skull_rock', 3)])]
for n, lab, kind, col, tier, kw, ins in BLOCKS3: COL[n] = c(col); block(block_tex(kind, n, c(col), dk(c(col), 0.7)) if kind == 'brick' else block_tex(kind, n, c(col)), n, lab, tier, 0.04, **kw); make('sail_loft' if 'sail' in n else 'shipwright' if kw.get('tool') == 'axe' else 'sawpit', n, ins, 1, 4, 60)
STAIRED2 = ['coquina_wall', 'tabby_wall', 'coral_stone_brick', 'holystoned_deck', 'mahogany_panel', 'teak_deck', 'pink_stucco', 'ochre_stucco', 'sea_blue_stucco', 'battlement_stone', 'lighthouse_white', 'reef_rock', 'gilded_stern', 'bone_wall']
for n in STAIRED2:
    lab, tier = next(i['label'] for i in items if i['name'] == n), next(i['tier'] for i in items if i['name'] == n)
    for what, base in (('column', 'wood_column'), ('stairs', 'wood_stairs')):
        d = copy.deepcopy(BT[base]); d.update(name='%s_%s' % (n, what), label='%s %s' % (lab, what), drop='%s_%s' % (n, what), texture='tiles/%s_%s.png' % (n, what))
        Image.open(os.path.join(TEX, 'tiles', n + '.png')).save(os.path.join(TEX, 'tiles', '%s_%s.png' % (n, what)))
        tiles.append(d); item('%s_%s' % (n, what), '%s %s' % (lab, what), tier, 'blocks', 0.04, places='%s_%s' % (n, what)); make('sawpit', '%s_%s' % (n, what), [(n, 1)], 1, 2, 60)
SHIPDECO = [('gunport_hatch', 'Gunport', P().r(2, 3, 13, 12, c('8a2a22')).r(4, 5, 11, 10, INK).r(5, 7, 10, 8, c('3a3a44')).r(2, 3, 13, 3, c('b04a3a')), [('sawn_planking', 2), ('iron_fittings', 1)], 'shipwright', {}),
            ('stern_window', 'Stern window', P().r(2, 2, 13, 13, GOLD).r(3, 3, 12, 12, c('a0d0e8')).r(7, 3, 8, 12, GOLD).r(3, 7, 12, 8, GOLD).p(4, 4, WHITE), [('glass', 4), ('gold_ingot', 1), ('sawn_planking', 1)], 'shipwright', {}),
            ('furled_sail', 'Furled sail', P().r(0, 4, 15, 5, OAK).r(1, 6, 14, 9, CANVAS).r(1, 9, 14, 9, dk(CANVAS, 0.8)).r(3, 6, 3, 9, ROPE).r(8, 6, 8, 9, ROPE).r(12, 6, 12, 9, ROPE), [('sailcloth', 2), ('cordage', 2), ('seasoned_timber', 1)], 'sail_loft', {}),
            ('canvas_awning', 'Awning', P().r(0, 2, 15, 4, c('c03030')).r(2, 2, 4, 4, WHITE).r(8, 2, 10, 4, WHITE).r(13, 2, 15, 4, WHITE).r(0, 5, 15, 5, dk(c('c03030'))).r(1, 5, 1, 15, OAK).r(14, 5, 14, 15, OAK), [('sailcloth', 2), ('scarlet_dye', 1), ('seasoned_timber', 2)], 'sail_loft', {}),
            ('bulwark_rail', 'Bulwark rail', P().r(0, 9, 15, 10, OAK).r(1, 11, 2, 15, dk(OAK)).r(5, 11, 6, 15, dk(OAK)).r(9, 11, 10, 15, dk(OAK)).r(13, 11, 14, 15, dk(OAK)), [('sawn_planking', 1), ('treenails', 2)], 'sawpit', {}),
            ('iron_grating', 'Deck grating', P().r(1, 1, 14, 14, OAK).r(3, 3, 5, 5, INK).r(7, 3, 9, 5, INK).r(11, 3, 12, 5, INK).r(3, 7, 5, 9, INK).r(7, 7, 9, 9, INK).r(11, 7, 12, 9, INK).r(3, 11, 5, 12, INK).r(7, 11, 9, 12, INK).r(11, 11, 12, 12, INK), [('sawn_planking', 2), ('wrought_iron', 1)], 'shipwright', {}),
            ('binnacle', 'Binnacle', P().r(5, 8, 10, 15, c('6a2a1a')).r(4, 4, 11, 8, BRASS).disc(7.5, 6, 1.8, c('ffe890')).r(5, 3, 10, 3, lt(BRASS)), [('ships_compass', 1), ('hardwood', 2), ('ships_lamp', 1)], 'instrument_maker', {'light': 10}),
            ('officers_bunk', "Officer's bunk", P().r(1, 9, 14, 12, c('6a2a1a')).r(2, 7, 13, 9, c('2a4a9a')).r(2, 7, 5, 8, WHITE).r(1, 13, 2, 15, c('4a1a10')).r(13, 13, 14, 15, c('4a1a10')).r(1, 4, 1, 9, c('6a2a1a')), [('mahogany_panel', 3), ('indigo_calico', 2), ('cotton_yarn', 4)], 'shipwright', {'bed': {'speed': 3.0}}),
            ('ships_mess_table', 'Mess table', P().r(1, 9, 14, 10, OAK).r(2, 4, 2, 9, ROPE).r(13, 4, 13, 9, ROPE).r(4, 7, 6, 8, c('d8d0c0')).r(9, 6, 10, 8, c('7a9a8a')), [('sawn_planking', 2), ('cordage', 2)], 'sawpit', {}),
            ('gun_rack', 'Gun rack', P().r(1, 2, 14, 3, OAK).r(1, 12, 14, 13, OAK).r(3, 1, 4, 14, c('5a5a66')).r(7, 1, 8, 14, c('5a5a66')).r(11, 1, 12, 14, c('5a5a66')).r(3, 10, 4, 14, c('6a3a1a')).r(7, 10, 8, 14, c('6a3a1a')).r(11, 10, 12, 14, c('6a3a1a')), [('sawn_planking', 2), ('gun_stock', 1)], 'gunsmith', {}),
            ('cutlass_rack', 'Cutlass rack', P().r(1, 3, 14, 4, OAK).line(3, 13, 6, 4, c('c0c8d0')).line(7, 13, 10, 4, c('c0c8d0')).line(11, 13, 13, 5, c('c0c8d0')).p(3, 13, BRASS).p(7, 13, BRASS).p(11, 13, BRASS), [('sawn_planking', 1), ('iron_cutlass', 1)], 'gun_foundry', {}),
            ('sea_trunk', 'Sea trunk', P().r(2, 6, 13, 14, c('3a2a22')).r(2, 6, 13, 7, c('5a4a3a')).r(2, 10, 13, 10, BRASS).r(7, 8, 8, 11, BRASS), [('mahogany_panel', 2), ('brass_sheet', 1), ('leather', 1)], 'cooperage', {'storage': {'columns': 4, 'rows': 3}, 'model': 'box'}),
            ('ships_stove', "Ship's stove", P().r(3, 6, 12, 14, c('3a3a44')).r(5, 9, 10, 12, c('f07020')).r(6, 10, 9, 11, c('ffd060')).r(7, 1, 8, 5, c('5a5a66')).r(2, 14, 13, 15, c('8a3a2a')), [('wrought_iron', 6), ('harbour_brick', 2)], 'gun_foundry', {'light': 11, 'hot': True}),
            ('chart_rack', 'Chart rack', P().r(2, 2, 13, 13, OAK).r(3, 3, 12, 12, dk(OAK)).disc(5, 5, 1.2, PAPER).disc(9, 5, 1.2, PAPER).disc(7, 8, 1.2, PAPER).disc(11, 9, 1.2, PAPER).disc(5, 11, 1.2, PAPER), [('sawn_planking', 2), ('vellum', 3)], 'chart_table', {}),
            ('hanging_cage', 'Gibbet cage', P().r(7, 0, 8, 3, c('5a5a66')).r(4, 4, 11, 4, c('5a5a66')).r(4, 4, 4, 13, c('5a5a66')).r(11, 4, 11, 13, c('5a5a66')).r(7, 4, 7, 13, c('5a5a66')).r(4, 13, 11, 13, c('5a5a66')).disc(7.5, 8, 1.6, c('e8e0d0')), [('wrought_iron', 4)], 'gun_foundry', {}),
            ('market_stall', 'Market stall', P().r(1, 2, 14, 4, c('e0b030')).r(3, 2, 5, 4, c('2a4a9a')).r(9, 2, 11, 4, c('2a4a9a')).r(1, 5, 1, 15, OAK).r(14, 5, 14, 15, OAK).r(1, 10, 14, 11, OAK).disc(4, 8.5, 1.4, c('f0d040')).disc(8, 8.5, 1.4, c('e05040')).disc(11, 8.5, 1.4, c('a0d030')), [('sawn_planking', 3), ('saffron_calico', 1)], 'sawpit', {}),
            ('net_float', 'Glass float', P().disc(7.5, 8, 4.5, c('70c0b0')).disc(6, 6.5, 1.4, WHITE).line(3, 8, 12, 8, ROPE).line(7, 3, 8, 13, ROPE), [('sea_glass', 4), ('cordage', 1)], 'ropewalk', {'light': 3}),
            ('whale_jaw_arch', 'Whalebone arch', P().line(2, 15, 5, 3, IVORY).line(3, 15, 6, 3, IVORY).line(13, 15, 10, 3, IVORY).line(12, 15, 9, 3, IVORY).r(6, 1, 9, 2, IVORY), [('scrimshaw_tooth', 2), ('marlin_bill', 2)], 'scrimshaw_bench', {})]
for n, lab, pic, ins, bench_, kw in SHIPDECO:
    deco(pic, n, lab, 3, ins, bench_, **kw)
    if kw.get('storage'): items[-1]['container'] = {'slots': 12}
t = P(); t.line(1, 0, 14, 15, ROPE); t.line(14, 0, 1, 15, ROPE); [t.r(2, y, 13, y, dk(ROPE)) for y in (3, 7, 11, 15)]; t.save('tiles', 'ratlines'); icon_of('ratlines')
tile('ratlines', 'Ratlines', False, tool='axe', mine_time=0.2, climbable=True); item('ratlines', 'Ratlines', 2, 'crafted', 0.2, places='ratlines'); make('ropewalk', 'ratlines', [('tarred_rope', 1), ('cordage', 2)], 1, 4, 60)
t = P(); t.r(6, 0, 9, 15, c('7a4a22')); t.r(6, 0, 6, 15, c('9a6a3a')); t.r(5, 7, 10, 8, c('5a5560')); t.save('tiles', 'mast_section'); icon_of('mast_section')
tile('mast_section', 'Mast', False, tool='axe', mine_time=0.4, climbable=True); item('mast_section', 'Mast', 2, 'crafted', 0.3, places='mast_section'); make('shipwright', 'mast_section', [('seasoned_timber', 2), ('iron_fittings', 1)], 1, 4, 60)

# ---- more places ----------------------------------------------------------------------------------------------------------------------------------------------
def X(base, **more_): d = dict(base); d.update({k: L(None, base['.'].get('wall'), v) if not isinstance(v, dict) else v for k, v in more_.items()}); return d
structure('harbour_dye_house', ['~RRRRRRRR~', '~#.l..l.#~', '~D.y.v.cD~', '~FFFFFFFF~'], X(BRICKS, y='dye_works', v='weavers_loom'))
structure('harbour_customs', ['~~RRRRRR~~', '~RRRRRRRR~', '~#.W..W.#~', '~D.d.s.1D~', '~FFFFFFFF~'], X(WHITE_, d='captains_desk'))
structure('harbour_chapel', ['~~~+~~~', '~~RRR~~', '~RRRRR~', '~#.W.#~', '~D.l.D~', '~FFFFF~'], X(WHITE_, **{'+': L(None, None, 'ships_bell')}))
structure('instrument_shop', ['~RRRRRR~', '~#.l.W#~', '~D.u.CD~', '~FFFFFF~'], X(BRICKS, u='instrument_maker'))
structure('harbour_stalls', ['~u~u~L~u~', '~FFFFFFF~'], X(BRICKS, u=L(None, None, 'market_stall')))
structure('fort_armoury', ['~RRRRRRR~', '~#.l.j.#~', '~D.u.j.D~', '~FFFFFFF~'], X(FORT, u='gunsmith', j='gun_rack'))
structure('fort_infirmary', ['~RRRRRR~', '~#.l..#~', '~D.u.hD~', '~FFFFFF~'], X(FORT, u='ships_surgery'))
structure('fort_gibbet', ['~u~~~u~', '~FFFFF~'], X(FORT, u=L(None, None, 'hanging_cage')))
structure('pirate_curing_shed', ['~RRRRRR~', '~#.l..#~', '~D.u.vD~', '~FFFFFF~'], X(SHACK, u='curing_shed', v='scrimshaw_bench'))
structure('pirate_trophy_wall', ['~u~j~u~', '~FFFFF~'], X(SHACK, u=L(None, None, 'skull_pile'), j=L(None, None, 'brethren_flagpole')))
structure('islander_tiki_ring', ['~t~u~v~', '~FFFFF~'], X(STILT, t=L(None, None, 'tiki_of_rain'), u=L(None, None, 'tiki_of_fire'), v=L(None, None, 'tiki_of_the_deep')))
structure('islander_pearl_hut', ['~RRRRR~', '~#.l.#~', '~D.u.D~', '~FFFFF~'], X(STILT, u='sea_jeweller'))
structure('fish_chandlery', ['~RRRRR~', '~#.l.#~', '~D.u.D~', '~FFFFF~'], X(SHACK, u='chandlery'))
for v_, add in (('company_harbour', ['harbour_dye_house', 'harbour_customs', 'harbour_chapel', 'instrument_shop', 'harbour_stalls']), ('crown_fort', ['fort_armoury', 'fort_infirmary', 'fort_gibbet']), ('free_port', ['pirate_curing_shed', 'pirate_trophy_wall']),
                ('stilt_town', ['islander_tiki_ring', 'islander_pearl_hut']), ('fishing_hamlet', ['fish_chandlery'])):
    next(v for v in villages if v['name'] == v_)['buildings'] += add
structure('sunken_frigate', ['~~~m~~~m~~~m~~~', '~~~m~~~m~~~m~~~', '#####.###.#####', '~#c.G..b..G.c#~', '~#..b.c..b...#~', '~~###########~~'], SUNK)
structure('sunken_temple', ['~~~I~~~I~~~', '~~IIiiiII~~', '~IIi.x.iII~', '~Ii..L..iI~', 'CIIIIIIIIIC'], dict(SUNK, L=L(None, 'carved_idol_stone', 'sunken_reliquary')))
ocean['wrecks'] += ['sunken_frigate', 'sunken_temple']
ocean['biomes'] = [b['name'] for b in biomes]; assert len(ocean['biomes']) <= 16

# ---- written out -------------------------------------------------------------------------------------------------------------------------------------
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v)) for k, v in obj.items()) + '\n}\n')
for r_ in recipes:
    if r_.get('formula') is None: r_.pop('formula', None)
for kind, group in (('tile', tiles), ('item', items), ('creature', mobs)):
    names = [t['name'] for t in group]; assert len(names) == len(set(names)), (kind, sorted({n for n in names if names.count(n) > 1}))
    long_ = [t['name'] for t in group if len(('caged_' if kind == 'creature' and not t.get('caged') else '') + t['name']) > 31]; assert not long_, long_
have_i = set(BI) | {i['name'] for i in items}; have_t = set(BT) | {t['name'] for t in tiles}
bad = sorted({i['item'] for r in recipes for i in r['inputs'] if i['item'] not in have_i} | {r['output'].get('item') for r in recipes if r['output'].get('item') and r['output']['item'] not in have_i} | {r['formula'] for r in recipes if r.get('formula') and r['formula'] not in have_i})
bad += sorted({x['item'] for r in recipes for x in r['output'].get('one_of', []) if x['item'] not in have_i})
bad += sorted({d['item'] for m in mobs for d in m.get('drops', []) if d['item'] not in have_i} | {i['places'] for i in items if i.get('places') and i['places'] not in have_t} | {m['caged'] for m in mobs if m.get('caged') and m['caged'] not in have_i})
bad += sorted({x['item'] for t in tiles for l in t.get('loot', []) for x in (l.get('one_of') or [l]) if x.get('item') and x['item'] not in have_i})
bad += sorted({d['item'] for t in tiles for d in t.get('drops', []) if d['item'] not in have_i} | {t['drop'] for t in tiles if t.get('drop') and t['drop'] not in have_i})
bad += sorted({v for s_ in structures for cell in s_['legend'].values() for v in cell.values() if v not in have_t} | {b[k] for b in biomes for k in ('surface', 'soil') if b[k] not in have_t} | {p['tile'] for b in biomes for p in b.get('plants', []) if p['tile'] not in have_t})
assert not bad, bad
for n, i in enumerate(items):
    if i.get('equip'):
        o = {}
        for k, v in i.items():
            o[k] = v
            if k == 'equip': o['worn'] = 'worn/%s.png' % i['name']
        items[n] = o
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'sets': sets, 'items': items}); dump('recipes.json', {'recipes': recipes}); dump('trees.json', {'trees': trees}); dump('mobs.json', {'mobs': mobs})
dump('worldgen.json', {'structures': structures, 'villages': villages, 'ocean': ocean, 'biomes': biomes, 'biome_plants': plants, 'veins': veins})
dump('trade.json', {'categories': [{'name': 'cargo', 'label': 'Cargo'}], 'villages': {'only': True, 'swing': 0.35, 'share': 0.6, 'days': 7, 'specialities': 5}})
json.dump({'label': 'Plunder & Thunder', 'icon': 'icon.png', 'description': 'A world of sea and islands: harbours, ships to build and to take, cargo, crews, buried treasure and what lies on the bottom.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
ship_pic(3, c('3a2a22'), c('2a2a2a'), c('1a1a1a'), 2, 3).im.save(os.path.join(mod, 'icon.png'))
BY = {}
for r_ in recipes:
    if r_['output'].get('item') and all(i['item'] != r_['output']['item'] for i in r_['inputs']): BY.setdefault(r_['output']['item'], []).append([i['item'] for i in r_['inputs']])
NATURAL = set(BI) | {d['item'] for m in mobs for d in m.get('drops', [])} | {d_['item'] for t_ in tiles for d_ in t_.get('drops', [])} | {t_['drop'] for t_ in tiles if t_.get('drop')}
NATURAL -= {s_[0] for s_ in SHIPS} | {i['name'] for i in items if i.get('places')} | {m[0] for m in MAT} | {g[0] for g in GUNS}
DEPTH = {}
def steps(i, seen=()):
    if i in DEPTH: return DEPTH[i]
    if i in NATURAL or i not in BY or i in seen: return 0
    d = min(1 + max((steps(x, seen + (i,)) for x in ins), default=0) for ins in BY[i])
    if not seen: DEPTH[i] = d
    return d
print('makings: ' + ', '.join('%s %d' % (s_[0], steps(s_[0])) for s_ in SHIPS))
print('%d tiles, %d items, %d recipes, %d trees, %d creatures, %d biomes, %d structures, %d sets' % (len(tiles), len(items), len(recipes), len(trees), len(mobs), len(biomes), len(structures), len(sets)))
tools = os.path.dirname(os.path.abspath(__file__))
subprocess.run([sys.executable, os.path.join(tools, 'worn_art.py'), repo], check=True)
subprocess.run([sys.executable, os.path.join(tools, 'caged_art.py'), repo], check=True)
subprocess.run([sys.executable, os.path.join(tools, 'stair_column_art.py'), repo], check=True)
