#!/usr/bin/env python3
"""Writes the Beyond the Sky mod: its data files and its textures.

    python3 tools/mod_beyond_the_sky.py .

Rockets built of parts on a pad, places with no air and little weight - orbit, the Moon, the asteroid belt, the red
planet - bases that must be sealed and aired, and a great deal to find out there. The plan and what the owner decided
are in ModDescriptions/BeyondTheSky.txt.

Metal things (ores, ingots, tools) are the game's own pictures in other colours; everything else is drawn here.
Either edit this and run it again, or edit the mod's files by hand and stop using this: not both.
"""
import colorsys, copy, json, os, random, subprocess, sys, zlib
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
DATA = os.path.join(repo, 'data')
mod = os.path.join(repo, 'mods/beyond_the_sky'); TEX = os.path.join(mod, 'textures')
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

# ==== Beyond the Sky ===================================================================================================================
STONE = c('8a8a8a')
T6, T7, T8, T9, T10, T11 = 6, 7, 8, 9, 10, 11
AGE_TIER = [6, 7, 9, 10]          # home and orbit; the Moon; the Belt; the red planet
AGE_NAME = ['flight', 'lunar', 'belt', 'mars']
WHITE2, HULL, DARK, AMBER, CYAN = c('e8ecf0'), c('b8c0cc'), c('2a2e3a'), c('f0a030'), c('60d0f0')

