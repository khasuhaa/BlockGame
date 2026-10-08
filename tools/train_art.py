#!/usr/bin/env python3
"""Draws the railway: the track, the station, what runs on the rails, and their icons.

    python3 tools/train_art.py .

Everything is drawn 16 pixels to the block and written out double (three times
over for the icons), as the rest of the art is. A vehicle is three blocks long
and two tall (the handcar one by one); (all of them); the bottom five rows of each are left
clear, so that its wheels stand on the rail and not on the ground under it.
The openings in the cab and the carriage are clear too: whoever is riding is
drawn first and seen through them. Rerunning draws the same again.
"""
import os, sys
from PIL import Image

ROOT = sys.argv[1] if len(sys.argv) > 1 else '.'
TEX = os.path.join(ROOT, 'data/textures')
h = lambda s: tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
INK, IRON, DARK, STEEL, MID, LIGHT = h('16161e'), h('282830'), h('424955'), h('78869c'), h('5d6879'), h('b4bcc8')
WOOD, WOOD_D, WOOD_L, PLANK = h('ae8249'), h('6a4827'), h('c99f63'), h('8c6537')
RED, RED_D, BRASS, GLOW = h('c61e38'), h('8a1528'), h('e2c466'), h('f07828')
BOARD, BOARD_D, BOARD_L = h('b08a5a'), h('7a5630'), h('cca876')
CLEAR = (0, 0, 0, 0)


