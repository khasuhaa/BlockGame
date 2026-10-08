#!/usr/bin/env python3
"""Gives every creature a cage: for each one in the game and in each mod that has no "caged" item, adds the
item (caged_<name>, in the category "creatures"), names it on the creature, and draws its icon - the creature
behind the bars the game's own caged creatures are behind.

    python3 tools/caged_art.py .

Run it after adding a creature, and after any mod's generator (which writes its creatures without one).
Run again with nothing new, it changes nothing.
"""
import glob, json, os, re, sys
from PIL import Image

ROOT = sys.argv[1] if len(sys.argv) > 1 else '.'
DATA = os.path.join(ROOT, 'data')
BARS = {(96, 98, 112, 255), (150, 152, 166, 255), (70, 72, 84, 255)}


def bars():
    """The cage by itself: what is in front of the dodo in the game's caged dodo."""
    src = Image.open(os.path.join(DATA, 'textures/items/caged_dodo.png')).convert('RGBA')
    out = Image.new('RGBA', src.size, (0, 0, 0, 0))
    for y in range(src.size[1]):
        for x in range(src.size[0]):
            p = src.getpixel((x, y))
            if p in BARS and (x % 6 in (0, 1) or y in (4, 5, 42, 43)) and 4 <= y <= 43: out.putpixel((x, y), p)
    return out


def icon(mob_png, out_png, cage):
    mob = Image.open(mob_png).convert('RGBA')
    if mob.size != (32, 32):
        k = min(32 / mob.size[0], 32 / mob.size[1])
        mob = mob.resize((max(1, int(mob.size[0] * k)), max(1, int(mob.size[1] * k))), Image.NEAREST)
    im = Image.new('RGBA', (48, 48), (0, 0, 0, 0))
    im.paste(mob, (8 + (32 - mob.size[0]) // 2, 10 + (32 - mob.size[1])), mob)
    im.alpha_composite(cage)
    os.makedirs(os.path.dirname(out_png), exist_ok=True)
    im.save(out_png)


cage, total = bars(), 0
for home in [DATA] + sorted(glob.glob(os.path.join(ROOT, 'mods/*'))):
    mobs_path, items_path = os.path.join(home, 'mobs.json'), os.path.join(home, 'items.json')
    if not os.path.exists(mobs_path) or not os.path.exists(items_path): continue
    lines, added = open(mobs_path).read().split('\n'), []
    for n, line in enumerate(lines):
        if not line.strip().startswith('{"name"'): continue
        mob = json.loads(line.strip().rstrip(','))
        if mob.get('caged'): continue
        name = 'caged_' + mob['name']
        png = os.path.join(home, 'textures', mob['texture'])
        if not os.path.exists(png): png = os.path.join(DATA, 'textures', mob['texture'])
        icon(png, os.path.join(home, 'textures/items', name + '.png'), cage)
        # What it costs goes by how hard the creature is to come by: its health, and how seldom it turns up.
        rare = mob.get('spawn', {}).get('chance', 0.1)
        price = int(round((1000 + 50 * mob.get('health', 5)) * (1 if rare >= 0.1 else 2 if rare >= 0.01 else 4), -1))
        added.append({'name': name, 'label': 'Caged ' + mob['label'][0].lower() + mob['label'][1:], 'texture': 'items/%s.png' % name,
                      'category': 'creatures', 'max_stack': 1, 'price': price})
        mob['caged'] = name
        lines[n] = re.match(r'\s*', line).group(0) + json.dumps(mob) + (',' if line.rstrip().endswith(',') else '')
    if not added: continue
    open(mobs_path, 'w').write('\n'.join(lines))
    text = open(items_path).read()
    end = text.rindex('}', 0, text.rindex(']')) + 1  # the end of the last item there is
    open(items_path, 'w').write(text[:end] + ''.join(',\n    ' + json.dumps(i) for i in added) + text[end:])
    total += len(added)
print('caged %d more kinds of creature' % total)
