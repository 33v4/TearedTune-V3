"""Renders docs/TearedTune_UI_preview.png.

DESIGN PREVIEW: drawn with Python/Pillow from the same layout constants, colours and default parameter values
as Source/PluginEditor.cpp. It is NOT a screenshot of the running plugin (no JUCE/Windows available when it was made).
Usage: python3 tools/make_preview.py docs/TearedTune_UI_preview.png
"""
import math, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUT = sys.argv[1] if len(sys.argv) > 1 else "TearedTune_UI_preview.png"
S = 3; W, H = 1020, 620
def C(h, a=255): h = h.lstrip('#'); return (int(h[0:2],16), int(h[2:4],16), int(h[4:6],16), a)
BG_TOP, BG_BOT = C('0a1628'), C('050912'); PANEL, PANEL2, BORDER = C('0f1c33'), C('14243f'), C('203a5e')
CYAN, PINK, TEXT, DIM, TRACK = C('3ad0ff'), C('ff5fa8'), C('e8f4ff'), C('7f9bbd'), C('1f3350')

def font(sz, bold=False):
    for c in (["/usr/share/fonts/opentype/inter/Inter-Bold.otf"] if bold else ["/usr/share/fonts/opentype/inter/Inter-Regular.otf"]) + ["/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"]:
        try: return ImageFont.truetype(c, int(sz*S))
        except Exception: pass
    return ImageFont.load_default()
def lerp(a, b, t): return tuple(int(a[i]+(b[i]-a[i])*t) for i in range(4))
def sc(v): return [int(x*S) for x in v]

img = Image.new("RGBA", (W*S, H*S)); d = ImageDraw.Draw(img, "RGBA")
for y in range(H*S): d.line([(0,y),(W*S,y)], fill=lerp(BG_TOP, BG_BOT, y/(H*S)))
for x in range(W*S): d.line([(x,0),(x,72*S)], fill=lerp(C('0d2748'), C('134a78'), x/(W*S)))
d.rectangle(sc((0,72,W,74)), fill=CYAN[:3]+(150,))

def drop(cx, cy, r, col):
    pts = [(cx, cy-r*1.9), (cx-r*0.85, cy-r*0.526), (cx+r*0.85, cy-r*0.526)]
    d.polygon([(x*S, y*S) for x,y in pts], fill=col); d.ellipse(sc((cx-r,cy-r,cx+r,cy+r)), fill=col)
for x,y,r in zip([395,435,473,510,542],[24,48,20,46,28],[6,4.5,7,5,4]): drop(x,y,r,CYAN[:3]+(70,))
drop(38,42,11,CYAN); drop(38,42,4.5,C('0d2748'))
d.text((64*S,29*S), "TEARED TUNE", font=font(32,True), fill=TEXT, anchor="lm")
d.rounded_rectangle(sc((306,20,346,40)), 6*S, fill=PINK); d.text((326*S,30*S), "V3", font=font(13,True), fill=C('1a0610'), anchor="mm")
d.text((66*S,56*S), "REAL-TIME PITCH CORRECTION", font=font(11,True), fill=TEXT[:3]+(190,), anchor="lm")
for (x0,x1,t,hl) in [(560,656,"NATURAL",False),(662,758,"CLASSIC",True),(764,864,"HARD TUNE",False),(896,996,"BYPASS",False)]:
    d.rounded_rectangle(sc((x0,22,x1,50)), 8*S, fill=C('1b3558') if hl else PANEL2, outline=BORDER, width=S)
    d.text(((x0+x1)/2*S,36*S), t, font=font(12.5,True), fill=TEXT, anchor="mm")

def panel(x,y,w,h,title):
    d.rounded_rectangle(sc((x,y,x+w,y+h)), 10*S, fill=PANEL, outline=BORDER, width=S)
    d.rounded_rectangle(sc((x+14,y+12,x+18,y+26)), 2*S, fill=CYAN)
    d.text(((x+24)*S,(y+19)*S), title, font=font(13,True), fill=TEXT, anchor="lm")
panel(12,84,996,236,"PITCH MONITOR"); panel(12,332,372,276,"KEY & SCALE"); panel(396,332,330,276,"CORRECTION"); panel(738,332,270,276,"PITCH & OUTPUT")