class Pic:
    def __init__(self, w, hgt):
        self.im = Image.new('RGBA', (w, hgt), CLEAR)
        self.w, self.h = w, hgt

    def px(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h: self.im.putpixel((x, y), c)

    def box(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1): self.px(x, y, c)

    def frame(self, x0, y0, x1, y1, fill, edge=INK):
        self.box(x0, y0, x1, y1, edge)
        self.box(x0 + 1, y0 + 1, x1 - 1, y1 - 1, fill)

    def wheel(self, cx, bottom, r, rim=IRON, hub=RED):
        """A wheel standing on row `bottom`, 2r+1 across."""
        cy = bottom - r
        for y in range(-r, r + 1):
            for x in range(-r, r + 1):
                d = x * x + y * y
                if d <= r * r + 1: self.px(cx + x, cy + y, INK if d > (r - 1) * (r - 1) else rim)
        self.px(cx, cy, hub)
        if r >= 3:
            for k in (-1, 1): self.px(cx + k, cy, STEEL); self.px(cx, cy + k, STEEL)

    def save(self, path, times=2):
        path = os.path.join(TEX, path)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        self.im.resize((self.w * times, self.h * times), Image.NEAREST).save(path)


def rail():
    p = Pic(16, 16)
    for x in (1, 6, 11):                       # sleepers
        p.box(x, 13, x + 3, 15, WOOD_D); p.box(x, 13, x + 3, 13, PLANK)
    p.box(0, 11, 15, 11, LIGHT); p.box(0, 12, 15, 12, STEEL); p.box(0, 13, 15, 13, MID) if False else None
    for x in range(16):
        if p.im.getpixel((x, 13)) == CLEAR: p.px(x, 13, MID)
    return p


def ramp():
    """Track climbing a block as it goes right: a block wide and two tall, on a trestle."""
    p = Pic(16, 32)
    for x in range(16):
        top = 27 - x
        # the trestle under it
        if x % 5 == 2:
            p.box(x, top + 3, x + 1, 31, WOOD_D)
            p.box(x, top + 3, x, 31, PLANK)
        p.px(x, top, LIGHT); p.px(x, top + 1, STEEL); p.px(x, top + 2, MID)
        if x % 5 in (0, 1): p.px(x, top + 3, WOOD_D)
    for y in (24, 30):
        for x in range(16):
            if y > 27 - x + 3 and p.im.getpixel((x, y)) == CLEAR: p.px(x, y, WOOD_D)
    return p


def station():
    """The board a station is named on, three blocks wide, on two posts."""
    p = Pic(48, 16)
    for x in (8, 38): p.box(x, 9, x + 1, 15, BOARD_D)
    p.frame(3, 0, 44, 9, BOARD)
    p.box(4, 1, 43, 1, BOARD_L)
    p.box(4, 8, 43, 8, BOARD_D)
    for x in (5, 42): p.px(x, 2, STEEL); p.px(x, 7, STEEL)
    return p


BASE = 26  # the row the wheels of a three-block vehicle stand on


def locomotive():
    p = Pic(48, 32)
    # frame and wheels
    p.box(3, 20, 44, 21, RED_D); p.box(3, 20, 44, 20, RED)
    for cx in (9, 21, 31): p.wheel(cx, BASE, 4)
    p.wheel(41, BASE, 2)
    p.box(9, 23, 31, 23, STEEL)                 # the rod between the driving wheels
    # boiler
    p.frame(16, 9, 43, 19, IRON)
    p.box(17, 10, 42, 10, DARK)
    for x in (22, 29, 36): p.box(x, 9, x, 19, BRASS)
    p.box(44, 11, 45, 18, DARK); p.px(45, 14, GLOW)   # the smokebox door and its lamp
    # stack and dome
    p.frame(36, 2, 40, 9, IRON); p.box(35, 1, 41, 2, INK); p.box(36, 1, 40, 1, DARK)
    p.frame(26, 6, 30, 9, BRASS)
    # cab, with an opening to see the driver through
    p.frame(1, 3, 16, 19, RED)
    p.box(0, 2, 17, 3, INK); p.box(1, 2, 16, 2, DARK)
    p.box(4, 6, 13, 13, INK); p.box(5, 7, 12, 12, CLEAR)
    p.box(2, 16, 15, 16, RED_D)
    # cowcatcher
    for i in range(4): p.box(44 + i, 22 + i, 44 + i, BASE, DARK)
    return p


def passenger():
    p = Pic(48, 32)
    p.box(2, 20, 45, 21, DARK)
    for cx in (8, 14, 33, 39): p.wheel(cx, BASE, 2)
    p.frame(1, 4, 46, 19, h('2c5fd6'))
    p.box(0, 2, 47, 4, INK); p.box(1, 2, 46, 3, DARK); p.box(1, 2, 46, 2, MID)
    p.box(2, 15, 45, 15, BRASS)
    p.box(2, 16, 45, 18, h('1f4aa8'))
    for x0 in (3, 34):                           # a window at each seat
        p.box(x0, 6, x0 + 10, 13, INK); p.box(x0 + 1, 7, x0 + 9, 12, CLEAR)
    p.frame(19, 7, 28, 19, h('1f4aa8'))          # the door between them
    p.box(21, 9, 26, 12, LIGHT); p.px(27, 14, BRASS)
    p.box(0, 18, 0, 20, STEEL); p.box(47, 18, 47, 20, STEEL)   # couplings
    return p


def freight():
    p = Pic(48, 32)
    p.box(2, 20, 45, 21, DARK)
    for cx in (8, 14, 33, 39): p.wheel(cx, BASE, 2)
    p.frame(1, 8, 46, 19, WOOD)
    for x in range(6, 46, 6): p.box(x, 9, x, 18, WOOD_D)
    p.box(2, 9, 45, 9, WOOD_L)
    for x in (1, 16, 31, 46): p.box(x, 7, x, 19, STEEL)
    p.box(1, 13, 46, 13, MID)
    p.box(0, 18, 0, 20, STEEL); p.box(47, 18, 47, 20, STEEL)
    return p


def handcar():
    """A deck on four wheels, three blocks long, with the pump in the middle: whoever works it stands at one end."""
    p = Pic(48, 32)
    for cx in (7, 15, 32, 40): p.wheel(cx, BASE, 3)
    p.box(2, 19, 45, 21, WOOD_D); p.box(2, 19, 45, 19, WOOD_L); p.box(2, 20, 45, 20, WOOD)
    p.box(1, 20, 1, 22, STEEL); p.box(46, 20, 46, 22, STEEL)
    for x in range(8, 46, 7): p.px(x, 20, WOOD_D)
    # the post, the beam that is pumped up and down across it, and a handle at each end of that
    p.frame(22, 8, 26, 19, DARK); p.box(23, 9, 23, 18, MID)
    p.box(21, 17, 27, 19, IRON)
    for x in range(12, 37): p.px(x, 6 + (x - 12) // 6, STEEL); p.px(x, 7 + (x - 12) // 6, MID)
    p.box(11, 4, 12, 9, IRON); p.box(36, 8, 37, 13, IRON)
    p.px(24, 8, BRASS); p.px(24, 9, BRASS)
    return p


def icon(p, grow=1):
    """A picture fitted onto a 16 by 16 icon, with the outline the icons have."""
    im = p.im
    if im.width > 16 or im.height > 16:
        box = im.getbbox()
        im = im.crop(box)
        k = max(im.width, im.height) / 14.0
        im = im.resize((max(1, round(im.width / k)), max(1, round(im.height / k))), Image.NEAREST)
    out = Image.new('RGBA', (16, 16), CLEAR)
    out.paste(im, ((16 - im.width) // 2, (16 - im.height) // 2))
    edged = out.copy()
    for y in range(16):
        for x in range(16):
            if out.getpixel((x, y))[3]: continue
            if any(0 <= x + a < 16 and 0 <= y + b < 16 and out.getpixel((x + a, y + b))[3] for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                edged.putpixel((x, y), INK)
    q = Pic(16, 16)
    q.im = edged
    return q


def small(what):
    """The big things again, drawn for an icon rather than shrunk to one."""
    p = Pic(16, 16)
    if what == 'station':
        for x in (4, 11): p.box(x, 9, x, 13, BOARD_D)
        p.box(1, 3, 14, 8, BOARD); p.box(1, 3, 14, 3, BOARD_L); p.box(1, 8, 14, 8, BOARD_D)
        p.box(4, 5, 11, 5, BOARD_D); p.box(5, 6, 10, 6, BOARD_D)
        return p
    if what == 'handcar':
        for cx in (3, 11): p.box(cx, 10, cx + 2, 12, IRON); p.px(cx + 1, 11, RED)
        p.box(1, 8, 14, 9, WOOD); p.box(1, 8, 14, 8, WOOD_L)
        p.box(7, 3, 8, 7, DARK)
        for x in range(3, 13): p.px(x, 2 + (x - 3) // 4, STEEL)
        p.box(3, 1, 3, 4, IRON); p.box(12, 3, 12, 6, IRON)
        return p
    for cx in ((3, 7, 11) if what == 'engine' else (3, 5, 10, 12)):
        p.px(cx, 12, IRON); p.px(cx, 13, IRON); p.px(cx + 1, 12, IRON); p.px(cx + 1, 13, IRON); p.px(cx, 12, RED)
    p.box(1, 11, 14, 11, DARK)
    if what == 'engine':
        p.box(1, 3, 5, 10, RED); p.box(2, 5, 4, 7, LIGHT); p.box(1, 2, 6, 2, DARK)
        p.box(6, 6, 13, 10, IRON); p.box(6, 6, 13, 6, DARK); p.px(8, 6, BRASS); p.px(8, 7, BRASS); p.px(8, 8, BRASS); p.px(8, 9, BRASS); p.px(8, 10, BRASS)
        p.box(11, 2, 12, 5, IRON); p.box(10, 2, 13, 2, DARK); p.px(14, 8, GLOW)
        p.box(1, 11, 14, 11, RED_D)
    elif what == 'carriage':
        p.box(1, 4, 14, 10, h('2c5fd6')); p.box(1, 3, 14, 3, DARK); p.box(1, 9, 14, 9, BRASS)
        for x in (2, 6, 10): p.box(x, 5, x + 2, 7, LIGHT)
    else:
        p.box(1, 5, 14, 10, WOOD); p.box(1, 5, 14, 5, WOOD_L)
        for x in (1, 5, 10, 14): p.box(x, 5, x, 10, STEEL if x in (1, 14) else WOOD_D)
        p.box(1, 8, 14, 8, MID)
    return p


def wheel_icon():
    p = Pic(16, 16)
    for y in range(16):
        for x in range(16):
            d = (x - 7.5) ** 2 + (y - 7.5) ** 2
            if d <= 49: p.px(x, y, INK if d > 36 else IRON if d > 20 else CLEAR)
    for k in range(3, 13):
        p.px(k, 7, STEEL); p.px(k, 8, STEEL); p.px(7, k, STEEL); p.px(8, k, STEEL)
    p.box(6, 6, 9, 9, RED); p.box(7, 7, 8, 8, BRASS)
    return p


rail().save('tiles/rail.png')
ramp().save('tiles/rail_ramp.png')
station().save('tiles/train_station.png')
locomotive().save('vehicles/steam_locomotive.png')
passenger().save('vehicles/passenger_car.png')
freight().save('vehicles/freight_car.png')
handcar().save('vehicles/handcar.png')
icon(rail()).save('items/rail.png', 3)
wheel_icon().save('items/train_wheel.png', 3)
icon(small('handcar')).save('items/handcar.png', 3)
icon(small('engine')).save('items/steam_locomotive.png', 3)
icon(small('carriage')).save('items/passenger_car.png', 3)
icon(small('wagon')).save('items/freight_car.png', 3)
icon(small('station')).save('items/train_station.png', 3)
print('drew the railway')
