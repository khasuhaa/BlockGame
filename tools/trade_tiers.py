#!/usr/bin/env python3
"""Puts the game's items into the categories, tiers and order of TradeTiers.txt.

    python3 tools/trade_tiers.py .

TradeTiers.txt is written for people, so how each of its entries is read is
set down by hand in READ below: the items an entry stands for, or nothing at
all where the game has no such item. Change READ when that document changes;
an item named here that the game does not have stops the script.

It rewrites data/items.json: every item READ names gets its "category" and
"tier" and is moved to its place in the document's order (a trade portal lists
a category tier by tier, and within a tier in the order items are defined);
every other item goes to the category "special", which only the dev portal
has. Then it writes TradeObtainment.txt: what the document leaves out, and
what it names that the game has not got. Rerunning changes nothing.
"""
import json, os, sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else '.'
ITEMS = json.load(open(os.path.join(ROOT, 'data/items.json')))['items']
BY = {i['name']: i for i in ITEMS}


def cs(*names):
    """A block with its column and stairs."""
    out = []
    for n in names: out += [n, n.replace('_block', '') + '_column', n.replace('_block', '') + '_stairs']
    return out


def suit(metal):
    return [metal + '_' + p for p in ('helmet', 'chestplate', 'leggings', 'boots')]


GEMS = ['amethyst', 'sapphire', 'emerald', 'ruby', 'diamond']
# (The trains, once the game has them.)
TRAIN = [n if n in BY else None for n in ('rail', 'train_wheel', 'handcar', 'steam_locomotive', 'freight_car', 'passenger_car')]
# tier: [(as the document has it, the items that is read as)]
READ = {
    'Blocks I': [('Dirt', ['dirt']), ('Wood, Wood Column/Stairs', cs('wood')), ('Stone, Column/Stairs', cs('stone')), ('Sand', ['sand'])],
    'Blocks II': [('Limestone, Column/Stairs', cs('limestone')), ('Sandstone, Column/Stairs', cs('sandstone')), ('Marble, Column/Stairs', cs('marble')),
                  ('Brick, Column/Stairs', ['red_brick', 'brick_column', 'brick_stairs'])],
    'Blocks III': [('Gravel', ['gravel']), ('Black sand', ['black_sand']), ('Compost', ['compost']), ('Plaster, Column/Stairs', cs('plaster')),
                   ('Glass, Column/Stairs', cs('glass')), ('Black Glass, Column/Stairs', cs('black_glass')), ('Ice, Column/Stairs', cs('ice')),
                   ('Reinforced Platform', ['reinforced_platform'])],
    'Blocks IV': [('Lapis, Column/Stairs', cs('lapis_lazuli')), ('Red Marble, Column/Stairs', cs('red_marble')), ('Basalt, Column/Stairs', cs('basalt'))],
    'Blocks V': [('Copper Block, Column/Stairs', cs('copper_block')), ('Tin Block, Column/Stairs', cs('tin_block')), ('Bronze Block, Column/Stairs', cs('bronze_block'))],
    'Blocks VI': [('Iron Block, Column/Stairs', cs('iron_block')), ('Steel Block, Column/Stairs', cs('steel_block')), ('Gold Block, Column/Stairs', cs('gold_block')),
                  ('Carbon Fiber Block, Column/Stairs', cs('carbon_fiber_block')), ('Titanium Block, Column/Stairs', cs('titanium_block')), ('Platinum Block, Column/Stairs', cs('platinum_block')),
                  ('Amethyst Block, Column/Stairs', cs('amethyst_block')), ('Rest of gem blocks in order', cs(*[g + '_block' for g in GEMS[1:]])),
                  ('Luminous Plaster', ['luminous_plaster'])],
    'Nature I': [('Stick', ['stick']), ('Worm', ['worms']), ('Pinecone', ['pine_cone'])],
    'Nature II': [('Apple', ['apple']), ('Mango', ['mango']), ('Maple Seed', ['maple_seed']), ('Prickly Pear', ['prickly_pear']), ('Coconut', ['coconut']),
                  ('Flax Seed', ['flax_seeds']), ('Orange', ['orange']), ('Lime', []), ('Sunflower Seed', ['sunflower_seed']), ('Corn', ['corn']),
                  ('Carrot', ['carrot']), ('Cherry', ['cherry']), ('Chilli', ['chilli']), ('Wheat', ['wheat']), ('Tomato', ['tomato'])],
    'Nature III': [('Kelp', ['kelp']), ('Flax', ['flax']), ('Vine', ['vine']), ('Dodo Egg', ['dodo_egg']), ('Dodo Feather', ['feather']),
                   ('Raw Dodo Meat', ['dodo_meat']), ('Raw Fish', ['fish'])],
    'Nature IV': [('Leather', ['leather']), ('Fur', ['fur']), ('Coffee Cherry', ['coffee_bean']), ('Flour', ['flour']), ('Yeast', ['yeast']), ('Salt', ['salt']),
                  ('Bucket of Milk', ['milk']), ('Yak Horn', ['yak_horn']), ('Yak Shavings', ['yak_shavings'])],
    'Nature V': [('Cooked Dodo Meat', ['cooked_dodo_meat']), ('Cooked Fish', ['cooked_fish']), ('Green Coffee Bean', ['green_coffee_bean']), ('Dough', ['dough']),
                 ('Mozzarella', ['mozzarella'])],
    'Nature VI': [('Roasted Coffee Bean', ['roasted_coffee_bean']), ('Cup of Coffee', ['coffee']), ('Fish Curry', ['fish_curry']), ('Dodo Stew', ['dodo_stew']),
                  ('Poison', ['poison']), ('Bread', ['bread']), ('Flatbread', ['flatbread']), ('Pizza', ['pizza']), ('Rainbow Essence', ['rainbow_essence']),
                  ('Rainbow Cake', ['rainbow_cake'])],
    'Tools I': [('Flint toolset', ['flint_pickaxe', 'flint_axe', 'flint_shovel', 'flint_machete', 'flint_spear'])],
    'Tools II': [('Stone toolset', ['stone_pickaxe', 'stone_axe', 'stone_shovel'])],
    'Tools III': [('Bronze/Tin tools', ['bronze_pickaxe', 'bronze_axe', 'bronze_machete', 'bronze_sword', 'tin_spade']), ('copper bow', ['copper_bow'])],
    'Tools IV': [('Iron toolset', ['iron_pickaxe', 'iron_axe', 'iron_machete', 'iron_sword'])],
    'Tools V': [('Steel toolset', ['steel_pickaxe']), ('crowbar', ['crowbar']), ('razor', ['razor'])],
    'Tools VI': [('Gold/Titanium toolset', ['gold_pickaxe', 'gold_shovel', 'titanium_pickaxe', 'titanium_axe', 'titanium_sword']), ('gold bow', ['gold_bow'])],
    'Clothing I': [('Linen clothes', ['linen_cap', 'linen_brimmed_hat', 'linen_shirt', 'linen_pants'])],
    'Clothing II': [('Leather clothes', ['leather_jacket', 'leather_pants', 'leather_boots'])],
    'Clothing III': [('Fur clothes', ['fur_coat', 'fur_boots']), ('tin hat', ['tin_foil_hat'])],
    'Clothing IV': [('Tin Armor', suit('tin'))],
    'Clothing V': [('Iron Armor', suit('iron'))],
    'Clothing VI': [('Carbon Fiber Armor', suit('carbon_fiber')), ('Titanium Armor', suit('titanium'))],
    'Ores I': [('Flint', ['flint']), ('Clay', ['clay']), ('oil', ['oil'])],
    'Ores II': [('Coal', ['coal']), ('Copper Ore', ['copper_ore']), ('Tin Ore', ['tin_ore']), ('Iron Ore', ['iron_ore']), ('Amethyst Shard', ['amethyst_shard']),
                ('Amethyst', ['amethyst'])],
    'Ores III': [('Gold ore', ['gold_ore']), ('copper ingot', ['copper_ingot']), ('tin ingot', ['tin_ingot']), ('sapphire shard', ['sapphire_shard']),
                 ('sapphire', ['sapphire'])],
    'Ores IV': [('Crushed limestone', ['crushed_limestone']), ('pig iron', ['pig_iron']), ('bronze ingot', ['bronze_ingot']), ('emerald shard', ['emerald_shard']),
                ('emerald', ['emerald'])],
    'Ores V': [('Platinum Ore', ['platinum_ore']), ('Titanium Ore', ['titanium_ore']), ('iron ingot', ['iron_ingot']), ('ruby shard', ['ruby_shard']), ('ruby', ['ruby'])],
    'Ores VI': [('Steel ingot', ['steel_ingot']), ('gold ingot', ['gold_ingot']), ('silicon ingot', ['silicon_ingot']), ('silicon crystal', ['silicon_crystal']),
                ('silicon wafer', ['silicon_wafer']), ('Platinum ingot', ['platinum_ingot']), ('titanium ingot', ['titanium_ingot']),
                ('diamond shard', ['diamond_shard']), ('diamond', ['diamond'])],
    'Crafted I': [('Basket', ['basket']), ('Ember', ['ember']), ('Charcoal', ['charcoal']), ('Torch', ['torch'])],
    'Crafted II': [('Chest', ['chest']), ('door', ['wooden_door']), ('trapdoor', ['trapdoor']), ('ladder', ['ladder']), ('window', ['window']), ('shelf', ['shelf']),
                   ('sign', ['sign']), ('bed', ['wooden_bed']), ('wood gate', ['wooden_gate'])],
    'Crafted III': [('Soft bed', ['soft_bed']), ('string', ['string']), ('oil lamp', ['oil_lantern']), ('carrot on a stick', ['carrot_on_a_stick']), ('cup', ['cup']),
                    ('linen', ['linen']), ('tin bucket', ['bucket']), ('boat', ['boat']), ('camera', ['camera']), ('tin foil', ['tin_foil']),
                    ('fishing rod', ['fishing_rod']), ('cage', ['cage'])],
    'Crafted IV': [('water bucket', []), ('steel lantern', ['steel_lantern']), ('steel downlight', ['steel_downlight']), ('steel uplight', ['steel_uplight']),
                   ('iron pot', ['iron_pot']),
                   ('all pigments', ['carbon_black', 'marble_white', 'red_ochre', 'indian_yellow', 'ultramarine_blue', 'tyrian_purple', 'copper_blue', 'emerald_green']),
                   ('mirror', ['mirror'])],
    'Crafted V': [('gold chest', ['golden_chest']), ('feeeding chest', ['feeder_chest']), ('safe', ['safe']), ('iron door', ['iron_door']),
                  ('iron trapdoor', ['iron_trapdoor']), ('tinted window', ['black_window']), ('display cabinet', ['display_cabinet']), ('ice torch', [])],
    'Crafted VI': [('Chandeliers', [g + '_chandelier' for g in GEMS])],
    'Energy V': [('Copper boiler', ['copper_boiler']), ('magnet', ['magnet']), ('electric motor', ['electric_motor']), ('copper wire', ['copper_wire'])],
    'Energy VI': [('Steam engine', ['steam_engine']), ('steam generator', ['steam_generator']), ('solar panel', ['solar_panel']), ('flywheel', ['flywheel']),
                  ('elevator shaft', ['elevator_shaft']), ('elevator motor', ['electric_elevator_motor']), ('rail', TRAIN[0:1]), ('train wheel', TRAIN[1:2]),
                  ('handcar', TRAIN[2:3]), ('steam locomotive', TRAIN[3:4]), ('freight cart', TRAIN[4:5]), ('passenger car', TRAIN[5:6]), ('fuel', ['fuel']), ('epoxy', ['epoxy']), ('resin', ['raw_resin']),
                  ('carbon fiber', ['carbon_fibers']), ('carbon fiber sheets', ['carbon_fiber_sheet']), ('carbon fiber wing', ['carbon_fiber_wing']),
                  ('jetpack chassis', ['jetpack_chassis']), ('jet engine', ['jet_engine']), ('jetpack', ['jetpack'])],
}
# Where an entry was read as something whose name in the game is different, or had to be guessed at.
NOTES = [
    '"tin hat" (Clothing III) is read as the Tin foil hat; the Tin helmet is counted with the Tin Armor in Clothing IV.',
    '"Rest of gem blocks in order" (Blocks VI) is read as the Sapphire, Emerald, Ruby and Diamond blocks, each with its column and stairs.',
    '"Brick" is the item Red brick; "Lapis" is Lapis lazuli; "Worm" is Worms; "Flax Seed" is Flax seeds; "Gold ore" is the item called Gold nugget.',
    '"Bucket of Milk" is the item Milk; "Cup of Coffee" is Coffee; "Coffee Cherry" is the item named coffee_bean, labelled Coffee cherry.',
    '"door", "bed" and "wood gate" are the Wooden door, Wooden bed and Wooden gate; "oil lamp" is the Oil lantern; "Torch" is the Basic torch.',
    '"gold chest" is the Golden chest; "feeeding chest" is the Feeder chest; "tinted window" is the Black window.',
    '"elevator motor" is the Electric elevator motor; "resin" is Raw resin; "carbon fiber" is Carbon fibers.',
    '"all pigments" is the eight pigments (Carbon black, Marble white, Red ochre, Indian yellow, Ultramarine blue, Tyrian purple, Copper blue, Emerald green). '
    'Paint and Paint stripper are not pigments and are in list A.',
    'The toolsets are read as every tool of that material the game has: Flint (pickaxe, axe, spade, machete, spear); Stone (pickaxe, axe, spade); '
    'Bronze/Tin (bronze pickaxe, axe, machete and sword, tin spade); Iron (pickaxe, axe, machete, sword); Steel (pickaxe - the only steel tool); '
    'Gold/Titanium (gold pickaxe and spade, titanium pickaxe, axe and sword).',
    'The Platinum block with its column and stairs, which the document left out, is in Blocks VI after titanium (the owner said Building VI; the document has '
    'no Building, and columns and stairs are with their blocks).',
    'Everything in list A is in the category Special, which only the dev portal has.',
    'What the upgrades cost is all in the game already: the five gems, Gold and Platinum coins, and crystals.',
]

