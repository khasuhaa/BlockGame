"""Writes the Culinary Delights mod: its data files and its textures. Run with the repository root as the argument."""
import sys, os, json, copy
from PIL import Image
repo = sys.argv[1]; mod = os.path.join(repo, 'mods/culinary_delights'); tex = os.path.join(mod, 'textures')
for d in ('tiles', 'items'): os.makedirs(os.path.join(tex, d), exist_ok=True)
base = os.path.join(repo, 'data')
def load(n): return json.load(open(os.path.join(base, n)))
T = {t['name']: t for t in load('tiles.json')['tiles']}
I = {t['name']: t for t in load('items.json')['items']}
TR = {t['name']: t for t in load('trees.json')['trees']}

# ---- drawing: sixteen pixels square, drawn double for blocks and treble for items
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
def block_icon(name):
    im = Image.new('RGBA', (48, 48), (0, 0, 0, 0)); im.paste(Image.open(os.path.join(tex, 'tiles', name + '.png')).convert('RGBA').crop((0, 0, 32, 32)), (8, 8))
    im.save(os.path.join(tex, 'items', name + '.png'))
def recolour(src, name, f):
    im = Image.open(os.path.join(base, 'textures', src)).convert('RGBA'); px = im.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, b, a = px[x, y]
            if a: px[x, y] = f(r, g, b) + (a,)
    im.save(os.path.join(tex, 'tiles', name + '.png'))

GREEN=(84,160,70,255); DGREEN=(52,110,50,255); LGREEN=(140,200,96,255); WOODL=(201,159,99,255); WOOD=(174,130,73,255); WOODM=(140,101,55,255); WOODD=(106,72,39,255)
STONE=(134,138,150,255); STONED=(88,90,100,255); IRON=(149,151,163,255); IRONL=(192,194,205,255); IROND=(107,109,120,255); K=(22,22,30,255)
WHITE=(236,236,230,255); CREAM=(240,226,190,255); CRUST=(204,150,84,255); GLASS=(190,225,235,170); GLASSE=(150,190,205,255); PLATE=(226,226,220,255); PLATED=(176,176,172,255)
GOLD=(215,159,29,255); FIRE=(240,140,40,255); HOT=(250,210,90,255); BRICK=(181,94,65,255); BRICKD=(111,52,35,255)

