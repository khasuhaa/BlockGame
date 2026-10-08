#!/usr/bin/env python3
"""Writes the Discovery Road mod: its data files and its textures.

    python3 tools/mod_discovery_road.py .

Relics - thirty-six of them, in nine sets of four - each of which does a little for whoever carries it in a relic
slot, and more with others of its set. They are found in relic chests, each of which holds what belongs to the
place it stands in (the loot pools below); broken down to relic dust at a reliquary; and restored from dust, at
random. Ancient urns lie about underground and in the dry places, and break into dust. A relic identifier,
carried, reads off what a relic does.

Either edit this and run it again, or edit the mod's files by hand and stop using this: not both.
"""
import json, os, sys
from PIL import Image

repo = sys.argv[1] if len(sys.argv) > 1 else '.'
mod = os.path.join(repo, 'mods/discovery_road'); tex = os.path.join(mod, 'textures')
for d in ('tiles', 'items'): os.makedirs(os.path.join(tex, d), exist_ok=True)

INK = (22, 22, 30, 255)
def c(s): return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
def dk(col, f=0.7): return tuple(int(v * f) for v in col[:3]) + (255,)
def lt(col, f=1.3): return tuple(min(255, int(v * f)) for v in col[:3]) + (255,)

class P:
    def __init__(s, fill=(0, 0, 0, 0)): s.im = Image.new('RGBA', (16, 16), fill); s.px = s.im.load()
    def r(s, x0, y0, x1, y1, col):
        for y in range(max(0, y0), min(15, y1) + 1):
            for x in range(max(0, x0), min(15, x1) + 1): s.px[x, y] = col
    def p(s, x, y, col): s.r(x, y, x, y, col)
    def ring(s, x0, y0, x1, y1, col):
        s.r(x0 + 1, y0, x1 - 1, y0, col); s.r(x0 + 1, y1, x1 - 1, y1, col); s.r(x0, y0 + 1, x0, y1 - 1, col); s.r(x1, y0 + 1, x1, y1 - 1, col)
    def outline(s):
        """The dark line round an icon that the game's icons have."""
        src = s.im.copy(); q = src.load()
        for y in range(16):
            for x in range(16):
                if q[x, y][3]: continue
                if any(0 <= x + a < 16 and 0 <= y + b < 16 and q[x + a, y + b][3] for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))): s.px[x, y] = INK
        return s
    def save(s, kind, name):
        k = 3 if kind == 'items' else 2
        s.im.resize((16 * k, 16 * k), Image.NEAREST).save(os.path.join(tex, kind, name + '.png')); return s

# ---- the shapes relics come in: each drawn in a main colour and one other -------------------------------------
def compass(a, b):
    t = P(); t.r(4, 3, 11, 12, a); t.r(3, 5, 12, 10, a); t.r(5, 4, 10, 11, lt(a)); t.r(5, 6, 10, 9, c('f0ead8')); t.p(7, 5, c('f0ead8')); t.p(8, 10, c('f0ead8'))
    t.r(7, 5, 7, 7, b); t.r(8, 8, 8, 10, INK); t.p(8, 2, dk(a)); t.p(7, 2, dk(a)); return t
def flask(a, b):
    t = P(); t.r(6, 2, 9, 3, dk(b)); t.r(7, 4, 8, 5, a); t.r(5, 6, 10, 12, a); t.r(4, 8, 11, 11, a); t.r(5, 9, 10, 11, b); t.p(6, 7, lt(a)); t.p(6, 8, lt(a)); return t
def post(a, b):
    t = P(); t.r(7, 4, 8, 13, a); t.r(3, 3, 12, 6, b); t.r(3, 3, 12, 3, lt(b)); t.p(13, 4, b); t.p(13, 5, b); t.r(5, 13, 10, 13, dk(a)); t.p(5, 5, INK); t.p(7, 5, INK); t.p(9, 5, INK); return t
def horseshoe(a, b):
    t = P(); t.r(4, 3, 5, 12, a); t.r(10, 3, 11, 12, a); t.r(5, 2, 10, 3, a); t.p(4, 12, lt(a)); t.p(11, 12, lt(a))
    for y in (5, 8, 11): t.p(4, y, b); t.p(11, y, b)
    t.r(6, 2, 9, 2, lt(a)); return t