covered, missing = {}, []
for tier, entries in READ.items():
    for k, (said, names) in enumerate(entries):
        names = entries[k] = (said, [n for n in names if n])[1]
        entries[k] = (said, names)
        if not names: missing.append((tier, said))
        for n in names:
            if n not in BY: sys.exit('READ names "%s" (%s), which the game does not have' % (n, tier))
            if n in covered: sys.exit('"%s" is read into both %s and %s' % (n, covered[n], tier))
            covered[n] = tier

left = [i for i in ITEMS if i['name'] not in covered]
TILES = {t['name']: t for t in json.load(open(os.path.join(ROOT, 'data/tiles.json')))['tiles']}
KINDS = ['Portals', 'Benches', 'Caged creatures', 'Eggs', 'Clothing', 'Tools', 'Other']


def kind(i):
    """What sort of thing an item the document leaves out is, for listing."""
    if 'category' not in i and i['name'] not in covered: return None
    put = i.get('places', '')
    put = TILES.get(put if isinstance(put, str) else '', {})
    if 'portal' in i['name']: return 'Portals'
    if put.get('bench'): return 'Benches'
    if 'releases' in i or i['name'].startswith('caged_'): return 'Caged creatures'
    if i['name'].endswith('_egg'): return 'Eggs'
    if 'equip' in i: return 'Clothing'
    if 'tool' in i: return 'Tools'
    return 'Other'


