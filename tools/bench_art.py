# Bench textures in the game's own manner: sixteen pixels square, drawn double, a few flat shades each.
import sys, os
from PIL import Image
repo = sys.argv[1]; out = os.path.join(repo, 'data/textures/tiles')
WL=(201,159,99,255); W=(174,130,73,255); WM=(140,101,55,255); WD=(106,72,39,255)          # wood, light to dark
ML=(192,194,205,255); M=(149,151,163,255); MD=(107,109,120,255)                              # iron
K=(22,22,30,255); KD=(58,58,69,255); SG=(88,90,100,255)                                      # black, soot, stone
BL=(208,127,93,255); B=(181,94,65,255); BD=(111,52,35,255)                                   # brick
CL=(232,185,151,255); C=(214,128,66,255); CD=(166,99,51,255)                                 # copper
SL=(180,188,200,255); S=(120,134,156,255); SD=(93,104,121,255); SK=(66,73,85,255)            # steel
GOLD=(215,159,29,255); FIRE=(240,140,40,255); HOT=(250,210,90,255); WHITE=(232,232,226,255)
RED=(190,60,60,255); BLUE=(70,120,200,255); GREEN=(80,150,80,255); YEL=(220,180,60,255); PURP=(150,90,170,255); TEAL=(110,190,200,255)
def dk(c,f=0.75): return tuple(int(v*f) for v in c[:3])+(255,)
class T:
    def __init__(s): s.im=Image.new('RGBA',(16,16),(0,0,0,0)); s.px=s.im.load()
    def r(s,x0,y0,x1,y1,c):
        for y in range(max(0,y0),min(15,y1)+1):
            for x in range(max(0,x0),min(15,x1)+1): s.px[x,y]=c
    def p(s,x,y,c): s.r(x,y,x,y,c)
    def save(s,name): s.im.resize((32,32),Image.NEAREST).save(os.path.join(out,name+'.png'))
def table(t,top=4,lip=WL,face=W,leg=WM,edge=WD,x0=0,x1=15):
    t.r(x0,top,x1,top,lip); t.r(x0,top+1,x1,top+1,face); t.r(x0+1,top+2,x1-1,15,(0,0,0,0))
    t.r(x0+1,top+2,x0+2,15,leg); t.r(x1-2,top+2,x1-1,15,leg); t.p(x0+1,top+2,edge); t.p(x1-1,top+2,edge)
    t.r(x0+1,top+2,x1-1,top+2,leg); t.p(x0,top+2,edge); t.p(x1,top+2,edge)
    for y in range(top+3,16): t.p(x0+1,y,edge); t.p(x1-1,y,edge)
def pips(t,n,y=15):
    for i in range(n): t.p(8-n+2*i,y,GOLD)

