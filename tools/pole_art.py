"""Draws the poles and the pole armour into data/textures. Run with the repository root as the argument."""
import sys, os
from PIL import Image
repo = sys.argv[1]; tex = os.path.join(repo, 'data/textures')
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
WHITE=(244,244,240,255); RED=(214,50,60,255); BLUE=(60,110,220,255); GREEN=(70,160,80,255); GOLD=(230,180,50,255); WOOD=(150,104,60,255); TAN=(206,170,110,255)
def cane(stripe):
    """A candy cane: a white post six wide with slanting stripes, which runs on from one block to the next."""
    t = P()
    for y in range(16):
        for x in range(5, 11):
            c = stripe if ((x + y) // 4) % 2 == 0 else WHITE
            t.p(x, y, lt(c, 1.1) if x == 5 else dk(c, 0.82) if x == 10 else c)
    return t
def tropical(trunk, band, leaf, flower):
    """A carved post: a trunk with bright bands, a frond either side and a flower."""
    t = P(); t.r(5, 0, 10, 15, trunk); t.r(5, 0, 5, 15, lt(trunk, 1.15)); t.r(10, 0, 10, 15, dk(trunk, 0.8))
    for y in (2, 10): t.r(5, y, 10, y + 1, band); t.p(5, y, lt(band)); t.p(10, y + 1, dk(band))
    t.r(6, 6, 9, 7, dk(trunk, 0.7)); t.p(7, 6, flower); t.p(8, 7, flower)
    for (x, y, d) in ((4, 13, -1), (11, 5, 1)):
        for k in range(4): t.p(x + d * k, y - k // 2, leaf); t.p(x + d * k, y - k // 2 + 1, dk(leaf))
    return t
def sparkle(t):
    for x, y in ((1, 2), (14, 4), (2, 13), (13, 12)): t.p(x, y, (255, 250, 200, 255))
    return t
def north_hat():
    t = P(); t.r(4, 6, 11, 10, RED); t.r(5, 4, 9, 5, RED); t.r(7, 2, 9, 3, RED); t.r(10, 2, 11, 3, WHITE); t.r(3, 11, 12, 12, WHITE); t.r(4, 6, 5, 7, lt(RED)); t.r(10, 8, 11, 10, dk(RED)); return t
def pole_boots():
    t = P()
    for x0 in (2, 9): t.r(x0, 4, x0 + 3, 12, BLUE); t.r(x0, 11, x0 + 5, 13, BLUE); t.r(x0, 4, x0 + 3, 5, WHITE); t.r(x0, 13, x0 + 5, 13, dk(BLUE)); t.p(x0, 6, lt(BLUE)); t.p(x0 + 1, 8, WHITE)
    return t
def grass_skirt():
    t = P(); t.r(3, 3, 12, 4, WOOD); t.r(7, 3, 8, 4, (240, 110, 150, 255))
    for x in range(3, 13): t.r(x, 5, x, 12 + (x % 3), GREEN if x % 2 else (140, 200, 90, 255))
    return t
def cornucopia():
    t = P(); t.r(2, 9, 4, 10, WOOD); t.r(4, 7, 8, 11, TAN); t.r(8, 5, 12, 12, TAN); t.r(8, 5, 12, 5, lt(TAN)); t.r(6, 7, 6, 11, WOOD); t.r(10, 5, 10, 12, WOOD)
    t.r(11, 6, 13, 8, RED); t.r(12, 9, 14, 11, GOLD); t.r(10, 9, 11, 10, (120, 60, 140, 255)); t.p(12, 5, GREEN); return t
cane(RED).save('tiles', 'north_pole'); cane(BLUE).save('tiles', 'south_pole')
tropical(WOOD, (240, 150, 50, 255), GREEN, (240, 90, 140, 255)).save('tiles', 'east_pole')
tropical(TAN, (60, 180, 190, 255), (120, 190, 80, 255), (250, 210, 70, 255)).save('tiles', 'west_pole')
for name, art in (('north_hat', north_hat), ('pole_boots', pole_boots), ('grass_skirt', grass_skirt), ('cornucopia_hat', cornucopia)):
    art().save('items', name); sparkle(art()).save('tiles', name + '_afloat')
print('done')