# ---- pictures of things to eat
def produce(shape, c, leaf=GREEN):
    t=P()
    if shape == 'round':
        t.r(4,5,11,12,c); t.r(5,4,10,13,c); t.r(5,5,6,6,lt(c,1.4)); t.r(9,11,10,12,dk(c)); t.r(7,2,8,3,WOODM); t.r(9,2,10,3,leaf)
    elif shape == 'berry':
        t.r(5,6,10,12,c); t.r(6,13,9,13,c); t.r(4,7,11,10,c); t.r(6,7,7,8,lt(c,1.4)); t.r(5,4,10,5,leaf); t.r(7,3,8,3,leaf)
        for x,y in ((6,10),(9,9),(8,12)): t.p(x,y,lt(c,1.5))
    elif shape == 'cluster':
        for x,y in ((5,5),(8,5),(11,5),(6,8),(9,8),(8,11)): t.r(x-1,y-1,x+1,y+1,c); t.p(x-1,y-1,lt(c,1.4))
        t.r(7,1,8,3,WOODM); t.r(9,2,11,3,leaf)
    elif shape == 'long':
        for i in range(10): t.r(2+i,11-i,4+i,13-i,c)
        for i in range(8): t.p(3+i,11-i,lt(c,1.3))
        t.r(12,1,13,2,leaf)
    elif shape == 'root':
        t.r(6,5,10,8,c); t.r(7,9,9,11,c); t.r(8,12,8,14,dk(c)); t.r(6,5,7,6,lt(c,1.3)); t.r(7,1,7,4,leaf); t.r(9,2,9,4,leaf); t.r(5,3,5,4,leaf)
    elif shape == 'bulb':
        t.r(5,7,10,12,c); t.r(4,8,11,11,c); t.r(6,13,9,13,dk(c)); t.r(7,4,8,6,c); t.r(7,2,8,3,leaf); t.r(6,8,6,10,lt(c,1.2)); t.r(9,8,9,12,dk(c,0.9))
    elif shape == 'leaf':
        t.r(4,4,11,12,c); t.r(3,6,12,10,c); t.r(6,3,9,13,c); t.r(7,4,8,13,dk(c)); t.r(5,6,6,7,lt(c,1.3)); t.r(10,9,11,10,dk(c,0.85)); t.r(7,13,8,14,dk(c,0.6))
    elif shape == 'grain':
        for x in (5,8,11):
            t.r(x,7,x,14,WOODL)
            for y in (2,4,6): t.r(x-1,y,x+1,y+1,c); t.p(x-1,y,lt(c,1.3))
        t.r(4,11,12,11,WOODM)
    elif shape == 'pod':
        for i in range(9): t.r(3+i,10-i//2,4+i,12-i//2,c)
        for x,y in ((5,10),(8,9),(11,7)): t.p(x,y,lt(c,1.4))
        t.r(12,4,13,5,leaf)
    elif shape == 'nut':
        t.r(5,5,10,12,c); t.r(4,7,11,10,c); t.r(7,5,8,12,dk(c)); t.r(5,6,6,7,lt(c,1.3)); t.r(6,4,9,4,dk(c,0.6))
    elif shape == 'gourd':
        t.r(3,6,12,12,c); t.r(4,5,11,13,c); t.r(6,5,6,13,dk(c)); t.r(9,5,9,13,dk(c)); t.r(4,7,4,9,lt(c,1.3)); t.r(7,2,8,4,DGREEN)
    elif shape == 'cap':
        t.r(4,4,11,7,c); t.r(3,6,12,8,c); t.r(5,3,10,3,lt(c)); t.r(6,9,9,13,CREAM); t.r(9,9,9,13,dk(CREAM)); t.p(5,5,WHITE); t.p(9,6,WHITE)
    return t
def jar(c, lid=WOODM):
    t=P(); t.r(5,2,10,3,lid); t.r(4,4,11,13,c); t.r(5,14,10,14,dk(c)); t.r(4,4,11,4,GLASSE); t.r(4,5,4,13,GLASSE); t.r(11,5,11,13,GLASSE); t.r(6,6,6,9,lt(c,1.5)); return t
def bottle(c):
    t=P(); t.r(7,1,8,4,GLASSE); t.r(7,0,8,0,WOODM); t.r(5,6,10,14,c); t.r(6,5,9,5,c); t.r(5,6,5,14,GLASSE); t.r(10,6,10,14,dk(c)); t.r(6,8,6,11,lt(c,1.5)); return t
def cup(c):
    t=P(); t.r(4,6,10,13,PLATE); t.r(5,14,9,14,PLATED); t.r(11,8,12,11,PLATE); t.p(11,9,(0,0,0,0)); t.p(11,10,(0,0,0,0)); t.r(5,6,9,7,c); t.r(10,7,10,13,PLATED); t.p(6,3,WHITE); t.p(8,4,WHITE); t.p(7,2,WHITE); return t
def bowl(c, top=None):
    t=P(); t.r(2,8,13,10,PLATE); t.r(3,11,12,12,PLATE); t.r(5,13,10,13,PLATED); t.r(3,7,12,8,c); t.r(13,9,13,10,PLATED); t.r(4,7,5,7,lt(c,1.3))
    if top: t.r(6,6,7,7,top); t.r(9,7,10,7,top)
    return t
def plate(a, b=None, c=None):
    t=P(); t.r(1,10,14,11,PLATE); t.r(3,12,12,12,PLATED); t.r(3,6,12,9,a); t.r(4,5,11,5,a); t.r(4,6,5,6,lt(a,1.3))
    if b: t.r(9,6,12,9,b)
    if c: t.r(6,7,7,8,c); t.p(10,5,c)
    return t
def pie(crust, fill):
    t=P(); t.r(2,8,13,12,crust); t.r(3,13,12,13,dk(crust)); t.r(3,6,12,8,fill); t.r(4,5,11,5,fill); t.r(2,8,13,8,lt(crust)); t.r(5,6,6,6,lt(fill,1.3)); t.p(8,6,crust); t.p(10,7,crust); return t
def cake(body, icing, dot=None):
    t=P(); t.r(3,6,12,13,body); t.r(3,9,12,9,icing); t.r(3,4,12,6,icing); t.r(4,7,4,7,icing); t.r(8,7,8,8,icing); t.r(11,7,11,7,icing); t.r(12,7,12,13,dk(body)); t.r(2,14,13,14,PLATE)
    if dot: t.r(7,2,8,3,dot)
    return t
def loaf(c, mark=None):
    t=P(); t.r(2,7,13,12,c); t.r(3,6,12,13,c); t.r(3,6,12,6,lt(c)); t.r(3,13,12,13,dk(c)); t.r(13,8,13,12,dk(c))
    for x in (5,8,11): t.r(x,7,x,8,mark if mark else dk(c,0.85))
    return t
def pile(c):
    t=P(); t.r(4,10,11,13,c); t.r(5,8,10,9,c); t.r(6,6,9,7,c); t.r(7,5,8,5,lt(c)); t.r(5,9,6,9,lt(c,1.2)); t.r(3,13,12,13,dk(c)); t.r(9,10,11,12,dk(c,0.88)); return t
def block(c, mark=None):
    t=P(); t.r(3,6,12,12,c); t.r(3,6,12,6,lt(c)); t.r(12,7,12,12,dk(c)); t.r(3,12,12,12,dk(c))
    if mark: t.r(5,8,6,9,mark); t.r(9,10,10,10,mark)
    return t
def platter(cs):
    t=P(); t.r(0,11,15,12,GOLD); t.r(2,13,13,13,dk(GOLD)); t.r(0,11,15,11,lt(GOLD))
    spots=((1,7,5,10),(6,6,9,10),(10,7,14,10),(4,4,7,6),(9,4,12,6))
    for i,c in enumerate(cs[:5]): x0,y0,x1,y1=spots[i]; t.r(x0,y0,x1,y1,c); t.r(x0,y0,x0+1,y0,lt(c,1.3))
    return t
def basket(cs):
    t=P(); t.r(2,8,13,13,WOOD); t.r(2,8,13,8,WOODL); t.r(2,13,13,13,WOODD)
    for x in (4,7,10): t.r(x,9,x,12,WOODM)
    t.r(3,3,3,7,WOODM); t.r(12,3,12,7,WOODM); t.r(4,2,11,2,WOODM)
    for i,c in enumerate(cs[:3]): t.r(4+3*i,5,6+3*i,7,c); t.p(4+3*i,5,lt(c,1.3))
    return t

tiles=[]; items=[]; recipes=[]; trees=[]
PRICE={k: v.get('price', 10) for k, v in I.items()}
def item(name, label, cat, picture, food=None, price=None, **kw):
    picture.save('items', name)
    d={'name': name, 'label': label, 'texture': 'items/%s.png' % name, 'category': cat, 'price': price if price else 10}
    if food is not None: d['food']=food
    d.update(kw); items.append(d); PRICE[name]=d['price']; return d
def rec(bench, out, n, ins, time, level=1):
    d={'bench': bench, 'output': {'item': out, 'count': n}, 'inputs': [{'item': a, 'count': b} for a, b in ins], 'time': time}
    if level > 1: d['level']=level
    assert len(ins) <= 5, out
    recipes.append(d)

# ---- crops: (name, label, tall, shape of what it bears, its colour, its leaves, food, what is eaten is sold as)
R=(214,58,58,255); PURPLE=(120,70,160,255); BLUE=(70,90,190,255); PINK=(226,90,130,255); YEL=(236,204,80,255); ORANGE=(232,140,50,255); TAN=(206,176,120,255); BROWN=(140,96,60,255)
CROPS=[
 ('strawberry','Strawberry',1,'berry',R,GREEN,0.08), ('blueberry','Blueberry',1,'cluster',BLUE,DGREEN,0.06), ('raspberry','Raspberry',1,'cluster',PINK,GREEN,0.06),
 ('garlic','Garlic',1,'bulb',CREAM,LGREEN,0.03), ('onion','Onion',1,'bulb',(190,110,150,255),LGREEN,0.05), ('potato','Potato',1,'nut',TAN,DGREEN,0.08),
 ('cabbage','Cabbage',1,'leaf',(150,205,130,255),GREEN,0.08), ('lettuce','Lettuce',1,'leaf',LGREEN,LGREEN,0.06), ('bell_pepper','Bell pepper',1,'round',(230,190,50,255),DGREEN,0.08),
 ('pumpkin','Pumpkin',1,'gourd',ORANGE,GREEN,0.12), ('melon','Melon',1,'gourd',(120,190,100,255),DGREEN,0.12), ('peanut','Peanut',1,'nut',(214,180,120,255),LGREEN,0.05),
 ('ginger','Ginger',1,'root',(220,190,130,255),LGREEN,0.03), ('basil','Basil',1,'leaf',(70,150,70,255),(70,150,70,255),0.02), ('mint','Mint',1,'leaf',(110,210,150,255),(110,210,150,255),0.02),
 ('soybean','Soybean',1,'pod',(170,200,90,255),GREEN,None), ('cucumber','Cucumber',1,'long',(80,160,80,255),LGREEN,0.06), ('eggplant','Eggplant',1,'long',PURPLE,DGREEN,0.06),
 ('beetroot','Beetroot',1,'root',(160,40,90,255),(150,60,90,255),0.06), ('spinach','Spinach',1,'leaf',DGREEN,DGREEN,0.05), ('sugar_beet','Sugar beet',1,'root',(236,226,206,255),GREEN,None),
 ('mushroom','Mushroom',1,'cap',(190,90,70,255),(180,160,140,255),0.06),
 ('rice','Rice',2,'grain',CREAM,LGREEN,None), ('grape','Grapes',2,'cluster',PURPLE,GREEN,0.08), ('hops','Hops',2,'cluster',LGREEN,DGREEN,None),
 ('barley','Barley',2,'grain',(226,196,110,255),(190,180,90,255),None), ('sesame','Sesame',2,'pod',(236,220,180,255),GREEN,None), ('green_bean','Green beans',2,'pod',GREEN,LGREEN,0.05),
 ('pea','Peas',2,'pod',LGREEN,GREEN,0.05), ('tea_leaf','Tea leaf',2,'leaf',(60,130,80,255),(60,130,80,255),None),
]
def crop_stage(stage, tall, c, leaf, top=False):
    t=P(); stem=dk(leaf,0.8)
    if tall == 1:
        if stage == 0: t.r(7,12,8,15,stem); t.r(6,11,6,12,leaf); t.r(9,11,9,12,leaf)
        elif stage == 1: t.r(7,10,8,15,stem); t.r(5,9,6,11,leaf); t.r(9,8,10,10,leaf); t.r(6,12,6,13,leaf)
        else:
            t.r(7,8,8,15,stem); t.r(3,8,12,12,leaf); t.r(5,6,10,13,leaf); t.r(4,9,5,9,lt(leaf)); t.r(9,7,10,7,lt(leaf)); t.r(3,12,12,12,dk(leaf))
            if stage == 3:
                for x,y in ((4,10),(7,8),(10,10),(8,12)): t.r(x,y,x+1,y+1,c); t.p(x,y,lt(c,1.3))
    else:
        h=[5,10,14,16][stage] if not top else 16
        for x in (5,8,11):
            y0=16-h+(1 if x != 8 else 0)
            t.r(x,y0,x,15,stem)
            for y in range(y0+1,15,3): t.p(x-1 if (y+x)%2 else x+1,y,leaf); t.p(x-1 if (y+x)%2 else x+1,y+1,leaf)
        if top:
            for x,y in ((5,4),(8,2),(11,5),(8,8)): t.r(x-1,y,x+1,y+2,c); t.p(x-1,y,lt(c,1.3))
    return t
SOIL=['dirt','grass','compost','sand']
for name, label, tall, shape, c, leaf, food in CROPS:
    for s in range(4):
        d={'name': '%s_%d' % (name, s), 'label': label + ' plant', 'solid': False, 'layer': 'background', 'removable': True, 'texture': 'tiles/%s_%d.png' % (name, s),
           'mine_time': 0.1, 'tool': 'machete', 'drops': [{'item': name, 'count': 1 if s < 2 or tall == 2 else (2 if s == 2 else 4)}], 'soil': SOIL}
        if s < 3: d['grows_into']='%s_%d' % (name, s + 1)
        if tall == 2 and s == 2: d['grows_top']=name + '_top'
        if tall == 2 and s == 3: d['topped_by']=name + '_top'
        crop_stage(s, tall, c, leaf).save('tiles', '%s_%d' % (name, s)); tiles.append(d)
    if tall == 2:
        crop_stage(3, 2, c, leaf, top=True).save('tiles', name + '_top')
        tiles.append({'name': name + '_top', 'label': label + ' plant', 'solid': False, 'layer': 'background', 'removable': True, 'texture': 'tiles/%s_top.png' % name,
                      'mine_time': 0.1, 'tool': 'machete', 'drops': [{'item': name, 'count': 3}], 'cut_to': name + '_2'})
    item(name, label, 'food' if food else 'materials', produce(shape, c, leaf), food=food, price=15, sows=name + '_0')

# ---- trees: (name, label, what it bears, its colour, shape of it, leaves, the tree whose shape it takes, food)
TREES=[
 ('lemon','Lemon','round',(244,224,80,255),(96,160,70,255),'orange',0.06), ('lime','Lime','round',(150,210,70,255),(70,140,60,255),'orange',0.06),
 ('peach','Peach','round',(250,170,120,255),(110,170,80,255),'apple',0.12), ('pear','Pear','gourd',(190,210,90,255),(90,150,70,255),'apple',0.12),
 ('plum','Plum','round',(120,60,130,255),(80,140,80,255),'cherry',0.1), ('banana','Banana','long',(240,210,70,255),(110,180,70,255),'mango',0.12),
 ('olive','Olive','cluster',(90,110,50,255),(130,160,110,255),'orange',0.04), ('cocoa_pod','Cocoa pod','gourd',(150,80,40,255),(60,130,60,255),'mango',None),
 ('cinnamon','Cinnamon','long',(170,100,50,255),(100,150,60,255),'mango',None), ('avocado','Avocado','gourd',(70,110,50,255),(80,150,70,255),'apple',0.12),
 ('walnut','Walnut','nut',(150,110,70,255),(100,150,70,255),'apple',0.08), ('fig','Fig','round',(110,60,100,255),(90,160,80,255),'cherry',0.1),
]
for name, label, shape, c, leaf, like, food in TREES:
    tree=name + '_tree'
    logd=copy.deepcopy(T['apple_log']); logd.update({'name': tree + '_log', 'label': label + ' trunk', 'texture': ['tiles/%s_log_%d.png' % (tree, k) for k in (0, 1)]}); tiles.append(logd)
    lead=copy.deepcopy(T['apple_leaves']); lead.pop('texture_seasons', None)
    lead.update({'name': tree + '_leaves', 'label': label + ' leaves', 'texture': ['tiles/%s_leaves_%d.png' % (tree, k) for k in (0, 1, 2)]}); tiles.append(lead)
    for k in (0, 1): recolour('tiles/apple_log_%d.png' % k, '%s_log_%d' % (tree, k), lambda r, g, b, c=c: (int(r * 0.85 + c[0] * 0.12), int(g * 0.85 + c[1] * 0.1), int(b * 0.85 + c[2] * 0.08)))
    for k in (0, 1, 2):
        def f(r, g, b, leaf=leaf):
            v=(r + g + b) / 3 / 255.0 + 0.45
            return tuple(min(255, int(x * v)) for x in leaf[:3])
        recolour('tiles/apple_leaves_%d.png' % k, '%s_leaves_%d' % (tree, k), f)
        im=Image.open(os.path.join(tex, 'tiles', '%s_leaves_%d.png' % (tree, k))); px=im.load()
        for (x, y) in ((6 + 8 * k, 8), (20, 20 - 4 * k), (10, 24)):    # what it bears, showing among the leaves
            for dx in (0, 1):
                for dy in (0, 1):
                    if px[(x + dx) % 32, (y + dy) % 32][3]: px[(x + dx) % 32, (y + dy) % 32]=c
        im.save(os.path.join(tex, 'tiles', '%s_leaves_%d.png' % (tree, k)))
    d=copy.deepcopy(TR[like]); d.update({'name': tree, 'log': tree + '_log', 'leaves': tree + '_leaves', 'fruit': name}); trees.append(d)
    item(name, label, 'food' if food else 'materials', produce(shape, c, leaf), food=food, price=20, plants=tree)

# ---- the kitchen: benches
def bench_tile(name, label, kind, level, t, price, upgrade=None, tool='axe', made=None):
    t.save('tiles', name); block_icon(name)
    d={'name': name, 'label': label, 'solid': False, 'layer': 'background', 'removable': True, 'mine_time': 0.4, 'drop': name, 'texture': 'tiles/%s.png' % name,
       'bench': True, 'bench_kind': kind, 'bench_level': level, 'tool': tool, 'popup_name': label.lower()}
    if upgrade: d['upgrade']={'to': upgrade[0], 'cost': [{'item': upgrade[1], 'count': upgrade[2]}]}
    tiles.append(d)
    items.append({'name': name, 'label': label + ['', '', ' II', ' III'][level] , 'texture': 'items/%s.png' % name, 'category': 'benches', 'price': price, 'places': name}); PRICE[name]=price
def table_art(top=WOODL, face=WOOD, leg=WOODM):
    t=P(); t.r(0,7,15,7,top); t.r(0,8,15,8,face); t.r(1,9,2,15,leg); t.r(13,9,14,15,leg); t.r(1,9,14,9,leg); return t
t=table_art(); t.r(3,5,9,6,WOODL); t.r(3,6,9,6,WOODM); t.r(11,3,11,6,IRONL); t.r(11,2,12,2,WOODD); t.r(5,4,6,4,(214,58,58,255)); t.r(7,4,8,4,GREEN); bench_tile('prep_table','Prep table','prep_table',1,t,60)
t=P(); t.r(4,11,11,15,WOODM); t.r(4,11,11,11,WOODL); t.r(3,6,12,10,STONE); t.r(3,6,12,6,lt(STONE)); t.r(5,7,10,8,STONED); t.r(9,2,10,7,WOODL); t.r(9,1,11,2,WOOD); bench_tile('mortar','Mortar','mortar',1,t,50,tool='pickaxe')
t=P(); t.r(3,5,12,11,IROND); t.r(2,6,13,10,IROND); t.r(3,5,12,5,IRON); t.r(4,4,11,4,(190,110,60,255)); t.r(1,6,1,7,IRON); t.r(14,6,14,7,IRON); t.r(4,12,4,15,STONED); t.r(11,12,11,15,STONED); t.r(6,13,9,15,FIRE); t.r(7,14,8,15,HOT); t.p(6,2,WHITE); t.p(9,1,WHITE); bench_tile('cooking_pot','Cooking pot','cooking_pot',1,t,700,tool='pickaxe')
t=P(); t.r(1,3,14,15,BRICK); t.r(1,3,14,3,lt(BRICK)); t.r(1,7,14,7,BRICKD); t.r(1,12,14,12,BRICKD); t.p(5,5,BRICKD); t.p(11,5,BRICKD); t.r(4,8,11,13,K); t.r(5,10,10,13,FIRE); t.r(6,11,9,13,HOT); t.r(4,8,11,8,IROND); t.r(10,0,12,2,STONED); bench_tile('baking_oven','Baking oven','baking_oven',1,t,340,tool='pickaxe')
t=P(); t.r(3,2,12,15,WOOD); t.r(2,5,13,12,WOOD); t.r(3,2,12,2,WOODL); t.r(6,2,6,15,WOODM); t.r(10,2,10,15,WOODM); t.r(2,5,13,5,IROND); t.r(2,12,13,12,IROND); t.r(7,8,8,9,K); t.r(13,6,13,11,WOODD); bench_tile('fermenting_barrel','Fermenting barrel','fermenting_barrel',1,t,280)
for lvl in (1, 2, 3):
    t=table_art(WHITE, (214,214,206,255), WOODM); t.r(0,8,1,10,WHITE); t.r(14,8,15,10,WHITE); t.r(5,3,10,6,IRONL); t.r(6,2,9,2,IRONL); t.r(7,1,8,1,GOLD); t.r(4,6,11,6,IRON); t.r(6,3,6,4,WHITE)
    for i in range(lvl - 1): t.p(6 + 2 * i, 11, GOLD)
    n='chefs_table' + ('' if lvl == 1 else '_%d' % lvl)
    bench_tile(n, "Chef's table", 'chefs_table', lvl, t, [900, 6000, 26000][lvl - 1], upgrade=None if lvl == 3 else ('chefs_table_%d' % (lvl + 1), ['picnic_basket', 'harvest_feast'][lvl - 1], 1))
rec('workbench','prep_table',1,[('wood',4),('stick',2)],20)
rec('workbench','mortar',1,[('stone',3),('stick',1)],20)
rec('workbench','cooking_pot',1,[('iron_pot',1),('stone',4)],40)
rec('workbench','baking_oven',1,[('stone',8),('iron_ingot',1)],60)
rec('workbench','fermenting_barrel',1,[('wood',6),('iron_ingot',1)],40)
rec('workbench','chefs_table',1,[('wood',4),('glass',2),('iron_ingot',1)],60)

# ---- what is made. Each: bench, level, name, label, picture, food, how many, what of, seconds. Its price is worked out from what goes into it.
BROTH=(200,150,80,255); SAUCE=(196,60,50,255); CHOC=(96,56,36,255); CARAMEL=(200,130,50,255); CHEESE=(240,206,90,255); BUTTER=(244,226,130,255); SOY=(70,40,30,255); WINE=(120,30,60,255)
RICEC=(240,236,220,255); NOODLE=(236,210,140,255); MEAT=(150,84,60,255); JAMR=(190,40,60,255); JAMB=(60,60,150,255); JAMP=(110,50,120,255); TEA=(150,110,60,255); BEER=(220,170,60,255)
def make(bench, level, name, label, picture, food, n, ins, time, cat='food'):
    cost=sum(PRICE[a] * b for a, b in ins) / n
    price=max(10, int(round(cost * 1.15 / 5.0)) * 5)
    item(name, label, cat if food is not None or cat != 'food' else 'materials', picture, food=food, price=price)
    rec(bench, name, n, ins, time, level)
M='mortar'; PR='prep_table'; PO='cooking_pot'; OV='baking_oven'; FB='fermenting_barrel'; CH='chefs_table'
# the mortar: powders, pastes and oils
make(M,1,'sugar','Sugar',pile(WHITE),None,2,[('sugar_beet',2)],10)
make(M,1,'cocoa_powder','Cocoa powder',pile(CHOC),None,2,[('cocoa_pod',1)],10)
make(M,1,'ground_cinnamon','Ground cinnamon',pile((180,110,60,255)),None,2,[('cinnamon',1)],10)
make(M,1,'chilli_powder','Chilli powder',pile((200,60,40,255)),None,2,[('chilli',2)],10)
make(M,1,'garlic_paste','Garlic paste',jar(CREAM),None,2,[('garlic',2)],10)
make(M,1,'ginger_paste','Ginger paste',jar((226,200,120,255)),None,2,[('ginger',2)],10)
make(M,1,'curry_powder','Curry powder',pile((220,160,40,255)),None,2,[('chilli_powder',1),('ginger_paste',1),('garlic_paste',1),('ground_cinnamon',1)],20)
make(M,1,'rice_flour','Rice flour',pile(RICEC),None,2,[('rice',3)],10)
make(M,1,'malt','Malt',pile((200,160,90,255)),None,2,[('barley',3)],10)
make(M,1,'tea_leaves','Dried tea',pile((70,110,60,255)),None,2,[('tea_leaf',2)],10)
make(M,1,'peanut_butter','Peanut butter',jar((196,140,70,255)),0.15,1,[('peanut',4),('salt',1)],15)
make(M,1,'tahini','Tahini',jar((226,206,160,255)),0.1,1,[('sesame',4)],15)
rec(M,'oil',2,[('olive',3)],10); rec(M,'oil',2,[('sesame',4)],10); rec(M,'flour',2,[('barley',4)],10)
# the fermenting barrel
make(FB,1,'butter','Butter',block(BUTTER),0.08,2,[('milk',2),('salt',1)],40)
make(FB,1,'cream','Cream',jar(WHITE),0.08,2,[('milk',2)],30)
make(FB,1,'yogurt','Yogurt',bowl(WHITE),0.2,2,[('milk',2)],60)
make(FB,1,'cheese_wheel','Cheese',block(CHEESE,(226,180,60,255)),0.25,1,[('milk',3),('salt',1)],90)
make(FB,1,'soy_sauce','Soy sauce',bottle(SOY),None,2,[('soybean',3),('salt',1)],60)
make(FB,1,'tofu','Tofu',block(CREAM),0.2,2,[('soybean',3)],40)
make(FB,1,'miso','Miso',jar((180,120,70,255)),0.1,1,[('soybean',2),('rice',1),('salt',1)],90)
make(FB,1,'vinegar','Vinegar',bottle((226,210,150,255)),None,2,[('grape',2)],60)
make(FB,1,'wine','Wine',bottle(WINE),{'hunger': 0.05, 'energy': 0.2},1,[('grape',5)],120)
make(FB,1,'beer','Beer',cup(BEER),{'hunger': 0.1, 'energy': 0.15},2,[('malt',2),('hops',2),('yeast',1)],120)
make(FB,1,'pickles','Pickles',jar((110,160,70,255)),0.15,2,[('cucumber',2),('vinegar',1),('salt',1)],60)
make(FB,1,'kimchi','Kimchi',jar((214,90,50,255)),0.25,2,[('cabbage',2),('chilli_powder',1),('garlic_paste',1),('salt',1)],90)
make(FB,1,'sauerkraut','Sauerkraut',jar((226,220,150,255)),0.2,2,[('cabbage',2),('salt',1)],60)
make(FB,1,'raisins','Raisins',pile((90,50,80,255)),0.1,2,[('grape',2)],40)
# (steamed rice first: other things are made of it)
make(PO,1,'steamed_rice','Steamed rice',bowl(RICEC),0.2,2,[('rice',2)],20)
# the prep table: cold things and things got ready
make(PR,1,'salad','Garden salad',bowl(LGREEN,(214,58,58,255)),0.3,1,[('lettuce',1),('tomato',1),('cucumber',1)],10)
make(PR,1,'fruit_salad','Fruit salad',bowl(PINK,YEL),0.35,1,[('strawberry',1),('blueberry',1),('banana',1),('peach',1)],10)
make(PR,1,'coleslaw','Coleslaw',bowl(CREAM,ORANGE),0.3,1,[('cabbage',1),('carrot',1),('cream',1)],10)
make(PR,1,'guacamole','Guacamole',bowl((120,170,70,255),(214,58,58,255)),0.35,1,[('avocado',1),('onion',1),('lime',1),('tomato',1)],10)
make(PR,1,'salsa','Salsa',bowl(SAUCE,GREEN),0.25,1,[('tomato',2),('onion',1),('chilli',1),('lime',1)],10)
make(PR,1,'pesto','Pesto',jar((90,150,70,255)),0.15,1,[('basil',2),('garlic_paste',1),('oil',1),('walnut',1)],15)
make(PR,1,'hummus','Hummus',bowl((226,200,150,255),GREEN),0.3,1,[('pea',2),('tahini',1),('garlic_paste',1),('lemon',1)],15)
make(PR,1,'pasta_dough','Pasta dough',block(NOODLE),None,2,[('flour',2),('dodo_egg',1)],15,cat='materials')
make(PR,1,'noodles','Noodles',bowl(NOODLE),None,2,[('rice_flour',2),('salt',1)],15,cat='materials')
make(PR,1,'trail_mix','Trail mix',pile((170,120,80,255)),0.3,2,[('peanut',2),('walnut',1),('raisins',1)],10)
make(PR,1,'smoothie','Berry smoothie',cup(PINK),{'hunger': 0.2, 'energy': 0.15},1,[('strawberry',1),('banana',1),('yogurt',1),('cup',1)],10)
make(PR,1,'lemonade','Lemonade',cup((244,230,120,255)),{'hunger': 0.08, 'energy': 0.2},2,[('lemon',2),('sugar',1),('mint',1),('cup',2)],10)
make(PR,1,'sandwich','Sandwich',loaf(CRUST,GREEN),0.55,2,[('bread',1),('lettuce',1),('tomato',1),('cooked_dodo_meat',1),('cheese_wheel',1)],15)
make(PR,1,'sushi','Sushi',plate(RICEC,(240,120,100,255),K),0.45,2,[('steamed_rice',1),('fish',1),('soy_sauce',1),('cucumber',1)],20)
make(PR,1,'caprese','Caprese',plate(WHITE,(214,58,58,255),GREEN),0.35,1,[('mozzarella',1),('tomato',1),('basil',1),('oil',1)],10)
# the cooking pot: sauces, sweets, soups and stews
make(PO,1,'tomato_sauce','Tomato sauce',jar(SAUCE),0.1,2,[('tomato',3),('garlic_paste',1),('basil',1)],30)
make(PO,1,'strawberry_jam','Strawberry jam',jar(JAMR),0.15,2,[('strawberry',3),('sugar',1)],30)
make(PO,1,'blueberry_jam','Blueberry jam',jar(JAMB),0.15,2,[('blueberry',3),('sugar',1)],30)
make(PO,1,'plum_jam','Plum jam',jar(JAMP),0.15,2,[('plum',3),('sugar',1)],30)
make(PO,1,'marmalade','Marmalade',jar(ORANGE),0.15,2,[('orange',2),('lemon',1),('sugar',1)],30)
make(PO,1,'chocolate','Chocolate',block(CHOC),{'hunger': 0.15, 'energy': 0.1},2,[('cocoa_powder',2),('sugar',1),('butter',1)],40)
make(PO,1,'caramel','Caramel',block(CARAMEL),0.1,2,[('sugar',2),('butter',1),('cream',1)],30)
make(PO,1,'mashed_potato','Mashed potato',bowl(CREAM,BUTTER),0.4,1,[('potato',2),('butter',1),('milk',1)],30)
make(PO,1,'vegetable_soup','Vegetable soup',bowl(BROTH,GREEN),0.5,2,[('potato',1),('carrot',1),('onion',1),('cabbage',1)],40)
make(PO,1,'pumpkin_soup','Pumpkin soup',bowl(ORANGE,WHITE),0.5,2,[('pumpkin',1),('onion',1),('cream',1)],40)
make(PO,1,'pea_soup','Pea soup',bowl((130,180,90,255),WHITE),0.45,2,[('pea',3),('onion',1),('mint',1)],40)
make(PO,1,'borscht','Borscht',bowl((170,40,80,255),WHITE),0.5,2,[('beetroot',2),('cabbage',1),('potato',1),('cream',1)],40)
make(PO,1,'mushroom_risotto','Mushroom risotto',bowl(RICEC,(160,110,80,255)),0.6,1,[('rice',2),('mushroom',2),('butter',1),('cheese_wheel',1)],60)
make(PO,1,'curry','Vegetable curry',bowl((220,150,40,255),RICEC),0.65,2,[('curry_powder',1),('potato',1),('onion',1),('eggplant',1),('steamed_rice',1)],60)
make(PO,1,'ramen','Ramen',bowl(BROTH,NOODLE),0.65,1,[('noodles',1),('miso',1),('dodo_egg',1),('spinach',1),('soy_sauce',1)],60)
make(PO,1,'spaghetti','Spaghetti',plate(NOODLE,SAUCE,GREEN),0.65,2,[('pasta_dough',1),('tomato_sauce',1),('cheese_wheel',1),('basil',1)],60)
make(PO,1,'bean_stew','Bean stew',bowl((150,90,60,255),GREEN),0.55,2,[('green_bean',2),('tomato_sauce',1),('onion',1),('garlic_paste',1)],50)
make(PO,1,'stir_fry','Stir fry',plate(GREEN,(230,190,50,255),SOY),0.6,2,[('bell_pepper',1),('eggplant',1),('tofu',1),('soy_sauce',1),('ginger_paste',1)],40)
make(PO,1,'fried_rice','Fried rice',bowl((226,200,130,255),GREEN),0.6,2,[('steamed_rice',2),('dodo_egg',1),('pea',1),('soy_sauce',1)],40)
make(PO,1,'tea','Tea',cup(TEA),{'hunger': 0.03, 'energy': 0.3},2,[('tea_leaves',1),('mint',1),('cup',2)],20)
make(PO,1,'hot_chocolate','Hot chocolate',cup(CHOC),{'hunger': 0.15, 'energy': 0.3},1,[('cocoa_powder',1),('milk',1),('sugar',1),('cup',1)],20)
make(PO,1,'mulled_wine','Mulled wine',cup(WINE),{'hunger': 0.1, 'energy': 0.4},2,[('wine',1),('ground_cinnamon',1),('orange',1),('cup',2)],30)
make(PO,1,'peanut_noodles','Peanut noodles',bowl(NOODLE,(196,140,70,255)),0.6,1,[('noodles',1),('peanut_butter',1),('soy_sauce',1),('cucumber',1)],40)
# the oven: baking and roasting
make(OV,1,'baked_potato','Baked potato',plate(TAN,BUTTER),0.4,1,[('potato',1),('butter',1)],40)
make(OV,1,'roasted_vegetables','Roasted vegetables',plate(ORANGE,PURPLE,GREEN),0.5,2,[('pumpkin',1),('eggplant',1),('bell_pepper',1),('oil',1)],50)
make(OV,1,'garlic_bread','Garlic bread',loaf(CRUST,CREAM),0.5,2,[('bread',1),('garlic_paste',1),('butter',1)],30)
make(OV,1,'cookies','Cookies',plate((200,150,90,255),CHOC),0.3,4,[('flour',1),('sugar',1),('butter',1),('chocolate',1)],40)
make(OV,1,'croissant','Croissant',loaf((226,180,100,255)),0.35,2,[('dough',1),('butter',2)],50)
make(OV,1,'banana_bread','Banana bread',loaf((180,130,70,255),YEL),0.5,1,[('banana',2),('flour',1),('sugar',1),('dodo_egg',1)],60)
make(OV,1,'apple_pie','Apple pie',pie(CRUST,(226,190,110,255)),0.6,1,[('apple',3),('dough',1),('sugar',1),('ground_cinnamon',1)],80)
make(OV,1,'pumpkin_pie','Pumpkin pie',pie(CRUST,ORANGE),0.6,1,[('pumpkin',1),('dough',1),('sugar',1),('cream',1),('ground_cinnamon',1)],80)
make(OV,1,'berry_tart','Berry tart',pie(CRUST,JAMR),0.5,1,[('dough',1),('strawberry_jam',1),('raspberry',2),('cream',1)],60)
make(OV,1,'lemon_tart','Lemon tart',pie(CRUST,(244,230,120,255)),0.5,1,[('dough',1),('lemon',2),('sugar',1),('dodo_egg',1)],60)
make(OV,1,'peach_cobbler','Peach cobbler',pie(CRUST,(250,170,120,255)),0.55,1,[('peach',3),('flour',1),('sugar',1),('butter',1)],60)
make(OV,1,'fig_rolls','Fig rolls',plate((190,150,100,255),(110,60,100,255)),0.35,3,[('fig',2),('flour',1),('sugar',1)],40)
make(OV,1,'walnut_brownie','Walnut brownie',block(CHOC,(150,110,70,255)),{'hunger': 0.4, 'energy': 0.15},2,[('chocolate',1),('flour',1),('walnut',1),('dodo_egg',1),('sugar',1)],60)
make(OV,1,'cheesecake','Cheesecake',cake(CREAM,JAMB,JAMB),0.6,1,[('cheese_wheel',1),('cream',1),('sugar',1),('dodo_egg',1),('blueberry_jam',1)],90)
make(OV,1,'chocolate_cake','Chocolate cake',cake(CHOC,(130,80,50,255),(214,58,58,255)),{'hunger': 0.6, 'energy': 0.2},1,[('flour',1),('chocolate',2),('dodo_egg',1),('sugar',1),('butter',1)],90)
make(OV,1,'lasagna','Lasagna',plate(SAUCE,CHEESE,NOODLE),0.75,2,[('pasta_dough',1),('tomato_sauce',1),('cheese_wheel',1),('cooked_dodo_meat',1)],90)
make(OV,1,'stuffed_pepper','Stuffed pepper',plate((230,190,50,255),RICEC,GREEN),0.55,2,[('bell_pepper',2),('steamed_rice',1),('cheese_wheel',1),('tomato_sauce',1)],60)
make(OV,1,'eggplant_bake','Eggplant bake',plate(PURPLE,SAUCE,CHEESE),0.6,2,[('eggplant',2),('tomato_sauce',1),('mozzarella',1),('basil',1)],60)
make(OV,1,'roast_dodo','Roast dodo',plate(MEAT,TAN,GREEN),0.8,1,[('dodo_meat',2),('garlic_paste',1),('potato',1),('basil',1),('butter',1)],120)
make(OV,1,'glazed_fish','Glazed fish',plate((230,160,120,255),SOY,GREEN),0.65,1,[('fish',1),('soy_sauce',1),('ginger_paste',1),('lime',1)],60)
# the chef's table: meals put together, feasts of them, and the end of it all
make(CH,1,'breakfast_platter','Breakfast platter',platter([(226,180,100,255),JAMR,TEA,PINK]),{'hunger': 0.8, 'energy': 0.4},1,[('croissant',1),('strawberry_jam',1),('tea',1),('fruit_salad',1)],60)
make(CH,1,'picnic_basket','Picnic basket',basket([CRUST,(244,230,120,255),CHOC]),{'hunger': 0.9, 'energy': 0.3},1,[('sandwich',1),('lemonade',1),('cookies',1),('trail_mix',1)],60)
make(CH,1,'cheese_board','Cheese board',platter([CHEESE,PURPLE,(150,110,70,255),WINE]),{'hunger': 0.8, 'health': 0.2},1,[('cheese_wheel',1),('grape',2),('walnut',1),('fig',1),('wine',1)],60)
make(CH,1,'mezze','Mezze',platter([(226,200,150,255),(120,170,70,255),SAUCE,(110,160,70,255)]),{'hunger': 0.85, 'health': 0.2},1,[('hummus',1),('guacamole',1),('salsa',1),('pickles',1),('flatbread',1)],60)
make(CH,2,'italian_feast','Italian feast',platter([NOODLE,SAUCE,CRUST,WINE,GREEN]),{'hunger': 1.0, 'health': 0.5, 'energy': 0.3},1,[('spaghetti',1),('lasagna',1),('garlic_bread',1),('caprese',1),('wine',1)],120)
make(CH,2,'eastern_feast','Eastern feast',platter([RICEC,BROTH,(226,200,130,255),TEA,(214,90,50,255)]),{'hunger': 1.0, 'health': 0.5, 'energy': 0.3},1,[('sushi',1),('ramen',1),('fried_rice',1),('kimchi',1),('tea',1)],120)
make(CH,2,'harvest_feast','Harvest feast',platter([MEAT,CREAM,ORANGE,WINE,GREEN]),{'hunger': 1.0, 'health': 0.5, 'energy': 0.3},1,[('roast_dodo',1),('mashed_potato',1),('pumpkin_pie',1),('roasted_vegetables',1),('mulled_wine',1)],120)
make(CH,2,'garden_feast','Garden feast',platter([LGREEN,BROTH,(230,190,50,255),(244,230,120,255),PURPLE]),{'hunger': 1.0, 'health': 0.5, 'energy': 0.3},1,[('salad',1),('vegetable_soup',1),('stuffed_pepper',1),('eggplant_bake',1),('lemonade',1)],120)
make(CH,2,'spice_feast','Spice feast',platter([(220,150,40,255),GREEN,NOODLE,BEER,(150,90,60,255)]),{'hunger': 1.0, 'health': 0.5, 'energy': 0.3},1,[('curry',1),('stir_fry',1),('peanut_noodles',1),('bean_stew',1),('beer',1)],120)
make(CH,2,'dessert_trolley','Dessert trolley',platter([CHOC,CREAM,JAMR,(244,230,120,255),CHOC]),{'hunger': 0.8, 'energy': 0.8},1,[('chocolate_cake',1),('cheesecake',1),('berry_tart',1),('lemon_tart',1),('hot_chocolate',1)],120)
make(CH,3,'royal_banquet','Royal banquet',platter([GOLD,SAUCE,GREEN,WINE,CREAM]),{'hunger': 1.0, 'health': 1.0, 'energy': 1.0, 'energy_over': True},1,[('italian_feast',1),('eastern_feast',1),('harvest_feast',1),('garden_feast',1),('spice_feast',1)],300)
t=cake(GOLD,WHITE,(214,58,58,255)); t.p(3,2,HOT); t.p(12,3,HOT); t.p(5,1,WHITE); t.p(10,1,WHITE)
make(CH,3,'ambrosia','Ambrosia',t,{'hunger': 1.0, 'health': 1.0, 'energy': 1.0, 'energy_over': True, 'oxygen': 1.0},1,[('royal_banquet',1),('dessert_trolley',1),('cheese_board',1),('mezze',1),('rainbow_cake',1)],600)

# ---- where it all grows: six new kinds of country, and a little of it in the old ones
biomes=[
 {'name': 'berry_meadow', 'label': 'Berry meadow', 'heat': [-0.4, 0.1], 'dryness': [-0.1, 0.12], 'surface': 'grass', 'soil': 'dirt', 'tree_one_in': 9,
  'trees': [{'tree': 'plum_tree', 'weight': 2}, {'tree': 'pear_tree', 'weight': 2}, {'tree': 'peach_tree', 'weight': 1}],
  'plants': [{'tile': 'strawberry_0', 'one_in': 14}, {'tile': 'blueberry_0', 'one_in': 16}, {'tile': 'raspberry_0', 'one_in': 16}, {'tile': 'mint_0', 'one_in': 30}, {'tile': 'lettuce_0', 'one_in': 35}]},
 {'name': 'citrus_grove', 'label': 'Citrus grove', 'heat': [0.1, 0.55], 'dryness': [-0.45, 0.05], 'surface': 'grass', 'soil': 'dirt', 'tree_one_in': 6,
  'trees': [{'tree': 'lemon_tree', 'weight': 3}, {'tree': 'lime_tree', 'weight': 3}, {'tree': 'avocado_tree', 'weight': 1}, {'tree': 'orange', 'weight': 1}],
  'plants': [{'tile': 'basil_0', 'one_in': 22}, {'tile': 'mint_0', 'one_in': 28}, {'tile': 'bell_pepper_0', 'one_in': 28}, {'tile': 'melon_0', 'one_in': 35}, {'tile': 'cucumber_0', 'one_in': 35}]},
 {'name': 'spice_jungle', 'label': 'Spice jungle', 'heat': [0.55, 1], 'dryness': [-1, -0.45], 'surface': 'grass', 'soil': 'dirt', 'tree_one_in': 4, 'tree_spacing': 3,
  'trees': [{'tree': 'banana_tree', 'weight': 3}, {'tree': 'cocoa_pod_tree', 'weight': 3}, {'tree': 'cinnamon_tree', 'weight': 2}, {'tree': 'avocado_tree', 'weight': 1}],
  'plants': [{'tile': 'ginger_0', 'one_in': 20}, {'tile': 'sesame_0', 'one_in': 26}, {'tile': 'peanut_0', 'one_in': 26}, {'tile': 'eggplant_0', 'one_in': 32}, {'tile': 'chilli_0', 'one_in': 32}]},
 {'name': 'vineyard_hills', 'label': 'Vineyard hills', 'heat': [0.1, 0.5], 'dryness': [0.05, 0.5], 'surface': 'grass', 'soil': 'dirt', 'tree_one_in': 10,
  'trees': [{'tree': 'olive_tree', 'weight': 3}, {'tree': 'fig_tree', 'weight': 2}, {'tree': 'walnut_tree', 'weight': 1}],
  'plants': [{'tile': 'grape_0', 'one_in': 9}, {'tile': 'barley_0', 'one_in': 22}, {'tile': 'hops_0', 'one_in': 24}, {'tile': 'garlic_0', 'one_in': 30}, {'tile': 'onion_0', 'one_in': 30}]},
 {'name': 'rice_terraces', 'label': 'Rice terraces', 'heat': [-0.4, 0.1], 'dryness': [-1, -0.6], 'surface': 'grass', 'soil': 'dirt', 'tree_one_in': 14,
  'trees': [{'tree': 'peach_tree', 'weight': 2}, {'tree': 'plum_tree', 'weight': 1}],
  'plants': [{'tile': 'rice_0', 'one_in': 7}, {'tile': 'soybean_0', 'one_in': 18}, {'tile': 'tea_leaf_0', 'one_in': 18}, {'tile': 'cabbage_0', 'one_in': 26}, {'tile': 'spinach_0', 'one_in': 30}, {'tile': 'mushroom_0', 'one_in': 40}]},
 {'name': 'harvest_fields', 'label': 'Harvest fields', 'heat': [-1, -0.4], 'dryness': [-0.2, 0.1], 'surface': 'grass', 'soil': 'dirt', 'tree_one_in': 16,
  'trees': [{'tree': 'walnut_tree', 'weight': 2}, {'tree': 'pear_tree', 'weight': 1}],
  'plants': [{'tile': 'pumpkin_0', 'one_in': 16}, {'tile': 'potato_0', 'one_in': 14}, {'tile': 'sugar_beet_0', 'one_in': 18}, {'tile': 'beetroot_0', 'one_in': 22}, {'tile': 'green_bean_0', 'one_in': 22}, {'tile': 'pea_0', 'one_in': 22}, {'tile': 'onion_0', 'one_in': 28}, {'tile': 'mushroom_0', 'one_in': 36}]},
]
more=[{'biome': 'plains', 'tile': 'potato_0', 'one_in': 60}, {'biome': 'plains', 'tile': 'lettuce_0', 'one_in': 70}, {'biome': 'orchard', 'tile': 'strawberry_0', 'one_in': 45},
      {'biome': 'tropics', 'tile': 'ginger_0', 'one_in': 50}, {'biome': 'pine_forest', 'tile': 'mushroom_0', 'one_in': 25}, {'biome': 'maple_forest', 'tile': 'mushroom_0', 'one_in': 30}]

def dump(name, obj):
    with open(os.path.join(mod, name), 'w') as f:
        if isinstance(obj, dict) and len(obj) == 1 and isinstance(list(obj.values())[0], list):
            k, v = list(obj.items())[0]
            f.write('{\n  "%s": [\n' % k + ',\n'.join('    ' + json.dumps(e) for e in v) + '\n  ]\n}\n')
        else: json.dump(obj, f, indent=1); f.write('\n')
dump('tiles.json', {'tiles': tiles}); dump('items.json', {'items': items}); dump('recipes.json', {'recipes': recipes}); dump('trees.json', {'trees': trees})
dump('worldgen.json', {'biomes': biomes, 'biome_plants': more}); dump('mod.json', {'label': 'Culinary Delights', 'icon': 'icon.png'})
t=cake(CHOC,CREAM,(214,58,58,255)); u=P(); u.r(0,0,15,15,(60,40,50,255)); u.im.alpha_composite(t.im); u.im.resize((64, 64), Image.NEAREST).save(os.path.join(mod, 'icon.png'))
print(len(tiles), 'tiles', len(items), 'items', len(recipes), 'recipes', len(trees), 'trees', len(biomes), 'biomes')