def flower(a, b):
    t = P()
    for x, y in ((7, 3), (4, 6), (10, 6), (5, 10), (9, 10)): t.r(x, y, x + 1, y + 1, a); t.p(x, y, lt(a))
    t.r(6, 5, 9, 9, a); t.r(7, 6, 8, 8, b); t.p(7, 6, lt(b)); return t
def acorn(a, b):
    t = P(); t.r(5, 4, 10, 6, b); t.r(4, 5, 11, 6, b); t.p(7, 3, dk(b)); t.p(8, 2, dk(b)); t.r(5, 7, 10, 11, a); t.r(6, 12, 9, 12, a); t.r(7, 13, 8, 13, dk(a)); t.p(6, 8, lt(a)); t.p(6, 9, lt(a)); return t
def knot(a, b):
    t = P(); t.ring(3, 4, 8, 10, a); t.ring(7, 5, 12, 11, b); t.p(7, 7, lt(a)); t.p(8, 8, lt(b)); t.r(2, 11, 3, 13, a); t.r(12, 2, 13, 4, b); return t
def idol(a, b):
    t = P(); t.r(6, 2, 9, 5, a); t.p(7, 3, b); t.p(9, 3, b); t.r(5, 6, 10, 11, a); t.r(4, 7, 4, 9, a); t.r(11, 7, 11, 9, a); t.r(6, 12, 7, 13, dk(a)); t.r(8, 12, 9, 13, dk(a)); t.r(7, 8, 8, 9, b); t.p(6, 2, lt(a)); return t
def brooch(a, b):
    t = P(); t.r(5, 4, 10, 11, a); t.r(4, 6, 11, 9, a); t.r(6, 5, 9, 10, b); t.r(7, 4, 8, 11, dk(b)); t.p(6, 6, lt(b)); t.p(3, 5, a); t.p(12, 5, a); t.p(3, 10, a); t.p(12, 10, a); t.p(7, 3, a); t.p(8, 3, a); return t
def disc(a, b):
    t = P(); t.r(5, 3, 10, 12, a); t.r(3, 5, 12, 10, a); t.r(4, 4, 11, 11, a); t.r(6, 6, 9, 9, b); t.p(6, 6, lt(b))
    for x, y in ((7, 1), (8, 14), (1, 7), (14, 8), (3, 3), (12, 12), (3, 12), (12, 3)): t.p(x, y, lt(a))
    return t
def ring(a, b):
    t = P(); t.ring(4, 5, 11, 12, a); t.ring(5, 6, 10, 11, dk(a)); t.r(6, 2, 9, 5, b); t.p(6, 2, lt(b)); t.p(7, 1, b); t.p(8, 1, b); t.p(5, 6, lt(a)); return t
def skin(a, b):
    t = P(); t.r(5, 5, 10, 12, a); t.r(4, 7, 11, 11, a); t.r(6, 3, 8, 4, dk(a)); t.r(7, 2, 9, 2, b); t.r(5, 9, 10, 9, dk(a)); t.p(6, 6, lt(a)); t.r(10, 4, 12, 5, b); return t
def pearl(a, b):
    t = P(); t.r(5, 3, 10, 12, a); t.r(3, 5, 12, 10, a); t.r(4, 4, 11, 11, a); t.r(6, 5, 7, 6, c('ffffff')); t.r(8, 9, 10, 11, b); t.p(10, 8, b); return t
def crown(a, b):
    t = P(); t.r(3, 8, 12, 11, a); t.r(3, 12, 12, 12, dk(a))
    for x in (3, 6, 9, 12): t.r(x, 4, x, 7, a); t.p(x, 3, b)
    t.p(4, 7, a); t.p(11, 7, a); t.p(7, 7, a); t.p(8, 7, a); t.p(5, 10, b); t.p(8, 10, b); t.p(10, 10, b); t.r(3, 8, 12, 8, lt(a)); return t