# Into the data: the document's items first, in its order, then the rest as they were.
order = [n for entries in READ.values() for _, names in entries for n in names]
for n in order:
    BY[n]['category'] = covered[n].split(' ')[0].lower()
    BY[n]['tier'] = ['I', 'II', 'III', 'IV', 'V', 'VI'].index(covered[n].split(' ')[1]) + 1
for i in left:
    i.pop('tier', None)
    if 'category' in i: i['category'] = 'special'
ITEMS[:] = [BY[n] for n in order] + left
open(os.path.join(ROOT, 'data/items.json'), 'w').write('{\n  "items": [\n' + ',\n'.join('    ' + json.dumps(i) for i in ITEMS) + '\n  ]\n}\n')
out = ['TRADE OBTAINMENT', 'The game\'s items (data/items.json, the game without mods) set against TradeTiers.txt.',
       'Written by tools/trade_tiers.py; how each entry of the document was read is at the end.', '',
       '%d items in the game. %d are in the document. %d are not (list A).' % (len(ITEMS), len(covered), len(left)),
       '%d things in the document are not in the game (list B).' % len(missing), '',
       '=' * 78, 'A. IN THE GAME, NOT IN THE DOCUMENT  (%d)' % len(left), '=' * 78,
       'All of these are in the category Special, at the dev portal only. Grouped by what they are; the name the data uses',
       'follows in brackets where it differs.', '']