def woodwork():
    t=T()  # a sawhorse with a log across it
    t.r(0,6,15,6,WL); t.r(0,7,15,7,W)
    for i in range(8): t.p(2+i//3,8+i,WD); t.p(5-i//3,8+i,WM); t.p(10+i//3,8+i,WM); t.p(13-i//3,8+i,WD)
    t.r(3,3,12,5,WM); t.r(3,3,12,3,W); t.r(3,3,3,5,WL); t.r(12,3,12,5,WD)
    return t
def tool_bench(tier):
    m=[SG,SG,C,M,S,TEAL,KD][tier-1]
    t=T()  # a board with tools hung on it, over a shelf
    t.r(2,0,13,7,WL); t.r(2,0,13,0,W); t.r(2,7,13,7,W)
    t.r(3,2,5,2,m); t.r(4,3,4,5,WD)          # hammer
    t.r(7,2,8,2,m); t.r(7,3,7,5,m); t.p(8,5,m)  # spanner
    t.r(11,2,11,4,m); t.r(11,5,11,6,WD)      # driver
    t.r(0,8,15,8,WL); t.r(0,9,15,9,W); t.r(1,10,2,15,WM); t.r(13,10,14,15,WM)
    for y in range(10,16): t.p(1,y,WD); t.p(14,y,WD)
    t.r(3,11,12,11,WM)
    if tier>1: pips(t,tier-1,13)
    return t
def craft_bench(tier):
    cloth=[RED,BLUE,PURP,GREEN][tier-1]
    t=T()  # a chest of drawers under a cloth
    t.r(1,7,14,15,W); t.r(1,7,1,15,WD); t.r(14,7,14,15,WD); t.r(1,15,14,15,WD)
    t.r(2,10,13,10,WM); t.r(2,13,13,13,WM); t.r(7,8,8,8,GOLD); t.r(7,11,8,11,GOLD); t.r(7,14,8,14,GOLD)
    t.r(0,5,15,6,cloth); t.r(0,5,15,5,tuple(min(255,int(v*1.2)) for v in cloth[:3])+(255,)); t.p(0,7,dk(cloth)); t.p(15,7,dk(cloth))
    t.r(3,3,4,4,cloth); t.r(10,2,10,4,M); t.r(12,2,12,4,M)
    if tier>1: pips(t,tier-1,9)
    return t
def metalwork(tier):
    a=MD if tier==1 else SK; al=M if tier==1 else S
    t=T()  # an anvil on a stump
    t.r(4,10,11,15,WM); t.r(4,10,11,10,WL); t.r(4,11,4,15,WD); t.r(11,11,11,15,WD); t.r(7,12,7,14,WD)
    t.r(2,4,13,5,a); t.r(2,4,13,4,al); t.r(0,5,1,5,a); t.r(5,6,10,7,a); t.r(4,8,11,9,a); t.r(4,8,11,8,al)
    if tier>1: pips(t,tier-1,15)
    return t
def tailor(tier):
    cloth=PURP if tier==1 else BLUE
    t=T()  # a table with a bolt of cloth over its end and a spool
    table(t,6)
    t.r(1,3,7,5,cloth); t.r(1,3,7,3,tuple(min(255,int(v*1.2)) for v in cloth[:3])+(255,)); t.r(0,6,3,11,cloth); t.r(0,11,3,11,dk(cloth)); t.r(3,6,3,10,dk(cloth))
    t.r(11,2,12,5,RED); t.r(10,2,13,2,WL); t.r(10,5,13,5,WL)
    if tier>1: pips(t,tier-1,9)
    return t
def press(tier):
    c,cl,cd=[(WM,W,WD),(MD,M,KD),(S,SL,SK)][tier-1]
    t=T()  # two posts, a beam, the screw and its plate
    t.r(1,2,2,15,c); t.r(13,2,14,15,c); t.p(1,15,cd); t.p(14,15,cd)
    t.r(0,1,15,2,c); t.r(0,1,15,1,cl); t.r(0,13,15,15,c); t.r(0,13,15,13,cl); t.r(0,15,15,15,cd)
    t.r(7,0,8,7,ML); t.p(7,4,MD); t.p(8,6,MD); t.r(5,0,10,0,WD); t.r(4,8,11,9,c); t.r(4,8,11,8,cl)
    t.r(5,11,10,12,WL)
    if tier>1: pips(t,tier-1,14)
    return t
def compost():
    t=T()  # a slatted bin, heaped
    t.r(3,2,12,4,(96,72,44,255)); t.r(5,1,10,1,(96,72,44,255)); t.p(5,2,GREEN); t.p(8,1,GREEN); t.p(10,3,GREEN); t.p(7,3,YEL)
    for y in (5,8,11,14): t.r(1,y,14,y+1,W); t.r(1,y,14,y,WL)
    t.r(1,5,1,15,WD); t.r(14,5,14,15,WD); t.r(7,5,8,15,WM)
    return t
def easel():
    t=T()
    for i in range(7): t.p(4-i//3,9+i,WD); t.p(11+i//3,9+i,WD)
    t.r(7,9,8,15,WM); t.r(7,0,8,1,WD)
    t.r(3,2,12,9,WHITE); t.r(3,10,12,10,WM)
    t.r(4,7,11,8,GREEN); t.r(5,3,6,4,YEL); t.r(9,4,10,6,BLUE); t.p(8,5,RED)
    return t
def builder():
    t=T()  # a stone slab on two brick piers, bricks on top
    t.r(0,6,15,7,M); t.r(0,6,15,6,ML); t.r(0,8,15,8,MD)
    for x0 in (1,11):
        t.r(x0,9,x0+3,15,B); t.r(x0,11,x0+3,11,BD); t.r(x0,14,x0+3,14,BD); t.p(x0+1,9,BD); t.p(x0+2,12,BD); t.p(x0+1,15,BD)
    t.r(2,4,5,5,B); t.r(6,4,9,5,BL); t.r(4,2,7,3,B); t.p(5,4,BD); t.p(9,4,BD); t.p(7,2,BD)
    t.r(12,3,13,5,ML); t.p(12,2,WD)
    return t
def armor(tier):
    c,cl,cd=[(C,CL,CD),(M,ML,MD),(TEAL,(170,225,230,255),(70,140,150,255))][tier-1]
    t=T()  # a stand with a breastplate on it
    t.r(7,3,8,14,WD); t.r(4,14,11,15,WM); t.r(4,15,11,15,WD)
    t.r(4,5,11,10,c); t.r(4,5,11,5,cl); t.r(3,5,3,7,cd); t.r(12,5,12,7,cd); t.r(7,6,8,9,cl); t.r(5,11,10,11,cd)
    t.r(6,1,9,3,c); t.r(6,1,9,1,cl); t.r(7,2,8,2,K)
    if tier>1: pips(t,tier-1,13)
    return t
def pizza():
    t=T()  # a plastered dome over a brick base, a fire in its mouth
    P=(226,214,196,255); PD=(184,170,150,255)
    t.r(10,0,11,3,B); t.r(10,0,11,0,BD)
    t.r(3,3,12,10,P); t.r(1,5,14,10,P); t.r(2,4,13,4,P); t.r(1,5,1,10,PD); t.r(14,5,14,10,PD)
    t.r(5,6,10,10,K); t.r(6,5,9,5,K); t.r(6,8,9,10,FIRE); t.r(7,9,8,10,HOT)
    t.r(0,11,15,15,B); t.r(0,11,15,11,BL); t.r(0,13,15,13,BD); t.p(4,12,BD); t.p(11,12,BD); t.p(7,14,BD); t.p(13,14,BD); t.p(2,14,BD)
    return t
def train():
    t=T()  # a truck on a length of track
    t.r(0,13,15,13,ML); t.r(0,14,15,14,MD); t.r(1,15,2,15,WD); t.r(6,15,7,15,WD); t.r(11,15,12,15,WD)
    t.r(2,5,12,10,RED); t.r(2,5,12,5,(220,100,90,255)); t.r(2,8,12,8,dk(RED)); t.r(3,3,11,4,K); t.r(4,2,7,2,KD)
    t.r(3,11,5,12,KD); t.r(9,11,11,12,KD); t.p(4,11,M); t.p(10,11,M)
    t.r(14,8,15,12,WM)
    return t
def mixing():
    t=T()  # a barrel vat with a paddle, and flasks on a shelf over it
    t.r(0,4,15,4,WL)
    for x,c in ((2,GREEN),(6,BLUE),(12,RED)): t.p(x,1,ML); t.r(x-1,2,x+1,3,c)
    t.r(2,7,13,15,W); t.r(2,7,2,15,WD); t.r(13,7,13,15,WD); t.r(5,7,5,15,WM); t.r(10,7,10,15,WM)
    t.r(2,9,13,9,MD); t.r(2,13,13,13,MD); t.r(3,6,12,6,GREEN)
    t.r(8,2,8,6,WL); t.p(9,1,WL)
    return t
def dye():
    t=T()  # three tubs of colour under a rail with cloth on it
    t.r(0,1,15,1,WD); t.r(1,1,1,15,WM); t.r(14,1,14,15,WM)
    t.r(3,2,6,6,PURP); t.r(9,2,12,5,YEL)
    for x,c in ((2,RED),(6,GREEN),(10,BLUE)):
        t.r(x,10,x+3,14,W); t.r(x,10,x+3,10,c); t.r(x,12,x+3,12,WD)
    t.r(0,15,15,15,WD)
    return t
def steam_gen():
    t=T()  # a copper boiler over a firebox, with a stack
    t.r(11,0,12,4,KD); t.r(10,0,13,0,MD)
    t.r(1,4,14,10,C); t.r(1,4,14,4,CL); t.r(1,10,14,10,CD); t.p(1,4,(0,0,0,0)); t.p(14,4,(0,0,0,0)); t.p(1,10,(0,0,0,0)); t.p(14,10,(0,0,0,0))
    t.r(5,5,5,9,CD); t.r(10,5,10,9,CD); t.r(2,6,3,7,WHITE); t.p(3,6,RED)
    t.r(2,11,13,15,MD); t.r(2,11,13,11,M); t.r(5,12,10,15,K); t.r(6,13,9,15,FIRE); t.r(7,14,8,15,HOT)
    return t
def flywheel():
    t=T()  # a wheel in a frame
    ring=[(5,1),(6,1),(7,1),(8,1),(9,1),(10,1),(3,2),(4,2),(11,2),(12,2),(2,3),(13,3),(2,4),(13,4),(1,5),(14,5),(1,6),(14,6),(1,7),(14,7),(1,8),(14,8),(1,9),(14,9),(1,10),(14,10),(2,11),(13,11),(2,12),(13,12),(3,13),(4,13),(11,13),(12,13),(5,14),(6,14),(7,14),(8,14),(9,14),(10,14)]
    for x,y in ring: t.p(x,y,S)
    for x,y in ring[:len(ring)//3]: t.p(x,y,SL)
    t.r(7,2,8,13,SD); t.r(2,7,13,8,SD); t.r(6,6,9,9,C); t.r(7,7,8,8,CL)
    t.r(0,15,15,15,SK); t.p(1,14,SK); t.p(14,14,SK)
    return t
def box(t,x0,y0,x1,y1,c=S,cl=SL,cd=SK):
    t.r(x0,y0,x1,y1,c); t.r(x0,y0,x1,y0,cl); t.r(x0,y1,x1,y1,cd); t.r(x1,y0+1,x1,y1,cd)
def e_stove():
    t=T()  # a white range: two hot rings, knobs, an oven door
    box(t,1,3,14,15,WHITE,(250,250,246,255),(170,170,166,255)); t.r(0,2,15,2,SL)
    t.r(2,1,5,1,FIRE); t.r(10,1,13,1,FIRE)
    for x in (3,6,9,12): t.p(x,4,K)
    t.r(3,7,12,13,KD); t.r(4,8,11,12,K); t.r(5,11,10,11,FIRE); t.r(3,6,12,6,M)
    return t
def e_kiln():
    t=T()  # a brick drum bound in steel, a glowing spy hole
    t.r(3,0,12,1,SD); t.r(3,0,12,0,SL)
    t.r(1,2,14,14,B); t.r(1,2,14,2,BL); t.r(1,5,14,5,BD); t.r(1,11,14,11,BD); t.p(4,3,BD); t.p(10,4,BD); t.p(3,13,BD); t.p(11,12,BD)
    t.r(1,8,14,8,S); t.r(5,6,10,10,K); t.r(6,7,9,9,FIRE); t.r(7,8,8,8,HOT)
    t.r(0,15,15,15,SK)
    return t
def e_furnace():
    t=T()  # a dark steel cabinet with a glowing slot and vents
    box(t,2,0,13,15,SD,S,SK)
    t.r(4,2,11,7,K); t.r(5,3,10,6,FIRE); t.r(6,4,9,5,HOT)
    t.r(4,10,11,10,K); t.r(4,12,11,12,K); t.p(12,9,RED)
    return t
def e_press():
    t=T()  # a yellow frame with a bright ram
    Y=YEL; YD=dk(YEL)
    t.r(1,0,14,2,Y); t.r(1,2,14,2,YD); t.r(1,3,2,15,Y); t.r(13,3,14,15,Y); t.p(2,15,YD); t.p(13,15,YD)
    t.r(6,3,9,8,ML); t.r(6,3,6,8,WHITE); t.r(4,9,11,9,SK); t.r(4,12,11,13,S); t.r(4,12,11,12,SL)
    t.r(0,14,15,15,SK); t.p(4,15,Y); t.p(7,15,Y); t.p(10,15,Y)
    return t
def e_metal():
    t=T()  # a steel bench with an anvil and a spark
    table(t,7,SL,S,SD,SK)
    t.r(2,3,7,4,KD); t.r(2,3,7,3,MD); t.r(1,4,1,4,KD); t.r(3,5,6,6,KD)
    t.r(11,2,11,6,YEL); t.p(10,6,HOT); t.p(12,5,WHITE); t.p(9,5,(150,220,255,255)); t.p(13,6,(150,220,255,255))
    t.r(4,11,11,11,SD); t.p(6,12,GREEN); t.p(9,12,RED)
    return t
def e_sluice():
    t=T()  # a sloping trough of running water on a pump box
    for x in range(16):
        y=3+x//3
        t.r(x,y,x,y,BLUE); t.r(x,y+1,x,y+2,WM); t.p(x,y+2,WD)
    t.p(2,3,WHITE); t.p(9,6,WHITE)
    t.r(1,7,2,15,MD); box(t,8,11,14,15); t.r(10,12,12,14,K); t.p(11,13,ML); t.r(15,9,15,15,BLUE)
    return t
def egg_extractor():
    t=T()  # an egg under glass, on a machine base
    G=(170,214,230,140)
    t.r(4,1,11,8,G); t.r(3,3,12,8,G); t.r(5,0,10,0,SL); t.r(3,2,3,2,SL); t.r(12,2,12,2,SL); t.r(2,3,2,8,SL); t.r(13,3,13,8,SL)
    t.r(6,3,9,7,(240,232,210,255)); t.r(7,2,8,2,(240,232,210,255)); t.r(6,7,9,7,(200,190,160,255)); t.p(7,4,WHITE)
    box(t,1,9,14,15,(88,120,150,255),(130,160,190,255),(56,80,104,255)); t.r(3,11,5,13,WHITE); t.p(4,12,RED); t.r(8,11,12,11,GREEN); t.r(8,13,12,13,K)
    return t
def solar():
    t=T()  # a tilted panel of blue cells on a post
    D=(40,70,160,255); L=(80,120,215,255)
    for x in range(16):
        y=9-x//2
        t.p(x,y-1,SL); t.r(x,y,x,y+1,D if (x//3)%2 else L)
    t.r(7,9,8,14,S); t.r(4,15,11,15,SK)
    return t
def refinery():
    t=T()  # a still column and a tank, piped together, with a flare
    t.r(3,0,3,0,HOT); t.r(3,1,3,2,MD)
    t.r(1,3,5,15,S); t.r(1,3,1,15,SL); t.r(5,3,5,15,SK); t.r(1,6,5,6,SD); t.r(1,10,5,10,SD); t.r(1,14,5,14,SD)
    t.r(8,7,14,14,YEL); t.r(8,7,14,7,(240,215,120,255)); t.r(8,14,14,14,dk(YEL)); t.r(8,15,9,15,SK); t.r(13,15,14,15,SK)
    t.r(6,4,11,4,C); t.r(11,4,11,6,C); t.r(6,12,7,12,C); t.r(10,10,12,12,K)
    return t
jobs={'woodwork_bench':woodwork(),'compost_bin':compost(),'easel':easel(),'builder_bench':builder(),'pizza_oven':pizza(),'train_yard':train(),
 'mixing_bench':mixing(),'dye_bench':dye(),'steam_generator':steam_gen(),'flywheel':flywheel(),'electric_stove':e_stove(),'electric_kiln':e_kiln(),
 'electric_furnace':e_furnace(),'electric_press':e_press(),'electric_metalwork_bench':e_metal(),'electric_sluice':e_sluice(),'egg_extractor':egg_extractor(),
 'solar_panel':solar(),'refinery':refinery()}
for i in range(1,8): jobs['tool_bench'+('' if i==1 else '_%d'%i)]=tool_bench(i)
for i in range(1,5): jobs['craft_bench'+('' if i==1 else '_%d'%i)]=craft_bench(i)
for i in range(1,3): jobs['metalwork_bench'+('' if i==1 else '_%d'%i)]=metalwork(i); jobs['tailor_bench'+('' if i==1 else '_%d'%i)]=tailor(i)
for i in range(1,4): jobs['press'+('' if i==1 else '_%d'%i)]=press(i); jobs['armor_bench'+('' if i==1 else '_%d'%i)]=armor(i)
for n,t in jobs.items(): t.save(n)
print(len(jobs))