def coin(a, b):
    t = P(); t.r(5, 3, 10, 12, a); t.r(3, 5, 12, 10, a); t.r(4, 4, 11, 11, a); t.ring(5, 5, 10, 10, dk(a)); t.r(7, 6, 8, 9, b); t.p(6, 7, b); t.p(9, 8, b); t.p(5, 4, lt(a)); t.p(4, 5, lt(a)); return t
def feather(a, b):
    t = P()
    for i in range(9): t.r(4 + i, 11 - i, 6 + i, 12 - i, a)
    for i in range(9): t.p(5 + i, 11 - i, lt(a))
    t.r(11, 3, 13, 4, b); t.r(2, 13, 3, 14, dk(b)); t.p(4, 12, dk(b)); t.p(8, 10, b); t.p(10, 8, b); return t
def paw(a, b):
    t = P(); t.r(5, 8, 10, 13, a); t.r(6, 7, 9, 7, a)
    for x in (4, 7, 10): t.r(x, 3, x + 1, 5, a); t.p(x, 2, b)
    t.r(6, 9, 9, 11, b); t.r(6, 13, 9, 14, dk(a)); return t
def bracelet(a, b):
    t = P(); t.ring(3, 4, 12, 11, a); t.ring(4, 5, 11, 10, dk(a))
    for x, y in ((5, 4), (9, 4), (3, 7), (12, 8), (6, 11), (10, 11)): t.p(x, y, b)
    t.p(7, 3, lt(b)); t.p(8, 12, lt(b)); return t
def lantern(a, b):
    t = P(); t.r(6, 1, 9, 2, dk(a)); t.r(5, 3, 10, 4, a); t.r(5, 5, 10, 11, dk(a)); t.r(6, 5, 9, 11, b); t.r(7, 6, 8, 9, lt(b)); t.r(5, 12, 10, 13, a); t.p(5, 8, a); t.p(10, 8, a); return t
def pin(a, b):
    t = P()
    for i in range(8): t.p(4 + i, 12 - i, a); t.p(5 + i, 12 - i, dk(a))
    t.r(8, 2, 13, 3, b); t.r(11, 4, 13, 6, b); t.p(7, 3, b); t.p(13, 7, b); t.p(9, 2, lt(b)); return t
def heart(a, b):
    t = P(); t.r(3, 4, 6, 7, a); t.r(9, 4, 12, 7, a); t.r(4, 3, 5, 3, a); t.r(10, 3, 11, 3, a); t.r(3, 6, 12, 8, a); t.r(4, 9, 11, 10, a); t.r(5, 11, 10, 11, a); t.r(6, 12, 9, 12, a); t.r(7, 13, 8, 13, a)
    t.r(6, 6, 9, 9, b); t.p(4, 5, lt(a)); t.p(7, 7, lt(b)); return t
def skull(a, b):
    t = P(); t.r(4, 3, 11, 9, a); t.r(3, 5, 12, 8, a); t.r(5, 10, 10, 12, a); t.r(5, 6, 6, 8, b); t.r(9, 6, 10, 8, b); t.p(7, 9, dk(a)); t.p(8, 9, dk(a)); t.p(6, 12, INK); t.p(8, 12, INK); t.p(5, 4, lt(a)); return t