def plant(arch, col, stage, stem=c('3a8a4a')):
    """Something growing, at one of four stages."""
    t = P(); h = [3, 6, 9, 12][stage]; top = 15 - h
    if arch == 'grass':
        for x, lean in ((5, -1), (7, 0), (9, 1), (11, 0))[:2 if stage < 2 else 4]: t.line(x, 15, x + lean, top + (x % 3), stem if x % 2 else col)
        if stage == 3: t.p(7, top - 1, lt(col, 1.3)).p(9, top, lt(col, 1.3))
    elif arch == 'flower':
        t.r(7, top + 2, 7, 15, stem)
        if stage >= 2: t.disc(7.5, top + 1.5, 1.2 + stage * 0.5, col); t.p(7, top + 1, lt(col, 1.4))
        elif stage == 1: t.p(7, top + 1, col)
    elif arch == 'root':
        for x, lean in ((6, -2), (7, 0), (9, 2)): t.line(7, 13, x + lean, top + 2, stem)
        if stage >= 2: t.r(6, 13, 9, 15, col); t.p(6, 13, lt(col))
    elif arch == 'mushroom':
        t.r(7, 16 - max(2, h // 2), 8, 15, c('e8dcc0')); w = 1 + stage
        t.r(7 - w, 15 - h // 2 - 2, 8 + w, 15 - h // 2, col); t.r(8 - w, 15 - h // 2 - 3, 7 + w, 15 - h // 2 - 3, lt(col)); t.p(7 - w + 1, 15 - h // 2 - 1, lt(col, 1.5))
    elif arch == 'fern':
        t.r(7, top + 1, 7, 15, stem)
        for k in range(stage + 1):
            y = 13 - k * 3; t.line(7, y, 4 - (k % 2), y - 2, col); t.line(7, y, 10 + (k % 2), y - 2, col)
    elif arch == 'vine':
        ys = list(range(15, top, -1))
        for k, y in enumerate(ys): t.p(7 + (1 if (k // 2) % 2 else -1), y, stem)
        for k, y in enumerate(ys[2::3]): t.r(8 if k % 2 else 5, y, 9 if k % 2 else 6, y + 1, col)
    elif arch == 'coral':
        t.r(7, 12 - stage, 8, 15, col)
        for k in range(stage + 1): t.line(7, 13 - k * 2, 4 - k % 2, 10 - k * 2, col); t.line(8, 12 - k * 2, 11 + k % 2, 9 - k * 2, lt(col))
    else:
        t.r(7, 13, 8, 15, dk(stem)); t.disc(7.5, 12 - stage, 1.5 + stage, stem)
        for k in range(stage * 2): t.p(5 + (k * 3) % 6, 13 - stage - (k * 2) % (2 + stage), col)
    return t
def panel(col, rivets=True, stripe=None, window=None):
    t = P(col); t.r(0, 0, 15, 0, lt(col, 1.12)); t.r(0, 15, 15, 15, dk(col, 0.7)); t.r(0, 0, 0, 15, lt(col, 1.12)); t.r(15, 0, 15, 15, dk(col, 0.7))
    if rivets: [t.p(x, y, dk(col, 0.6)) for x in (2, 13) for y in (2, 13)]
    if stripe: t.r(0, 6, 15, 9, stripe)
    if window: t.r(4, 4, 11, 11, dk(col, 0.5)); t.r(5, 5, 10, 10, window); t.p(6, 6, WHITE)
    return t
def machine(col, face, light=None, legs=True):
    t = P().r(2, 3, 13, 13, col); t.r(2, 3, 13, 3, lt(col)); t.r(2, 13, 13, 13, dk(col, 0.6)); face(t)
    if legs: t.r(2, 14, 4, 15, dk(col, 0.5)); t.r(11, 14, 13, 15, dk(col, 0.5))
    if light: t.p(12, 4, light)
    return t
def can(col, band=None, cap=c('8a8e9a')):
    t = P().r(5, 4, 10, 13, col); t.r(6, 2, 9, 3, cap); t.r(5, 4, 5, 13, lt(col)); t.r(10, 5, 10, 13, dk(col)); t.r(5, 13, 10, 13, dk(col, 0.6))
    if band: t.r(5, 7, 10, 9, band)
    return t.outline()
def chip(col, pins=GOLD, mark=None):
    t = P().r(3, 4, 12, 11, col); t.r(3, 4, 12, 4, lt(col)); [t.p(x, y, pins) for x in (4, 6, 8, 10) for y in (3, 12)]; t.r(6, 6, 9, 9, dk(col, 0.6))
    if mark: t.p(7, 7, mark).p(8, 8, mark)
    return t.outline()
def frame_pic(col):
    t = P(); t.r(2, 2, 13, 3, col); t.r(2, 12, 13, 13, col); t.r(2, 2, 3, 13, col); t.r(12, 2, 13, 13, col); t.line(3, 3, 12, 12, dk(col)); t.line(12, 3, 3, 12, dk(col)); return t.outline()
def sample(col, vial=c('d8e8f0')):
    t = P().r(6, 2, 9, 3, c('5a5560')); t.r(5, 4, 10, 13, vial); t.r(5, 8, 10, 13, col); t.r(5, 8, 10, 8, lt(col)); t.p(6, 5, WHITE); return t.outline()
def card(col, mark):
    t = P().r(3, 3, 12, 12, col); t.r(3, 3, 12, 3, lt(col)); t.r(5, 5, 10, 6, mark); t.r(5, 8, 8, 8, mark); t.r(5, 10, 10, 10, dk(col, 0.6)); return t.outline()
def glyph(col, n):
    t = P().r(3, 2, 12, 13, c('3a3a44')); t.r(3, 2, 12, 2, c('5a5a66')); g = rnd('glyph%d' % n)
    for _ in range(7): x, y = g.randrange(5, 10), g.randrange(4, 11); t.r(x, y, x + g.randrange(0, 2), y + g.randrange(0, 2), col)
    t.p(7, 7, WHITE); return t.outline()
def suit_piece(kind, col, vis=CYAN):
    t = P()
    if kind == 'helmet': t.disc(7.5, 7.5, 5, col); t.r(5, 6, 10, 9, vis); t.p(6, 6, WHITE); t.r(4, 12, 11, 13, dk(col))
    elif kind == 'chest': t.r(4, 3, 11, 12, col); t.r(2, 3, 3, 9, col); t.r(12, 3, 13, 9, col); t.r(6, 5, 9, 7, vis); t.r(4, 12, 11, 12, dk(col)); t.p(5, 9, AMBER).p(10, 9, RED)
    elif kind == 'legs': t.r(4, 2, 11, 5, col); t.r(4, 6, 6, 13, col); t.r(9, 6, 11, 13, col); t.r(4, 9, 6, 9, dk(col)); t.r(9, 9, 11, 9, dk(col))
    else: t.r(3, 7, 6, 13, col); t.r(9, 7, 12, 13, col); t.r(2, 12, 6, 13, dk(col)); t.r(9, 12, 13, 13, dk(col)); t.r(3, 7, 6, 7, lt(col))
    return t.outline()
def deco(pic, name, label, tier, ins, bench='machine_shop', g=1, count=1, mult=1.0, **kw):
    pic.save('tiles', name); icon_of(name); kw.setdefault('mine_time', 0.3)
    tile(name, label, False, **kw); item(name, label, tier, 'crafted', mult, places=name)
    if ins: rec(bench, name, ins, count, level=g, time=120)
FORMULA_OF = {}
def make(bench, out, ins, g=1, count=1, time=160, need=None):
    rec(bench, out, ins, count, level=g, time=time, formula=need)
def comp(pic, name, label, tier, mult=0.8, cat='crafted', **kw): return thing(pic, name, label, tier, cat, mult, **kw)

# ---- benches ----------------------------------------------------------------------------------------------------------------------------
GRADE = [c('8a8e9a'), c('d8e0f0'), c('f0a030'), c('e05030')]
def bench_face(kind):
    def f(t):
        if kind == 'machine_shop': t.r(4, 5, 8, 9, DARK); t.r(5, 6, 7, 8, c('c0c8d0')); t.r(10, 5, 11, 11, c('5a5560')); t.r(9, 5, 12, 5, c('c0c8d0'))
        elif kind == 'electronics_bench': t.r(3, 6, 12, 10, c('1a5a3a')); [t.p(x, y, GOLD) for x in (4, 6, 8, 10) for y in (7, 9)]; t.r(6, 8, 9, 8, c('30c070'))
        elif kind == 'propellant_plant': t.r(3, 5, 6, 12, c('d8e0f0')); t.r(8, 5, 11, 12, c('f0a030')); t.r(6, 7, 8, 7, DARK); t.r(4, 4, 5, 4, DARK)
        elif kind == 'assembly_bay': t.r(3, 4, 12, 5, AMBER); [t.p(x, 4, DARK) for x in (4, 6, 8, 10)]; t.r(6, 6, 9, 12, c('e8ecf0')); t.r(7, 5, 8, 5, RED); t.r(5, 12, 10, 12, DARK)
        elif kind == 'suit_workshop': t.disc(7.5, 6.5, 2.5, c('e8ecf0')); t.r(6, 6, 9, 7, CYAN); t.r(5, 9, 10, 12, c('e8ecf0')); t.p(7, 10, AMBER)
        elif kind == 'galley': t.r(3, 6, 12, 8, DARK); t.r(4, 5, 6, 5, c('c0c8d0')); t.r(8, 9, 11, 12, c('c0c8d0')); t.r(9, 10, 10, 11, AMBER)
        elif kind == 'sample_lab': t.r(3, 9, 12, 9, c('c0c8d0')); t.r(4, 5, 5, 8, c('60d0f0')); t.r(7, 4, 8, 8, c('a0f080')); t.r(10, 6, 11, 8, c('f08060')); t.r(9, 10, 12, 12, DARK)
        elif kind == 'test_stand': t.r(6, 4, 9, 9, c('c0c8d0')); t.r(5, 9, 10, 10, DARK); t.r(6, 11, 9, 12, AMBER); t.r(7, 13, 8, 13, c('ffe890'))
    return f
# kind, label, how many grades, the frame colour, whether it wants power
BENCHES = [('machine_shop', 'Machine shop', 4, '6a7080'), ('electronics_bench', 'Electronics bench', 4, '4a5a6a'), ('propellant_plant', 'Propellant plant', 4, '7a6a5a'), ('assembly_bay', 'Assembly bay', 4, '5a6a7a'),
           ('suit_workshop', 'Suit workshop', 4, '6a6a80'), ('galley', 'Galley', 1, '808890'), ('sample_lab', 'Sample lab', 4, '708088'), ('test_stand', 'Engine test stand', 1, '5a5a5a')]
UPG = {2: [('lunite_ingot', 6), ('processor', 2)], 3: [('meteoric_iron_ingot', 6), ('flight_computer', 1)], 4: [('martian_steel_ingot', 6), ('quantum_core', 1)]}
for kind, lab, grades, col in BENCHES:
    for g in range(1, grades + 1):
        n = kind if g == 1 else '%s_%d' % (kind, g)
        machine(c(col), bench_face(kind), GRADE[g - 1]).save('tiles', n); icon_of(n)
        tile(n, lab, False, mine_time=0.4, bench=True, bench_kind=kind, bench_level=g, upgrade={'to': '%s_%d' % (kind, g + 1), 'cost': [{'item': i, 'count': k} for i, k in UPG[g + 1]]} if g < grades else None)
        item(n, lab + (' ' + NUM[g] if grades > 1 else ''), AGE_TIER[g - 1], 'benches', 2.0, places=n)
for n, ins in (('machine_shop', [('steel_ingot', 8), ('electric_motor', 1)]), ('electronics_bench', [('steel_ingot', 4), ('silicon_wafer', 4), ('copper_wire', 8)]), ('propellant_plant', [('steel_ingot', 8), ('glass', 6), ('copper_boiler', 1)]),
               ('assembly_bay', [('steel_ingot', 12), ('titanium_ingot', 4), ('electric_motor', 2)]), ('suit_workshop', [('steel_ingot', 4), ('linen', 8), ('glass', 2)]), ('galley', [('steel_ingot', 4), ('glass', 2)]),
               ('sample_lab', [('steel_ingot', 4), ('glass', 8), ('silicon_wafer', 2)]), ('test_stand', [('steel_ingot', 10), ('stone', 20)])):
    rec('workbench', n, ins, level=4)
# The telescope: where the charts are drawn that a rocket is steered by.
t = P().line(3, 12, 11, 4, c('c0c8d0')); t.line(4, 13, 12, 5, c('8a8e9a')); t.r(11, 3, 13, 5, DARK); t.p(12, 4, CYAN); t.line(7, 9, 5, 15, c('5a5560')); t.line(8, 9, 10, 15, c('5a5560')); t.save('tiles', 'observatory'); icon_of('observatory')
tile('observatory', 'Observatory', False, mine_time=0.4, bench=True, bench_kind='observatory', bench_level=1); item('observatory', 'Observatory', 6, 'benches', 2.0, places='observatory')
rec('workbench', 'observatory', [('glass', 12), ('gold_ingot', 2), ('steel_ingot', 6)], level=4)

# ---- metals of each place ---------------------------------------------------------------------------------------------------------------
HOSTC = {'stone': STONE[:3], 'mare_basalt': (58, 58, 66), 'highland_rock': (176, 176, 180), 'asteroid_rock': (90, 84, 80), 'mars_rock': (150, 78, 52), 'mars_basalt': (86, 54, 48)}
# name, label, tier, colours, the rock it lies in, hardness, its pick's power, tool speed
METALS = [('aluminium', 'Aluminium', 6, ramp('5a6068', 'c8d0d8', 'ffffff'), 'stone', 0, 0, 0), ('lunite', 'Lunite', 7, ramp('3a4a6a', '9ab0e0', 'f0f8ff'), 'mare_basalt', 7, 8, 10.0),
          ('tranquillium', 'Tranquillium', 8, ramp('2a5a5a', '60d0c0', 'e0fff8'), 'highland_rock', 8, 9, 12.0), ('meteoric_iron', 'Meteoric iron', 9, ramp('2a2a30', '8a8e9a', 'e8ecf0'), 'asteroid_rock', 9, 10, 14.0),
          ('iridium', 'Iridium', 9, ramp('4a4a5a', 'd0d0e8', 'ffffff'), 'asteroid_rock', 9, 0, 0), ('martian_steel', 'Martian steel', 10, ramp('5a1a10', 'd06040', 'ffd0b0'), 'mars_rock', 10, 11, 16.0),
          ('tharsium', 'Tharsium', 11, ramp('3a1a4a', 'c070e0', 'ffe0ff'), 'mars_basalt', 11, 12, 19.0), ('uranium', 'Uranium', 10, ramp('2a4a1a', '80e040', 'e0ffb0'), 'mars_basalt', 10, 0, 0)]
MR = {}
for n, lab, tier, r, host, hard, power, speed in METALS:
    MR[n] = r; hc = HOSTC[host]; hostr = lambda t, hc=hc: tuple(min(255, int(v * (0.7 + 0.5 * t))) for v in hc)
    clone_tile('copper_ore', n + '_ore', lab + ' ore', r, 'ore', hostr, hardness=hard or None, mine_time=1.2 + (tier - 5) * 0.15)
    clone_item('copper_ore', n + '_ore', lab + ' ore', r, tier, 'ores', 0.15, 'ore', hostr)
    clone_item('iron_ingot', n + '_ingot', lab + ' ingot', r, tier, 'ores', 0.5)
    clone_tile('iron_block', n + '_block', lab + ' block', r); clone_item('iron_block', n + '_block', lab + ' block', r, tier, 'blocks', 4.5, places=n + '_block')
    col = c('%02x%02x%02x' % r(0.6))
    comp(trinket('plate', col), n + '_plate', lab + ' plate', tier, 0.6)
    if not power: continue
    for tool, base, cls, k in (('pickaxe', 'iron_pickaxe', 'pickaxe', 2.0), ('axe', 'iron_axe', 'axe', 1.6), ('shovel', 'gold_shovel', 'shovel', 1.2), ('machete', 'iron_machete', 'machete', 1.2)):
        clone_item(base, n + '_' + tool, lab + ' ' + tool, r, tier, 'tools', k, 'gray', tool={'class': cls, 'speed': speed}, power=power if tool == 'pickaxe' else None)
    clone_item('iron_sword', n + '_blade', lab + ' blade', r, tier, 'tools', 1.6, 'gray', damage=int(speed * 2.4))

# ---- electronics, frames, seals: what everything else is made of, each made with the last ---------------------------------------------------
# name, label, tier, colour
ELEC = [('circuit_board', 'Circuit board', 6, '1a6a3a'), ('processor', 'Processor', 7, '2a2a3a'), ('sensor_array', 'Sensor array', 8, '3a4a8a'), ('flight_computer', 'Flight computer', 9, '5a3a6a'), ('quantum_core', 'Quantum core', 10, '1a3a4a')]
for n, lab, tier, col in ELEC: comp(chip(c(col), GOLD, CYAN if tier > 7 else None), n, lab, tier, 1.0)
comp(trinket('thread', c('c87830')), 'wiring_loom', 'Wiring loom', 6, 0.3); comp(trinket('ring', c('2a2a30'), c('5a5a66')), 'pressure_seal', 'Pressure seal', 6, 0.3)
comp(P().r(4, 3, 11, 12, c('d0d8e0')).r(5, 4, 10, 7, CYAN).r(5, 9, 6, 10, RED).r(8, 9, 10, 10, c('30c070')).outline(), 'life_support_unit', 'Life-support unit', 6, 1.5)
comp(trinket('plate', c('2a2a30')).r(4, 5, 11, 10, c('1a3a8a')).line(4, 7, 11, 7, c('60a0f0')).outline(), 'solar_cell', 'Solar cell', 6, 0.6)
comp(P().disc(7.5, 7.5, 5, c('e8ecf0')).disc(7.5, 7.5, 3.4, c('c0c8d0')).r(7, 1, 8, 5, DARK).outline(), 'heat_tile', 'Heat tile', 6, 0.4)
comp(trinket('cloth', c('f0d060')), 'insulation_foil', 'Insulation foil', 6, 0.3); comp(trinket('cloth', c('e8ecf0')), 'pressure_fabric', 'Pressure fabric', 6, 0.5)
comp(P().r(3, 6, 12, 9, c('8a8e9a')).disc(4, 7.5, 2, c('5a5560')).disc(11, 7.5, 2, c('5a5560')).r(7, 4, 8, 11, AMBER).outline(), 'turbopump', 'Turbopump', 6, 1.2)
comp(P().r(5, 2, 10, 5, c('8a8e9a')).line(5, 6, 3, 13, c('5a5560')).line(10, 6, 12, 13, c('5a5560')).r(3, 13, 12, 13, DARK).r(6, 6, 9, 10, c('3a3a44')).outline(), 'engine_bell', 'Engine bell', 6, 1.0)
comp(P().r(6, 1, 9, 14, c('c0c8d0')).r(6, 1, 6, 14, WHITE).r(4, 6, 11, 7, DARK).outline(), 'actuator', 'Actuator', 6, 0.8)
FRAMES = [('aluminium_frame', 'Aluminium frame', 6, 'aluminium'), ('lunite_frame', 'Lunite frame', 7, 'lunite'), ('tranquillium_frame', 'Tranquillium frame', 8, 'tranquillium'), ('meteoric_frame', 'Meteoric frame', 9, 'meteoric_iron'),
          ('martian_frame', 'Martian frame', 10, 'martian_steel'), ('tharsium_frame', 'Tharsium frame', 11, 'tharsium')]
for n, lab, tier, m in FRAMES: comp(frame_pic(c('%02x%02x%02x' % MR[m](0.6))), n, lab, tier, 1.0)
# Smelting and the first parts.
make('furnace', 'aluminium_ingot', [('aluminium_ore', 2), ('coal', 1)], 3)
make('machine_shop', 'wiring_loom', [('copper_wire', 4), ('linen', 1)], 1, 2, 80); make('machine_shop', 'pressure_seal', [('raw_resin', 2), ('carbon_black', 1)], 1, 2, 80)
make('machine_shop', 'insulation_foil', [('tin_foil', 2), ('gold_ingot', 1)], 1, 4, 80); make('suit_workshop', 'pressure_fabric', [('linen', 4), ('raw_resin', 2), ('carbon_fibers', 1)], 1, 2, 120)
make('machine_shop', 'aluminium_plate', [('aluminium_ingot', 1)], 1, 1, 60); make('machine_shop', 'aluminium_frame', [('aluminium_plate', 2), ('steel_ingot', 1)], 1, 1, 120)
make('electronics_bench', 'circuit_board', [('silicon_wafer', 1), ('copper_wire', 2), ('gold_ingot', 1)], 1, 2, 160); make('electronics_bench', 'solar_cell', [('silicon_wafer', 2), ('glass', 1), ('wiring_loom', 1)], 1, 2, 160)
make('machine_shop', 'heat_tile', [('clay', 4), ('carbon_fiber_sheet', 1)], 1, 4, 120); make('machine_shop', 'turbopump', [('electric_motor', 1), ('aluminium_plate', 2), ('pressure_seal', 2)], 1, 1, 200)
make('machine_shop', 'engine_bell', [('steel_ingot', 3), ('heat_tile', 2)], 1, 1, 200); make('machine_shop', 'actuator', [('electric_motor', 1), ('aluminium_frame', 1), ('circuit_board', 1)], 1, 1, 200)
make('assembly_bay', 'life_support_unit', [('circuit_board', 2), ('pressure_seal', 4), ('turbopump', 1), ('aluminium_frame', 1)], 1, 1, 300)
# Each tier of electronics has the last in it, and something of the place it belongs to.
make('electronics_bench', 'processor', [('circuit_board', 2), ('lunite_plate', 1), ('moon_crystal', 1)], 2, 1, 240, 'lunar_survey')
make('electronics_bench', 'sensor_array', [('processor', 2), ('tranquillium_plate', 1), ('helium3_cell', 1)], 2, 1, 240, 'lunar_survey')
make('electronics_bench', 'flight_computer', [('sensor_array', 2), ('iridium_plate', 1), ('star_sapphire', 1)], 3, 1, 300, 'belt_assay')
make('electronics_bench', 'quantum_core', [('flight_computer', 2), ('tharsium_plate', 1), ('mars_opal', 1)], 4, 1, 400, 'mars_survey')
for (n, lab, tier, m), prev, g, need in zip(FRAMES[1:], FRAMES, (2, 2, 3, 4, 4), ('lunar_survey', 'lunar_survey', 'belt_assay', 'mars_survey', 'mars_survey')):
    make('machine_shop', n, [(m + '_plate', 2), (prev[0], 1), ('pressure_seal', 1)], g, 1, 200, need)
for n, lab, tier, r, host, hard, power, speed in METALS[1:]:
    g = 2 if tier <= 8 else 3 if tier == 9 else 4
    make('machine_shop', n + '_ingot', [(n + '_ore', 2), ('coal', 1)], g, 1, 120); make('machine_shop', n + '_plate', [(n + '_ingot', 1)], g, 1, 60)
    make('machine_shop', n + '_block', [(n + '_ingot', 9)], g); make('machine_shop', n + '_ingot', [(n + '_block', 1)], g, 9, 60)
    if not power: continue
    fr = next(f[0] for f in FRAMES if f[3] == n)
    for tool, k in (('pickaxe', 3), ('axe', 3), ('shovel', 2), ('machete', 2), ('blade', 4)): make('machine_shop', n + '_' + tool, [(n + '_plate', k), (fr, 1), ('actuator' if tool in ('pickaxe', 'blade') else 'wiring_loom', 1)], g, 1, 240)

# ---- propellants and air ------------------------------------------------------------------------------------------------------------------
PROP = [('kerosene_can', 'Kerosene', 6, '8a6a3a', 'f0a030'), ('lox_can', 'Liquid oxygen', 6, 'c0e0f0', '3070d0'), ('hydrogen_can', 'Liquid hydrogen', 8, 'e8ecf0', 'e05030'), ('methane_can', 'Liquid methane', 10, 'd0c0e0', '8040c0')]
for n, lab, tier, col, band in PROP: comp(can(c(col), c(band)), n, lab, tier, 0.4, max_stack=20)
comp(can(c('e8ecf0'), c('30a0e0'), c('30a0e0')), 'oxygen_canister', 'Oxygen canister', 6, 0.5, max_stack=20, food={'hunger': 0.01, 'oxygen': 1.0})
comp(can(c('3a3a44'), c('80e040'), GOLD), 'fuel_rod', 'Fission fuel rod', 10, 2.0, max_stack=10)
comp(can(c('f0e8c0'), c('60d0f0')), 'helium3_cell', 'Helium-3 cell', 8, 1.5, max_stack=20)
make('propellant_plant', 'kerosene_can', [('oil', 4), ('tin_foil', 1)], 1, 2, 120); make('propellant_plant', 'lox_can', [('ice', 2), ('tin_foil', 1)], 1, 2, 120)
make('propellant_plant', 'oxygen_canister', [('lox_can', 1), ('pressure_seal', 1)], 1, 2, 80); make('propellant_plant', 'oxygen_canister', [('moon_ice', 2), ('tin_foil', 1)], 2, 3, 80)
make('propellant_plant', 'hydrogen_can', [('moon_ice', 3), ('insulation_foil', 1)], 2, 2, 160, 'lunar_survey'); make('propellant_plant', 'lox_can', [('moon_ice', 2), ('tin_foil', 1)], 2, 3, 120)
make('propellant_plant', 'helium3_cell', [('helium3_dust', 6), ('insulation_foil', 1)], 2, 1, 200, 'lunar_survey')
make('propellant_plant', 'methane_can', [('mars_ice', 2), ('perchlorate', 1), ('insulation_foil', 1)], 4, 2, 160, 'mars_survey'); make('propellant_plant', 'fuel_rod', [('uranium_ingot', 3), ('martian_steel_plate', 1)], 4, 1, 400, 'mars_survey')
make('propellant_plant', 'lox_can', [('water_ice', 2), ('tin_foil', 1)], 3, 3, 120); make('propellant_plant', 'oxygen_canister', [('mars_ice', 2), ('tin_foil', 1)], 4, 4, 80)

# ---- rockets ----------------------------------------------------------------------------------------------------------------------------------
def rocket_tile(pic, name, label, tier, ins, g, need=None, mult=1.5, **rocket):
    pic.save('tiles', name); icon_of(name); tile(name, label, True, mine_time=0.6, rocket=rocket); item(name, label, tier, 'crafted', mult, places=name); make('assembly_bay', name, ins, g, 1, 300, need)
def engine_pic(col, flame):
    t = P(); t.r(4, 0, 11, 6, col); t.r(4, 0, 4, 6, lt(col)); t.r(5, 7, 10, 8, DARK); t.line(5, 9, 3, 15, c('5a5560')); t.line(10, 9, 12, 15, c('5a5560')); t.r(5, 9, 10, 14, c('3a3a44')); t.r(6, 12, 9, 14, flame); return t
def tank_pic(col, band):
    t = P(col); t.r(0, 0, 1, 15, lt(col, 1.15)); t.r(14, 0, 15, 15, dk(col, 0.75)); t.r(0, 5, 15, 6, band); t.r(0, 0, 15, 0, dk(col, 0.8)); t.r(0, 15, 15, 15, dk(col, 0.8)); t.p(4, 9, DARK).p(4, 10, DARK); return t
# name, label, tier, colour, flame, thrust, how sure, what it burns, grade of bay, what it is made of besides
ENGINES = [('solid_booster', 'Solid booster', 6, 'e8ecf0', 'f0a030', 1, 0.86, None, 1, [('aluminium_frame', 1), ('engine_bell', 1), ('coal', 8), ('salt', 4)], None),
           ('kerosene_engine', 'Kerosene engine', 6, 'c0c8d0', 'ffd060', 2, 0.90, 'kerosene_can', 1, [('aluminium_frame', 1), ('engine_bell', 1), ('turbopump', 1), ('circuit_board', 1)], None),
           ('hydrogen_engine', 'Hydrogen engine', 8, 'd8e0f0', '60d0f0', 3, 0.93, 'hydrogen_can', 2, [('tranquillium_frame', 1), ('engine_bell', 2), ('turbopump', 2), ('sensor_array', 1)], 'lunar_survey'),
           ('methane_engine', 'Methane engine', 10, 'e0b0a0', 'c080ff', 5, 0.95, 'methane_can', 4, [('martian_frame', 1), ('engine_bell', 2), ('turbopump', 2), ('quantum_core', 1)], 'mars_survey'),
           ('nuclear_engine', 'Nuclear thermal engine', 11, '6a8a5a', 'a0ff60', 8, 0.97, 'fuel_rod', 4, [('tharsium_frame', 2), ('engine_bell', 3), ('fuel_rod', 2), ('quantum_core', 2)], 'mars_survey')]
for n, lab, tier, col, flame, thrust, sure, burns, g, ins, need in ENGINES:
    rocket_tile(engine_pic(c(col), c(flame)), n, lab, tier, ins + ([(burns, 2)] if burns else []), g, need, fuel=10 if not burns else 0, thrust=thrust, reliability=sure)
    # (Run on the stand and found good, it is that much surer.)
    engine_pic(c(col), c(flame)).r(4, 1, 11, 2, c('30c070')).save('tiles', 'proven_' + n); icon_of('proven_' + n)
    tile('proven_' + n, 'Proven ' + lab.lower(), True, mine_time=0.6, rocket={'fuel': 10 if not burns else 0, 'thrust': thrust, 'reliability': min(0.995, sure + 0.06)})
    item('proven_' + n, 'Proven ' + lab.lower(), tier, 'crafted', 2.0, places='proven_' + n); rec('test_stand', 'proven_' + n, [(n, 1)] + ([(burns, 2)] if burns else [('coal', 4)]), time=600)
TANKS = [('kerosene_tank', 'Kerosene tank', 6, 'e8ecf0', 'f0a030', 30, 1, [('aluminium_plate', 3), ('kerosene_can', 4), ('lox_can', 4)], None), ('hydrogen_tank', 'Hydrogen tank', 8, 'f0e0c0', 'e05030', 60, 2, [('tranquillium_plate', 3), ('insulation_foil', 2), ('hydrogen_can', 4), ('lox_can', 4)], 'lunar_survey'),
         ('methane_tank', 'Methane tank', 10, 'e0c0d0', '8040c0', 110, 4, [('martian_steel_plate', 3), ('insulation_foil', 2), ('methane_can', 4), ('lox_can', 4)], 'mars_survey'),
         ('reactor_tank', 'Reaction-mass tank', 11, 'c0e0b0', '40a020', 200, 4, [('tharsium_plate', 3), ('hydrogen_can', 6), ('fuel_rod', 1)], 'mars_survey')]
for n, lab, tier, col, band, fuel, g, ins, need in TANKS: rocket_tile(tank_pic(c(col), c(band)), n, lab, tier, ins, g, need, fuel=fuel, thrust=0)
rocket_tile(panel(c('e8ecf0'), True, c('2a2e3a')), 'interstage', 'Interstage', 6, [('aluminium_frame', 1), ('actuator', 1)], 1, None, 0.8, fuel=2, thrust=0)
t = P(); t.r(7, 0, 8, 9, RED); t.r(6, 2, 9, 4, c('e8ecf0')); t.line(4, 15, 7, 9, c('8a8e9a')); t.line(11, 15, 8, 9, c('8a8e9a')); t.r(3, 14, 12, 15, c('8a8e9a'))
rocket_tile(t, 'escape_tower', 'Launch escape tower', 6, [('aluminium_frame', 1), ('solid_booster', 1), ('circuit_board', 1)], 1, None, 1.0, fuel=0, thrust=0, escape=True)
t = P(c('5a5a5a')); t.r(0, 0, 15, 3, c('8a8a8a')); [t.r(x, 0, x + 1, 3, AMBER if (x // 2) % 2 else DARK) for x in range(0, 16, 2)]; t.r(6, 6, 9, 15, DARK)
t.save('tiles', 'launch_pad'); icon_of('launch_pad'); tile('launch_pad', 'Launch pad', True, mine_time=1.0, pad=True); item('launch_pad', 'Launch pad', 6, 'crafted', 2.0, places='launch_pad')
make('assembly_bay', 'launch_pad', [('steel_ingot', 12), ('stone', 20), ('heat_tile', 4)], 1, 1, 300)
t = P(); t.r(1, 0, 2, 15, c('c03020')); t.r(13, 0, 14, 15, c('c03020')); [t.r(1, y, 14, y, c('e04030')) for y in (3, 9, 15)]; t.line(2, 4, 13, 8, c('8a2010')); t.line(13, 10, 2, 14, c('8a2010'))
t.save('tiles', 'gantry'); icon_of('gantry'); tile('gantry', 'Gantry', False, mine_time=0.3, climbable=True); item('gantry', 'Gantry', 6, 'crafted', 0.3, places='gantry'); make('machine_shop', 'gantry', [('steel_ingot', 2)], 1, 4, 80)
# The capsules: each a way to one place, launched from on top of a rocket that can get there - and the way home from it.
def capsule_pic(col, win, legs=False):
    t = P(); t.r(5, 2, 10, 3, col); t.r(4, 4, 11, 12, col); t.r(3, 7, 12, 12, col); t.r(6, 5, 9, 7, win); t.p(6, 5, WHITE); t.r(3, 12, 12, 13, c('3a3a44')); t.r(4, 4, 4, 12, lt(col))
    if legs: t.line(3, 13, 1, 15, c('8a8e9a')); t.line(12, 13, 14, 15, c('8a8e9a'))
    else: t.r(5, 14, 10, 15, c('5a3a2a'))
    return t
# name, label, tier, colour, where it goes, fuel and thrust it wants, grade of bay, the chart it is steered by, what it is made of
CAPSULES = [('orbital_capsule', 'Orbital capsule', 6, 'd0d8e0', 'orbit', 60, 2, 1, 'orbital_chart', [('aluminium_frame', 3), ('heat_tile', 6), ('life_support_unit', 1), ('circuit_board', 2), ('pressure_seal', 4)]),
            ('lunar_lander', 'Lunar lander', 7, 'f0d060', 'moon', 150, 4, 1, 'lunar_chart', [('orbital_capsule', 1), ('aluminium_frame', 4), ('actuator', 4), ('insulation_foil', 6), ('kerosene_engine', 1)]),
            ('belt_hauler', 'Belt hauler', 9, 'c0c8d0', 'belt', 320, 8, 2, 'belt_chart', [('lunar_lander', 1), ('tranquillium_frame', 4), ('sensor_array', 2), ('hydrogen_engine', 1), ('life_support_unit', 2)]),
            ('mars_lander', 'Mars lander', 10, 'e07050', 'mars', 600, 14, 3, 'mars_chart', [('belt_hauler', 1), ('meteoric_frame', 6), ('flight_computer', 2), ('heat_tile', 12), ('hydrogen_engine', 2)])]
for n, lab, tier, col, dest, fuel, thrust, g, chart, ins in CAPSULES:
    capsule_pic(c(col), CYAN, n != 'orbital_capsule').save('tiles', n); icon_of(n)
    tile(n, lab, False, mine_time=0.6, leads_to=dest, launch={'fuel': fuel, 'thrust': thrust}, light=6, model='panel'); item(n, lab, tier, 'portals', 6.0, places=n); make('assembly_bay', n, ins, g, 1, 900, chart)
# Charts, from the observatory; and what is learned from samples, at the lab: each is what a later making must have been found for.
for n, lab, col, ins in (('orbital_chart', 'Orbital chart', '3070d0', [('glass', 2), ('linen', 2), ('circuit_board', 1)]), ('lunar_chart', 'Lunar chart', 'c0c8d0', [('orbital_chart', 1), ('orbit_sample', 3)]),
                         ('belt_chart', 'Belt chart', '8a8478', [('lunar_chart', 1), ('lunar_survey', 1), ('sensor_array', 1)]), ('mars_chart', 'Mars chart', 'd06040', [('belt_chart', 1), ('belt_assay', 1), ('flight_computer', 1)])):
    comp(card(c('efe2b8'), c(col)), n, lab, 6, 2.0, 'special', max_stack=1); rec('observatory', n, ins, time=400)
SAMPLES = [('orbit_sample', 'Upper-air sample', 6, '80c0f0'), ('moon_sample', 'Lunar sample', 7, 'b0b0b8'), ('belt_sample', 'Asteroid sample', 9, '6a6058'), ('mars_sample', 'Martian sample', 10, 'c05030'), ('life_sample', 'Living sample', 10, '60c060')]
for n, lab, tier, col in SAMPLES: comp(sample(c(col)), n, lab, tier, 0.6, 'nature')
for n, lab, col, ins, g in (('lunar_survey', 'Lunar survey', 'c0c8d0', [('moon_sample', 6), ('moon_ice', 2), ('circuit_board', 1)], 1), ('belt_assay', 'Belt assay', '8a8478', [('belt_sample', 6), ('water_ice', 2), ('processor', 1)], 2),
                            ('mars_survey', 'Mars survey', 'd06040', [('mars_sample', 6), ('mars_ice', 2), ('flight_computer', 1)], 3), ('xenobiology', 'Xenobiology notes', '60c060', [('life_sample', 8), ('mars_survey', 1)], 4)):
    comp(card(c('d8e8f0'), c(col)), n, lab, 7, 3.0, 'special', max_stack=1); rec('sample_lab', n, ins, level=g, time=600)
# A sounding rocket brings back the first sample: made, set off, and what comes down is the sample.
comp(P().r(7, 1, 8, 12, c('e8ecf0')).p(7, 0, RED).r(6, 11, 9, 13, RED).r(7, 14, 8, 15, AMBER).outline(), 'sounding_rocket', 'Sounding rocket', 6, 1.0)
make('assembly_bay', 'sounding_rocket', [('aluminium_plate', 2), ('solid_booster', 1), ('circuit_board', 1)], 1, 1, 200); rec('observatory', 'orbit_sample', [('sounding_rocket', 1)], 2, time=300)

# ---- the rock and ground of each place ----------------------------------------------------------------------------------------------------
# name, label, how it is drawn, colour, tier, what else
BLOCKS = [('debris_plating', 'Debris plating', 'paving', '6a7080', 6, {}),
          ('mare_basalt', 'Mare basalt', 'rock', '3a3a42', 7, {'hardness': 6, 'mine_time': 1.1}), ('highland_rock', 'Highland anorthosite', 'rock', 'b0b0b4', 7, {'hardness': 6, 'mine_time': 1.1}),
          ('regolith', 'Regolith', 'rock', '9a9aa0', 7, {'tool': 'shovel', 'mine_time': 0.4}), ('dark_regolith', 'Dark regolith', 'rock', '5a5a62', 7, {'tool': 'shovel', 'mine_time': 0.4}),
          ('impact_glass', 'Impact glass', 'rock', '2a4a3a', 7, {'light': 3, 'hardness': 6}), ('regolith_brick', 'Regolith brick', 'brick', 'a0a0a8', 7, {}), ('sintered_block', 'Sintered regolith', 'paving', '8a8a92', 7, {}),
          ('asteroid_rock', 'Asteroid rock', 'rock', '5a5450', 9, {'hardness': 8, 'mine_time': 1.3}), ('carbon_rock', 'Carbonaceous rock', 'rock', '2a2826', 9, {'hardness': 8, 'mine_time': 1.2}),
          ('nickel_rock', 'Nickel-iron rock', 'rock', '8a8680', 9, {'hardness': 9, 'mine_time': 1.5}), ('comet_ice', 'Comet ice', 'rock', 'b0d8e8', 9, {'hardness': 8, 'light': 2}),
          ('mars_rock', 'Martian rock', 'rock', '96503a', 10, {'hardness': 9, 'mine_time': 1.3}), ('mars_basalt', 'Martian basalt', 'rock', '563630', 10, {'hardness': 10, 'mine_time': 1.5}),
          ('rust_sand', 'Rust sand', 'rock', 'c8703a', 10, {'tool': 'shovel', 'mine_time': 0.4}), ('rust_soil', 'Rust soil', 'rock', 'a05a34', 10, {'tool': 'shovel', 'mine_time': 0.4}),
          ('mars_salt_crust', 'Salt crust', 'rock', 'e0d8d0', 10, {'tool': 'shovel', 'mine_time': 0.5}), ('polar_frost', 'Polar frost', 'rock', 'e8f0f8', 10, {'tool': 'shovel', 'mine_time': 0.4}),
          ('mars_brick', 'Martian brick', 'brick', 'b06040', 10, {}), ('mars_concrete', 'Martian concrete', 'paving', 'a87868', 10, {}), ('volcanic_glass', 'Volcanic glass', 'rock', '1a1418', 10, {'hardness': 10}),
          ('ancient_alloy', 'Ancient alloy', 'paving', '4a6a6a', 9, {'hardness': 9, 'mine_time': 1.6}), ('glyph_stone', 'Glyph stone', 'brick', '3a4a4a', 9, {'hardness': 9, 'light': 4}),
          ('cave_moss_ground', 'Mossy rock', 'rock', '4a6a3a', 10, {'mine_time': 0.8}), ('garden_rock', 'Garden rock', 'rock', '3a5a5a', 9, {'mine_time': 1.0})]
COL = {}
for n, lab, kind, col, tier, kw in BLOCKS: COL[n] = c(col); block(block_tex(kind, n, c(col)), n, lab, tier, 0.03, **kw)
rock('void_edge', c('06060c'), 10).save('tiles', 'void_edge'); tile('void_edge', 'The edge of the void', removable=False, drop=None)
# Ice and what is in the dust.
for n, lab, col, tier in (('moon_ice', 'Lunar ice', 'c0e0f0', 7), ('water_ice', 'Water ice', 'a0d0f0', 9), ('mars_ice', 'Martian ice', 'e0f0f8', 10)):
    rock(n + '_block', c(col), 30).save('tiles', n + '_block'); tile(n + '_block', lab, mine_time=0.8, drop=None, drops=[{'item': n, 'count': 2}], light=2); comp(gemstone(c(col)), n, lab, tier, 0.2, 'ores')
for n, lab, col, tier, host in (('helium3_dust', 'Helium-3 dust', 'f0e8a0', 8, 'regolith'), ('perchlorate', 'Perchlorate', 'e0e0c0', 10, 'rust_soil'), ('sulphur_bloom', 'Sulphur', 'e0d040', 10, 'mars_basalt')):
    rock(n + '_ore', COL[host], vein=c(col)).save('tiles', n + '_ore'); tile(n + '_ore', lab + ' deposit', ore=True, mine_time=0.9, drop=n, tool='shovel' if host != 'mars_basalt' else 'pickaxe')
    comp(trinket('dust', c(col)), n, lab, tier, 0.3, 'ores')
for n, lab, r, tier in (('moon_crystal', 'Moon crystal', ramp('3a5a8a', 'a0d0ff', 'ffffff'), 7), ('star_sapphire', 'Star sapphire', ramp('1a1a6a', '5070f0', 'e0e8ff'), 9), ('mars_opal', 'Mars opal', ramp('6a2a1a', 'f08050', 'fff0c0'), 10)):
    clone_tile('diamond', n, lab, r, hardness=tier - 1, light=9); clone_item('diamond', n, lab, r, tier, 'ores', 1.0)
    crystal(c('%02x%02x%02x' % r(0.6))).save('tiles', n + '_spire'); tile(n + '_spire', lab + ' spire', True, mine_time=1.2, light=10, pulse=0.1, drop=n, hardness=tier - 1)

# ---- a base: hull, air, light, power ------------------------------------------------------------------------------------------------------------
block(panel(HULL), 'hull_panel', 'Hull panel', 6, 0.3); make('machine_shop', 'hull_panel', [('aluminium_plate', 1), ('insulation_foil', 1)], 1, 4, 80)
block(panel(c('8a94a4'), False, c('f0a030')), 'hull_floor', 'Deck plating', 6, 0.3); make('machine_shop', 'hull_floor', [('aluminium_plate', 1), ('steel_ingot', 1)], 1, 4, 80)
block(panel(HULL, True, None, c('20304a')), 'hull_window', 'Viewport', 6, 0.5, see_through=True, light=None); make('machine_shop', 'hull_window', [('aluminium_frame', 1), ('glass', 4), ('pressure_seal', 2)], 1, 4, 120)
block(panel(c('e0e4ea'), False), 'hab_wall', 'Habitat wall', 7, 0.3); make('machine_shop', 'hab_wall', [('lunite_plate', 1), ('insulation_foil', 1)], 2, 6, 80, 'lunar_survey')
block(panel(c('c8703a'), False, c('a05a34'), c('f0d0b0')), 'dome_glass', 'Dome glass', 10, 0.5, see_through=True); make('machine_shop', 'dome_glass', [('martian_steel_plate', 1), ('glass', 4), ('pressure_seal', 2)], 4, 6, 120, 'mars_survey')
make('machine_shop', 'regolith_brick', [('regolith', 4)], 2, 4, 60); make('machine_shop', 'sintered_block', [('regolith', 2), ('dark_regolith', 2)], 2, 4, 60); make('machine_shop', 'mars_brick', [('rust_soil', 4)], 4, 4, 60)
make('machine_shop', 'mars_concrete', [('rust_sand', 2), ('mars_rock', 2), ('sulphur_bloom', 1)], 4, 8, 60)
clone_tile('iron_door', 'airlock_door', 'Airlock door', ramp('3a3e4a', 'c0c8d4', 'ffffff'), 'tint', drop='airlock_door'); clone_item('iron_door', 'airlock_door', 'Airlock door', ramp('3a3e4a', 'c0c8d4', 'ffffff'), 6, 'crafted', 1.0, 'tint', places='airlock_door')
make('machine_shop', 'airlock_door', [('aluminium_frame', 2), ('pressure_seal', 4), ('actuator', 1)], 1, 1, 200)
t = P(); t.r(0, 0, 1, 15, c('8a8e9a')); t.r(14, 0, 15, 15, c('8a8e9a')); t.line(1, 0, 14, 7, c('6a6e7a')); t.line(14, 8, 1, 15, c('6a6e7a')); t.r(0, 7, 15, 8, c('8a8e9a'))
t.save('tiles', 'truss'); icon_of('truss'); tile('truss', 'Truss', False, mine_time=0.3, climbable=True); item('truss', 'Truss', 6, 'crafted', 0.2, places='truss'); make('machine_shop', 'truss', [('aluminium_plate', 1)], 1, 4, 60)
FUELS_AIR = [{'item': 'oxygen_canister', 'ticks': 6000}, {'item': 'lox_can', 'ticks': 12000}]
t = machine(c('c0c8d0'), lambda t: (t.r(4, 5, 11, 11, DARK), [t.r(4, y, 11, y, c('60d0f0')) for y in (6, 8, 10)]), c('30c070'), False)
t.save('tiles', 'air_vent'); icon_of('air_vent'); tile('air_vent', 'Air vent', False, mine_time=0.4, bench=True, bench_kind='air_vent', bench_level=1, fuel_capacity=120000, fuels=FUELS_AIR, air=True, light=5, light_needs_fuel=True)
item('air_vent', 'Air vent', 6, 'benches', 2.0, places='air_vent'); make('assembly_bay', 'air_vent', [('life_support_unit', 1), ('aluminium_frame', 1), ('turbopump', 1)], 1, 1, 300)
t = machine(c('8a9aa8'), lambda t: (t.r(4, 5, 7, 11, c('30a0e0')), t.r(9, 5, 11, 8, DARK), t.r(9, 10, 11, 11, c('30c070'))), AMBER)
t.save('tiles', 'oxygen_plant'); icon_of('oxygen_plant'); tile('oxygen_plant', 'Oxygen plant', False, mine_time=0.4, bench=True, bench_kind='oxygen_plant', bench_level=1, electric={'per_second': 60})
item('oxygen_plant', 'Oxygen plant', 7, 'benches', 2.0, places='oxygen_plant'); make('assembly_bay', 'oxygen_plant', [('life_support_unit', 2), ('lunite_frame', 2), ('processor', 1)], 2, 1, 400, 'lunar_survey')
for ice, n in (('moon_ice', 4), ('water_ice', 5), ('mars_ice', 6), ('ice', 2)): rec('oxygen_plant', 'oxygen_canister', [(ice, 2)], n, time=60)
# Power: by the sun, by a decaying thing, by splitting atoms; kept in cells; carried by cable.
for n, lab, tier, col, gen, ins, g, need in (('solar_array', 'Solar array', 6, '1a3a8a', {'power': 30, 'needs': 'sun'}, [('solar_cell', 6), ('aluminium_frame', 1), ('wiring_loom', 2)], 1, None),
                                             ('radio_generator', 'Radioisotope generator', 9, '6a8a5a', {'power': 60, 'needs': 'fuel'}, [('meteoric_frame', 1), ('uranium_ingot', 2), ('flight_computer', 1)], 3, 'belt_assay'),
                                             ('fission_reactor', 'Fission reactor', 10, '4a6a4a', {'power': 300, 'needs': 'fuel'}, [('martian_frame', 3), ('quantum_core', 1), ('turbopump', 2), ('heat_tile', 8)], 4, 'mars_survey')):
    t = machine(c('5a6270'), lambda t, col=col: (t.r(3, 5, 12, 11, c(col)), t.line(3, 8, 12, 8, lt(c(col), 1.4)), t.line(7, 5, 7, 11, lt(c(col), 1.4))), c('30c070'))
    t.save('tiles', n); icon_of(n); tile(n, lab, False, mine_time=0.4, bench=True, bench_kind=n, bench_level=1, generator=gen, fuel_capacity=600000 if gen['needs'] == 'fuel' else None,
                                         fuels=[{'item': 'fuel_rod', 'ticks': 200000}, {'item': 'helium3_cell', 'ticks': 60000}] if gen['needs'] == 'fuel' else None)
    item(n, lab, tier, 'energy', 2.0, places=n); make('assembly_bay', n, ins, g, 1, 400, need)
t = machine(c('5a6270'), lambda t: (t.r(4, 5, 11, 11, DARK), t.r(5, 6, 10, 10, c('30c070')), t.r(7, 3, 8, 4, GOLD)), None, False)
t.save('tiles', 'power_cell'); icon_of('power_cell'); tile('power_cell', 'Power cell', False, mine_time=0.4, bench=True, bench_kind='power_cell', bench_level=1, battery=60000); item('power_cell', 'Power cell', 6, 'energy', 1.5, places='power_cell')
make('assembly_bay', 'power_cell', [('aluminium_frame', 1), ('circuit_board', 2), ('salt', 6), ('copper_wire', 4)], 1, 1, 300)
clone_tile('copper_wire', 'power_cable', 'Power cable', ramp('3a2a10', 'f0a030', 'ffe0a0'), 'tint', drop='power_cable'); clone_item('copper_wire', 'power_cable', 'Power cable', ramp('3a2a10', 'f0a030', 'ffe0a0'), 6, 'energy', 0.1, 'tint', places='power_cable')
make('machine_shop', 'power_cable', [('wiring_loom', 1), ('raw_resin', 1)], 1, 8, 60)
# Things to live among.
lamp = P().r(6, 0, 9, 1, c('5a5560')).r(4, 2, 11, 5, c('f0f4ff')).r(5, 6, 10, 6, c('c0d0ff'))
deco(lamp, 'hab_light', 'Habitat light', 6, [('glass', 1), ('wiring_loom', 1)], 'machine_shop', 1, 2, light=14)
deco(P().r(2, 9, 13, 11, c('d0d8e0')).r(2, 8, 5, 8, c('60a0f0')).r(2, 12, 3, 15, c('5a5560')).r(12, 12, 13, 15, c('5a5560')), 'bunk', 'Bunk', 6, [('aluminium_frame', 1), ('pressure_fabric', 2)], 'machine_shop', bed={'speed': 2.5})
deco(machine(c('3a3e4a'), lambda t: (t.r(3, 4, 12, 9, c('102030')), t.line(4, 8, 7, 5, c('30c070')), t.line(7, 5, 11, 7, c('30c070')), t.r(4, 11, 6, 12, AMBER), t.r(8, 11, 11, 12, c('c0c8d0')))), 'console', 'Console', 6, [('aluminium_frame', 1), ('circuit_board', 2), ('glass', 1)], 'electronics_bench', light=7)
deco(chest_pic(c('c0c8d0'), c('f0a030')), 'cargo_locker', 'Cargo locker', 6, [('aluminium_plate', 4), ('pressure_seal', 1)], 'machine_shop', storage={'columns': 4, 'rows': 4}, model='box'); items[-1]['container'] = {'slots': 16}
deco(P().r(3, 9, 12, 10, c('c0c8d0')).r(4, 11, 5, 15, c('5a5560')).r(10, 11, 11, 15, c('5a5560')).r(5, 6, 7, 8, c('e8ecf0')).p(9, 8, RED), 'mess_table', 'Mess table', 6, [('aluminium_plate', 2)], 'machine_shop')
deco(P().r(5, 6, 10, 15, c('5a6270')).r(4, 4, 11, 6, c('c0c8d0')).r(6, 8, 9, 13, c('30a0e0')).p(7, 9, WHITE), 'water_recycler', 'Water recycler', 7, [('lunite_frame', 1), ('turbopump', 1), ('pressure_seal', 4)], 'assembly_bay', 2)
deco(P().r(7, 4, 8, 15, c('8a8e9a')).r(2, 0, 13, 3, c('c0c8d0')).r(3, 1, 12, 2, c('e8ecf0')).p(7, 4, RED), 'comm_dish', 'Comm dish', 6, [('aluminium_frame', 1), ('circuit_board', 1)], 'electronics_bench', light=4)
deco(P().r(7, 2, 8, 15, c('c0c8d0')).r(9, 2, 14, 6, c('e04030')).r(9, 4, 14, 4, WHITE).r(5, 14, 10, 15, c('5a5560')), 'mission_flag', 'Mission flag', 6, [('pressure_fabric', 1), ('aluminium_plate', 1)], 'suit_workshop')
deco(P().r(3, 5, 12, 13, c('5a6270')).r(4, 6, 11, 12, c('60a0f0')).r(5, 7, 10, 11, c('a0e0ff')).r(6, 3, 9, 4, c('8a8e9a')).p(6, 8, WHITE), 'specimen_tank', 'Specimen tank', 9, [('meteoric_frame', 1), ('glass', 4), ('life_sample', 1)], 'sample_lab', 3, light=8)
deco(P().r(6, 8, 9, 15, c('8a8e9a')).disc(7.5, 5, 3.6, c('c0c8d0')).disc(7.5, 5, 2.2, c('2a4a8a')).p(7, 4, c('60c060')), 'globe_stand', 'Planet globe', 7, [('lunite_plate', 2), ('moon_sample', 2)], 'sample_lab', 2)
deco(P().r(2, 12, 13, 15, c('5a5560')).r(4, 4, 11, 11, c('102030')).line(5, 9, 10, 6, AMBER).r(6, 2, 9, 3, c('8a8e9a')), 'seismometer', 'Seismometer', 7, [('lunite_frame', 1), ('sensor_array', 1)], 'electronics_bench', 2)

# ---- suits, and the pack on the back -----------------------------------------------------------------------------------------------------------
# name, label, tier, colour, breath (times a bare head's), armour, warmth, grade, what a piece is made of, what must be known
SUITS = [('flight', 'Flight suit', 6, 'e08030', 6.0, 0.03, 0.4, 1, [('pressure_fabric', 2), ('pressure_seal', 1)], None), ('eva', 'EVA suit', 7, 'e8ecf0', 14.0, 0.06, 0.6, 1, [('pressure_fabric', 3), ('insulation_foil', 2), ('life_support_unit', 1)], None),
         ('lunar', 'Lunar suit', 8, 'd8d8e0', 24.0, 0.10, 0.8, 2, [('pressure_fabric', 3), ('tranquillium_plate', 1), ('sensor_array', 1)], 'lunar_survey'),
         ('belt', 'Belt hardsuit', 9, '8a8e9a', 40.0, 0.18, 0.8, 3, [('meteoric_iron_plate', 2), ('pressure_fabric', 2), ('flight_computer', 1)], 'belt_assay'),
         ('mars', 'Mars suit', 10, 'd06040', 60.0, 0.16, 0.9, 4, [('martian_steel_plate', 2), ('pressure_fabric', 3), ('quantum_core', 1)], 'mars_survey'),
         ('explorer', 'Explorer exosuit', 11, 'c070e0', 120.0, 0.24, 1.0, 4, [('tharsium_plate', 2), ('alien_fibre', 2), ('quantum_core', 1)], 'xenobiology')]
for n, lab, tier, col, breath, armor, warm, g, ins, need in SUITS:
    for slot, kind, plab in (('head', 'helmet', 'helmet'), ('chest', 'chest', 'torso'), ('legs', 'legs', 'legs'), ('boots', 'boots', 'boots')):
        nm = '%s_suit_%s' % (n, plab); suit_piece(kind, c(col)).save('items', nm)
        item(nm, '%s %s' % (lab, plab), tier, 'clothing', 1.0, equip=slot, max_stack=1, armor=armor, warmth=round(warm / 4 + 0.1, 2), cooling=round(warm / 4, 2), set=n + '_suit', boons={'lung': round(breath ** 0.25, 3)})
        make('suit_workshop', nm, ins, g, 1, 300, need)
    sets.append({'name': n + '_suit', 'label': lab, 'bonuses': [{'pieces': 4, 'boons': dict({'lung': 2.0}, **({'fireproof': 1.0} if tier >= 10 else {}), **({'walk': 1.15} if tier >= 9 else {}))}]})
clone_item('jetpack', 'thruster_pack', 'Thruster pack', ramp('3a3e4a', 'c0c8d4', 'ffffff'), 7, 'energy', 2.0, 'tint', lifts=True, pack=True, warmth=0.1); make('assembly_bay', 'thruster_pack', [('lunite_frame', 2), ('hydrogen_can', 4), ('processor', 1), ('actuator', 2)], 2, 1, 400, 'lunar_survey')

# ---- growing things: under glass, and wild ----------------------------------------------------------------------------------------------------
t = P().r(0, 10, 15, 15, c('c0c8d0')); t.r(1, 11, 14, 13, c('3a2a1a')); t.r(0, 10, 15, 10, WHITE); [t.p(x, 12, c('60c060')) for x in (3, 7, 11)]
block(t, 'hydroponic_tray', 'Hydroponic tray', 6, 0.5); make('machine_shop', 'hydroponic_tray', [('aluminium_plate', 2), ('compost', 2), ('pressure_seal', 1)], 1, 2, 120)
TRAY = ['hydroponic_tray', 'dirt', 'compost']
# name, label, tier, shape, colour, how much it feeds
CROPS = [('space_lettuce', 'Space lettuce', 6, 'bush', '70d060', 0.12), ('dwarf_wheat', 'Dwarf wheat', 6, 'grass', 'e0c060', 0.0), ('red_potato', 'Red potato', 6, 'root', 'c05040', 0.2), ('soy_bean', 'Soy bean', 7, 'vine', 'a0c060', 0.12),
         ('tank_tomato', 'Tank tomato', 7, 'flower', 'e04030', 0.15), ('moon_melon', 'Moon melon', 8, 'vine', 'c0e0a0', 0.25), ('spirulina', 'Spirulina', 7, 'grass', '208060', 0.08), ('ration_yeast', 'Ration yeast', 8, 'mushroom', 'e0d0a0', 0.1),
         ('belt_berry', 'Belt berry', 9, 'bush', '6050c0', 0.15), ('mars_maize', 'Mars maize', 10, 'grass', 'f0a040', 0.2)]
def grower(n, lab, tier, arch, col, food, soil, wild_light=None, needs_air=True, cat='nature', **ikw):
    for i in range(4):
        plant(arch, c(col), i).save('tiles', '%s_%d' % (n, i))
        tile('%s_%d' % (n, i), lab, False, mine_time=0.1, tool='machete', drop=None, drops=[{'item': n, 'count': 1 if i < 3 else 3}], soil=soil, grows_into='%s_%d' % (n, i + 1) if i < 3 else None, model='cross', needs_air=needs_air or None, light=wild_light)
    q = plant(arch, c(col), 3).outline(); q.save('items', n); item(n, lab, tier, cat, 0.1, sows=n + '_0', food=food or None, **ikw)
for n, lab, tier, arch, col, food in CROPS: grower(n, lab, tier, arch, col, food, TRAY)
# Wild things of the Belt's gardens and the red planet's caves: they want no air.
ALIEN_SOIL = ['cave_moss_ground', 'garden_rock', 'rust_soil', 'hydroponic_tray']
FLORA = [('glow_frond', 'Glow frond', 9, 'fern', '40e0c0'), ('void_lily', 'Void lily', 9, 'flower', 'c080ff'), ('star_coral', 'Star coral', 9, 'coral', 'f0a0c0'), ('ice_bloom', 'Ice bloom', 9, 'flower', 'a0e0ff'),
         ('rust_cap', 'Rust cap', 10, 'mushroom', 'c05030'), ('cave_lantern', 'Cave lantern', 10, 'flower', 'f0e060'), ('blood_moss', 'Blood moss', 10, 'bush', 'a02030'), ('spire_weed', 'Spire weed', 10, 'grass', '60a040'),
         ('thread_vine', 'Thread vine', 10, 'vine', 'd0d0a0'), ('ember_pod', 'Ember pod', 10, 'bush', 'f07020'), ('pale_tuber', 'Pale tuber', 10, 'root', 'e0d8c0'), ('violet_coral', 'Violet coral', 11, 'coral', '9050d0'),
         ('frost_fern', 'Frost fern', 10, 'fern', 'c0e8f8'), ('sun_lichen', 'Sun lichen', 10, 'bush', 'e0c040')]
for n, lab, tier, arch, col in FLORA: grower(n, lab, tier, arch, col, 0.1 if arch in ('root', 'mushroom', 'bush') else 0, ALIEN_SOIL, 7, False)
comp(trinket('thread', c('d0d0a0')), 'alien_fibre', 'Alien fibre', 10, 0.8); make('suit_workshop', 'alien_fibre', [('thread_vine', 4), ('blood_moss', 1)], 4, 2, 160, 'xenobiology')
# Giant fungi and stone-trees: the red planet's woods.
TREES = [('rust_shroom', 'Rust shroom', 'apple', 'c05030', 'e08050', 'rust_spore', 'Rust spore', 10), ('glass_tree', 'Glass tree', 'pine', '6a8a9a', 'a0e0f0', 'glass_seed', 'Glass seed', 10),
         ('coil_palm', 'Coil palm', 'coconut', '5a3a5a', 'c070e0', 'coil_nut', 'Coil nut', 10), ('garden_bulb', 'Garden bulb', 'cherry', '3a5a5a', '40e0c0', 'bulb_seed', 'Bulb seed', 9)]
near = lambda col: ramp('%02x%02x%02x' % dk(c(col), 0.62)[:3], col, '%02x%02x%02x' % lt(c(col), 1.12)[:3])
block(block_tex('planks', 'xeno_wood', c('7a4a5a')), 'xeno_wood', 'Xeno-wood', 10, 0.03, tool='axe')
for n, lab, shape, trunk, leaf, fruit, flab, tier in TREES:
    clone_tile('oak_log', n + '_log', lab + ' stem', near(trunk), drop='xeno_wood'); clone_tile('oak_leaves', n + '_leaves', lab + ' cap', near(leaf), light=7)
    q = P().disc(7.5, 9, 3.6, c(leaf)); q.p(6, 7, WHITE); q.r(7, 3, 8, 5, c(trunk)); q.outline().save('items', fruit); item(fruit, flab, tier, 'nature', 0.1, plants=n, food=0.08)
    tr = copy.deepcopy(BTR[shape]); tr.update(name=n, log=n + '_log', leaves=n + '_leaves', fruit=fruit, soil=ALIEN_SOIL); tr.pop('needs_water', None); trees.append(tr)
# What is cooked of it all.
def meal(name, label, tier, col, ins, cupped=False, **kw): thing(cup(c(col)) if cupped else bowl(c(col)), name, label, tier, 'nature', 1.0, cooked=True, **kw); rec('galley', name, ins, time=120)
def lasts(ticks, **boons): return {'ticks': ticks, 'boons': boons}
MEALS = [('ration_pack', 'Ration pack', 6, 'c8a060', [('dwarf_wheat', 2), ('red_potato', 1)], {'food': 0.5}), ('lettuce_wrap', 'Lettuce wrap', 6, '70d060', [('space_lettuce', 2), ('dwarf_wheat', 1)], {'food': 0.4}),
         ('potato_mash', 'Potato mash', 6, 'e0c8a0', [('red_potato', 3), ('salt', 1)], {'food': 0.6}), ('soy_curd', 'Soy curd', 7, 'f0e8d0', [('soy_bean', 3)], {'food': 0.5, 'effect': lasts(2400, stamina=1.3)}),
         ('tomato_soup', 'Tomato soup', 7, 'e04030', [('tank_tomato', 3), ('salt', 1)], {'food': 0.6, 'effect': lasts(2400, warmth=0.3)}), ('algae_bar', 'Algae bar', 7, '208060', [('spirulina', 4)], {'food': 0.4, 'effect': lasts(3600, lung=1.5)}),
         ('moon_melon_slices', 'Moon melon slices', 8, 'c0e0a0', [('moon_melon', 2)], {'food': 0.5, 'effect': lasts(2400, regen=3.0)}), ('yeast_loaf', 'Yeast loaf', 8, 'e0d0a0', [('ration_yeast', 3), ('dwarf_wheat', 2)], {'food': 0.8}),
         ('crew_dinner', 'Crew dinner', 8, 'c08050', [('soy_curd', 1), ('potato_mash', 1), ('lettuce_wrap', 1)], {'food': 1.0, 'effect': lasts(6000, regen=3.0, walk=1.15)}),
         ('berry_gel', 'Belt-berry gel', 9, '6050c0', [('belt_berry', 4)], {'food': 0.4, 'effect': lasts(3600, sight=1.0)}), ('frond_tea', 'Glow-frond tea', 9, '40e0c0', [('glow_frond', 3), ('water_ice', 1)], {'cupped': True, 'effect': lasts(3600, mine=1.3)}),
         ('coral_crisp', 'Coral crisps', 9, 'f0a0c0', [('star_coral', 3), ('salt', 1)], {'food': 0.4, 'effect': lasts(3600, armor=0.1)}), ('maize_cake', 'Mars maize cake', 10, 'f0a040', [('mars_maize', 3), ('soy_bean', 1)], {'food': 0.8}),
         ('rust_cap_stew', 'Rust-cap stew', 10, 'c05030', [('rust_cap', 3), ('pale_tuber', 2)], {'food': 0.9, 'effect': lasts(3600, warmth=0.5, strength=2)}), ('tuber_roast', 'Roast pale tuber', 10, 'e0d8c0', [('pale_tuber', 3)], {'food': 0.7}),
         ('lantern_syrup', 'Lantern syrup', 10, 'f0e060', [('cave_lantern', 4)], {'cupped': True, 'effect': lasts(3600, sight=1.0, walk=1.2)}), ('ember_chilli', 'Ember chilli', 10, 'f07020', [('ember_pod', 3), ('mars_maize', 2)], {'food': 0.8, 'effect': lasts(6000, warmth=0.8, fireproof=1.0)}),
         ('moss_broth', 'Blood-moss broth', 10, 'a02030', [('blood_moss', 3), ('mars_ice', 1)], {'food': 0.5, 'effect': lasts(1200, regen=12.0)}), ('lichen_wafer', 'Sun-lichen wafer', 10, 'e0c040', [('sun_lichen', 4)], {'food': 0.5, 'effect': lasts(6000, lung=3.0)}),
         ('coil_nut_butter', 'Coil-nut butter', 10, 'c070e0', [('coil_nut', 4)], {'food': 0.7, 'effect': lasts(3600, craft=1.4)}), ('colonist_feast', "Colonist's feast", 11, 'f0c080', [('maize_cake', 1), ('rust_cap_stew', 1), ('lantern_syrup', 1), ('tuber_roast', 1)], {'food': 1.0, 'effect': lasts(12000, regen=6.0, walk=1.25, mine=1.3)}),
         ('strider_steak', 'Strider steak', 10, '9a4a3a', [('strider_meat', 2), ('ember_pod', 1)], {'food': 1.0, 'effect': lasts(3600, strength=3)}), ('grazer_jerky', 'Grazer jerky', 10, '7a3a2a', [('grazer_meat', 2), ('salt', 2)], {'food': 0.7}),
         ('ray_fin_soup', 'Ray-fin soup', 9, '5a6ab0', [('ray_fin', 2), ('water_ice', 1)], {'food': 0.6, 'effect': lasts(6000, glide=1.0)})]
for n, lab, tier, col, ins, kw in MEALS: meal(n, lab, tier, col, ins, kw.pop('cupped', False), **kw)

# ---- what lives out there ------------------------------------------------------------------------------------------------------------------
def drone(col, eye, frame=0):
    t = P().r(4, 5, 11, 10, col); t.r(4, 5, 11, 5, lt(col)); t.r(6, 7, 9, 8, eye); t.r(2, 6 + frame, 3, 7 + frame, dk(col)); t.r(12, 7 - frame, 13, 8 - frame, dk(col)); t.r(7, 11, 8, 12 + frame, DARK); t.p(7, 3, RED if frame else AMBER); return t.outline()
PART = {'quad': hide, 'boar': meat, 'serpent': fang, 'bird': feather, 'crane': feather, 'rabbit': hide, 'ape': hide, 'golem': lambda col: trinket('rune', col, CYAN), 'stag': meat, 'wraith': lambda col: orb(col, spark=False),
        'fish': meat, 'wyrm': lambda col: trinket('plate', col), 'lion': hide, 'dragon': lambda col: trinket('plate', col), 'tortoise': lambda col: trinket('plate', col), 'tiger': fang, 'drone': lambda col: chip(col, c('8a8e9a'))}
def creature(kind, name, label, body, accent, health, part, spawn, tier, mean=False, extra=(), **kw):
    for f in (0, 1): (drone(c(body), c(accent), f) if kind == 'drone' else beast(kind, c(body), c(accent), f)).save('mobs', name + ('_walk' if f else ''))
    pn, plab = part; thing(PART[kind](c(body)), pn, plab, tier, 'nature', 0.3, **({'food': 0.25} if PART[kind] is meat else {}))
    flier = kind in ('bird', 'wraith', 'drone', 'dragon') and kw.pop('flies', True)
    m = {'name': name, 'label': label, 'texture': 'mobs/%s.png' % name, 'walk': ['mobs/%s_walk.png' % name, 'mobs/%s.png' % name], 'hop': 0.0 if kind in ('golem', 'serpent', 'drone') else 0.06, 'step_ticks': 3 if mean else 4,
         'move_chance': 0.5 if flier else 0.2, 'turn_chance': 0.25, 'health': health, 'cap': 3, 'spawn': spawn, 'flies': True if flier else None, 'drops': [drop(pn, 1, 2)] + [drop(i, a, b) for i, a, b in extra],
         'sting': {'ticks': 40 + tier * 8, 'rest_ticks': 60, 'sight': 8} if mean and not flier else None, 'strike': {'share': 0.03 * (tier - 5), 'range': 7, 'rest_ticks': 70} if mean and flier else None}
    m.update(kw); mobs.append(clean(m))
def on(tiles_, biomes_, chance): return {'above': tiles_, 'biome': biomes_, 'chance': chance}
def sky(biomes_, chance): return {'sky': True, 'biome': biomes_, 'chance': chance}
ORBIT_B = ['low_orbit', 'debris_field', 'graveyard_orbit']
MOON_B = ['mare', 'highlands', 'polar_crater', 'crystal_fields', 'glass_plains', 'ancient_site', 'ray_crater', 'lava_plain']
BELT_B = ['stony_belt', 'metal_belt', 'ice_belt', 'carbon_belt', 'crystal_belt', 'wreck_field', 'belt_garden', 'precursor_shell']
MARS_B = ['rust_plains', 'dune_sea', 'polar_cap', 'canyon_lands', 'volcanic_rise', 'mars_salt_flats', 'colony_ruins', 'living_caves', 'pyramid_fields', 'glass_forest']
creature('drone', 'debris_drone', 'Derelict drone', '8a8e9a', 'e04030', 30, ('drone_chip', 'Drone chip'), sky(['debris_field', 'graveyard_orbit'], 0.004), 6, True, [('circuit_board', 0, 1)])
creature('wraith', 'static_wisp', 'Static wisp', '60a0f0', 'ffffff', 20, ('wisp_charge', 'Wisp charge'), sky(ORBIT_B, 0.003), 6)
creature('tortoise', 'moon_mite', 'Moon mite', '9a9aa0', '5a5a62', 30, ('mite_shell', 'Mite shell'), on(['regolith', 'dark_regolith'], ['mare', 'highlands', 'lava_plain'], 0.1), 7, False, [('moon_sample', 0, 1)])
creature('rabbit', 'dust_hopper', 'Dust hopper', 'c0c0c8', 'f0f0ff', 24, ('hopper_pelt', 'Hopper pelt'), on(['regolith'], ['highlands', 'ray_crater', 'glass_plains'], 0.1), 7, False, hop=0.14)
creature('serpent', 'regolith_worm', 'Regolith worm', '7a7a82', 'e0a040', 60, ('worm_segment', 'Worm segment'), on(['dark_regolith', 'regolith'], ['mare', 'lava_plain', 'polar_crater'], 0.06), 7, True, [('helium3_dust', 0, 2)])
creature('golem', 'crystal_crawler', 'Crystal crawler', '6a90c0', 'c0e8ff', 80, ('crawler_shard', 'Crawler shard'), on(['impact_glass', 'regolith'], ['crystal_fields', 'glass_plains'], 0.08), 8, True, [('moon_crystal', 0, 1)])
creature('drone', 'sentinel_probe', 'Sentinel probe', '4a6a6a', '60ffd0', 90, ('sentinel_lens', 'Sentinel lens'), sky(['ancient_site'], 0.006), 8, True, [('processor', 0, 1)])
creature('stag', 'ice_strider', 'Ice strider', 'c0e0f0', '60a0d0', 50, ('strider_frost', 'Frost gland'), on(['dark_regolith'], ['polar_crater'], 0.1), 8, False, [('moon_ice', 0, 2)])
creature('golem', 'moon_colossus', 'Lunar colossus', '5a5a62', 'f0a030', 600, ('colossus_core', 'Colossus core'), on(['glyph_stone', 'ancient_alloy'], ['ancient_site'], 0.01), 8, True, [('moon_crystal', 2, 4), ('alien_glyph_1', 1, 1)], size=[3, 3], cap=1)
creature('bird', 'void_ray', 'Void ray', '3a4a8a', 'a0c0ff', 70, ('ray_fin', 'Ray fin'), sky(['stony_belt', 'ice_belt', 'crystal_belt'], 0.004), 9)
creature('golem', 'rock_borer', 'Rock borer', '5a5450', 'f08030', 110, ('borer_tooth', 'Borer tooth'), on(['asteroid_rock', 'nickel_rock'], ['stony_belt', 'metal_belt'], 0.08), 9, True, [('meteoric_iron_ore', 0, 2)])
creature('wraith', 'ice_wisp', 'Comet wisp', 'b0e0f8', 'ffffff', 60, ('wisp_ice', 'Wisp ice'), sky(['ice_belt'], 0.006), 9, False, [('water_ice', 0, 2)])
creature('tortoise', 'hull_leech', 'Hull leech', '4a3a3a', 'e04030', 90, ('leech_acid', 'Leech acid'), on(['debris_plating', 'carbon_rock'], ['wreck_field', 'carbon_belt'], 0.1), 9, True)
creature('drone', 'mining_drone', 'Rogue mining drone', 'c8a030', 'e04030', 120, ('drone_servo', 'Drone servo'), sky(['wreck_field', 'metal_belt'], 0.005), 9, True, [('sensor_array', 0, 1), ('iridium_ore', 0, 1)])
creature('wraith', 'star_jelly', 'Star jelly', 'c080ff', 'ffe0ff', 50, ('jelly_bell', 'Jelly bell'), sky(['belt_garden', 'crystal_belt'], 0.006), 9, False, [('life_sample', 0, 1)])
creature('rabbit', 'garden_skitter', 'Garden skitter', '40c0a0', 'e0fff0', 40, ('skitter_silk', 'Skitter silk'), on(['garden_rock'], ['belt_garden'], 0.15), 9, False, [('life_sample', 0, 1)], hop=0.14)
creature('dragon', 'belt_leviathan', 'Belt leviathan', '2a2a4a', 'c080ff', 900, ('leviathan_scale', 'Leviathan scale'), sky(['precursor_shell'], 0.002), 9, True, [('star_sapphire', 2, 4), ('alien_glyph_3', 1, 1)], size=[3, 3], cap=1)
creature('stag', 'sand_strider', 'Sand strider', 'c08050', 'f0d0a0', 90, ('strider_meat', 'Strider meat'), on(['rust_sand', 'rust_soil'], ['rust_plains', 'dune_sea', 'mars_salt_flats'], 0.1), 10, False, [('mars_sample', 0, 1)], mount={'speed': 1.8}, tame=4, companion={'bite': 20, 'sight': 9})
creature('boar', 'cave_grazer', 'Cave grazer', '6a8a5a', 'c0e0a0', 80, ('grazer_meat', 'Grazer meat'), on(['cave_moss_ground'], ['living_caves', 'glass_forest'], 0.15), 10, False, [('life_sample', 0, 1)])
creature('tortoise', 'rust_beetle', 'Rust beetle', 'a04a2a', 'f0a060', 100, ('beetle_plate', 'Beetle plate'), on(['rust_soil', 'mars_rock'], ['rust_plains', 'canyon_lands', 'colony_ruins'], 0.1), 10, True)
creature('wraith', 'dust_devil', 'Dust devil', 'c8703a', 'f0d0a0', 70, ('devil_grit', 'Devil grit'), sky(['dune_sea', 'rust_plains', 'mars_salt_flats'], 0.006), 10, True, [('perchlorate', 0, 2)])
creature('bird', 'spore_drifter', 'Spore drifter', 'c05030', 'f0e060', 60, ('drifter_spore', 'Drifter spore'), sky(['living_caves', 'glass_forest', 'volcanic_rise'], 0.006), 10, False, [('life_sample', 0, 1)])
creature('tiger', 'canyon_stalker', 'Canyon stalker', '7a3a2a', '2a1a1a', 180, ('stalker_fang', 'Stalker fang'), on(['mars_rock', 'rust_soil'], ['canyon_lands', 'volcanic_rise'], 0.06), 10, True)
creature('serpent', 'ice_borer', 'Ice borer', 'd0e8f8', '60a0e0', 120, ('borer_gland', 'Borer gland'), on(['polar_frost'], ['polar_cap'], 0.1), 10, True, [('mars_ice', 0, 2)])
creature('rabbit', 'glow_hopper', 'Glow hopper', 'f0e060', 'ffffff', 50, ('hopper_glow', 'Glow sac'), on(['cave_moss_ground'], ['living_caves'], 0.15), 10, False, hop=0.14, tame=4, companion={'bite': 8, 'sight': 8})
creature('tortoise', 'moss_crab', 'Moss crab', '4a6a3a', 'a0d070', 110, ('crab_claw', 'Crab claw'), on(['cave_moss_ground'], ['living_caves', 'glass_forest'], 0.1), 10, True)
creature('golem', 'ruin_warden', 'Ruin warden', '4a6a6a', 'ff5030', 260, ('warden_eye', 'Warden eye'), on(['glyph_stone', 'ancient_alloy'], ['pyramid_fields'], 0.06), 11, True, [('quantum_core', 0, 1)])
creature('drone', 'colony_drone', 'Colony drone', 'c0c8d0', '30c070', 80, ('colony_chip', 'Colony chip'), sky(['colony_ruins'], 0.006), 10, True, [('flight_computer', 0, 1)])
creature('lion', 'dune_lion', 'Dune lion', 'd09050', 'f0d090', 220, ('lion_hide', 'Dune lion hide'), on(['rust_sand'], ['dune_sea'], 0.04), 10, True, mount={'speed': 2.0}, tame=5, companion={'bite': 40, 'sight': 10})
creature('dragon', 'red_wyrm', 'Red wyrm', 'a02818', 'f0a030', 1200, ('wyrm_heart', 'Wyrm heart'), on(['volcanic_glass', 'mars_basalt'], ['volcanic_rise'], 0.005), 11, True, [('mars_opal', 2, 4), ('alien_glyph_5', 1, 1)], size=[3, 3], cap=1, flies=False, strike={'share': 0.2, 'range': 9, 'rest_ticks': 60})
creature('golem', 'pyramid_guardian', 'Pyramid guardian', '2a3a3a', '60ffd0', 1500, ('guardian_matrix', 'Guardian matrix'), on(['glyph_stone'], ['pyramid_fields'], 0.004), 11, True, [('tharsium_ingot', 3, 6), ('alien_glyph_6', 1, 1)], size=[3, 3], cap=1, strike={'share': 0.25, 'range': 10, 'rest_ticks': 50})

# ---- what is found: caches, records, relics, and the writing of those who were here first -----------------------------------------------------
def vault(name, label, col, trim, tier, cols, rows, loot, glow=5):
    chest_pic(c(col), c(trim)).save('tiles', name); icon_of(name)
    tile(name, label, False, mine_time=0.6, storage={'columns': cols, 'rows': rows}, loot=loot, light=glow, model='box'); item(name, label, tier, 'crafted', 1.0, places=name, container={'slots': cols * rows})
for k in range(1, 9): comp(glyph(c(['60ffd0', '60d0ff', 'c080ff', 'ffd060', 'ff7050', 'ffffff', 'a0ff60', 'ff80c0'][k - 1]), k), 'alien_glyph_%d' % k, 'Precursor glyph %s' % NUM[k], 9, 5.0, 'special', max_stack=1)
LOGS = ['First orbit', 'The long night', 'Tranquillity base', 'Ice at the pole', 'What the drill found', 'Belt claim 114', 'The hollow rock', 'Silence on the relay', 'Landfall', 'The first storm', 'Greenhouse four', 'They were here before us']
for k, lab in enumerate(LOGS, 1): comp(card(c('2a3a4a'), CYAN), 'mission_log_%d' % k, 'Log: ' + lab, 6 + k // 3, 1.5, 'special', max_stack=1)
# name, label, drawn as, colour, tier, what it gives, the set it is of
RELICS = [('selene_idol', 'Selene idol', 'seal', 'c0d0f0', 8, {'lung': 1.5}, 'selenite'), ('tide_stone', 'Tide stone', 'bead', '6090e0', 8, {'regen': 1.5}, 'selenite'), ('moon_compass', 'Moon compass', 'mirror', 'e0e8ff', 8, {'sight': 1.0}, 'selenite'), ('silver_band', 'Silver band', 'ring', 'd0d8e8', 8, {'luck': 0.1}, 'selenite'),
          ('belter_charm', "Belter's charm", 'pendant', '8a8478', 9, {'mine': 1.3}, 'belter'), ('claim_marker', 'Claim marker', 'needle', 'f0a030', 9, {'luck': 0.15}, 'belter'), ('ore_whistle', 'Ore whistle', 'bell', 'c0c8d0', 9, {'magnet': 1.0}, 'belter'), ('void_lantern', 'Void lantern', 'gourd', '5070f0', 9, {'sight': 1.0, 'walk': 1.1}, 'belter'),
          ('ares_mask', 'Ares mask', 'crown', 'd06040', 10, {'armor': 0.1, 'strength': 2}, 'ares'), ('dust_charm', 'Dust charm', 'bead', 'c8703a', 10, {'lung': 2.0}, 'ares'), ('canal_key', 'Canal key', 'key', 'f0d0a0', 10, {'walk': 1.2}, 'ares'), ('ember_band', 'Ember band', 'ring', 'f07020', 10, {'fireproof': 1.0, 'warmth': 0.3}, 'ares'),
          ('precursor_eye', 'Precursor eye', 'mirror', '60ffd0', 11, {'sight': 1.0, 'luck': 0.2}, 'precursor'), ('phase_shard', 'Phase shard', 'needle', 'c080ff', 11, {'unseen': 1.0}, 'precursor'), ('gravity_knot', 'Gravity knot', 'bead', '60d0ff', 11, {'flight': 1.0}, 'precursor'), ('star_seed', 'Star seed', 'egg', 'ffd060', 11, {'regen': 3.0, 'craft': 1.5}, 'precursor')]
for n, lab, kind, col, tier, boons, set_ in RELICS: thing(trinket(kind, c(col), c('8a8e9a')), n, lab, tier, 'special', 2.0, max_stack=1, relic=True, boons=boons, set=set_)
sets += [{'name': 'selenite', 'label': 'Selenite hoard', 'bonuses': [{'pieces': 2, 'boons': {'lung': 2.0}}, {'pieces': 4, 'boons': {'regen': 2.0, 'luck': 0.2}}]}, {'name': 'belter', 'label': "Belter's kit", 'bonuses': [{'pieces': 2, 'boons': {'mine': 1.3}}, {'pieces': 4, 'boons': {'mine': 1.6, 'flight': 1.0}}]},
         {'name': 'ares', 'label': 'Relics of Ares', 'bonuses': [{'pieces': 2, 'boons': {'armor': 0.1}}, {'pieces': 4, 'boons': {'strength': 4, 'walk': 1.3}}]}, {'name': 'precursor', 'label': 'Precursor works', 'bonuses': [{'pieces': 2, 'boons': {'craft': 1.5}}, {'pieces': 4, 'boons': {'lung': 50.0, 'ward': 0.3}}]}]
def some(*names, most=2): return {'one_of': [{'item': n, 'weight': w} for n, w in names], 'min': 1, 'max': most}
vault('supply_pod', 'Supply pod', 'c0c8d0', 'f0a030', 6, 4, 2, [drop('oxygen_canister', 1, 4), some(('circuit_board', 4), ('solar_cell', 4), ('aluminium_plate', 5), ('kerosene_can', 3), ('ration_pack', 4), ('pressure_seal', 4)), some(('mission_log_1', 2), ('mission_log_2', 2), ('life_support_unit', 1), ('orbit_sample', 4), most=1)])
vault('lunar_cache', 'Lunar cache', 'd8d8e0', '3070d0', 7, 4, 3, [drop('oxygen_canister', 2, 5), drop('moon_sample', 1, 4), some(('lunite_ingot', 5), ('processor', 3), ('moon_ice', 5), ('helium3_dust', 4), ('hydrogen_can', 3), ('moon_crystal', 2)),
      some(('mission_log_3', 2), ('mission_log_4', 2), ('mission_log_5', 2), ('selene_idol', 1), ('tide_stone', 1), ('moon_compass', 1), ('silver_band', 1), most=1)])
vault('belt_strongbox', 'Belt strongbox', '5a5a62', 'e0b030', 9, 4, 3, [drop('oxygen_canister', 2, 6), drop('belt_sample', 1, 4), some(('meteoric_iron_ingot', 5), ('iridium_ingot', 3), ('sensor_array', 3), ('water_ice', 5), ('star_sapphire', 2), ('fuel_rod', 1)),
      some(('mission_log_6', 2), ('mission_log_7', 2), ('mission_log_8', 2), ('belter_charm', 1), ('claim_marker', 1), ('ore_whistle', 1), ('void_lantern', 1), most=1)])
vault('colony_locker', 'Colony locker', 'e0e4ea', 'd06040', 10, 4, 4, [drop('oxygen_canister', 3, 8), drop('mars_sample', 1, 4), some(('martian_steel_ingot', 5), ('flight_computer', 3), ('mars_ice', 5), ('methane_can', 4), ('mars_opal', 2), ('uranium_ingot', 2), ('colonist_feast', 1)),
      some(('mission_log_9', 2), ('mission_log_10', 2), ('mission_log_11', 2), ('mission_log_12', 1), ('ares_mask', 1), ('dust_charm', 1), ('canal_key', 1), ('ember_band', 1), most=2)])
vault('precursor_casket', 'Precursor casket', '3a4a4a', '60ffd0', 9, 4, 2, [some(('alien_glyph_1', 3), ('alien_glyph_2', 3), ('alien_glyph_3', 2), ('alien_glyph_4', 2), ('alien_glyph_5', 2), ('alien_glyph_6', 1), ('alien_glyph_7', 1), ('alien_glyph_8', 1), most=1),
      some(('moon_crystal', 4), ('star_sapphire', 3), ('mars_opal', 2), ('ancient_cell', 3), ('tharsium_ingot', 1)), some(('precursor_eye', 1), ('phase_shard', 1), ('gravity_knot', 1), ('star_seed', 1), ('ancient_cell', 6), most=1)], 9)
comp(can(c('2a3a3a'), c('60ffd0'), c('60ffd0')), 'ancient_cell', 'Ancient cell', 9, 2.0, 'energy', max_stack=20, fuel=40000)
# What the glyphs teach: things that are not made the way anything else is.
t = P().line(3, 13, 12, 3, c('3a4a4a')); t.line(4, 13, 13, 3, c('60ffd0')); t.r(10, 2, 13, 5, c('c0fff0')); t.r(2, 12, 4, 14, DARK); t.outline().save('items', 'phase_drill')
item('phase_drill', 'Phase drill', 11, 'tools', 3.0, max_stack=1, tool={'class': 'pickaxe', 'speed': 30.0, 'ore_multiplier': 3}, power=13); make('assembly_bay', 'phase_drill', [('tharsium_frame', 2), ('ancient_cell', 4), ('quantum_core', 1), ('mars_opal', 2)], 4, 1, 900, 'alien_glyph_1')
thing(trinket('ring', c('60d0ff'), c('3a4a4a')), 'gravity_lens', 'Gravity lens', 11, 'special', 3.0, max_stack=1, relic=True, boons={'flight': 1.0, 'walk': 1.3}); make('assembly_bay', 'gravity_lens', [('ancient_cell', 6), ('star_sapphire', 3), ('quantum_core', 1)], 4, 1, 900, 'alien_glyph_2')
thing(trinket('seal', c('c080ff'), c('3a4a4a')), 'stasis_projector', 'Stasis projector', 10, 'tools', 2.0, max_stack=1, uses=60, range=12, cast={'hold': 300, 'splash': 3}); make('assembly_bay', 'stasis_projector', [('meteoric_frame', 1), ('ancient_cell', 3), ('flight_computer', 1)], 3, 1, 600, 'alien_glyph_3')
thing(trinket('mirror', c('60ffd0'), c('3a4a4a')), 'shield_emitter', 'Shield emitter', 10, 'tools', 2.0, max_stack=1, uses=40, range=0, cast={'shield': 2.0}); make('assembly_bay', 'shield_emitter', [('meteoric_frame', 1), ('ancient_cell', 3), ('moon_crystal', 4)], 3, 1, 600, 'alien_glyph_4')
thing(trinket('needle', c('ff7050'), c('3a4a4a')), 'plasma_lance', 'Plasma lance', 11, 'tools', 3.0, max_stack=1, damage=260, range=10); make('assembly_bay', 'plasma_lance', [('tharsium_frame', 1), ('ancient_cell', 6), ('wyrm_heart', 1), ('quantum_core', 1)], 4, 1, 900, 'alien_glyph_5')
thing(trinket('egg', c('ffffff'), c('60ffd0')), 'recall_beacon', 'Recall beacon', 10, 'tools', 2.0, max_stack=5, recalls=True, uses=1); make('assembly_bay', 'recall_beacon', [('ancient_cell', 2), ('sensor_array', 1)], 3, 2, 400, 'alien_glyph_6')
thing(pill(c('a0ff60'), 3, c('60ffd0')), 'xeno_serum', 'Xeno-serum', 10, 'special', 1.0, max_stack=10, cures=True, effect=lasts(1200, regen=30.0)); make('sample_lab', 'xeno_serum', [('blood_moss', 3), ('life_sample', 2), ('jelly_bell', 1)], 4, 2, 300, 'alien_glyph_7')
t = machine(c('2a3a3a'), lambda t: (t.r(4, 5, 11, 11, c('102020')), t.r(6, 7, 9, 9, c('60ffd0')), t.p(7, 8, WHITE)), c('60ffd0'))
t.save('tiles', 'matter_printer'); icon_of('matter_printer'); tile('matter_printer', 'Matter printer', False, mine_time=0.5, bench=True, bench_kind='matter_printer', bench_level=1, light=9); item('matter_printer', 'Matter printer', 11, 'benches', 3.0, places='matter_printer')
rec('workbench', 'matter_printer', [('tharsium_frame', 4), ('ancient_cell', 8), ('quantum_core', 2), ('alien_glyph_8', 1)], level=4, formula='alien_glyph_8')
for out, src, k in (('hull_panel', 'regolith', 4), ('hab_wall', 'regolith', 4), ('dome_glass', 'rust_sand', 4), ('aluminium_plate', 'regolith', 2), ('glass', 'rust_sand', 4), ('oxygen_canister', 'mars_ice', 6), ('pressure_seal', 'carbon_rock', 4), ('solar_cell', 'impact_glass', 2), ('ancient_cell', 'uranium_ingot', 1)):
    rec('matter_printer', out, [(src, 2)], k, time=40)

# ---- portals and trade to tier XI: two for the Moon, one for the Belt, two for the red planet ------------------------------------------------------
TIERCOL = {7: ramp('2a3a5a', 'a0b8e0', 'ffffff'), 8: ramp('1a4a4a', '60d0c0', 'ffffff'), 9: ramp('2a2a30', 'a0a4b0', 'ffffff'), 10: ramp('5a1a10', 'e07050', 'fff0e0'), 11: ramp('3a1a4a', 'c070e0', 'ffffff')}
PORTAL_COST = {7: 'lunite_block', 8: 'moon_crystal', 9: 'star_sapphire', 10: 'martian_steel_block', 11: 'mars_opal'}
for tier in range(7, 12):
    for base, n, lab, cost, crystals in (('portal_6', 'sky_portal_%d' % tier, 'Portal', [(PORTAL_COST[tier], 1)], 0), ('trade_portal_6', 'sky_trade_portal_%d' % tier, 'Trade portal', [(PORTAL_COST[tier], 1), ('platinum_coin', (tier - 6) * 2)], (tier - 6) * 400)):
        e = {'cost': [{'item': i, 'count': k} for i, k in cost]}
        if crystals: e['crystals'] = crystals
        clone_tile(base, n, lab, TIERCOL[tier], 'tint', bench_level=None, trade_level=None, extends=e); clone_item(base, n, lab + ' ' + NUM[tier], TIERCOL[tier], tier, 'portals', 1.0, 'tint', places=n)
def portal(tier, out, ins, count=1, time=400): rec('portal', out, ins, count, at='sky_portal_%d' % tier, time=time)
# Each new level makes something of its own.
for tier, (n, lab, kind, col, boons, ins) in zip(range(7, 12), (('lunar_talisman', 'Lunar talisman', 'pendant', 'a0b8e0', {'lung': 2.0, 'walk': 1.1}, [('lunite_block', 2), ('moon_sample', 8), ('processor', 2)]),
        ('crystal_heart', 'Crystal heart', 'bead', '60d0c0', {'regen': 2.0, 'warmth': 0.4}, [('moon_crystal', 6), ('tranquillium_block', 1), ('sensor_array', 2)]), ('star_compass', 'Star compass', 'mirror', '5070f0', {'sight': 1.0, 'mine': 1.3}, [('star_sapphire', 4), ('iridium_block', 1), ('flight_computer', 1)]),
        ('ares_sigil', 'Sigil of Ares', 'seal', 'e07050', {'strength': 3, 'armor': 0.15}, [('martian_steel_block', 2), ('stalker_fang', 4), ('quantum_core', 1)]), ('worldheart', 'Worldheart', 'egg', 'c070e0', {'regen': 4.0, 'lung': 10.0, 'luck': 0.2, 'craft': 1.5}, [('mars_opal', 6), ('tharsium_block', 2), ('wyrm_heart', 1), ('guardian_matrix', 1)]))):
    thing(trinket(kind, c(col), GOLD), n, lab, tier, 'special', 4.0, max_stack=1, relic=True, boons=boons); portal(tier, n, ins)

# ---- what was built out there, and by whom ------------------------------------------------------------------------------------------------------
def structure(name, rows, legend): assert len({len(r) for r in rows}) == 1, name; structures.append({'name': name, 'rows': rows, 'legend': legend})
def L(front=None, wall=None, back=None): return clean({'front': front, 'wall': wall, 'back': back})
HUMAN = {'H': L('hull_panel', 'hull_panel'), 'h': L(None, 'hull_panel'), 'W': L('hull_window', 'hull_panel'), 'F': L('hull_floor', 'hull_floor'), 'D': L('airlock_door', 'hull_panel'), 'T': L(None, None, 'truss'), 's': L(None, None, 'solar_array'),
         'l': L(None, 'hull_panel', 'hab_light'), 'c': L(None, 'hull_panel', 'console'), 'b': L(None, 'hull_panel', 'bunk'), 'v': L(None, 'hull_panel', 'air_vent'), 'o': L(None, 'hull_panel', 'supply_pod'), 'k': L(None, 'hull_panel', 'cargo_locker'),
         'd': L(None, None, 'comm_dish'), 'f': L(None, None, 'mission_flag'), 'G': L(None, None, 'gantry'), 'P': L('debris_plating', 'debris_plating'), 'p': L(None, 'debris_plating'), 'm': L(None, 'hull_panel', 'mess_table'),
         'u': L(None, 'hull_panel', 'lunar_cache'), 'x': L(None, 'hull_panel', 'belt_strongbox'), 'y': L(None, 'hull_panel', 'colony_locker'), 'e': L('kerosene_engine'), 't': L('kerosene_tank'), 'i': L('interstage'), 'q': L(None, None, 'power_cell'),
         'R': L('regolith_brick', 'regolith_brick'), 'r': L(None, 'regolith_brick'), 'M': L('mars_brick', 'mars_brick'), 'n': L(None, 'mars_brick'), 'O': L('dome_glass', 'mars_brick'), 'C': L('mars_concrete', 'mars_concrete'),
         'g': L('hydroponic_tray', 'mars_brick'), 'w': L(None, 'mars_brick', 'red_potato_3'), 'z': L(None, 'mars_brick', 'colony_locker'), 'a': L(None, 'mars_brick', 'air_vent'), 'j': L(None, 'mars_brick', 'hab_light'), 'J': L('airlock_door', 'mars_brick'),
         'S': L(None, 'hull_panel', 'seismometer'), 'Q': L(None, 'regolith_brick', 'lunar_cache'), 'V': L(None, 'regolith_brick', 'air_vent'), 'B': L(None, 'regolith_brick', 'bunk'), 'E': L('airlock_door', 'regolith_brick'), 'N': L(None, 'regolith_brick', 'hab_light')}
ALIEN = {'A': L('ancient_alloy', 'ancient_alloy'), 'a': L(None, 'ancient_alloy'), 'G': L('glyph_stone', 'glyph_stone'), 'g': L(None, 'glyph_stone'), 'K': L(None, 'glyph_stone', 'precursor_casket'), 'X': L('moon_crystal_spire'), 'Y': L('star_sapphire_spire'), 'Z': L('mars_opal_spire'),
         'I': L('impact_glass', 'ancient_alloy'), 'V': L('volcanic_glass', 'glyph_stone')}
# Orbit.
structure('dead_satellite', ['ss~H~ss', '~~HoH~~', '~~~d~~~', '~~~~~~~'], HUMAN)
structure('cargo_pod', ['~HHHH~', '~Ho.H~'.replace('.', 'h'), '~HHHH~', '~~~~~~'], HUMAN)
structure('station_wreck', ['~HHWHHHH~~', '~HlhhhcH~~', '~Dhhhhh~~~', '~HbhkhH~~~', '~FFFFF~~~~', '~~~~~~~~~~'], HUMAN)
structure('solar_fragment', ['sssTsss', '~~~T~~~', '~~~T~~~', '~~~~~~~'], HUMAN)
structure('spent_stage', ['~~i~~', '~~t~~', '~~t~~', '~~e~~', '~~~~~'], HUMAN)
structure('lost_lab', ['~HHHWHH~', '~HchhSH~', '~Dhhhh~~', '~HohhkH~', '~FFFFFF~', '~~~~~~~~'], HUMAN)
# The Moon.
structure('old_lander', ['~~~f~~~', '~~HWH~~', '~~HuH~~', '~T~e~T~', '~~~~~~~'], HUMAN)
structure('crashed_probe', ['~~~~d~', 's~HH~~', '~HoH~~', '~~~~~~'], HUMAN)
structure('abandoned_outpost', ['~RRRRRRRRR~', '~RNrrrNrVR~', '~ErrrrrrrR~', '~EBrrQrrBR~', '~RRRRRRRRR~'], HUMAN)
structure('survey_camp', ['~d~~~~f~', '~q~HH~~~', '~~HuhH~~', '~~~~~~~~'], HUMAN)
structure('precursor_obelisk', ['~~G~~', '~~G~~', '~XGX~', '~AGA~', 'AAAAA'], ALIEN)
structure('precursor_vault', ['~AAAAAAA~', '~AgggggA~', '~~gXgKg~~', '~AgggggA~', 'AAAAAAAAA'], ALIEN)
structure('crystal_shrine', ['~X~~~X~', '~G~K~G~', '~GaaaG~', 'AAAAAAA'], ALIEN)
structure('mass_driver_ruin', ['~~~~~~~~T~', '~~~~~~TT~~', '~~~~TT~~~~', '~HcTT~~~~q', 'RRRRRRRRRR'], HUMAN)
# The Belt.
structure('mining_rig', ['~~T~~~~', '~~T~q~~', '~HHHHH~', '~HchxH~', '~Dhhh~~', '~FFFFF~', '~~T~T~~'], HUMAN)
structure('miner_hab', ['~HHWHH~', '~HlhvH~', '~Dhhh~~', '~HbhkH~', '~FFFFF~', '~~~~~~~'], HUMAN)
structure('wrecked_hauler', ['~~~HHHH~~~', '~HHHxhHHH~', 'eHhhhhhhWH', '~HHHHHHHH~', '~~~~~~~~~~'], HUMAN)
structure('claim_beacon', ['~d~', '~T~', '~T~', '~q~', '~~~'], HUMAN)
structure('precursor_pod', ['~~AAA~~', '~AgggA~', '~AgKgA~', '~~AAA~~', '~~~~~~~'], ALIEN)
structure('sapphire_altar', ['~Y~~~Y~', '~G~~~G~', '~GaKaG~', '~AAAAA~'], ALIEN)
structure('ore_depot', ['~T~~~~T~', '~HHHHHH~', '~Hxhhk~~', '~FFFFFF~', '~~~~~~~~'], HUMAN)
# The red planet.
structure('colony_dome', ['~~~~OOOOO~~~~', '~~OOnnnnnOO~~', '~OnjnnnnnjnO~', '~Jnnnnnnnnna~', '~Jnwnwnwnznn~', '~MgggggggMMM~', 'CCCCCCCCCCCCC'], HUMAN)
structure('greenhouse_ruin', ['~OOOOOOO~', '~Onjnnn~~', '~~nwnwnO~', '~MgggggM~', 'CCCCCCCCC'], HUMAN)
structure('buried_rover', ['~~~d~~~', '~HHWH~~', '~HohH~~', '~~~~~~~'], HUMAN)
structure('weather_station', ['~~d~~', '~~T~s', '~~T~~', '~HzH~', 'CCCCC'], HUMAN)
structure('drill_site', ['~~~T~~~', '~~~T~~~', '~q~T~H~', '~HHTHy~', 'CCC~CCC'], HUMAN)
structure('crashed_ship', ['~~~~~HHHH~~~', '~~HHHhhhHH~~', '~HHhhyhhchW~', 'eHhhhhbhhhH~', '~~HHHHHHH~~~', '~~~~~~~~~~~~'], HUMAN)
structure('precursor_pyramid', ['~~~~~G~~~~~', '~~~~GgG~~~~', '~~~GgKgG~~~', '~~GgggggG~~', '~GgZgggZgG~', 'GGGGg~gGGGG', 'AAAAAAAAAAA'], ALIEN)
structure('precursor_gate', ['~GGGGG~', '~G~~~G~', '~G~~~G~', '~G~K~G~', 'AAAAAAA'], ALIEN)
structure('opal_tomb', ['~~AAAAA~~', '~AVgggVA~', '~~gZKZg~~', '~AAAAAAA~'], ALIEN)

# ---- the places ----------------------------------------------------------------------------------------------------------------------------------
def biome(name, label, dim, surface, soil, under, trees_=(), one_in=0, wild=(), spikes=(), treasure=None, **over):
    b = {'name': name, 'label': label, 'dimension': dim, 'surface': surface, 'soil': soil, 'under': {'tile': under, 'depth': 40}}
    if trees_: b.update(tree_one_in=one_in, trees=[{'tree': t, 'weight': w} for t, w in trees_])
    if wild: b['plants'] = [{'tile': t + '_0', 'one_in': n} for t, n in wild]
    if spikes: b['spikes'] = [clean({'tile': t, 'one_in': n, 'height': h, 'hang': hang or None, 'arch': arch or None}) for t, n, h, hang, arch in spikes]
    if treasure: b['treasure'] = {'tile': treasure[0], 'one_in': treasure[1]}
    b.update(over); biomes.append(b)
DP = 'debris_plating'
biome('low_orbit', 'Low orbit', 'orbit', DP, DP, DP, spikes=[('truss', 9, [1, 4], False, False)], treasure=('supply_pod', 40))
biome('debris_field', 'Debris field', 'orbit', 'hull_panel', DP, DP, spikes=[('truss', 5, [2, 6], False, False), ('hull_panel', 7, [1, 3], True, False)], treasure=('supply_pod', 18))
biome('graveyard_orbit', 'Graveyard orbit', 'orbit', 'hull_floor', DP, DP, spikes=[('truss', 6, [2, 7], False, True), ('hull_window', 12, [1, 2], False, False)], treasure=('supply_pod', 12))
MB, HR = 'mare_basalt', 'highland_rock'
biome('mare', 'Mare', 'moon', 'dark_regolith', 'dark_regolith', MB, spikes=[(MB, 14, [1, 3], False, False)], treasure=('lunar_cache', 160))
biome('highlands', 'Highlands', 'moon', 'regolith', 'regolith', HR, spikes=[(HR, 9, [1, 5], False, False), (HR, 12, [2, 5], True, False)], treasure=('lunar_cache', 160))
biome('polar_crater', 'Polar crater', 'moon', 'dark_regolith', 'moon_ice_block', MB, spikes=[('moon_ice_block', 5, [1, 5], False, False), ('moon_ice_block', 5, [2, 6], True, False)], cold=True)
biome('crystal_fields', 'Crystal fields', 'moon', 'regolith', 'regolith', HR, spikes=[('moon_crystal_spire', 4, [2, 9], False, False), ('moon_crystal_spire', 4, [2, 7], True, False)], treasure=('lunar_cache', 60))
biome('glass_plains', 'Glass plains', 'moon', 'impact_glass', 'dark_regolith', MB, spikes=[('impact_glass', 6, [1, 4], False, False), ('impact_glass', 14, [3, 6], False, True)])
biome('ancient_site', 'Precursor site', 'moon', 'glyph_stone', 'ancient_alloy', MB, spikes=[('glyph_stone', 6, [3, 9], False, False), ('ancient_alloy', 9, [4, 7], False, True), ('moon_crystal_spire', 12, [2, 5], False, False)], treasure=('precursor_casket', 40))
biome('ray_crater', 'Rayed crater', 'moon', 'regolith', 'regolith', HR, spikes=[(HR, 5, [2, 7], False, False), ('impact_glass', 10, [1, 3], False, False)], treasure=('lunar_cache', 90))
biome('lava_plain', 'Lava plain', 'moon', MB, MB, MB, spikes=[(MB, 6, [2, 8], True, False), (MB, 8, [1, 4], False, False)])
AR = 'asteroid_rock'
biome('stony_belt', 'Stony asteroids', 'belt', AR, AR, AR, spikes=[(AR, 8, [1, 4], False, False), (AR, 8, [1, 5], True, False)])
biome('metal_belt', 'Metal asteroids', 'belt', 'nickel_rock', 'nickel_rock', 'nickel_rock', spikes=[('nickel_rock', 6, [2, 6], False, False), ('meteoric_iron_block', 40, [1, 3], False, False)], treasure=('belt_strongbox', 120))
biome('ice_belt', 'Ice asteroids', 'belt', 'comet_ice', 'water_ice_block', 'comet_ice', spikes=[('comet_ice', 4, [2, 8], False, False), ('comet_ice', 4, [2, 9], True, False)], cold=True)
biome('carbon_belt', 'Carbon asteroids', 'belt', 'carbon_rock', 'carbon_rock', 'carbon_rock', spikes=[('carbon_rock', 7, [1, 5], False, False)])
biome('crystal_belt', 'Sapphire asteroids', 'belt', AR, AR, AR, spikes=[('star_sapphire_spire', 9, [2, 8], False, False), ('star_sapphire_spire', 11, [2, 6], True, False)], treasure=('belt_strongbox', 70))
biome('wreck_field', 'Wreck field', 'belt', DP, DP, AR, spikes=[('truss', 5, [2, 7], False, False), ('hull_panel', 6, [1, 4], True, False), ('truss', 9, [3, 6], False, True)], treasure=('belt_strongbox', 20))
biome('belt_garden', 'Belt garden', 'belt', 'garden_rock', 'garden_rock', AR, [('garden_bulb', 1)], 5, [('glow_frond', 6), ('void_lily', 8), ('star_coral', 8), ('ice_bloom', 10), ('belt_berry', 10)], [('star_sapphire_spire', 20, [1, 3], False, False)])
biome('precursor_shell', 'Precursor shell', 'belt', 'glyph_stone', 'ancient_alloy', 'ancient_alloy', spikes=[('glyph_stone', 5, [3, 10], False, False), ('glyph_stone', 5, [3, 9], True, False), ('ancient_alloy', 8, [4, 7], False, True)], treasure=('precursor_casket', 25))
MRk, MBs = 'mars_rock', 'mars_basalt'
biome('rust_plains', 'Rust plains', 'mars', 'rust_soil', 'rust_soil', MRk, wild=[('spire_weed', 40), ('sun_lichen', 50)], spikes=[(MRk, 12, [1, 4], False, False)], treasure=('colony_locker', 220))
biome('dune_sea', 'Dune sea', 'mars', 'rust_sand', 'rust_sand', MRk, spikes=[('rust_sand', 5, [1, 4], False, False)])
biome('polar_cap', 'Polar cap', 'mars', 'polar_frost', 'mars_ice_block', MRk, wild=[('frost_fern', 14)], spikes=[('mars_ice_block', 5, [2, 8], False, False), ('mars_ice_block', 6, [2, 7], True, False)], cold=True)
biome('canyon_lands', 'Canyon lands', 'mars', MRk, MRk, MBs, wild=[('sun_lichen', 20), ('thread_vine', 24)], spikes=[(MRk, 4, [3, 11], False, False), (MRk, 7, [4, 8], False, True), (MRk, 5, [2, 9], True, False)], treasure=('colony_locker', 160))
biome('volcanic_rise', 'Volcanic rise', 'mars', 'volcanic_glass', MBs, MBs, wild=[('ember_pod', 12)], spikes=[('volcanic_glass', 5, [2, 9], False, False), (MBs, 6, [2, 8], False, False), ('sulphur_bloom_ore', 16, [1, 3], False, False)], hot=True)
biome('mars_salt_flats', 'Salt flats', 'mars', 'mars_salt_crust', 'mars_salt_crust', MRk, spikes=[('mars_salt_crust', 9, [1, 3], False, False), ('mars_opal_spire', 30, [1, 4], False, False)])
biome('colony_ruins', 'Colony ruins', 'mars', 'mars_concrete', 'rust_soil', MRk, wild=[('red_potato', 30), ('mars_maize', 30)], spikes=[('mars_brick', 6, [2, 7], False, False), ('truss', 8, [3, 8], False, False), ('mars_brick', 10, [3, 6], False, True)], treasure=('colony_locker', 24))
biome('living_caves', 'Living ground', 'mars', 'cave_moss_ground', 'rust_soil', MRk, [('rust_shroom', 3), ('coil_palm', 1)], 5, [('rust_cap', 7), ('cave_lantern', 8), ('blood_moss', 8), ('pale_tuber', 10), ('thread_vine', 10), ('spire_weed', 10)], [('mars_opal_spire', 26, [1, 4], False, False)])
biome('pyramid_fields', 'Pyramid fields', 'mars', 'glyph_stone', 'ancient_alloy', MRk, wild=[('violet_coral', 12)], spikes=[('glyph_stone', 5, [3, 12], False, False), ('ancient_alloy', 9, [4, 8], False, True), ('mars_opal_spire', 14, [2, 6], False, False)], treasure=('precursor_casket', 22))
biome('glass_forest', 'Glass forest', 'mars', 'cave_moss_ground', 'rust_soil', MBs, [('glass_tree', 4), ('coil_palm', 1)], 3, [('violet_coral', 10), ('frost_fern', 10), ('cave_lantern', 12)], [('volcanic_glass', 9, [2, 7], False, False)])
def vein(name, tile_, dim, hosts, size, per, depth, **kw): veins.append(clean(dict({'name': name, 'tile': tile_, 'dimension': dim, 'in': hosts, 'size': size, 'per_100_columns': per, 'depth': depth}, **kw)))
vein('sky_bauxite', 'aluminium_ore', None, ['stone'], [3, 7], 10, [10, 260])
MOONR, BELTR, MARSR = [MB, HR], [AR, 'nickel_rock', 'carbon_rock'], [MRk, MBs]
vein('sky_lunite', 'lunite_ore', 'moon', MOONR, [3, 7], 34, [0, 260]); vein('sky_tranq', 'tranquillium_ore', 'moon', MOONR, [2, 5], 16, [110, 260]); vein('sky_he3', 'helium3_dust_ore', 'moon', ['regolith', 'dark_regolith'], [2, 5], 30, [0, 200])
vein('sky_moon_ice', 'moon_ice_block', 'moon', MOONR, [3, 8], 12, [100, 260]); vein('sky_moon_glass', 'impact_glass', 'moon', MOONR, [4, 10], 10, [0, 260])
vein('sky_meteoric', 'meteoric_iron_ore', 'belt', BELTR, [3, 8], 40, [0, 320]); vein('sky_iridium', 'iridium_ore', 'belt', BELTR, [2, 5], 18, [0, 320]); vein('sky_belt_ice', 'water_ice_block', 'belt', BELTR + ['comet_ice'], [3, 8], 20, [0, 320])
vein('sky_martian', 'martian_steel_ore', 'mars', MARSR, [3, 8], 36, [0, 300]); vein('sky_tharsium', 'tharsium_ore', 'mars', [MBs], [2, 5], 20, [170, 300]); vein('sky_uranium', 'uranium_ore', 'mars', [MBs], [2, 4], 14, [150, 300])
vein('sky_perchlorate', 'perchlorate_ore', 'mars', ['rust_soil', 'rust_sand'], [2, 6], 30, [0, 200]); vein('sky_mars_ice', 'mars_ice_block', 'mars', MARSR, [3, 9], 16, [80, 300]); vein('sky_sulphur', 'sulphur_bloom_ore', 'mars', [MBs], [2, 5], 16, [150, 300])
vein('sky_mars_scar', 'mars_opal_spire', 'mars', [MBs], [10, 30], 0.8, [180, 300], shape='scar')
for n, tile_, dim, per, depth in (('sky_moon_crystal', 'moon_crystal', 'moon', 7, [60, 260]), ('sky_sapphire', 'star_sapphire', 'belt', 6, [0, 320]), ('sky_opal', 'mars_opal', 'mars', 6, [100, 300])):
    decor.append({'name': n, 'tile': tile_, 'dimension': dim, 'per_100_columns': per, 'depth': depth})
for n, tile_, dim, per in (('sky_cave_lantern', 'cave_lantern_3', 'mars', 16), ('sky_cave_cap', 'rust_cap_3', 'mars', 14), ('sky_cave_moss', 'blood_moss_3', 'mars', 12), ('sky_moon_cache', 'lunar_cache', 'moon', 1.2), ('sky_mars_cache', 'colony_locker', 'mars', 1.0),
                           ('sky_mars_casket', 'precursor_casket', 'mars', 0.6), ('sky_moon_casket', 'precursor_casket', 'moon', 0.5)):
    decor.append({'name': n, 'tile': tile_, 'dimension': dim, 'per_100_columns': per, 'depth': [70, 290], 'on_floor': True})
dimensions = [
    {'name': 'orbit', 'label': 'Orbit', 'generator': 'islands', 'height': 200, 'rock': DP, 'edge': 'void_edge', 'platform': 'hull_floor', 'falls_home': True, 'climate_wavelength': 120, 'gravity': 0.0, 'airless': True, 'stars': True,
     'islands': {'per_100_columns': 3, 'length': [6, 24], 'depth': [2, 5]}, 'sky_day': [4, 6, 16], 'sky_night': [2, 2, 8], 'ambient': 7,
     'structures': ['dead_satellite', 'cargo_pod', 'station_wreck', 'solar_fragment', 'spent_stage', 'lost_lab'], 'structure_share': 0.7},
    {'name': 'moon', 'label': 'The Moon', 'generator': 'surface', 'height': 260, 'rock': MB, 'edge': 'void_edge', 'platform': 'sintered_block', 'climate_wavelength': 110, 'gravity': 0.17, 'airless': True, 'stars': True,
     'surface': {'level': 0.34, 'relief': 9, 'craters': 3.5, 'crater_radius': [5, 18], 'tubes': 3.0}, 'strata': [{'tile': HR, 'from': 0.0}, {'tile': MB, 'from': 0.62}], 'sky_day': [8, 8, 12], 'sky_night': [2, 2, 6], 'ambient': 6,
     'structures': ['old_lander', 'crashed_probe', 'abandoned_outpost', 'survey_camp', 'precursor_obelisk', 'precursor_vault', 'crystal_shrine', 'mass_driver_ruin'], 'structure_share': 0.09,
     'geodes': [{'shell': 'impact_glass', 'gem': 'moon_crystal', 'per_100_columns': 1.2, 'radius': [3, 7], 'depth': [90, 250]}]},
    {'name': 'belt', 'label': 'The Belt', 'generator': 'islands', 'height': 320, 'rock': AR, 'edge': 'void_edge', 'platform': DP, 'climate_wavelength': 90, 'gravity': 0.0, 'airless': True, 'stars': True,
     'islands': {'per_100_columns': 9, 'length': [6, 64], 'depth': [4, 24]}, 'sky_day': [3, 3, 10], 'sky_night': [2, 2, 8], 'ambient': 6,
     'structures': ['mining_rig', 'miner_hab', 'wrecked_hauler', 'claim_beacon', 'precursor_pod', 'sapphire_altar', 'ore_depot'], 'structure_share': 0.3,
     'geodes': [{'shell': 'nickel_rock', 'gem': 'star_sapphire', 'per_100_columns': 1.6, 'radius': [3, 7]}, {'shell': 'comet_ice', 'gem': 'water_ice_block', 'per_100_columns': 1.2, 'radius': [3, 6]}]},
    {'name': 'mars', 'label': 'The red planet', 'generator': 'surface', 'height': 300, 'rock': MRk, 'edge': 'void_edge', 'platform': 'mars_concrete', 'climate_wavelength': 120, 'gravity': 0.38, 'airless': True,
     'surface': {'level': 0.3, 'relief': 12, 'craters': 1.2, 'crater_radius': [6, 16], 'tubes': 4.5, 'canyon_width': 96, 'canyon_depth': 64}, 'strata': [{'tile': MBs, 'from': 0.68}], 'sky_day': [206, 150, 112], 'sky_night': [26, 16, 20], 'ambient': 5,
     'structures': ['colony_dome', 'greenhouse_ruin', 'buried_rover', 'weather_station', 'drill_site', 'crashed_ship', 'precursor_pyramid', 'opal_tomb'], 'structure_share': 0.1,
     'geodes': [{'shell': 'volcanic_glass', 'gem': 'mars_opal', 'per_100_columns': 1.4, 'radius': [3, 8], 'depth': [120, 290]}]}]

# ==== more of everything, and longer makings ============================================================================================
IT = {i['name']: i for i in items}
def swap(out, old, new, count=None):
    """In every recipe for a thing, one input for another."""
    for r_ in recipes:
        if r_['output']['item'] != out: continue
        for i in r_['inputs']:
            if i['item'] == old: i['item'] = new; i['count'] = count or i['count']
def also(out, name, count=1, bench=None):
    for r_ in recipes:
        if r_['output']['item'] == out and (bench is None or r_['bench'] == bench) and all(i['item'] != name for i in r_['inputs']) and all(i['item'] != out for i in r_['inputs']): r_['inputs'].append({'item': name, 'count': count})
TIER_METAL = {6: 'aluminium', 7: 'lunite', 8: 'tranquillium', 9: 'meteoric_iron', 10: 'martian_steel', 11: 'tharsium'}
GRADE_OF = {6: 1, 7: 2, 8: 2, 9: 3, 10: 4, 11: 4}
NEED_OF = {6: None, 7: 'lunar_survey', 8: 'lunar_survey', 9: 'belt_assay', 10: 'mars_survey', 11: 'mars_survey'}

# ---- specimens: what a rock sometimes has in it ---------------------------------------------------------------------------------------------
SPEC = [('mare_basalt', 'olivine_specimen', 'Olivine', '70a040'), ('highland_rock', 'anorthite_specimen', 'Anorthite', 'e8e8f0'), ('regolith', 'agglutinate_specimen', 'Agglutinate', '6a6a72'), ('dark_regolith', 'ilmenite_specimen', 'Ilmenite', '2a2a3a'),
        ('impact_glass', 'tektite_specimen', 'Tektite', '1a3a2a'), ('asteroid_rock', 'chondrule_specimen', 'Chondrule', '9a8a7a'), ('carbon_rock', 'graphite_specimen', 'Graphite', '3a3a3a'), ('nickel_rock', 'kamacite_specimen', 'Kamacite', 'c0c0c8'),
        ('comet_ice', 'clathrate_specimen', 'Clathrate', 'c0f0ff'), ('mars_rock', 'hematite_specimen', 'Hematite', '8a2a1a'), ('mars_basalt', 'pyroxene_specimen', 'Pyroxene', '3a5a3a'), ('rust_sand', 'jarosite_specimen', 'Jarosite', 'd0a030'),
        ('rust_soil', 'blueberry_specimen', 'Hematite spherule', '4a5a8a'), ('mars_salt_crust', 'gypsum_specimen', 'Gypsum', 'f0f0e8'), ('polar_frost', 'dry_ice_specimen', 'Dry ice', 'f0f8ff'), ('volcanic_glass', 'obsidian_specimen', 'Martian obsidian', '1a1018'),
        ('cave_moss_ground', 'stromatolite_specimen', 'Stromatolite', '6a8a5a'), ('garden_rock', 'biolith_specimen', 'Biolith', '40a090')]
for host, n, lab, col in SPEC:
    tier = IT[host]['tier']; comp(gemstone(c(col), True), n, lab, tier, 1.0, 'ores')
    t_ = next(t for t in tiles if t['name'] == host); t_.pop('drop', None); t_['drops'] = [{'item': host, 'count': 1}, {'item': n, 'count': 1, 'chance': 0.04}]
    smp = 'moon_sample' if tier == 7 else 'belt_sample' if tier == 9 else 'life_sample' if 'lith' in n or 'strom' in n else 'mars_sample'
    rec('sample_lab', smp, [(n, 1)], 3, time=120)
# Fossils, in the red planet's ground: something lived here.
FOSSILS = [('fossil_frond', 'Fossil frond'), ('fossil_shell', 'Coiled fossil'), ('fossil_track', 'Fossil trackway'), ('fossil_tooth', 'Fossil tooth'), ('fossil_egg', 'Stone egg'), ('fossil_wing', 'Fossil wing'), ('fossil_skull', 'Long skull'), ('fossil_mat', 'Fossil mat')]
for k, (n, lab) in enumerate(FOSSILS):
    q = P().r(3, 4, 12, 12, c('8a5a40')); q.r(3, 4, 12, 4, c('b07a58')); g_ = rnd(n)
    for _ in range(9): q.p(g_.randrange(5, 11), g_.randrange(6, 11), c('e8dcc0'))
    q.outline().save('items', n); item(n, lab, 10, 'special', 2.0)
for host in ('rust_soil', 'cave_moss_ground', 'mars_rock'):
    t_ = next(t for t in tiles if t['name'] == host)
    for k, (n, lab) in enumerate(FOSSILS):
        if (k + len(host)) % 3 == 0 and len(t_['drops']) < 6: t_['drops'].append({'item': n, 'count': 1, 'chance': 0.006})
rec('sample_lab', 'life_sample', [('fossil_frond', 1)], 4, level=3, time=200)
for n, lab in FOSSILS[1:]: rec('sample_lab', 'life_sample', [(n, 1)], 4, level=3, time=200)

# ---- refining: each metal worked further before it is any use ----------------------------------------------------------------------------------
REFINE = {'lunite': [('lunite_concentrate', 'Lunite concentrate', 'lox_can')], 'tranquillium': [('tranquillium_concentrate', 'Tranquillium concentrate', 'lox_can'), ('tranquillium_bloom', 'Tranquillium bloom', 'helium3_dust')],
          'meteoric_iron': [('meteoric_concentrate', 'Meteoric concentrate', 'water_ice'), ('meteoric_bloom', 'Meteoric bloom', 'hydrogen_can')], 'iridium': [('iridium_concentrate', 'Iridium concentrate', 'water_ice'), ('iridium_sponge', 'Iridium sponge', 'hydrogen_can')],
          'martian_steel': [('martian_concentrate', 'Martian concentrate', 'perchlorate'), ('martian_bloom', 'Martian bloom', 'sulphur_bloom'), ('wrought_martian_steel', 'Wrought martian steel', 'methane_can')],
          'tharsium': [('tharsium_concentrate', 'Tharsium concentrate', 'perchlorate'), ('tharsium_bloom', 'Tharsium bloom', 'sulphur_bloom'), ('wrought_tharsium', 'Wrought tharsium', 'methane_can'), ('annealed_tharsium', 'Annealed tharsium', 'fuel_rod')],
          'uranium': [('yellowcake', 'Yellowcake', 'perchlorate')]}
for m, stages in REFINE.items():
    tier = next(x[2] for x in METALS if x[0] == m); col = c('%02x%02x%02x' % MR[m](0.6)); prev = m + '_ingot'
    for k, (n, lab, reagent) in enumerate(stages):
        comp([trinket('dust', col), ingot(dk(col, 0.8)), ingot(col), ingot(lt(col, 1.2)).p(9, 8, WHITE)][min(k, 3)], n, lab, tier, 0.6 + 0.2 * k, 'ores')
        make('machine_shop', n, [(prev, 2), (reagent, 1)], GRADE_OF[tier], 2, 160, NEED_OF[tier]); prev = n
    swap(m + '_plate', m + '_ingot', prev)
swap('fuel_rod', 'uranium_ingot', 'yellowcake')
# Wafers cut from each place's crystal, and doped: what its electronics are built on.
for gem, n, lab, col, tier, dope in (('moon_crystal', 'lunar_wafer', 'Lunar wafer', 'a0d0ff', 7, 'helium3_dust'), ('star_sapphire', 'sapphire_wafer', 'Sapphire wafer', '5070f0', 9, 'iridium_sponge'), ('mars_opal', 'opal_wafer', 'Opal wafer', 'f08050', 10, 'yellowcake')):
    comp(P().disc(7.5, 7.5, 5, c(col)).disc(7.5, 7.5, 3.5, lt(c(col))).r(7, 2, 8, 13, dk(c(col))).outline(), n, lab, tier, 0.8); make('electronics_bench', n, [(gem, 1)], GRADE_OF[tier], 2, 160, NEED_OF[tier])
    comp(P().disc(7.5, 7.5, 5, c(col)).disc(7.5, 7.5, 3.5, lt(c(col))).r(5, 5, 10, 10, GOLD).r(6, 6, 9, 9, dk(c(col))).outline(), 'doped_' + n, 'Doped ' + lab.lower(), tier, 1.2); make('electronics_bench', 'doped_' + n, [(n, 1), (dope, 2)], GRADE_OF[tier], 1, 200, NEED_OF[tier])
comp(trinket('needle', c('d0d0e8'), GOLD), 'iridium_contact', 'Iridium contact', 9, 0.8); make('electronics_bench', 'iridium_contact', [('iridium_plate', 1), ('wiring_loom', 1)], 3, 4, 160, 'belt_assay')
swap('processor', 'moon_crystal', 'doped_lunar_wafer'); swap('sensor_array', 'helium3_cell', 'doped_lunar_wafer'); also('sensor_array', 'helium3_cell')
swap('flight_computer', 'star_sapphire', 'doped_sapphire_wafer'); also('flight_computer', 'iridium_contact', 4); swap('quantum_core', 'mars_opal', 'doped_opal_wafer'); also('quantum_core', 'iridium_contact', 4)
# A module of each tier, and an assembly of modules: what every finished thing of the tier is built round.
ELEC_OF = {6: 'circuit_board', 7: 'processor', 8: 'sensor_array', 9: 'flight_computer', 10: 'quantum_core', 11: 'quantum_core'}
POWER_OF = {6: 'solar_cell', 7: 'helium3_cell', 8: 'helium3_cell', 9: 'iridium_contact', 10: 'fuel_rod', 11: 'fuel_rod'}
for tier, m in TIER_METAL.items():
    fr = next(f[0] for f in FRAMES if f[3] == m); col = c('%02x%02x%02x' % MR[m](0.6)); lab = next(x[1] for x in METALS if x[0] == m)
    comp(P().r(3, 3, 12, 12, col).r(3, 3, 12, 3, lt(col)).r(5, 5, 10, 8, c('102030')).r(6, 6, 7, 7, c('30c070')).r(5, 10, 6, 10, AMBER).r(9, 10, 10, 10, RED).outline(), m + '_module', lab + ' module', tier, 1.5)
    make('assembly_bay', m + '_module', [(fr, 1), (ELEC_OF[tier], 1), ('wiring_loom', 2)] + ([(TIER_METAL[tier - 1] + '_module', 1)] if tier > 6 else []), GRADE_OF[tier], 1, 300, NEED_OF[tier])
    comp(P().r(2, 4, 13, 12, col).r(2, 4, 13, 4, lt(col)).r(3, 6, 6, 10, c('102030')).r(8, 6, 12, 7, AMBER).r(8, 9, 12, 10, c('8a8e9a')).r(6, 2, 9, 3, DARK).outline(), m + '_assembly', lab + ' assembly', tier, 3.0)
    make('assembly_bay', m + '_assembly', [(m + '_module', 2), (POWER_OF[tier], 2), ('actuator', 1)], GRADE_OF[tier], 1, 400, NEED_OF[tier])
# The precursors' way of making, for the last tier: four things, each of the one before.
comp(can(c('2a3a3a'), c('ffd060'), c('60ffd0')), 'resonant_cell', 'Resonant cell', 11, 2.0); make('sample_lab', 'resonant_cell', [('ancient_cell', 2), ('martian_steel_module', 1)], 4, 1, 400, 'xenobiology')
comp(trinket('ring', c('60ffd0'), c('2a3a3a')), 'phase_coil', 'Phase coil', 11, 3.0); make('electronics_bench', 'phase_coil', [('resonant_cell', 1), ('doped_opal_wafer', 2), ('iridium_contact', 4)], 4, 1, 400, 'xenobiology')
comp(chip(c('2a3a3a'), c('60ffd0'), c('ffd060')), 'phase_array', 'Phase array', 11, 4.0); make('electronics_bench', 'phase_array', [('phase_coil', 2), ('quantum_core', 1), ('tharsium_module', 1)], 4, 1, 600, 'xenobiology')
comp(orb(c('2a3a3a'), c('60ffd0')), 'precursor_matrix', 'Precursor matrix', 11, 6.0); make('assembly_bay', 'precursor_matrix', [('phase_array', 1), ('tharsium_assembly', 1), ('violet_coral', 4)], 4, 1, 900, 'xenobiology')
make('sample_lab', 'ancient_cell', [('fuel_rod', 1), ('mars_opal', 1), ('jelly_bell', 1)], 4, 2, 400, 'xenobiology')

# ---- drills, guns, and more to wear -------------------------------------------------------------------------------------------------------------
for n, lab, tier, r, host, hard, power, speed in METALS:
    if not power: continue
    col = c('%02x%02x%02x' % r(0.6)); g = GRADE_OF[tier]
    q = P().r(2, 6, 8, 10, c('5a5560')); q.r(3, 5, 7, 5, AMBER); q.line(9, 6, 14, 8, col); q.line(9, 10, 14, 8, col); q.r(9, 7, 11, 9, lt(col)); q.r(4, 11, 6, 14, DARK); q.outline().save('items', n + '_drill')
    item(n + '_drill', lab + ' drill', tier, 'tools', 3.0, max_stack=1, tool={'class': 'pickaxe', 'speed': round(speed * 1.6, 1), 'ore_multiplier': 2}, power=power); make('assembly_bay', n + '_drill', [(n + '_plate', 3), (n + '_assembly', 1)], g, 1, 400, NEED_OF[tier])
    q = P().r(2, 7, 12, 9, col); q.r(2, 7, 12, 7, lt(col)); q.r(13, 8, 14, 8, CYAN); q.r(4, 10, 6, 13, DARK); q.r(6, 5, 9, 6, c('5a5560')); [q.p(x, 8, AMBER) for x in (4, 7, 10)]; q.outline().save('items', n + '_coilgun')
    item(n + '_coilgun', lab + ' coilgun', tier, 'tools', 3.0, max_stack=1, damage=int(speed * 3.2), range=12); make('assembly_bay', n + '_coilgun', [(n + '_plate', 2), (n + '_module', 1), (POWER_OF[tier], 2)], g, 1, 400, NEED_OF[tier])

# ---- more to build with -----------------------------------------------------------------------------------------------------------------------
MORE_BLOCKS = [('hazard_plate', 'Hazard plating', panel(c('f0c030'), False, c('2a2a2a')), 6, [('aluminium_plate', 1), ('carbon_black', 1)], 1), ('deck_grating', 'Deck grating', panel(c('5a6270'), False).r(3, 3, 12, 12, c('3a3e4a')), 6, [('steel_ingot', 1)], 1),
               ('bulkhead', 'Bulkhead', panel(c('7a8490'), True).r(7, 0, 8, 15, c('5a6270')), 6, [('aluminium_plate', 2)], 1), ('lab_tile', 'Lab tile', panel(c('f0f4f8'), False), 6, [('aluminium_plate', 1), ('glass', 1)], 1),
               ('lunar_concrete', 'Lunar concrete', block_tex('paving', 'lunar_concrete', c('7a7a84')), 7, [('regolith', 3), ('moon_ice', 1)], 2), ('glass_brick', 'Tektite brick', block_tex('brick', 'glass_brick', c('2a5a4a')), 7, [('impact_glass', 4)], 2),
               ('asteroid_brick', 'Asteroid brick', block_tex('brick', 'asteroid_brick', c('6a6258')), 9, [('asteroid_rock', 4)], 3), ('nickel_plate_block', 'Nickel plating', panel(c('a0a098'), True), 9, [('nickel_rock', 4)], 3),
               ('ice_brick', 'Comet-ice brick', block_tex('brick', 'ice_brick', c('b8e0f0')), 9, [('comet_ice', 4)], 3), ('rust_adobe', 'Rust adobe', block_tex('brick', 'rust_adobe', c('c87850')), 10, [('rust_sand', 3), ('mars_ice', 1)], 4),
               ('basalt_tile', 'Basalt tile', block_tex('paving', 'basalt_tile', c('4a3430')), 10, [('mars_basalt', 4)], 4), ('salt_brick', 'Salt brick', block_tex('brick', 'salt_brick', c('e8e0d8')), 10, [('mars_salt_crust', 4)], 4),
               ('carved_glyph', 'Carved glyph stone', block_tex('paving', 'carved_glyph', c('3a5050')).r(6, 4, 9, 11, c('60ffd0')).r(4, 7, 11, 8, c('60ffd0')), 9, [('glyph_stone', 2), ('moon_crystal', 1)], 3), ('alloy_tile', 'Ancient tile', panel(c('5a7a7a'), False), 9, [('ancient_alloy', 4)], 3)]
for n, lab, pic, tier, ins, g in MORE_BLOCKS: block(pic, n, lab, tier, 0.05); COL[n] = pic.px[8, 8]; make('machine_shop', n, ins, g, 4, 80)
STAIRED = ['regolith_brick', 'sintered_block', 'mars_brick', 'mars_concrete', 'lunar_concrete', 'asteroid_brick', 'rust_adobe', 'basalt_tile', 'salt_brick', 'glyph_stone', 'ancient_alloy', 'glass_brick']
for n in STAIRED:
    lab, tier = IT.get(n, {}).get('label') or next(i['label'] for i in items if i['name'] == n), next(i['tier'] for i in items if i['name'] == n)
    for what, base in (('column', 'wood_column'), ('stairs', 'wood_stairs')):
        d = copy.deepcopy(BT[base]); d.update(name='%s_%s' % (n, what), label='%s %s' % (lab, what), drop='%s_%s' % (n, what), texture='tiles/%s_%s.png' % (n, what)); d.pop('burns', None)
        Image.open(os.path.join(TEX, 'tiles', n + '.png')).save(os.path.join(TEX, 'tiles', '%s_%s.png' % (n, what)))
        tiles.append(d); item('%s_%s' % (n, what), '%s %s' % (lab, what), tier, 'blocks', 0.04, places='%s_%s' % (n, what)); make('machine_shop', '%s_%s' % (n, what), [(n, 1)], 1, 2, 60)
# More furniture and gear for a base.
def sat(col, wing):
    return P().r(6, 5, 9, 10, col).r(0, 6, 5, 9, wing).r(10, 6, 15, 9, wing).line(0, 7, 5, 7, lt(wing)).line(10, 7, 15, 7, lt(wing)).r(7, 2, 8, 4, c('c0c8d0')).p(7, 1, RED)
deco(sat(c('d0a030'), c('1a3a8a')), 'relay_satellite', 'Relay satellite', 6, [('aluminium_module', 1), ('solar_cell', 4)], 'assembly_bay', light=5)
deco(sat(c('c0c8d0'), c('2a2a6a')), 'survey_satellite', 'Survey satellite', 7, [('lunite_module', 1), ('solar_cell', 4), ('sensor_array', 1)], 'assembly_bay', 2, light=5)
deco(sat(c('e0e4ea'), c('3a5a8a')).disc(7.5, 12, 2.5, c('2a3a4a')), 'space_telescope', 'Space telescope', 9, [('meteoric_iron_module', 1), ('solar_cell', 6), ('star_sapphire', 2)], 'assembly_bay', 3, light=5)
deco(P().r(2, 9, 13, 12, c('c0c8d0')).disc(4, 13, 2, DARK).disc(11, 13, 2, DARK).r(4, 6, 9, 8, c('e8ecf0')).r(10, 4, 11, 8, c('8a8e9a')).r(9, 3, 13, 4, c('1a3a8a')), 'rover_exhibit', 'Rover', 7, [('lunite_assembly', 1), ('solar_cell', 2), ('actuator', 4)], 'assembly_bay', 2, light=4)
deco(P().r(3, 3, 12, 14, c('c0c8d0')).r(4, 4, 11, 9, c('60d0f0')).r(5, 5, 10, 8, c('a0f0ff')).r(5, 11, 10, 12, c('5a6270')).p(6, 6, WHITE), 'cryo_pod', 'Cryo pod', 9, [('meteoric_iron_module', 1), ('glass', 4), ('water_ice', 4)], 'assembly_bay', 3, light=7, bed={'speed': 4.0})
deco(P().r(4, 2, 11, 15, c('5a6270')).r(5, 3, 10, 9, c('102030')).r(6, 4, 9, 8, c('30c070')).r(5, 11, 10, 13, c('3a3e4a')).p(6, 12, AMBER).p(9, 12, RED), 'server_rack', 'Server rack', 8, [('tranquillium_module', 1), ('processor', 4)], 'electronics_bench', 2, light=6)
deco(P().r(2, 12, 13, 15, c('5a6270')).r(6, 4, 9, 11, c('c0c8d0')).r(4, 2, 11, 4, c('e8ecf0')).line(2, 12, 6, 6, c('8a8e9a')).line(13, 12, 9, 6, c('8a8e9a')), 'drill_derrick', 'Drill derrick', 9, [('meteoric_iron_assembly', 1), ('truss', 8)], 'assembly_bay', 3)
deco(P().r(3, 6, 12, 14, c('e8ecf0')).r(4, 7, 11, 10, c('60c060')).p(5, 8, c('a0f0a0')).p(8, 9, c('a0f0a0')).r(3, 5, 12, 5, c('8a8e9a')), 'terrarium', 'Terrarium', 10, [('dome_glass', 4), ('cave_lantern', 2), ('blood_moss', 2)], 'sample_lab', 4, light=8)
deco(P().r(6, 9, 9, 15, c('8a8e9a')).r(3, 3, 12, 8, c('102030')).line(4, 7, 7, 4, CYAN).line(7, 4, 11, 6, CYAN).p(10, 5, WHITE), 'holo_map', 'Holo map', 9, [('flight_computer', 1), ('glass', 2), ('meteoric_frame', 1)], 'electronics_bench', 3, light=9)
deco(P().r(3, 4, 12, 13, c('8a5a40')).r(4, 5, 11, 12, c('efe2b8')).r(5, 6, 7, 8, c('70a040')).r(9, 6, 10, 8, c('8a2a1a')).r(5, 10, 6, 11, c('c0f0ff')).r(8, 10, 10, 11, c('3a3a3a')), 'specimen_case', 'Specimen case', 7, [('lunite_plate', 2), ('glass', 2), ('olivine_specimen', 1), ('anorthite_specimen', 1)], 'sample_lab', 2)
deco(P().r(2, 6, 13, 13, c('8a5a40')).r(3, 7, 12, 12, c('2a1a12')).line(4, 11, 11, 8, c('e8dcc0')).p(5, 9, c('e8dcc0')).p(10, 10, c('e8dcc0')), 'fossil_display', 'Fossil display', 10, [('martian_steel_plate', 2), ('glass', 2), ('fossil_skull', 1), ('fossil_frond', 1)], 'sample_lab', 4)
deco(P().r(5, 3, 10, 15, c('3a4a4a')).r(6, 4, 9, 14, c('102020')).r(7, 5, 8, 13, c('60ffd0')).p(7, 2, c('60ffd0')), 'glyph_pillar', 'Glyph pillar', 9, [('glyph_stone', 4), ('ancient_cell', 1)], 'machine_shop', 3, light=11, pulse=0.1)
deco(P().disc(7.5, 8, 6, c('3a4a4a')).disc(7.5, 8, 4.4, (0, 0, 0, 0)).p(7, 2, c('60ffd0')).p(8, 14, c('60ffd0')).p(2, 8, c('60ffd0')).p(13, 8, c('60ffd0')), 'ring_monument', 'Ring monument', 11, [('ancient_alloy', 8), ('precursor_matrix', 1)], 'assembly_bay', 4, light=12, pulse=0.12)
deco(P().r(7, 6, 8, 15, c('8a8e9a')).r(4, 2, 11, 5, c('e8ecf0')).r(5, 3, 10, 4, AMBER), 'landing_light', 'Landing light', 6, [('aluminium_plate', 1), ('wiring_loom', 1), ('glass', 1)], 'machine_shop', 1, 2, light=15, pulse=0.2)
deco(P().r(2, 10, 13, 15, c('5a6270')).r(3, 4, 6, 9, c('f0a030')).r(8, 3, 12, 9, c('d8e0f0')).r(4, 9, 11, 10, DARK), 'tank_farm', 'Fuel store', 6, [('aluminium_plate', 4), ('kerosene_can', 2), ('lox_can', 2)], 'machine_shop', 1)
deco(P().r(3, 12, 12, 15, c('5a6270')).r(5, 5, 10, 11, c('e04030')).r(6, 6, 9, 8, WHITE).r(6, 3, 9, 4, c('8a8e9a')), 'emergency_beacon', 'Emergency beacon', 6, [('circuit_board', 1), ('aluminium_plate', 1)], 'electronics_bench', light=12, pulse=0.3)
deco(P().r(2, 6, 13, 14, c('e8ecf0')).r(3, 7, 12, 9, c('30a0e0')).r(3, 11, 12, 12, c('c0c8d0')).r(6, 4, 9, 5, c('8a8e9a')), 'med_bay', 'Med bay', 8, [('tranquillium_module', 1), ('pressure_fabric', 2), ('glass', 2)], 'assembly_bay', 2, light=8, bed={'speed': 3.5})
deco(P().r(4, 8, 11, 15, c('5a6270')).r(2, 5, 13, 7, c('8a8e9a')).r(6, 2, 9, 4, c('f0a030')).p(7, 1, c('ffe890')), 'heater', 'Heater', 6, [('aluminium_frame', 1), ('copper_wire', 4)], 'machine_shop', hot=True, light=9)

# ---- more that lives out there -------------------------------------------------------------------------------------------------------------------
creature('bird', 'solar_moth', 'Solar moth', 'f0d060', 'ffffff', 16, ('moth_scale', 'Solar scale'), sky(ORBIT_B, 0.004), 6)
creature('tortoise', 'hull_barnacle', 'Hull barnacle', '6a7080', 'c0c8d0', 26, ('barnacle_plate', 'Barnacle plate'), on(['debris_plating', 'hull_panel', 'hull_floor'], ORBIT_B, 0.08), 6)
creature('ape', 'glass_ape', 'Tektite ape', '2a5a4a', 'a0f0d0', 70, ('ape_glasshide', 'Glassy hide'), on(['impact_glass'], ['glass_plains'], 0.08), 8, True)
creature('quad', 'lava_tube_hound', 'Tube hound', '4a4a52', 'f0a030', 60, ('hound_pelt', 'Tube hound pelt'), on(['mare_basalt', 'dark_regolith'], ['lava_plain', 'mare'], 0.07), 7, True, tame=3, companion={'bite': 12, 'sight': 9})
creature('crane', 'ray_stalker', 'Ray stalker', 'e0e0e8', '5a5a62', 44, ('stalker_quill', 'Stalker quill'), on(['regolith'], ['ray_crater', 'highlands'], 0.06), 7)
creature('quad', 'belt_hound', 'Void hound', '3a3a5a', 'c0d0ff', 100, ('void_pelt', 'Void hound pelt'), on(['asteroid_rock', 'carbon_rock'], ['stony_belt', 'carbon_belt'], 0.06), 9, True, tame=4, companion={'bite': 24, 'sight': 10})
creature('serpent', 'ore_eel', 'Ore eel', 'c0c0c8', 'f0a030', 80, ('eel_wire', 'Eel filament'), on(['nickel_rock'], ['metal_belt'], 0.08), 9, True, [('iridium_ore', 0, 1)])
creature('crane', 'garden_heron', 'Garden heron', '40e0c0', 'c080ff', 50, ('heron_plume', 'Garden plume'), on(['garden_rock'], ['belt_garden'], 0.1), 9, False, [('life_sample', 0, 1)])
creature('golem', 'shell_warden', 'Shell warden', '3a4a4a', '60ffd0', 240, ('warden_plate', 'Warden plate'), on(['glyph_stone', 'ancient_alloy'], ['precursor_shell'], 0.06), 9, True, [('ancient_cell', 0, 1)])
creature('quad', 'salt_fox', 'Salt fox', 'f0e8e0', 'd0a080', 60, ('salt_fur', 'Salt fox fur'), on(['mars_salt_crust'], ['mars_salt_flats'], 0.1), 10, False, tame=4, companion={'bite': 14, 'sight': 9})
creature('ape', 'canyon_ape', 'Canyon ape', '9a5a3a', 'e0b080', 140, ('ape_cord', 'Canyon ape sinew'), on(['mars_rock'], ['canyon_lands'], 0.06), 10, True)
creature('crane', 'frost_wader', 'Frost wader', 'e8f0f8', '60a0e0', 70, ('wader_down', 'Wader down'), on(['polar_frost'], ['polar_cap'], 0.08), 10)
creature('stag', 'glass_elk', 'Glass elk', '6a8a9a', 'a0e0f0', 130, ('elk_antler', 'Glass antler'), on(['cave_moss_ground'], ['glass_forest'], 0.08), 10, False, mount={'speed': 1.9}, tame=5, companion={'bite': 26, 'sight': 9})
creature('lion', 'ember_lion', 'Ember lion', '5a2018', 'f07020', 300, ('ember_mane', 'Ember mane'), on(['volcanic_glass', 'mars_basalt'], ['volcanic_rise'], 0.04), 11, True)
creature('fish', 'cave_swimmer', 'Brine swimmer', '60a0a0', 'f0e060', 30, ('swimmer_meat', 'Swimmer meat'), {'water': True, 'biome': ['living_caves', 'polar_cap', 'glass_forest'], 'chance': 0.03}, 10, False, swim={}, move_chance=0.96, turn_chance=0.06, cap=8)

# ---- more that grows, and more to eat -------------------------------------------------------------------------------------------------------------
MORE_FLORA = [('crater_lichen', 'Crater lichen', 7, 'bush', 'a0a8b0', ['regolith', 'dark_regolith', 'hydroponic_tray']), ('glass_reed', 'Glass reed', 7, 'grass', '60c0a0', ['impact_glass', 'hydroponic_tray']), ('tube_cap', 'Tube cap', 7, 'mushroom', 'c0c0e0', ['mare_basalt', 'dark_regolith', 'hydroponic_tray']),
              ('comet_frost', 'Comet frost', 9, 'coral', 'c0f0ff', ['comet_ice', 'hydroponic_tray']), ('carbon_fan', 'Carbon fan', 9, 'coral', '4a4a4a', ['carbon_rock', 'hydroponic_tray']), ('salt_star', 'Salt star', 10, 'flower', 'f8f0e8', ['mars_salt_crust', 'hydroponic_tray']),
              ('dune_thorn', 'Dune thorn', 10, 'grass', 'd0a050', ['rust_sand', 'hydroponic_tray']), ('magma_bulb', 'Magma bulb', 11, 'root', 'f04020', ['volcanic_glass', 'mars_basalt', 'hydroponic_tray'])]
for n, lab, tier, arch, col, soil in MORE_FLORA: grower(n, lab, tier, arch, col, 0.08 if arch in ('mushroom', 'root') else 0, soil, 6, False)
for b_, pl in (('mare', [('tube_cap', 30)]), ('highlands', [('crater_lichen', 26)]), ('ray_crater', [('crater_lichen', 20)]), ('glass_plains', [('glass_reed', 9)]), ('lava_plain', [('tube_cap', 12)]), ('ice_belt', [('comet_frost', 8)]), ('carbon_belt', [('carbon_fan', 9)]),
               ('mars_salt_flats', [('salt_star', 10)]), ('dune_sea', [('dune_thorn', 14)]), ('volcanic_rise', [('magma_bulb', 12)])):
    next(x for x in biomes if x['name'] == b_).setdefault('plants', []).extend({'tile': t + '_0', 'one_in': n} for t, n in pl)
MEALS2 = [('lichen_crackers', 'Lichen crackers', 7, 'a0a8b0', [('crater_lichen', 3), ('dwarf_wheat', 1)], {'food': 0.4}), ('tube_cap_broth', 'Tube-cap broth', 7, 'c0c0e0', [('tube_cap', 3), ('moon_ice', 1)], {'food': 0.5, 'effect': lasts(3600, lung=2.0)}),
          ('reed_noodles', 'Glass-reed noodles', 8, '60c0a0', [('glass_reed', 3), ('soy_bean', 1)], {'food': 0.6, 'effect': lasts(2400, walk=1.2)}), ('mite_roast', 'Roast moon mite', 7, '9a9aa0', [('mite_shell', 2), ('red_potato', 1)], {'food': 0.6, 'effect': lasts(2400, armor=0.1)}),
          ('worm_jerky', 'Worm jerky', 8, '7a7a82', [('worm_segment', 2), ('salt', 1)], {'food': 0.7}), ('frost_sorbet', 'Comet sorbet', 9, 'c0f0ff', [('comet_frost', 3), ('belt_berry', 1)], {'food': 0.3, 'effect': lasts(3600, fireproof=1.0)}),
          ('carbon_toast', 'Carbon-fan toast', 9, '4a4a4a', [('carbon_fan', 2), ('yeast_loaf', 1)], {'food': 0.8, 'cures': True}), ('heron_stew', 'Garden stew', 9, '40e0c0', [('heron_plume', 1), ('void_lily', 2), ('glow_frond', 2)], {'food': 0.7, 'effect': lasts(3600, regen=5.0)}),
          ('salt_star_pickle', 'Salt-star pickle', 10, 'f8f0e8', [('salt_star', 3), ('mars_ice', 1)], {'food': 0.4, 'effect': lasts(6000, stamina=1.5)}), ('thorn_tea', 'Dune-thorn tea', 10, 'd0a050', [('dune_thorn', 3), ('mars_ice', 1)], {'cupped': True, 'effect': lasts(3600, lung=4.0, warmth=0.3)}),
          ('magma_curry', 'Magma-bulb curry', 11, 'f04020', [('magma_bulb', 2), ('grazer_meat', 2), ('mars_maize', 2)], {'food': 1.0, 'effect': lasts(6000, strength=4, fireproof=1.0)}), ('swimmer_fillet', 'Brine fillet', 10, '60a0a0', [('swimmer_meat', 2), ('salt_star', 1)], {'food': 0.8, 'effect': lasts(3600, lung=6.0)}),
          ('elk_roast', 'Glass elk roast', 10, '6a8a9a', [('elk_antler', 1), ('grazer_meat', 2), ('pale_tuber', 2)], {'food': 1.0, 'effect': lasts(6000, walk=1.3)}), ('explorer_ration', "Explorer's ration", 11, 'c070e0', [('magma_curry', 1), ('thorn_tea', 1), ('lichen_wafer', 1)], {'food': 1.0, 'effect': lasts(12000, lung=6.0, regen=6.0, strength=3)})]
for n, lab, tier, col, ins, kw in MEALS2: meal(n, lab, tier, col, ins, kw.pop('cupped', False), **kw)

# ---- more that was built out there ------------------------------------------------------------------------------------------------------------------
HUMAN.update({'A': L(None, 'hull_panel', 'server_rack'), 'I': L(None, 'hull_panel', 'cryo_pod'), 'K': L(None, None, 'landing_light'), 'U': L(None, None, 'relay_satellite'), 'X': L('bulkhead', 'bulkhead'), 'Y': L('hazard_plate', 'hazard_plate'),
              'Z': L(None, None, 'drill_derrick'), '1': L(None, None, 'rover_exhibit'), '2': L(None, 'hull_panel', 'med_bay'), '3': L(None, 'mars_brick', 'terrarium'), '4': L(None, 'mars_brick', 'bunk'), '5': L(None, 'regolith_brick', 'console'),
              '6': L('lunar_concrete', 'lunar_concrete'), '7': L(None, 'lunar_concrete'), '8': L(None, 'lunar_concrete', 'lunar_cache'), '9': L('rust_adobe', 'rust_adobe'), '0': L(None, 'rust_adobe'), '!': L(None, 'rust_adobe', 'colony_locker'), '?': L(None, None, 'emergency_beacon')})
ALIEN.update({'P': L(None, 'glyph_stone', 'glyph_pillar'), 'C': L('carved_glyph', 'carved_glyph'), 'c': L(None, 'carved_glyph'), 'T': L('alloy_tile', 'alloy_tile'), 'R': L(None, None, 'ring_monument')})
MORE_ST = {'orbit': [('relay_wreck', ['~~U~~', '~~T~~', 'ssTss', '~~~~~'], HUMAN), ('escape_pod', ['~HWH~', '~HIH~', '~HHH~', '~~~~~'], HUMAN), ('docking_ring', ['~XYYYX~', '~Y~~~Y~', '~Y~o~Y~', '~XYYYX~', '~~~~~~~'], HUMAN),
                     ('fuel_depot_wreck', ['~t~t~', '~t~t~', '~XkX~', '~YYY~', '~~~~~'], HUMAN), ('science_module', ['~HHWWHH~', '~HAhhcH~', '~Dhhhh~~', '~H2hhkH~', '~FFFFFF~', '~~~~~~~~'], HUMAN), ('old_capsule', ['~~H~~', '~HWH~', '~HoH~', '~~~~~'], HUMAN)],
           'moon': [('rover_site', ['~~~~~~?~', '~1~~f~~~', '~~~~~~~~'], HUMAN), ('lunar_bunker', ['~66666666~', '~6N7775V6~', '~E777777~~', '~EB7787B6~', '~66666666~'], HUMAN), ('ice_drill', ['~~Z~~~', '~~~~q~', '~HuH~~', '~~~~~~'], HUMAN),
                    ('landing_field', ['K~~~~~~K', '~~~i~~~~', '~~~t~~~~', '~~~e~~~~', 'YYYYYYYY'], HUMAN), ('precursor_ring', ['~~~R~~~', '~G~~~G~', '~G~K~G~', 'TTTTTTT'], ALIEN), ('glyph_gallery', ['~CCCCCCC~', '~CPcccPC~', '~~ccKcc~~', '~TTTTTTT~'], ALIEN),
                    ('spire_garden', ['X~X~X~X', 'G~G~G~G', 'TTTTTTT'], ALIEN), ('observatory_ruin', ['~~~d~~~', '~RRRRR~', '~RNr5R~', '~ErrrQ~', '~RRRRR~'], HUMAN)],
           'belt': [('refinery_wreck', ['~~T~~~T~~', '~XXXXXXX~', '~XxhAhkX~', '~Dhhhhh~~', '~YYYYYYY~', '~~~~~~~~~'], HUMAN), ('cryo_vault', ['~HHHHHHH~', '~HIhIhIH~', '~Dhhhhh~~', '~HxhhhkH~', '~FFFFFFF~', '~~~~~~~~~'], HUMAN), ('survey_buoy', ['~?~', '~T~', '~q~', '~~~'], HUMAN),
                    ('tug_wreck', ['~~HHH~~', 'eHhxhW~', '~~HHH~~', '~~~~~~~'], HUMAN), ('precursor_spindle', ['~~G~~', '~~P~~', '~~G~~', '~GKG~', '~~G~~', '~~~~~'], ALIEN), ('glyph_ring', ['~CC~CC~', '~C~~~C~', '~~~K~~~', '~C~~~C~', '~CC~CC~', '~~~~~~~'], ALIEN),
                    ('garden_shrine', ['~Y~~~Y~', '~C~R~C~', '~CcKcC~', '~TTTTT~'], ALIEN), ('prospector_camp', ['~f~~~~', '~q~HH~', '~~HxH~', '~~~~~~'], HUMAN)],
           'mars': [('adobe_hut', ['~99999~', '~9j0!9~', '~J0000~', '~J04009', '~CCCCC~'], HUMAN), ('colony_hall', ['~MMMMMMMMMMM~', '~MjnnnnnnnjM~', '~Jnnnnnnnnna~', '~J4n4n3nznnM~', '~MMMMMMMMMMM~', 'CCCCCCCCCCCCC'][:5] + ['~CCCCCCCCCCC~'], HUMAN),
                    ('landing_zone', ['K~~~~~~~K', '~~~~i~~~~', '~~~~t~~~~', '~~~~t~~~~', '~~~~e~~~~', 'YYYYYYYYY'], HUMAN), ('reactor_ruin', ['~XXXXXXX~', '~XAhhhqX~', '~Dhhhhh~~', '~XyhhhkX~', '~YYYYYYY~'], HUMAN), ('rover_graveyard', ['~1~~1~~?~', '~~~~~~~~~'], HUMAN),
                    ('sphinx', ['~~~GGG~~~~', '~~GGgGG~~~', '~GGgKgGGGG', 'GGGGGGGGGG', 'TTTTTTTTTT'], ALIEN), ('glyph_avenue', ['P~~P~~P~~P', 'G~~G~~G~~G', 'TTTTTTTTTT'], ALIEN), ('opal_garden', ['Z~~Z~~Z', 'C~R~~~C', 'CccKccC', 'TTTTTTT'], ALIEN),
                    ('great_ring', ['~~~~R~~~~', '~~G~~~G~~', '~G~~~~~G~', '~G~~K~~G~', 'TTTTTTTTT'], ALIEN), ('ice_station', ['~~d~~~~', '~HHHHH~', '~HlhvH~', '~Dhhy~~', '~FFFFF~'], HUMAN), ('lava_tube_base', ['MMMMMMMMM', 'MjnnnnnaM', 'Jnnnnnnn~', 'J4n3nznnM', 'MMMMMMMMM'], HUMAN),
                    ('monolith', ['~G~', '~G~', '~G~', '~G~', '~G~', 'TTT'], ALIEN)]}
for dim_, sts in MORE_ST.items():
    d_ = next(x for x in dimensions if x['name'] == dim_)
    for n, rows, legend in sts: structure(n, rows, legend); d_['structures'].append(n)

# ---- samples from the ground itself; seed stock; what beasts and specimens are good for -----------------------------------------------------------
for host, smp in (('regolith', 'moon_sample'), ('dark_regolith', 'moon_sample'), ('asteroid_rock', 'belt_sample'), ('carbon_rock', 'belt_sample'), ('rust_soil', 'mars_sample'), ('rust_sand', 'mars_sample'), ('cave_moss_ground', 'life_sample'), ('garden_rock', 'life_sample')):
    t_ = next(t for t in tiles if t['name'] == host)
    if 'drops' not in t_: t_['drops'] = [{'item': t_.pop('drop'), 'count': 1}]
    if len(t_['drops']) < 8: t_['drops'].insert(1, {'item': smp, 'count': 1, 'chance': 0.07})
for n, lab, tier, arch, col in FLORA: rec('sample_lab', 'life_sample', [(n, 4)], level=3, time=160)
for crop, src in (('space_lettuce', 'flax'), ('dwarf_wheat', 'wheat'), ('red_potato', 'carrot'), ('soy_bean', 'corn'), ('tank_tomato', 'tomato'), ('moon_melon', 'tomato'), ('spirulina', 'kelp'), ('ration_yeast', 'flour'), ('belt_berry', 'cherry'), ('mars_maize', 'corn')):
    g_ = GRADE_OF.get(next(x[2] for x in CROPS if x[0] == crop), 1); rec('sample_lab', crop, [(src, 4), ('compost', 1)], 2, level=g_ if g_ > 1 else None, time=160)
comp(sample(c('60a040'), c('e8ecf0')), 'biomass', 'Biomass', 7, 0.3, 'nature'); comp(sample(c('a0e040'), c('e8ecf0')), 'nutrient_gel', 'Nutrient gel', 7, 0.6, 'nature')
for m_ in mobs:
    part_ = m_['drops'][0]['item']
    if part_ in IT or any(i['name'] == part_ for i in items): rec('sample_lab', 'biomass', [(part_, 1)], 2, time=80)
rec('sample_lab', 'nutrient_gel', [('biomass', 3), ('compost', 1)], 2, time=120); also('hydroponic_tray', 'nutrient_gel'); rec('galley', 'ration_pack', [('nutrient_gel', 2), ('salt', 1)], 2, time=80)
# A specimen can be mounted and shown.
for host, n, lab, col in SPEC:
    tier = next(i['tier'] for i in items if i['name'] == n); nm = n.replace('_specimen', '_plaque')
    deco(P().r(3, 11, 12, 14, c('5a3a2a')).r(4, 10, 11, 10, c('8a6a4a')).r(5, 3, 10, 9, c(col)).r(5, 3, 10, 3, lt(c(col))).p(6, 5, WHITE).r(6, 12, 9, 12, GOLD), nm, 'Mounted ' + lab.lower(), tier, [(n, 1), ('aluminium_plate', 1)], 'sample_lab', 1)
for k, (n, lab) in enumerate(FOSSILS):
    deco(P().r(2, 4, 13, 13, c('8a6a4a')).r(3, 5, 12, 12, c('3a2a1a')).line(4, 10, 11, 6 + k % 3, c('e8dcc0')).p(5 + k, 7, c('e8dcc0')).r(6, 14, 9, 14, GOLD), n + '_mount', 'Mounted ' + lab.lower(), 10, [(n, 1), ('martian_steel_plate', 1), ('glass', 1)], 'sample_lab', 4)
# What was found can be put right: each relic restored is the better for it.
for n, lab, kind, col, tier, boons, set_ in RELICS:
    up = {}
    for k_, v_ in boons.items(): up[k_] = round(1.0 + (v_ - 1.0) * 2.0, 2) if k_ in ('lung', 'regen', 'mine', 'walk', 'craft') else (min(0.5, round(v_ * 2.0, 2)) if k_ in ('luck', 'armor', 'warmth') else v_ * 2 if k_ == 'strength' else v_)
    thing(trinket(kind, lt(c(col), 1.2), GOLD), 'restored_' + n, 'Restored ' + lab.lower(), tier, 'special', 5.0, max_stack=1, relic=True, boons=up, set='restored_' + set_)
    make('sample_lab', 'restored_' + n, [(n, 1), (TIER_METAL[tier] + '_module', 1), ({8: 'moon_crystal', 9: 'star_sapphire', 10: 'mars_opal', 11: 'ancient_cell'}[tier], 2)], GRADE_OF[tier], 1, 600, NEED_OF[tier])
for st in list(sets):
    if st['name'] in ('selenite', 'belter', 'ares', 'precursor'): sets.append({'name': 'restored_' + st['name'], 'label': st['label'] + ', restored', 'bonuses': st['bonuses']})
# Bigger tanks, a nose for the stack, a pad that takes more.
for n, lab, tier, col, band, fuel, g, ins, need in TANKS:
    rocket_tile(tank_pic(lt(c(col), 1.1), c(band)).r(0, 10, 15, 11, c(band)), 'long_' + n, 'Long ' + lab.lower(), tier, [(n, 2), (TIER_METAL[tier] + '_plate', 2), ('interstage', 1)], g, need, fuel=int(fuel * 2.3), thrust=0)

# ---- what every finished thing is built round, by its tier ----------------------------------------------------------------------------------------------
for i_ in list(items):
    n, tier = i_['name'], i_.get('tier', 0)
    if tier < 6 or n.endswith(('_module', '_assembly')): continue
    m = TIER_METAL[tier]
    if tier == 11 and i_['category'] in ('tools', 'clothing') and not i_.get('relic'): also(n, 'precursor_matrix')
    if n.endswith(('_drill', '_coilgun')): continue
    if i_['category'] == 'tools' and (n.endswith(('_pickaxe', '_axe', '_shovel', '_machete', '_blade')) or i_.get('cast') or n in ('phase_drill', 'plasma_lance', 'recall_beacon')): also(n, m + '_module')
    elif i_['category'] == 'clothing': also(n, m + '_module')
    elif i_['category'] == 'energy' and i_.get('places'): also(n, m + '_assembly')
    elif i_.get('places') and next((t for t in tiles if t['name'] == i_['places'] and (t.get('rocket') or t.get('launch') or t.get('air') or t.get('electric'))), None): also(n, m + '_assembly')

# ---- written out -----------------------------------------------------------------------------------------------------------------------------------
def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        f.write('{\n' + ',\n'.join('  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]' if isinstance(v, list) else '  "%s": %s' % (k, json.dumps(v)) for k, v in obj.items()) + '\n}\n')
for r_ in recipes:
    if r_.get('formula') is None: r_.pop('formula', None)
for kind, group in (('tile', tiles), ('item', items), ('creature', mobs)):
    names = [t['name'] for t in group]; assert len(names) == len(set(names)), (kind, sorted({n for n in names if names.count(n) > 1}))
    long_ = [t['name'] for t in group if len(('caged_' if kind == 'creature' else '') + t['name']) > 31]; assert not long_, long_
have_i = set(BI) | {i['name'] for i in items}; have_t = set(BT) | {t['name'] for t in tiles}
bad = sorted({i['item'] for r in recipes for i in r['inputs'] if i['item'] not in have_i} | {r['output']['item'] for r in recipes if r['output']['item'] not in have_i} | {r['formula'] for r in recipes if r.get('formula') and r['formula'] not in have_i})
bad += sorted({d['item'] for m in mobs for d in m['drops'] if d['item'] not in have_i} | {i['places'] for i in items if i.get('places') and i['places'] not in have_t})
bad += sorted({x['item'] for t in tiles for l in t.get('loot', []) for x in (l.get('one_of') or [l]) if x.get('item') and x['item'] not in have_i})
bad += sorted({d['item'] for t in tiles for d in t.get('drops', []) if d['item'] not in have_i} | {t['drop'] for t in tiles if t.get('drop') and t['drop'] not in have_i})
bad += sorted({v for s_ in structures for cell in s_['legend'].values() for v in cell.values() if v not in have_t} | {b[k] for b in biomes for k in ('surface', 'soil') if b[k] not in have_t})
bad += sorted({sp['tile'] for b in biomes for sp in b.get('spikes', []) if sp['tile'] not in have_t} | {p['tile'] for b in biomes for p in b.get('plants', []) if p['tile'] not in have_t} | {v['tile'] for v in veins + decor if v['tile'] not in have_t})
assert not bad, bad
for n, i in enumerate(items):
    if i.get('equip'):
        o = {}
        for k, v in i.items():
            o[k] = v
            if k == 'equip': o['worn'] = 'worn/%s.png' % i['name']
        items[n] = o
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'sets': sets, 'items': items}); dump('recipes.json', {'recipes': recipes}); dump('trees.json', {'trees': trees}); dump('mobs.json', {'mobs': mobs})
dump('worldgen.json', {'structures': structures, 'dimensions': dimensions, 'biomes': biomes, 'veins': veins, 'cave_decor': decor})
json.dump({'label': 'Beyond the Sky', 'icon': 'icon.png', 'description': 'Rockets, orbit, the Moon, the asteroid belt and the red planet: places with no air, and a great deal to find in them.'}, open(os.path.join(mod, 'mod.json'), 'w'), indent=1)
capsule_pic(c('e8ecf0'), CYAN, True).im.resize((48, 48), Image.NEAREST).save(os.path.join(mod, 'icon.png'))
# How long each making is: the fewest makings, one under another (what is dug, grown, found or dropped is none).
NATURAL = {d_['item'] for t_ in tiles for d_ in t_.get('drops', [])} | {t_['drop'] for t_ in tiles if t_.get('drop')} | {d_['item'] for m_ in mobs for d_ in m_['drops']} | {t_['fruit'] for t_ in trees} | set(BI)
NATURAL -= {i['name'] for i in items if i['category'] in ('tools', 'clothing', 'benches', 'portals', 'energy') or i.get('places')} | {e[0] for e in ELEC}
BY = {}
for r_ in recipes:
    if all(i['item'] != r_['output']['item'] for i in r_['inputs']): BY.setdefault(r_['output']['item'], []).append([i['item'] for i in r_['inputs']])  # (what must be known is not a making)
DEPTH = {}
def steps(i, seen=()):
    if i in DEPTH: return DEPTH[i]
    if i in NATURAL or i not in BY or i in seen: return 0
    d = min(1 + max((steps(x, seen + (i,)) for x in ins), default=0) for ins in BY[i])
    if not seen: DEPTH[i] = d
    return d
table = {}
for i in items:
    if i['name'] in BY and i['category'] in ('tools', 'clothing', 'portals', 'energy', 'benches', 'special') and i.get('tier', 0) >= 6 or i['name'] in [c_[0] for c_ in CAPSULES]:
        table.setdefault(IT_TIER.get(i['name'], i.get('tier', 6)) if False else i.get('tier', {'orbital_capsule': 6, 'lunar_lander': 7, 'belt_hauler': 9, 'mars_lander': 10}.get(i['name'], 6)), []).append((steps(i['name']), i['name']))
for t_ in sorted(table):
    ds = sorted(table[t_]); print('tier %d: %d finished things, makings %d to %d (middle %d); longest: %s' % (t_, len(ds), ds[0][0], ds[-1][0], ds[len(ds) // 2][0], ', '.join('%s %d' % (n, d) for d, n in ds[-3:])) + '; shortest: ' + ', '.join('%s %d' % (n, d) for d, n in ds[:3]))
print('%d tiles, %d items, %d recipes, %d trees, %d creatures, %d biomes, %d structures, %d sets' % (len(tiles), len(items), len(recipes), len(trees), len(mobs), len(biomes), len(structures), len(sets)))
tools = os.path.dirname(os.path.abspath(__file__))
subprocess.run([sys.executable, os.path.join(tools, 'worn_art.py'), repo], check=True)
subprocess.run([sys.executable, os.path.join(tools, 'caged_art.py'), repo], check=True)
subprocess.run([sys.executable, os.path.join(tools, 'stair_column_art.py'), repo], check=True)