# ---------------- pitch graph ----------------
gx0,gy0,gx1,gy1 = 20,108,1000,312
d.rounded_rectangle(sc((gx0,gy0,gx1,gy1)), 8*S, fill=C('070e1c'), outline=BORDER, width=S)
plot = (gx0+50, gy0+8, gx1-52, gy1-8)
mask = {0,2,4,5,7,9,11}   # C major
notes = [60,62,64,67,65,64,62,60]; offs=[35,-30,25,-45,40,-20,30,-35]
N=256; rng=np.random.default_rng(3)
det=[]; tgt=[]
for k in range(N):
    t=k/N*8.0; i=min(7,int(t)); lt=t-i
    vib = 0.18*math.sin(2*math.pi*5.5*lt*1.0) if lt>0.3 else 0
    gap = (k%64) > 58
    det.append(None if gap else notes[i]+offs[i]/100+vib+rng.normal(0,.015))
    tgt.append(None if gap else notes[i]+ (0.45*vib if True else 0))
center=63.0; rangeS=7.0
def my(m): return (plot[1]+plot[3])/2 - (m-center)/rangeS*((plot[3]-plot[1])/2)
names=["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]
for n in range(55,72):
    y=my(n); on=(n%12) in mask
    if y<plot[1]-6 or y>plot[3]+6: continue
    d.line([(plot[0]*S,y*S),(plot[2]*S,y*S)], fill=CYAN[:3]+(56,) if on else (255,255,255,13), width=int((1.2 if on else 1)*S))
    d.text(((gx0+8)*S,y*S), f"{names[n%12]}{n//12-1}", font=font(12 if on else 10, on), fill=TEXT if on else DIM[:3]+(115,), anchor="lm")
def curve(vals, col, th):
    layer=Image.new("RGBA", img.size, (0,0,0,0)); ld=ImageDraw.Draw(layer); seg=[]
    def flush():
        if len(seg)>1:
            ld.line(seg, fill=col[:3]+(56,), width=int((th+4)*S), joint="curve"); ld.line(seg, fill=col, width=int(th*S), joint="curve")
        seg.clear()
    for k,v in enumerate(vals):
        if v is None: flush(); continue
        seg.append(((plot[0]+(plot[2]-plot[0])*k/(N-1))*S, my(v)*S))
    flush(); img.alpha_composite(layer)
curve(tgt, PINK, 2.4); curve(det, CYAN, 1.8)
d = ImageDraw.Draw(img, "RGBA")
d.text(((plot[0]+6)*S,(gy0+14)*S), "YOUR PITCH", font=font(10.5,True), fill=CYAN, anchor="lm")
d.text(((plot[0]+96)*S,(gy0+14)*S), "CORRECTED", font=font(10.5,True), fill=PINK, anchor="lm")
d.text(((plot[2]-6)*S,(gy0+16)*S), "C4   -35 ct", font=font(15,True), fill=TEXT, anchor="rm")
for x,lv,nm in [(plot[2]+8,0.62,"IN"),(plot[2]+26,0.55,"OUT")]:
    m=(x,plot[1]+14,x+9,plot[3]-22); d.rounded_rectangle(sc(m),3*S,fill=TRACK)
    top=m[3]-(m[3]-m[1])*lv
    for yy in range(int(top*S), int(m[3]*S)): d.line([(m[0]*S,yy),(m[2]*S,yy)], fill=lerp(PINK,CYAN,(yy-m[1]*S)/((m[3]-m[1])*S)))
    d.text(((x+4.5)*S,(m[3]+9)*S), nm, font=font(9.5,True), fill=DIM, anchor="mm")

# ---------------- key & scale ----------------
def combo(x,y,w,text,cap,cx,cy):
    d.text((cx*S,cy*S), cap, font=font(11,True), fill=DIM, anchor="lm")
    d.rounded_rectangle(sc((x,y,x+w,y+28)), 6*S, fill=PANEL2, outline=BORDER, width=S)
    d.text(((x+10)*S,(y+14)*S), text, font=font(14), fill=TEXT, anchor="lm")
    d.polygon(sc((x+w-20,y+11,x+w-10,y+11,x+w-15,y+17)), fill=CYAN)
combo(24,380,84,"C","KEY",24,371); combo(118,380,254,"Major","SCALE",118,371); combo(24,432,348,"Mid voice (80-800 Hz)","VOICE RANGE",24,423)
d.text((24*S,477*S), "ALLOWED NOTES  -  CLICK A KEY TO TURN IT ON / OFF", font=font(11,True), fill=DIM, anchor="lm")
kx,ky,kw,kh=24,488,348,108; wW=kw/7; whites=[0,2,4,5,7,9,11]; blacks={1:1,3:2,6:4,8:5,10:6}
for i,pc in enumerate(whites):
    on = pc in mask; r=(kx+wW*i+1,ky,kx+wW*(i+1)-1,ky+kh)
    d.rounded_rectangle(sc(r),5*S,fill=CYAN if on else PANEL2,outline=C('7ee0ff') if on else BORDER,width=S)
    d.text(((r[0]+r[2])/2*S,(r[3]-14)*S), names[pc], font=font(13,True), fill=C('04121e') if on else DIM, anchor="mm")
    if pc==0: d.ellipse(sc(((r[0]+r[2])/2-5,ky+8,(r[0]+r[2])/2+5,ky+18)), fill=PINK)
for pc,wi in blacks.items():
    on = pc in mask; bw=wW*0.6; cx=kx+wW*wi; r=(cx-bw/2,ky,cx+bw/2,ky+kh*0.62)
    d.rounded_rectangle(sc(r),5*S,fill=C('1c8fb8') if on else C('070e1c'),outline=BORDER,width=S)
    d.text(((r[0]+r[2])/2*S,(r[3]-12)*S), names[pc], font=font(11,True), fill=C('04121e') if on else DIM, anchor="mm")

# ---------------- knobs ----------------
def knob(cx,cy,r,pos,name,txt,top,bipolar=False):
    d.text((cx*S,top*S), name, font=font(11.5,True), fill=DIM, anchor="mm")
    st,en=1.2*math.pi,2.8*math.pi; ang=st+pos*(en-st); tr=r-2
    box=sc((cx-tr,cy-tr,cx+tr,cy+tr))
    d.arc(box, math.degrees(st)-90, math.degrees(en)-90, fill=TRACK, width=4*S)
    a0,a1 = ((st+en)/2, ang) if bipolar else (st, ang)
    if a0>a1: a0,a1=a1,a0
    if a1-a0>0.01:
        d.arc(box, math.degrees(a0)-90, math.degrees(a1)-90, fill=CYAN[:3]+(64,), width=9*S)
        d.arc(box, math.degrees(a0)-90, math.degrees(a1)-90, fill=CYAN, width=4*S)
    br=r-9
    for i in range(int(br*2*S)):
        t=i/(br*2*S); y=(cy-br)*S+i; half=math.sqrt(max(0,(br*S)**2-(y-cy*S)**2))
        d.line([(cx*S-half,y),(cx*S+half,y)], fill=lerp(C('2b4568'),C('0a1424'),t))
    d.ellipse(sc((cx-br,cy-br,cx+br,cy+br)), outline=C('3a5a85'), width=int(1.2*S))
    qx,qy=cx+math.sin(ang)*br*0.42,cy-math.cos(ang)*br*0.42; px,py=cx+math.sin(ang)*br*0.88,cy-math.cos(ang)*br*0.88
    d.line([(qx*S,qy*S),(px*S,py*S)], fill=PINK, width=int(3.6*S))
    d.text((cx*S,(cy+r+10)*S), txt, font=font(12.5), fill=TEXT, anchor="mm")
def pos(v,lo,hi,sk=1): return ((v-lo)/(hi-lo))**sk
def grid(x,y,w,h,cols,items):
    cw=(w-20)/cols; rows=math.ceil(len(items)/cols); rh=(h-40)/rows
    for i,(nm,p,txt,bi) in enumerate(items):
        cx=x+10+cw*(i%cols)+cw/2; ty=y+32+rh*(i//cols); r=min(cw/2-14,(rh-18-30)/2)
        knob(cx, ty+18+r+3, r, p, nm, txt, ty+9, bi)
grid(396,332,330,276,2,[("RETUNE SPEED",pos(20,0,400,.5),"20 ms",False),("AMOUNT",1.0,"100 %",False),("KEEP VIBRATO",0.0,"0 %",False),("FLEX",0.0,"0 ct",False)])
grid(738,332,270,276,3,[("TRANSPOSE",.5,"0 st",True),("DETUNE",.5,"0 ct",True),("REF A",.5,"440.0 Hz",False),("MIX",1.0,"100 %",False),("INPUT",.5,"0.0 dB",True),("OUTPUT",.5,"0.0 dB",True)])

img = img.resize((W*2,H*2), Image.LANCZOS).convert("RGB"); img.save(OUT); print("saved", OUT, img.size)
