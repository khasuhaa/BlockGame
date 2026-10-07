#!/usr/bin/env python3
"""Draws every portal: the blocks in the world and the icons in the inventory.

    python3 tools/portal_art.py .

A portal is an arch of stone with a spiral turning in it and a stone set in the
keystone that tells its tier, stood on a stepped foot. It is drawn 16 pixels to
the block and written out double, as all the blocks are; the icon is the whole
of it drawn small, three times over. Rerunning draws the same again.
"""
import math, os, sys
from PIL import Image

ROOT = sys.argv[1] if len(sys.argv) > 1 else '.'
TILES = os.path.join(ROOT, 'data/textures/tiles')
ITEMS = os.path.join(ROOT, 'data/textures/items')
INK = (22, 22, 30)

h = lambda s: tuple(int(s[i:i + 2], 16) for i in (0, 2, 4))
# frame: dark, middle, light, brightest. swirl: deep, dark, middle, light.
STONE = [h('585a64'), h('6e717c'), h('868a96'), h('a3a7b2')]
GOLD = [h('6e5420'), h('957430'), h('bd993f'), h('e2c466')]
BLUE = [h('20226e'), h('303295'), h('403fbd'), h('6d66e2')]
TEAL = [h('14405c'), h('1f6890'), h('3a9cc8'), h('8ad8f4')]
VIOLET = [h('2b1750'), h('4a2a86'), h('7848c4'), h('b08af0')]
GREEN = [h('103f26'), h('1c6a3c'), h('34a060'), h('86e4a6')]
RED = [h('3f101a'), h('6a1b32'), h('a03353'), h('e486a7')]
# the stone in the keystone, by tier: bright and shaded
GEMS = [(h('c0c2cd'), h('9597a3')), (h('b37aec'), h('8846c6')), (h('5a90f2'), h('2c5fd6')),
        (h('4dd682'), h('1da557')), (h('ee4e64'), h('c61e38')), (h('c6f1fa'), h('8cd6e9'))]


def swirl(x, y, cx, cy, tones):
    """A two-armed spiral about (cx, cy): which of the swirl's tones a pixel is."""
    dx, dy = x + 0.5 - cx, (y + 0.5 - cy) * 0.75
    r = math.hypot(dx, dy)
    if r < 1.2: return tones[3]
    v = (math.atan2(dy, dx) / math.pi + r * 0.22) % 1.0
    return tones[2] if v < 0.2 else tones[1] if v < 0.55 else tones[0] if v < 0.85 else tones[1]


def gate(frame, tones, gem, tall, foot):
    """The whole portal, 16 wide and `tall` blocks high. `foot` is 'plinth' for a block of its own under the
    arch, or 'sill' for an arch that runs to the ground with a step across the bottom."""
    H = 16 * tall
    im = Image.new('RGBA', (16, H), (0, 0, 0, 0))
    put = lambda x, y, c: im.putpixel((x, y), tuple(c) + (255,))
    bottom = H - 16 if foot == 'plinth' else H - 2          # where the opening ends
    cy = (3 + bottom) / 2.0
    # How far in from each side the stone comes at each row of the arch's head.
    outer = {1: 5, 2: 3, 3: 2}
    inner = {1: 16, 2: 16, 3: 6, 4: 5, 5: 4}
    for y in range(1, bottom):
        lo, hi = outer.get(y, 1), inner.get(y, 3)
        for x in range(lo, 16 - lo):
            if x < hi or x >= 16 - hi:
                edge = x == lo or x == 15 - lo
                c = frame[0] if edge and x >= 8 else frame[2] if edge else frame[1]
                if y in outer and not edge and x in (hi - 1, 16 - hi): c = frame[0]
                put(x, y, c)
            else:
                put(x, y, swirl(x, y, 8, cy, tones))
    # The arch's top edge catches the light; its joints, and a light let into each post.
    for x in range(5, 11): put(x, 1, frame[3])
    for y in range(8, bottom, 6):
        put(1, y, frame[0]); put(2, y, frame[0]); put(13, y, frame[0]); put(14, y, frame[0])
    for y in range(11, bottom - 2, 6):
        put(2, y, tones[3]); put(13, y, tones[3])
    # Sparks in the swirl.
    for x, y in ((5, 9), (10, 6), (9, cy + 5), (6, cy + 8), (10, bottom - 4), (5, bottom - 7)):
        y = int(y)
        if 6 <= y < bottom - 1: put(x, y, (255, 255, 255))
    # The keystone and its stone.
    for x in (6, 7, 8, 9):
        put(x, 0, INK)
    put(7, 0, gem[0]); put(8, 0, gem[1]); put(7, 1, gem[0]); put(8, 1, gem[1]); put(6, 1, INK); put(9, 1, INK)
    put(7, 2, INK); put(8, 2, INK)
    if foot == 'plinth':
        # A slab under the posts, a block under that with the swirl's light in a slot, and a wider foot.
        for x in range(16):
            put(x, bottom, frame[3] if x not in (0, 15) else frame[2]); put(x, bottom + 1, frame[2]); put(x, bottom + 2, frame[0])
        for y in range(bottom + 3, H - 3):
            for x in range(2, 14): put(x, y, frame[0] if x == 13 else frame[2] if x == 2 else frame[1])
        for x in range(5, 11):
            put(x, bottom + 6, INK); put(x, bottom + 7, tones[2] if x in (5, 10) else tones[3]); put(x, bottom + 8, tones[1])
        for x in range(16):
            put(x, H - 3, frame[2]); put(x, H - 2, frame[1]); put(x, H - 1, frame[0])
    else:
        for x in range(1, 15):
            put(x, bottom, frame[3]); put(x, bottom + 1, frame[0])
    return im