for cat in KINDS + [None]:
    group = [i for i in left if kind(i) == cat]
    if not group: continue
    out.append('%s  (%d)' % (cat if cat else 'No category at all: coins, and things that are never held', len(group)))
    for i in group:
        plain = i['label'].lower().replace(' ', '_').replace("'", '')
        out.append('    %s%s' % (i['label'], '' if plain == i['name'] else '  [%s]' % i['name']))
    out.append('')
out += ['=' * 78, 'B. IN THE DOCUMENT, NOT IN THE GAME  (%d)' % len(missing), '=' * 78, '']
for tier, said in missing: out.append('    %-18s %s' % (said, '(' + tier + ')'))
out += ['', '    A bucket holding water is the Tin bucket itself, which carries how full it is: there is no item', '    "water bucket" to list.',
        '    The game has a Train yard bench, and nothing yet that is made at it.', '',
        '=' * 78, 'HOW THE DOCUMENT WAS READ', '=' * 78, '']
for n in NOTES:
    words, line = n.split(' '), '  - '
    for w in words:
        if len(line) + len(w) > 96:
            out.append(line.rstrip()); line = '    '
        line += w + ' '
    out.append(line.rstrip())
out += ['', 'In the document, tier by tier (%d items):' % len(covered), '']
for tier, entries in READ.items():
    out.append('  %-13s %s' % (tier, ', '.join(BY[n]['label'] for _, names in entries for n in names)))
open(os.path.join(ROOT, 'TradeObtainment.txt'), 'w').write('\n'.join(out) + '\n')
print('%d in the game, %d in the document, %d not; %d in the document and not the game' % (len(ITEMS), len(covered), len(left), len(missing)))
