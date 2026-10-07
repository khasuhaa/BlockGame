#!/usr/bin/env python3
"""Draws the character's skin and what each piece of clothing looks like worn.

    python3 tools/worn_art.py .

A skin is 64 by 32: each part of the body is a box, opened out flat (see
src/playermodel.c). The base skin goes to data/textures/player/base.png. Every
item with an "equip" slot, in the game and in each mod, gets a picture of the
same shape in textures/worn/, coloured from its icon, and a "worn" field naming
it. Run again after adding clothing; it draws everything afresh.
"""
import glob, json, os, re, sys
from PIL import Image

ROOT = sys.argv[1] if len(sys.argv) > 1 else '.'
W, H = 64, 32
# name: corner on the skin, then width, height and depth of the box
BOXES = {'head': (0, 0, 8, 8, 8), 'leg': (0, 16, 4, 12, 4), 'body': (16, 16, 8, 12, 4), 'arm': (40, 16, 4, 12, 4)}


def faces(part):
    u, v, w, h, d = BOXES[part]
    return {'right': (u, v + d, d, h), 'front': (u + d, v + d, w, h), 'left': (u + d + w, v + d, d, h),
            'back': (u + d + w + d, v + d, w, h), 'top': (u + d, v, w, d), 'under': (u + d + w, v, w, d)}


def speck(x, y):
    return ((x * 73856093) ^ (y * 19349663)) >> 3 & 7


def shade(c, by):
    return tuple(max(0, min(255, int(k * by))) for k in c[:3]) + (255,)


def rough(c, x, y):
    """A flat colour with the odd lighter or darker pixel in it, as the tiles have."""
    s = speck(x, y)
    return shade(c, 1.08 if s == 0 else 0.92 if s == 1 else 1.0)


def band(im, part, r0, r1, colour, sides=('right', 'front', 'left', 'back'), edge=None):
    """Rows r0 to r1 round the sides of a part; `colour` may be a function of (column, row)."""
    for side in sides:
        x, y, w, h = faces(part)[side]
        for r in range(max(r0, 0), min(r1, h)):
            for c in range(w):
                k = colour(c, r) if callable(colour) else colour
                if k is None: continue
                if edge and r == r1 - 1: k = edge
                im.putpixel((x + c, y + r), rough(k, x + c, y + r))


def lid(im, part, colour, face='top'):
    x, y, w, h = faces(part)[face]
    for r in range(h):
        for c in range(w):
            k = colour(c, r) if callable(colour) else colour
            if k is not None: im.putpixel((x + c, y + r), rough(k, x + c, y + r))


def base_skin():
    skin, hair, white, shorts, eye = (240, 200, 160), (90, 60, 40), (236, 236, 228), (70, 96, 150), (40, 40, 60)
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    for part in BOXES:
        band(im, part, 0, 12, skin)
        lid(im, part, skin)
        lid(im, part, shade(skin, 0.9), 'under')
    # Hair: over the top, down the back, and a fringe.
    lid(im, 'head', hair)
    band(im, 'head', 0, 6, hair, sides=('back',))
    band(im, 'head', 0, 3, hair, sides=('left', 'right'))
    band(im, 'head', 0, 2, hair, sides=('front',))
    fx, fy = faces('head')['front'][:2]
    for c in (1, 5):
        im.putpixel((fx + c, fy + 4), (250, 250, 250, 255))
        im.putpixel((fx + c + 1, fy + 4), eye + (255,))
    for c in (3, 4): im.putpixel((fx + c, fy + 6), shade(skin, 0.72))
    # An undershirt, cut low at the neck and with no sleeves.
    band(im, 'body', 0, 12, white, edge=shade(white, 0.85))
    lid(im, 'body', lambda c, r: white if c in (0, 1, 6, 7) else None)
    bx, by = faces('body')['front'][:2]
    for c, r in ((3, 0), (4, 0), (2, 0), (5, 0), (3, 1), (4, 1)): im.putpixel((bx + c, by + r), skin + (255,))
    band(im, 'arm', 0, 1, lambda c, r: None)
    # Shorts.
    band(im, 'leg', 0, 5, shorts, edge=shade(shorts, 0.8))
    lid(im, 'leg', shorts)
    return im


def tones(icon):
    """Dark, middle and light of an icon, and its two commonest colours that differ."""
    px = [p for p in Image.open(icon).convert('RGBA').getdata() if p[3] > 200]
    lum = lambda p: p[0] * 0.3 + p[1] * 0.59 + p[2] * 0.11
    body = sorted((p for p in px if lum(p) > 45), key=lum) or sorted(px, key=lum)
    pick = lambda f: body[min(len(body) - 1, int(len(body) * f))][:3]
    count = {}
    for p in body: count[p[:3]] = count.get(p[:3], 0) + 1
    common = sorted(count, key=lambda k: -count[k])
    first = common[0]
    far = lambda a, b: sum(abs(x - y) for x, y in zip(a, b))
    second = next((k for k in common[1:] if far(k, first) > 150), pick(0.85))
    return pick(0.2), pick(0.5), pick(0.85), first, second


