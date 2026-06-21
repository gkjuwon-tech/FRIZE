# -*- coding: utf-8 -*-
"""FRIZE VISOR 1인칭 AR 시야 ― 고글 너머로 대원이 보는 화면.
실사 화재현장 위에 택티컬 AR HUD(나침반·표적 브래킷·진입 화살표·생체/가스·미니트윈)를
합성한다. 연기 속에서 '지금 필요한 것'만 띄우는 전술 HUD 톤.
  python3 generate_goggle_view.py   → goggle_ar_view.png (1920x1080)
"""
import os, math
from PIL import Image, ImageDraw, ImageFont, ImageEnhance, ImageFilter
HERE=os.path.dirname(os.path.abspath(__file__))
W,H=1920,1080
CY=(96,214,236); CYd=(60,150,170); AMB=(232,176,72); RED=(232,72,58); WHT=(228,236,240); DIM=(150,170,178)
KO="/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"
MONO="/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"; MONOB="/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
def F(p,s):
    try:return ImageFont.truetype(p,s)
    except:return ImageFont.load_default()
ko=lambda s:F(KO,s); mono=lambda s:F(MONO,s); monob=lambda s:F(MONOB,s)

# ── 베이스: 화재현장 실사(cover-fit, 약간 어둡게+쿨) ──
base=None
for c in ("goggle_pov.jpg","../../frize_cockpit/assets/goggle_pov.jpg"):
    p=os.path.join(HERE,c)
    if os.path.exists(p): base=Image.open(p).convert("RGB"); break