def shard(a, b):
    t = P()
    for i in range(6): t.r(7 - i // 2, 2 + i * 2, 8 + i // 3, 3 + i * 2, a)
    t.r(4, 9, 6, 12, dk(a)); t.r(9, 6, 11, 10, b); t.p(7, 3, lt(a)); t.p(7, 5, lt(a)); t.p(10, 7, lt(b)); return t
def star(a, b):
    t = P(); t.r(7, 2, 8, 13, a); t.r(2, 7, 13, 8, a); t.r(5, 5, 10, 10, a); t.r(6, 6, 9, 9, b); t.p(4, 4, a); t.p(11, 4, a); t.p(4, 11, a); t.p(11, 11, a); t.p(7, 7, c('ffffff')); return t
def chime(a, b):
    t = P(); t.r(3, 2, 12, 3, a); t.p(7, 1, dk(a)); t.p(8, 1, dk(a))
    for x, n in ((4, 7), (7, 9), (10, 6)): t.r(x, 4, x, 4, dk(a)); t.r(x, 5, x + 1, 4 + n, b); t.p(x, 5, lt(b))
    return t
def hammer(a, b):
    t = P(); t.r(3, 3, 11, 6, a); t.r(3, 3, 11, 3, lt(a)); t.r(12, 4, 12, 5, a)
    for i in range(7): t.r(7, 7 + i, 8, 7 + i, b)
    t.p(7, 13, dk(b)); t.p(8, 13, dk(b)); return t
def gear(a, b):
    t = P(); t.r(4, 4, 11, 11, a); t.r(6, 6, 9, 9, b)
    for x, y in ((7, 2), (8, 2), (7, 13), (8, 13), (2, 7), (2, 8), (13, 7), (13, 8), (3, 3), (12, 3), (3, 12), (12, 12)): t.p(x, y, a)
    t.p(7, 3, a); t.p(8, 3, a); t.p(7, 12, a); t.p(8, 12, a); t.p(3, 7, a); t.p(3, 8, a); t.p(12, 7, a); t.p(12, 8, a); t.p(5, 5, lt(a)); return t
def stone(a, b):
    t = P(); t.r(4, 5, 11, 12, a); t.r(3, 7, 12, 11, a); t.r(5, 4, 9, 4, a); t.r(6, 7, 9, 10, b); t.r(7, 8, 8, 9, lt(b)); t.p(5, 6, lt(a)); t.r(4, 12, 11, 12, dk(a)); return t

# ---- the sets ------------------------------------------------------------------------------------------------
# set: (label, [(pieces, boons)...], [relics: (name, label, shape, main, other, boons)])
SETS = {
    'wayfarer': ("Wayfarer's", [(2, {'walk': 1.05}), (4, {'walk': 1.1, 'stamina': 0.9})], [
        ('worn_compass', 'Worn compass', compass, 'b08a3a', 'c61e38', {'walk': 1.08}),
        ('wayfarers_flask', "Wayfarer's flask", flask, '8a6a4a', '4a86c8', {'appetite': 0.85}),
        ('trail_marker', 'Trail marker', post, '8c6537', 'cca876', {'stamina': 0.85}),
        ('lucky_horseshoe', 'Lucky horseshoe', horseshoe, '8a8e9a', '424955', {'luck': 0.06})]),
    'grove': ('Grove', [(2, {'regen': 1.25}), (4, {'craft': 1.15, 'warmth': 0.1})], [
        ('blossom_charm', 'Blossom charm', flower, 'f0a0c0', 'f6dc6a', {'regen': 1.5}),
        ('amber_acorn', 'Amber acorn', acorn, 'e09a2c', '7a5630', {'stamina': 0.8}),
        ('carvers_knot', "Carver's knot", knot, 'ae8249', '5a9a4a', {'craft': 1.12}),
        ('pinecone_idol', 'Pinecone idol', idol, '7a5630', 'e2c466', {'warmth': 0.15})]),
    'dune': ('Dune', [(2, {'armor': 0.04}), (4, {'thorns': 1, 'appetite': 0.85})], [
        ('scarab_brooch', 'Scarab brooch', brooch, 'd8b44a', '2a8a8a', {'armor': 0.06}),
        ('sun_disc', 'Sun disc', disc, 'e8b830', 'f07828', {'warmth': 0.2}),
        ('thorn_ring', 'Thorn ring', ring, '6a8a3a', 'c8d88a', {'thorns': 1}),
        ('nomads_waterskin', "Nomad's waterskin", skin, 'a87850', 'e8d8b0', {'appetite': 0.75})]),
    'tide': ('Tide', [(2, {'lung': 1.5}), (4, {'strength': 2, 'luck': 0.05})], [
        ('pearl_of_breath', 'Pearl of breath', pearl, 'd8e8f0', '8ab8d8', {'lung': 2.0}),
        ('coral_crown', 'Coral crown', crown, 'e86a6a', 'f0e0d0', {'regen': 1.3}),
        ('drowned_coin', 'Drowned coin', coin, '5aa890', '2a6a5a', {'luck': 0.08}),
        ('sailors_knot', "Sailor's knot", knot, 'd8c8a0', '3a6aa8', {'strength': 2})]),
    'jungle': ('Jungle', [(2, {'strength': 1}), (4, {'walk': 1.1, 'strength': 2})], [
        ('jade_idol', 'Jade idol', idol, '3aa870', 'e2c466', {'strength': 3}),
        ('feathered_charm', 'Feathered charm', feather, 'e84a4a', 'f0c830', {'walk': 1.1}),
        ('monkeys_paw', "Monkey's paw", paw, '6a4a32', 'a88868', {'luck': 0.1}),
        ('vine_bracelet', 'Vine bracelet', bracelet, '4a8a3a', 'e86a9a', {'regen': 1.4})]),
    'deep': ('Deepdelver', [(2, {'mine': 1.1}), (4, {'mine': 1.2, 'luck': 0.08})], [
        ('ember_lantern', 'Ember lantern', lantern, '6a6e7a', 'f07828', {'mine': 1.12}),
        ('prospectors_pin', "Prospector's pin", pin, '8c6537', 'b4bcc8', {'mine': 1.18}),
        ('geode_heart', 'Geode heart', heart, '6a6e7a', 'b37aec', {'armor': 0.08}),
        ('canary_feather', 'Canary feather', feather, 'f6dc4a', 'e09a2c', {'lung': 1.6})]),
    'abyss': ('Abyssal', [(2, {'armor': 0.06}), (4, {'strength': 3, 'thorns': 2})], [
        ('obsidian_skull', 'Obsidian skull', skull, '3a3440', 'c61e38', {'strength': 5}),
        ('magma_core', 'Magma core', stone, '4a2a2a', 'f07828', {'warmth': 0.4}),
        ('void_shard', 'Void shard', shard, '4a2a86', 'b08af0', {'thorns': 3}),
        ('mountain_heart', 'Heart of the mountain', heart, '8a3a3a', 'f6dc6a', {'regen': 2.0, 'armor': 0.08})]),
    'sky': ('Skyborne', [(2, {'walk': 1.06}), (4, {'walk': 1.12, 'luck': 0.08})], [
        ('cloud_feather', 'Cloud feather', feather, 'e8f0f8', 'a8c8e8', {'walk': 1.18}),
        ('star_fragment', 'Star fragment', star, 'f6e88a', 'ffffff', {'luck': 0.14}),
        ('wind_chime', 'Wind chime', chime, '8a6a4a', 'c6f1fa', {'stamina': 0.7}),
        ('sunstone', 'Sunstone', stone, 'e8a030', 'f6e88a', {'warmth': 0.3, 'regen': 1.2})]),
    'artisan': ("Artisan's", [(2, {'craft': 1.1}), (4, {'craft': 1.2, 'mine': 1.1})], [
        ('gilded_hammer', 'Gilded hammer', hammer, 'e2c466', '8c6537', {'craft': 1.25}),
        ('tinkers_gear', "Tinker's gear", gear, 'b87a3a', '424955', {'craft': 1.15}),
        ('old_coin', 'Old coin', coin, 'b8a060', '7a6a3a', {'luck': 0.05}),
        ('hearthstone', 'Hearthstone', stone, '7a7e8a', 'f07828', {'warmth': 0.15, 'appetite': 0.9})]),
}
PRICE = {'wayfarer': 2400, 'grove': 2800, 'dune': 3000, 'tide': 3200, 'jungle': 3400, 'deep': 4200, 'abyss': 6500, 'sky': 5600, 'artisan': 3000}

def of(s): return [r[0] for r in SETS[s][2]]
def loot(*groups):
    out = []
    for names, weight in groups: out += [{'item': n, 'weight': weight} for n in names]
    return out

# ---- where each is found. A chest holds what every pool its place is in has, taken together: so the everyday
# relics turn up anywhere, each place has a set that is mostly its own, and a few places have what nowhere else has.
# (Biomes of other mods are named too: with that mod off they are no matter.)
POOLS = [
    ('anywhere', 'anywhere', [], None, loot((of('artisan'), 2), (of('wayfarer'), 1))),
    ('open_country', 'the plains', ['plains', 'savanna', 'salt_flats', 'berry_meadow'], (-8, 60), loot((of('wayfarer'), 6), (of('grove'), 1))),
    ('woods', 'the woods', ['cherry_grove', 'orchard', 'pine_forest', 'maple_forest', 'birch_woods', 'mossy_hollow', 'misty_marsh'], (-8, 60),
     loot((of('grove'), 6), (of('wayfarer'), 2))),
    ('sands', 'the desert', ['desert', 'red_mesa', 'salt_flats'], (-8, 60), loot((of('dune'), 6), (of('wayfarer'), 1))),
    ('shore', 'the shore and the sea', ['beach', 'ocean', 'coral_reef', 'frozen_sea', 'seagrass_meadow'], (-45, 60), loot((of('tide'), 6), (of('dune'), 1))),
    ('jungle', 'the tropics', ['tropics', 'bamboo_grove', 'savanna'], (-8, 60), loot((of('jungle'), 6), (of('tide'), 1))),
    ('cold_country', 'the cold country', ['pine_forest', 'frozen_tundra', 'slate_highlands', 'frozen_sea'], (-8, 400),
     loot((['pinecone_idol', 'hearthstone'], 4), (['sun_disc', 'sunstone', 'magma_core'], 1))),
    ('fire_country', 'the volcanic wastes', ['volcanic_wastes'], (-40, 400), loot((of('abyss'), 3), (['sun_disc', 'ember_lantern'], 2))),
    ('under_foot', 'just under the ground', [], (-25, -9), loot((of('wayfarer'), 2), (of('deep'), 2), (of('artisan'), 1))),
    ('depths', 'the depths', [], (-110, -26), loot((of('deep'), 6), (of('artisan'), 1))),
    ('abyss', 'the very bottom of the world', [], (-400, -111), loot((of('abyss'), 6), (of('deep'), 2))),
    ('heights', 'the heights and the sky', [], (61, 400), loot((of('sky'), 6), (of('wayfarer'), 1))),
]

items, tiles, recipes = [], [], []

# Relics.
for sname, (slabel, bonuses, relics) in SETS.items():
    for name, label, shape, a, b, boons in relics:
        shape(c(a), c(b)).outline().save('items', name)
        items.append({'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'max_stack': 1, 'relic': True, 'set': sname, 'boons': boons,
                      'category': 'relics', 'price': PRICE[sname], 'tier': 6})
sets = [{'name': n, 'label': s[0], 'bonuses': [{'pieces': p, 'boons': b} for p, b in s[1]]} for n, s in SETS.items()]

# Relic dust: what a relic comes down to, and goes back up from.
t = P(); t.r(4, 10, 11, 12, c('c8b89a')); t.r(5, 8, 10, 9, c('d8c8aa')); t.r(6, 7, 8, 7, c('e8dcc0')); t.p(5, 11, c('f6e88a')); t.p(9, 9, c('f6e88a')); t.p(7, 12, c('8ab8d8')); t.p(10, 11, c('e86a9a')); t.r(4, 12, 11, 12, c('a89878'))
t.outline().save('items', 'relic_dust')
items.append({'name': 'relic_dust', 'label': 'Relic dust', 'texture': 'items/relic_dust.png', 'category': 'relics', 'price': 250, 'tier': 5})

# The relic chest: a chest of old dark wood, bound in bronze gone green, with a seal on its face.
WOOD, BRONZE, SEAL = c('4a3a52'), c('3a9a8a'), c('e2c466')
t = P(WOOD)
t.r(0, 0, 15, 0, lt(WOOD)); t.r(0, 15, 15, 15, dk(WOOD)); t.r(0, 6, 15, 7, dk(WOOD, 0.55))
for x in (0, 1, 14, 15): t.r(x, 0, x, 15, BRONZE)
t.r(0, 0, 15, 1, BRONZE); t.r(0, 14, 15, 15, dk(BRONZE)); t.r(0, 5, 15, 5, BRONZE); t.r(0, 8, 15, 8, dk(BRONZE))
for x in (4, 11): t.r(x, 2, x, 13, dk(WOOD, 0.8))
t.r(6, 5, 9, 9, SEAL); t.r(7, 6, 8, 8, c('c61e38')); t.p(6, 5, lt(SEAL)); t.r(6, 10, 9, 10, dk(SEAL)); t.p(2, 2, lt(BRONZE)); t.p(13, 2, lt(BRONZE)); t.p(2, 12, lt(BRONZE)); t.p(13, 12, lt(BRONZE))
t.save('tiles', 'relic_chest')
im = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); im.paste(Image.open(os.path.join(tex, 'tiles/relic_chest.png')), (8, 8)); im.save(os.path.join(tex, 'items/relic_chest.png'))
tiles.append({'name': 'relic_chest', 'label': 'Relic chest', 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.7, 'drop': 'relic_chest',
              'texture': 'tiles/relic_chest.png', 'storage': {'columns': 4, 'rows': 2}, 'tool': 'pickaxe', 'light': 4, 'model': 'box',
              'loot': [{'area': True, 'min': 1, 'max': 1}, {'area': True, 'min': 0, 'max': 1}, {'item': 'relic_dust', 'min': 1, 'max': 3},
                       {'item': 'gold_coin', 'min': 0, 'max': 2}]})