def save_tile(im, top, name):
    im.crop((0, top, 16, top + 16)).resize((32, 32), Image.NEAREST).save(os.path.join(TILES, name))


def icon(frame, tones, gem, foot):
    """The portal small, on a 16 by 16 icon."""
    im = Image.new('RGBA', (16, 16), (0, 0, 0, 0))
    put = lambda x, y, c: im.putpixel((x, y), tuple(c) + (255,))
    bottom = 12 if foot == 'plinth' else 14
    outer = {1: 6, 2: 4}
    inner = {1: 16, 2: 16, 3: 6}
    for y in range(1, bottom):
        lo, hi = outer.get(y, 3), inner.get(y, 5)
        for x in range(lo, 16 - lo):
            if x < hi or x >= 16 - hi: put(x, y, frame[2] if x == lo else frame[0] if x == 15 - lo else frame[1])
            else: put(x, y, swirl(x, y, 8, (3 + bottom) / 2.0, tones))
    put(7, 0, gem[0]); put(8, 0, gem[1]); put(7, 1, gem[0]); put(8, 1, gem[1])
    put(6, 6, (255, 255, 255)); put(9, bottom - 3, (255, 255, 255))
    for x in range(2, 14):
        put(x, bottom, frame[3])
        if foot == 'plinth':
            put(x, bottom + 2, frame[1]); put(x, bottom + 1, frame[1] if 3 <= x <= 12 else frame[2])
        else:
            put(x, bottom + 1, frame[0])
    if foot == 'plinth':
        for x in range(6, 10): put(x, bottom + 1, tones[3])
        for x in range(2, 14): put(x, 15, frame[0])
    # An outline, as the other icons have.
    out = im.copy()
    for y in range(16):
        for x in range(16):
            if im.getpixel((x, y))[3]: continue
            if any(0 <= x + a < 16 and 0 <= y + b < 16 and im.getpixel((x + a, y + b))[3] for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out.putpixel((x, y), INK + (255,))
    return out.resize((48, 48), Image.NEAREST)


# The portals a player makes, tier by tier: three blocks, the lowest its foot.
for tier in range(1, 7):
    whole = gate(STONE, TEAL, GEMS[tier - 1], 3, 'plinth')
    save_tile(whole, 0, 'portal_top_%d.png' % tier)
    if tier == 1:
        save_tile(whole, 16, 'portal_body.png')
        save_tile(whole, 32, 'portal_plinth.png')
    icon(STONE, TEAL, GEMS[tier - 1], 'plinth').save(os.path.join(ITEMS, 'portal.png' if tier == 1 else 'portal_%d.png' % tier))
    # The one the world begins at: two blocks, stood on its base.
    whole = gate(STONE, VIOLET, GEMS[tier - 1], 2, 'sill')
    save_tile(whole, 0, 'spawn_portal_top_%d.png' % tier)
    if tier == 1: save_tile(whole, 16, 'spawn_portal.png')

# What the first portal stands on: dressed stone, with its light let into the top.
base = Image.new('RGBA', (16, 16), STONE[1] + (255,))
for x in range(16):
    base.putpixel((x, 0), STONE[3] + (255,)); base.putpixel((x, 15), STONE[0] + (255,))
    base.putpixel((x, 5), STONE[0] + (255,)); base.putpixel((x, 10), STONE[0] + (255,))
for y, cols in ((range(1, 5), (5, 10)), (range(6, 10), (2, 8, 13)), (range(11, 15), (5, 10))):
    for yy in y:
        for x in cols: base.putpixel((x, yy), STONE[0] + (255,))
for x in range(6, 10):
    base.putpixel((x, 1), INK + (255,)); base.putpixel((x, 2), VIOLET[3] + (255,)); base.putpixel((x, 3), VIOLET[2] + (255,))
for y in range(16): base.putpixel((0, y), STONE[2] + (255,)) if y not in (5, 10, 15) else None
base.resize((32, 32), Image.NEAREST).save(os.path.join(TILES, 'portal_base.png'))

# The ones to trade at.
for name, frame, tones, gem in (('trade_portal', GOLD, GREEN, (h('ffe9a0'), GOLD[3])), ('dev_portal', BLUE, RED, (h('ffc0d4'), RED[3]))):
    whole = gate(frame, tones, gem, 3, 'plinth')
    save_tile(whole, 0, name + '_top.png')
    save_tile(whole, 16, name + '_body.png')
    save_tile(whole, 32, name + '_plinth.png')
    icon(frame, tones, gem, 'plinth').save(os.path.join(ITEMS, name + '.png'))
print('drew the portals')