if base is None: base=Image.new("RGB",(W,H),(30,26,24))
bw,bh=base.size; s=max(W/bw,H/bh); base=base.resize((int(bw*s),int(bh*s)))
base=base.crop(((base.width-W)//2,(base.height-H)//2,(base.width-W)//2+W,(base.height-H)//2+H))
base=ImageEnhance.Brightness(base).enhance(0.82); base=ImageEnhance.Color(base).enhance(0.78)
img=base.convert("RGB"); d=ImageDraw.Draw(img,"RGBA")

def line(p,col,w=1): d.line(p,fill=col,width=w)
def text(x,y,s,f,c,anchor="la"): d.text((x,y),s,font=f,fill=c,anchor=anchor)
def bracket(x0,y0,x1,y1,col,g=18,w=3):
    for (a,b),(dx,dy) in [((x0,y0),(1,1)),((x1,y0),(-1,1)),((x0,y1),(1,-1)),((x1,y1),(-1,-1))]:
        line([(a,b),(a+dx*g,b)],col,w); line([(a,b),(a,b+dy*g)],col,w)

# ── 상단: 나침반 헤딩 스트립 ──
d.rectangle([0,0,W,4],fill=(*CY,180))
hdg=58
for deg in range(hdg-60,hdg+61,5):
    x=W/2+(deg-hdg)*9; major=(deg%45==0)
    line([(x,16),(x,16+(14 if major else 7))],(*WHT,180 if major else 90),2 if major else 1)
    if major:
        lbl={0:"N",45:"NE",90:"E",135:"SE",180:"S",225:"SW",270:"W",315:"NW",360:"N"}.get(deg%360,str(deg%360))
        text(x,36,lbl,monob(15),WHT,"ma")
# 헤딩 포인터
d.polygon([(W/2-7,12),(W/2+7,12),(W/2,24)],fill=(*CY,255))
text(W/2,54,f"{hdg:03d}°  ENU",mono(13),CY,"ma")

# 코너 정보(상단)
text(28,22,"VISOR-1",monob(20),WHT); text(28,46,"대원 A · 진입조",ko(14),DIM)
text(W-28,22,"09:41:22",monob(19),WHT,"ra"); text(W-28,46,"5G · MESH OK · REC ●",mono(13),(*RED,255),"ra")

# ── 표적 브래킷(요구조자/화점/가스) ──
# 요구조자
bx=(int(W*0.40),int(H*0.46),int(W*0.50),int(H*0.74)); bracket(*bx,RED)
text((bx[0]+bx[2])//2,bx[1]-26,"▼ PERSON",monob(17),RED,"ma")
text((bx[0]+bx[2])//2,bx[3]+6,"요구조자 · 4.2 m · 36.5℃",ko(14),RED,"ma")
# 화점
fx=(int(W*0.60),int(H*0.40),int(W*0.69),int(H*0.62)); bracket(*fx,AMB)
text((fx[0]+fx[2])//2,fx[1]-24,"FIRE 640℃",monob(16),AMB,"ma")
# 가스 포켓
gx=(int(W*0.26),int(H*0.34),int(W*0.33),int(H*0.50)); bracket(*gx,AMB,12,2)
text((gx[0]+gx[2])//2,gx[1]-22,"CO 1180 ppm",mono(13),AMB,"ma")

# ── 중앙 레티클 ──
cx,cy=W//2,H//2-10
for a in (0,90,180,270):
    dx,dy=math.cos(math.radians(a)),math.sin(math.radians(a))
    line([(cx+dx*10,cy+dy*10),(cx+dx*22,cy+dy*22)],(*WHT,150),2)
d.ellipse([cx-3,cy-3,cx+3,cy+3],outline=(*WHT,170),width=1)

# ── 대형 진입(출구) 내비 화살표 ──
ax,ay=int(W*0.50),int(H*0.80)
chev=[(ax-70,ay-34),(ax,ay),(ax-70,ay+34),(ax-50,ay+34),(ax+20,ay),(ax-50,ay-34)]
d.polygon([(p[0]+40,p[1]) for p in chev],fill=(*CY,210))
text(ax+120,ay-12,"출구 EXIT",ko(20),CY); text(ax+120,ay+14,"12 m · 우측 복도",ko(15),WHT)

# ── 하단 HUD 바: 생체 + 가스 + 환경 ──
by=H-120
d.rectangle([0,by,W,H],fill=(8,12,16,150))
line([(0,by),(W,by)],(*CYd,150),1)
def gauge(x,label,val,frac,col,unit=""):
    text(x,by+16,label,ko(13),DIM); text(x,by+34,val,monob(22),WHT)
    text(x+74,by+44,unit,mono(12),DIM)
    d.rectangle([x,by+66,x+150,by+71],fill=(60,68,74,200))
    d.rectangle([x,by+66,x+int(150*max(0,min(1,frac))),by+71],fill=(*col,255))
gauge(40,"심박 HR","171",0.85,RED,"bpm")
gauge(230,"산소 O₂","62",0.62,AMB,"%")
gauge(420,"SCBA 잔압","148",0.49,CY,"bar")
gauge(610,"체온","38.4",0.7,AMB,"℃")
# 가스 패널
gx0=820
text(gx0,by+16,"대기/가스",ko(13),DIM)
for i,(k,v,c) in enumerate([("CO","1180 ppm",RED),("LEL","11 %",AMB),("O₂","18.4 %",AMB),("HCN","6 ppm",DIM),("AMB","86 ℃",AMB)]):
    text(gx0+i*150,by+38,k,mono(13),DIM); text(gx0+i*150,by+56,v,monob(16),c)
# 우하단: 임무/배터리
text(W-40,by+20,"임무: 3F-E 요구조자 구조",ko(15),WHT,"ra")
text(W-40,by+46,"고글 배터리 62%  ·  컴퓨트 71℃",mono(13),DIM,"ra")
text(W-40,by+72,"명령: ADVANCE → 우측 복도 진입",ko(15),CY,"ra")

# ── 우상단: 미니 트윈(자기 위치 + 팀 + 화점) ──
mx,my,mw,mh=W-300,86,260,160
d.rectangle([mx,my,mx+mw,my+mh],fill=(6,10,14,180),outline=(*CYd,160))
text(mx+10,my+8,"TWIN · 2F",mono(12),DIM)
# 간이 평면(방 사각형들)
for rx,ry,rw,rh in [(20,40,70,50),(100,40,60,50),(170,40,70,50),(20,100,110,40),(150,100,90,40)]:
    d.rectangle([mx+rx,my+ry,mx+rx+rw,my+ry+rh],outline=(80,92,100,200))
# 마커
def mk(fx,fy,col,r=4): d.ellipse([mx+fx-r,my+fy-r,mx+fx+r,my+fy+r],fill=col)
mk(60,70,CY); mk(130,120,(120,200,140,255)); mk(200,70,(120,200,140,255))
mk(45,60,RED); d.ellipse([mx+115,my+115,mx+145,my+145],outline=(*RED,180),width=2)  # 화점

# ── 바이저 비네팅 + 스캔라인 ──
vg=Image.new("L",(W,H),0); ImageDraw.Draw(vg).rounded_rectangle([60,40,W-60,H-40],radius=120,fill=255)
vg=vg.filter(ImageFilter.GaussianBlur(70))
img=Image.composite(img,ImageEnhance.Brightness(img).enhance(0.35),vg)
d=ImageDraw.Draw(img,"RGBA")
sl=Image.new("L",(W,H),0); sd=ImageDraw.Draw(sl)
for y in range(0,H,3): sd.line([(0,y),(W,y)],fill=10)
img=Image.composite(Image.new("RGB",(W,H),(0,0,0)),img,sl)

img.save(os.path.join(HERE,"goggle_ar_view.png"))
print("wrote goggle_ar_view.png")