items.append({'name': 'relic_chest', 'label': 'Relic chest', 'places': 'relic_chest', 'container': {'slots': 8}, 'texture': 'items/relic_chest.png',
              'category': 'relics', 'price': 900, 'tier': 5})

# An ancient urn: breaks into dust, and now and then there was a coin in it.
CLAY = c('b8744a')
t = P(); t.r(6, 3, 9, 4, dk(CLAY)); t.r(5, 5, 10, 6, CLAY); t.r(4, 7, 11, 12, CLAY); t.r(5, 13, 10, 14, dk(CLAY)); t.r(4, 9, 11, 9, c('e8d8b0')); t.r(4, 10, 11, 10, c('3a6aa8'))
t.p(5, 7, lt(CLAY)); t.p(5, 8, lt(CLAY)); t.p(9, 6, INK); t.p(10, 7, INK); t.p(10, 8, INK)
t.save('tiles', 'ancient_urn')
tiles.append({'name': 'ancient_urn', 'label': 'Ancient urn', 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.15, 'texture': 'tiles/ancient_urn.png',
              'tool': 'pickaxe', 'drops': [{'item': 'relic_dust', 'count': 1}, {'item': 'relic_dust', 'count': 1, 'chance': 0.4}, {'item': 'gold_coin', 'count': 1, 'chance': 0.15}]})