def worn(name, slot, icon):
    dark, mid, light, one, two = tones(icon)
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    has = lambda *words: any(w in name for w in words)
    stripes = lambda c, r: one if (c + r) // 2 % 2 == 0 else two
    if slot == 'head':
        if has('north'):
            # A striped stocking cap with a pale band round the brow.
            lid(im, 'head', stripes)
            band(im, 'head', 0, 3, stripes)
            band(im, 'head', 2, 3, light)
        elif has('cornucopia'):
            # A woven horn, with the fruit showing along its rim.
            weave = lambda c, r: mid if (c + r) % 2 == 0 else dark
            lid(im, 'head', weave)
            band(im, 'head', 0, 3, weave)
            band(im, 'head', 2, 3, lambda c, r: two if c % 3 == 0 else one if c % 3 == 1 else light)
        elif has('foil'):
            lid(im, 'head', lambda c, r: light if (c + r) % 3 == 0 else mid)
            band(im, 'head', 0, 2, lambda c, r: light if (c * 2 + r) % 3 == 0 else mid, edge=dark)
        elif has('cap', 'hat'):
            # Cloth: the crown, and a darker brim along the front.
            lid(im, 'head', mid)
            band(im, 'head', 0, 3, mid, edge=dark)
            if has('brim'): band(im, 'head', 2, 4, dark)
        else:
            # A helmet: all but the face, with a guard down over the nose.
            lid(im, 'head', lambda c, r: light if 2 <= c <= 5 and 2 <= r <= 5 else mid)
            band(im, 'head', 0, 8, mid, sides=('left', 'right', 'back'), edge=dark)
            band(im, 'head', 0, 3, mid, sides=('front',), edge=dark)
            band(im, 'head', 3, 8, lambda c, r: mid if c in (0, 7) else None, sides=('front',))
            band(im, 'head', 3, 5, lambda c, r: dark if c in (3, 4) else None, sides=('front',))
    elif slot == 'chest':
        if has('plate'):
            # A breastplate and a guard on each shoulder.
            band(im, 'body', 0, 12, lambda c, r: light if r in (2, 3) and 1 <= c <= 6 else mid, edge=dark)
            lid(im, 'body', lambda c, r: mid if c in (0, 1, 6, 7) else None)
            band(im, 'arm', 0, 5, mid, edge=dark)
            lid(im, 'arm', light)
        elif has('shirt'):
            band(im, 'body', 0, 12, mid, edge=dark)
            lid(im, 'body', lambda c, r: mid if c in (0, 1, 6, 7) else None)
            band(im, 'body', 0, 1, lambda c, r: None if c in (3, 4) else dark, sides=('front',))
            band(im, 'arm', 0, 5, mid, edge=dark)
            lid(im, 'arm', mid)
        else:
            # A coat: sleeves to the wrist, done up down the front.
            trim = light if has('fur') else dark
            band(im, 'body', 0, 12, mid, edge=trim)
            lid(im, 'body', lambda c, r: trim if c in (0, 1, 6, 7) else None)
            band(im, 'body', 0, 12, lambda c, r: dark if c == 4 else trim if r == 0 else None, sides=('front',))
            band(im, 'arm', 0, 10, mid, edge=trim)
            lid(im, 'arm', mid)
    elif slot == 'legs':
        if has('skirt'):
            # Strands hanging from the waist, no two the same length.
            band(im, 'leg', 0, 1, dark)
            band(im, 'leg', 1, 8, lambda c, r: None if r > 4 + speck(c * 5, c) % 3 else (one if c % 2 == 0 else two))
            lid(im, 'leg', dark)
        elif has('leggings'):
            band(im, 'leg', 0, 9, lambda c, r: light if r in (5, 6) and c in (1, 2) else mid, edge=dark)
            lid(im, 'leg', mid)
        else:
            band(im, 'leg', 0, 11, mid, edge=dark)
            lid(im, 'leg', mid)
    else:
        if has('pole'):
            band(im, 'leg', 7, 12, stripes)
            lid(im, 'leg', one, 'under')
        else:
            band(im, 'leg', 8, 12, mid)
            band(im, 'leg', 8, 9, light if has('fur') else dark)
            band(im, 'leg', 11, 12, dark)
            lid(im, 'leg', dark, 'under')
    return im


def save(im, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.save(path)


save(base_skin(), os.path.join(ROOT, 'data/textures/player/base.png'))
total = 0
for home in ['data'] + sorted(glob.glob(os.path.join(ROOT, 'mods/*/items.json'))):
    home = os.path.join(ROOT, 'data') if home == 'data' else os.path.dirname(home)
    path = os.path.join(home, 'items.json')
    lines = open(path).read().split('\n')
    for i, line in enumerate(lines):
        if '"equip"' not in line: continue
        item = json.loads(line.strip().rstrip(','))
        # (A mod's item may be drawn from an icon the game has.)
        icon = os.path.join(home, 'textures', item['texture'])
        if not os.path.exists(icon): icon = os.path.join(ROOT, 'data/textures', item['texture'])
        file = 'worn/%s.png' % item['name']
        save(worn(item['name'], item['equip'], icon), os.path.join(home, 'textures', file))
        line = re.sub(r', "worn": "[^"]*"', '', line)
        lines[i] = re.sub(r'("equip": "[^"]*")', r'\1, "worn": "%s"' % file, line)
        total += 1
    open(path, 'w').write('\n'.join(lines))
print('drew the skin and %d things to wear' % total)