# The reliquary: a stone table under a glass dome, where relics are broken down and restored.
t = P(); STONE, GLASS = c('6e717c'), c('a8d8e8')
t.r(1, 9, 14, 10, lt(STONE, 1.2)); t.r(1, 11, 14, 11, dk(STONE)); t.r(2, 12, 4, 15, STONE); t.r(11, 12, 13, 15, STONE); t.r(6, 12, 9, 13, dk(STONE))
t.ring(4, 2, 11, 9, GLASS); t.r(5, 2, 10, 2, lt(GLASS, 1.1)); t.r(7, 5, 8, 7, SEAL); t.p(7, 4, c('c61e38')); t.p(6, 3, c('ffffff')); t.r(5, 8, 10, 8, dk(STONE))
t.save('tiles', 'reliquary')
t.outline().save('items', 'reliquary')
tiles.append({'name': 'reliquary', 'label': 'Reliquary', 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.4, 'drop': 'reliquary',
              'texture': 'tiles/reliquary.png', 'bench': True, 'bench_kind': 'reliquary', 'bench_level': 1, 'popup_name': 'reliquary', 'tool': 'pickaxe'})
items.append({'name': 'reliquary', 'label': 'Reliquary', 'places': 'reliquary', 'texture': 'items/reliquary.png', 'category': 'special', 'price': 600})

# The relic identifier: a lens in a brass frame. Carried, the cursor over a relic says what it does, and what its set does.
t = P(); t.r(4, 2, 9, 7, c('c8a040')); t.r(3, 3, 10, 6, c('c8a040')); t.r(5, 3, 8, 6, c('a8d8e8')); t.r(4, 4, 9, 5, c('a8d8e8')); t.p(5, 3, c('ffffff')); t.p(6, 4, c('ffffff'))
for i in range(5): t.r(9 + i, 8 + i, 10 + i, 9 + i, c('8c6537'))
t.p(9, 7, c('c8a040')); t.p(8, 5, c('f6e88a'))
t.outline().save('items', 'relic_identifier')
items.append({'name': 'relic_identifier', 'label': 'Relic identifier', 'texture': 'items/relic_identifier.png', 'max_stack': 1, 'identifies': True,
              'category': 'relics', 'price': 800, 'tier': 5})

def rec(bench, out, n, ins, time, level=1):
    return {'bench': bench, 'level': level, 'output': out if isinstance(out, dict) else {'item': out, 'count': n}, 'inputs': [{'item': a, 'count': b} for a, b in ins], 'time': time}
recipes.append(rec('workbench', 'reliquary', 1, [('stone', 6), ('glass', 2), ('gold_ingot', 1)], 90, level=2))
# Any relic breaks down to dust; dust, and a gem to bind it, makes a relic again - which, there is no saying.
every = [r[0] for s in SETS.values() for r in s[2]]
for name in every: recipes.append(rec('reliquary', 'relic_dust', 3, [(name, 1)], 10))
recipes.append(rec('reliquary', {'one_of': [{'item': n, 'weight': 3 if n in of('wayfarer') + of('artisan') else 1 if n in of('abyss') + of('sky') else 2} for n in every], 'count': 1},
                   1, [('relic_dust', 12), ('amethyst', 1)], 60))
recipes.append(rec('reliquary', 'relic_identifier', 1, [('glass', 2), ('gold_ingot', 1), ('relic_dust', 4)], 60))
recipes.append(rec('reliquary', 'relic_chest', 1, [('chest', 1), ('relic_dust', 4), ('bronze_ingot', 2)], 60))

worldgen = {
    'loot_pools': [dict({'name': n, 'label': l, 'loot': lo}, **({'biomes': b} if b else {}), **({'y': list(y)} if y else {})) for n, l, b, y, lo in POOLS],
    # Chests in the caves at every depth, and now and then one out on the ground; urns in the caves and the dry places.
    'cave_decor': [{'name': 'relic_chest', 'tile': 'relic_chest', 'per_100_columns': 1.5, 'depth': [3, 190]},
                   {'name': 'ancient_urn', 'tile': 'ancient_urn', 'per_100_columns': 5, 'depth': [2, 190]}],
    'biome_plants': [{'biome': b, 'tile': 'relic_chest', 'one_in': 260} for b in ('plains', 'desert', 'cherry_grove', 'orchard', 'tropics', 'pine_forest', 'maple_forest', 'beach')] +
                    [{'biome': b, 'tile': 'ancient_urn', 'one_in': 70} for b in ('desert', 'beach')],
}

def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        if len(obj) == 1 and isinstance(next(iter(obj.values())), list):
            k, v = next(iter(obj.items()))
            f.write('{\n  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]\n}\n')
        else: json.dump(obj, f, indent=1); f.write('\n')
dump('tiles.json', {'tiles': tiles}); dump('recipes.json', {'recipes': recipes}); dump('worldgen.json', worldgen)
dump('items.json', {'items': items, 'sets': sets})
dump('trade.json', {'categories': [{'name': 'relics', 'label': 'Relics'}]})
dump('mod.json', {'label': 'Discovery Road', 'icon': 'icon.png'})
# Its icon: a road going off to a chest under a star.
t = P(c('2a3a5a')); t.r(0, 11, 15, 15, c('3a6a4a'))
for i in range(6): t.r(7 - i // 2, 15 - i, 8 + i // 2 - (1 if i > 3 else 0), 15 - i, c('c8b89a'))
t.r(6, 7, 10, 9, WOOD); t.r(6, 7, 10, 7, BRONZE); t.p(8, 8, SEAL); t.p(12, 2, c('f6e88a')); t.p(12, 1, c('f6e88a')); t.p(11, 2, c('f6e88a')); t.p(13, 2, c('f6e88a')); t.p(12, 3, c('f6e88a')); t.p(3, 4, c('ffffff')); t.p(6, 2, c('ffffff'))
t.im.resize((64, 64), Image.NEAREST).save(os.path.join(mod, 'icon.png'))
print(len(tiles), 'tiles', len(items), 'items', len(recipes), 'recipes', len(sets), 'sets', len(POOLS), 'loot pools')
