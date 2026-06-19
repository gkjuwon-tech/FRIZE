# -*- coding: utf-8 -*-
"""FRIZE 콕핏 UI 합성 샷 ― 실전 지상관제(GCS) 톤.
참고: Mission Planner / QGroundControl 같은 실제 Qt/데스크톱 관제툴.
원칙: 유틸리티적·고밀도·플랫·네이티브 위젯, 맵 중심, 상태바. 그림자/라운드카드/네온/컨셉사진 없음.
중앙 맵은 합성 사이트맵, 좌상단은 실제 카메라 영상 타일.
"""
import os, math
from PIL import Image, ImageDraw, ImageFont, ImageEnhance

HERE=os.path.dirname(os.path.abspath(__file__)); W,H=1680,980
# ── 유틸리티 뉴트럴 팔레트 ──
TITLE=(60,64,68); TOOL=(51,54,58); BARDIV=(70,75,81)
PANEL=(43,46,50); PANEL2=(38,41,45); MAPBG=(28,31,38); BLOCK=(35,39,46); ROAD=(47,52,60)
HAIR=(64,69,75); HAIR2=(54,58,64)
TEXT=(214,217,221); SUB=(139,145,153); FAINT=(96,102,110)
GREEN=(63,166,106); AMBER=(199,154,58); RED=(192,57,43); BLUE=(74,144,194); STEEL=(120,140,152)
def Fn(p,s):
    try:return ImageFont.truetype(p,s)
    except:return ImageFont.load_default()
SEG="C:/Windows/Fonts/segoeui.ttf";SEGB="C:/Windows/Fonts/segoeuib.ttf"
MONO="C:/Windows/Fonts/consola.ttf";MONOB="C:/Windows/Fonts/consolab.ttf";KO="C:/Windows/Fonts/malgun.ttf"
seg=lambda s:Fn(SEG,s);segb=lambda s:Fn(SEGB,s);mono=lambda s:Fn(MONO,s);monob=lambda s:Fn(MONOB,s);ko=lambda s:Fn(KO,s)
img=Image.new("RGB",(W,H),PANEL2);d=ImageDraw.Draw(img,"RGBA")
def A(c,a=255):return (c[0],c[1],c[2],a)
def t(x,y,s,f,c,anchor="la"):d.text((x,y),s,font=f,fill=A(c) if isinstance(c,tuple) and len(c)==3 else c,anchor=anchor)
def box(x0,y0,x1,y1,fill=PANEL,outline=HAIR):d.rectangle([x0,y0,x1,y1],fill=A(fill) if fill else None,outline=A(outline) if outline else None,width=1)
def groupbox(x0,y0,x1,y1,title):
    box(x0,y0,x1,y1,PANEL,HAIR)
    d.rectangle([x0,y0,x1,y0+20],fill=A(PANEL2)); d.line([x0,y0+20,x1,y0+20],fill=A(HAIR2))
    t(x0+8,y0+10,title,segb(11),SUB,anchor="lm")

# ===== 타이틀바 =====
d.rectangle([0,0,W,28],fill=A(TITLE))
t(10,14,"FRIZE Cockpit",segb(12),TEXT,anchor="lm")
t(150,14,"—  Site: SEOUL-HQ  ·  incident #2026-0618-014",seg(11),SUB,anchor="lm")
for i,(sym) in enumerate(["—","▢","✕"]):
    cx=W-66+i*22; t(cx,14,sym,seg(11),SUB if i<2 else (200,120,120),anchor="mm")

# ===== 툴바(메뉴 탭 + 연결상태) =====
d.rectangle([0,28,W,70],fill=A(TOOL)); d.line([0,70,W,70],fill=A(BARDIV))
tabs=["OVERVIEW","MAP","UNITS","PLAYBACK","SETTINGS"]; tx=12
for i,tb in enumerate(tabs):
    w=d.textlength(tb,font=segb(12))+22
    if i==0:
        d.rectangle([tx,30,tx+w,69],fill=A(PANEL)); d.rectangle([tx,30,tx+w,32],fill=A(BLUE))
    t(tx+w/2,50,tb,segb(12),TEXT if i==0 else SUB,anchor="mm"); tx+=w
# 연결 상태 인디케이터(우측, 소형)
def ind(x,label,val,col):
    t(x,42,label,mono(10),FAINT,anchor="la"); d.ellipse([x,55,x+8,63],fill=A(col)); t(x+12,59,val,mono(11),TEXT,anchor="lm");
    return x
rx=W-560
for label,val,col in [("MQTT","1883 up",GREEN),("CORE","8000 up",GREEN),("GNSS","RTK 24sv",BLUE),("UNITS","3 online",GREEN),("ALERTS","4",RED)]:
    t(rx,42,label,mono(9),FAINT); d.ellipse([rx,54,rx+7,61],fill=A(col)); t(rx+11,58,val,mono(10),TEXT,anchor="lm")
    rx+=d.textlength(val,font=mono(10))+34
t(W-12,50,"21:47:55",mono(12),TEXT,anchor="rm")

# ===== 상태바 =====
d.rectangle([0,958,W,980],fill=A(TOOL)); d.line([0,958,W,958],fill=A(BARDIV))
t(10,969,"MQTT tcp://localhost:1883  connected",mono(10),SUB,anchor="lm")
t(430,969,"ENU origin 37.56650, 126.97800  (Seoul)",mono(10),SUB,anchor="lm")
t(W-12,969,"FRIZE Core v0.1.0   |   render 5 Hz   |   interlock: ON",mono(10),SUB,anchor="rm")

# ================= 좌측 컬럼 =================
LX0,LX1=8,618
# --- 영상 타일 ---
VY0,VY1=80,360; groupbox(LX0,VY0,LX1,VY1,"VIDEO  ·  visor-01 / ALPHA-1  ·  THERMAL")
vx0,vy0,vx1,vy1=LX0+1,VY0+21,LX1-1,VY1-1
fg=Image.open(os.path.join(HERE,"fireground.jpg")).convert("RGB")
vw,vh=vx1-vx0,vy1-vy0; sc=max(vw/fg.width,vh/fg.height)
fg2=fg.resize((int(fg.width*sc),int(fg.height*sc))); ox=(fg2.width-vw)//2; oy=(fg2.height-vh)//2
vid=fg2.crop((ox,oy,ox+vw,oy+vh))
vid=ImageEnhance.Color(vid).enhance(0.25); vid=ImageEnhance.Contrast(vid).enhance(1.05); vid=ImageEnhance.Brightness(vid).enhance(0.9)
img.paste(vid,(vx0,vy0)); d=ImageDraw.Draw(img,"RGBA")
# 감지 1개(절제: 가는 사각 + 평범한 라벨)
bx0,by0,bx1,by1=vx0+int(vw*0.40),vy0+int(vh*0.55),vx0+int(vw*0.40)+int(vw*0.17),vy0+int(vh*0.55)+int(vh*0.30)
d.rectangle([bx0,by0,bx1,by1],outline=A((230,230,230),200),width=1)
t(bx0,by0-13,"downed person  0.92",mono(10),(235,235,235))
# 영상 OSD(좌하단 텍스트, MP처럼)
t(vx0+8,vy1-30,"GAS CO 1480  O2 18.2  LEL 12",mono(10),(235,210,120))
t(vx0+8,vy1-16,"REC ●  640x512  60Hz  lat 38ms",mono(10),(200,205,210))
t(vx1-8,vy0+12,"AR-LINK ▲",mono(10),GREEN,anchor="rt")

# --- 명령 툴바(플랫 텍스트 버튼) ---
CY0,CY1=366,398; box(LX0,CY0,LX1,CY1,PANEL,HAIR)
t(LX0+8,CY0+16,"CMD",segb(11),SUB,anchor="lm")
cmds=[("Highlight",TEXT),("Advance",TEXT),("Retreat",AMBER),("Recon",FAINT),("Hold",FAINT),("RTL",FAINT),("Spray",RED)]
cx=LX0+52
for name,col in cmds:
    w=d.textlength(name,font=seg(12))+18; d.line([cx,CY0+6,cx,CY1-6],fill=A(HAIR2))
    t(cx+w/2+1,CY0+16,name,seg(12),col,anchor="mm"); cx+=w+2

# --- 텔레메트리(선택 유닛, 고밀도 key/value) ---
TY0,TY1=404,642; groupbox(LX0,TY0,LX1,TY1,"TELEMETRY  ·  ALPHA-1 (visor-01)")
rows=[("CO",  "1480 ppm",RED), ("O2","18.2 %",RED), ("LEL","12 %",RED), ("H2S","22 ppm",AMBER),
      ("SCBA","48 bar",AMBER), ("HR","168 bpm",AMBER), ("AMB.TEMP","54 °C",AMBER), ("STATE","CRITICAL",RED),
      ("BATT","73 %",TEXT), ("LINK","5G -71dBm",TEXT), ("HDG","215°",TEXT), ("CPU","58 °C",TEXT),
      ("POS E","+12.4 m",TEXT), ("POS N","-8.1 m",TEXT), ("POS U","+1.6 m",TEXT), ("FIX","RTK",BLUE)]
cw=(LX1-LX0-2)//2; ry=TY0+30
for i,(k,v,col) in enumerate(rows):
    coli=i%2; rowi=i//2
    x=LX0+6+coli*cw; y=TY0+30+rowi*26
    t(x,y,k,mono(11),SUB); t(x+cw-12,y,v,monob(12),col,anchor="ra")
    d.line([x,y+18,x+cw-10,y+18],fill=A(HAIR2))

# --- 경보 로그(테이블) ---
AY0,AY1=648,954; groupbox(LX0,AY0,LX1,AY1,"ALERTS  ·  4 active")
t(LX0+10,AY0+32,"TIME",mono(10),FAINT); t(LX0+78,AY0+32,"LV",mono(10),FAINT)
t(LX0+108,AY0+32,"MESSAGE",mono(10),FAINT); t(LX1-70,AY0+32,"UNIT",mono(10),FAINT)
d.line([LX0+6,AY0+46,LX1-6,AY0+46],fill=A(HAIR))
alerts=[("21:47:31","CRIT",RED,"치명가스 CO_IDLH 초과 — 즉시 대피 지시","visor-02"),
        ("21:47:18","CRIT",RED,"쓰러진 사람 감지 — 최우선 구조","visor-01"),
        ("21:46:55","HIGH",AMBER,"SCBA 잔압 48bar — 후퇴 검토","visor-02"),
        ("21:46:40","HIGH",AMBER,"가스통 화점 4m 근접 — 방수 주의","visor-01"),
        ("21:46:02","INFO",SUB,"SCOUT-1 정찰 구역 B 진입","scout-01"),
        ("21:45:40","INFO",SUB,"ALPHA-1 3층 동측 진입","visor-01")]
ay=AY0+54
for tm,lv,col,msg,unit in alerts:
    t(LX0+10,ay,tm,mono(10),SUB)
    d.rectangle([LX0+78,ay+1,LX0+98,ay+13],fill=A(col)); t(LX0+88,ay+7,lv[0],monob(9),(20,22,24),anchor="mm")
    t(LX0+108,ay,msg,ko(11),TEXT); t(LX1-70,ay,unit,mono(10),SUB)
    d.line([LX0+6,ay+19,LX1-6,ay+19],fill=A(HAIR2)); ay+=24

# ================= 맵 (중앙/우측, 톱다운 사이트맵) =================
MX0,MY0,MX1,MY1=626,80,1672,954; box(MX0,MY0,MX1,MY1,MAPBG,HAIR)
# 도로(굵은 라인)
for y in [MY0+230,MY0+560]:
    d.rectangle([MX0,y,MX1,y+22],fill=A(ROAD))
for x in [MX0+300,MX0+640]:
    d.rectangle([x,MY0,x+22,MY1],fill=A(ROAD))
# 블록(건물 footprint)
import random; random.seed(5)
for _ in range(26):
    bx=random.randint(MX0+10,MX1-130); by=random.randint(MY0+10,MY1-110)
    bw=random.randint(60,150); bh=random.randint(50,120)
    d.rectangle([bx,by,bx+bw,by+bh],fill=A(BLOCK),outline=A(HAIR2))
# 격자(옅게)
for gx in range(MX0,MX1,52): d.line([gx,MY0,gx,MY1],fill=A((255,255,255),6))
for gy in range(MY0,MY1,52): d.line([MX0,gy,MX1,gy],fill=A((255,255,255),6))
# 사고 건물(강조 footprint)
icx,icy=MX0+540,MY0+430
d.rectangle([icx-90,icy-70,icx+90,icy+70],outline=A(AMBER),width=2); t(icx-86,icy-86,"INCIDENT BLDG · 5F",mono(10),AMBER)
# 위험구역(반투명 채움 + 가는 외곽 + 작은 라벨)
for (cx,cy,r,col,lab) in [(icx+10,icy+10,70,RED,"FIRE"),(icx+60,icy-30,34,AMBER,"GAS CYL"),(icx-40,icy+40,30,RED,"DOWNED")]:
    ov=Image.new("RGBA",(W,H),(0,0,0,0)); ImageDraw.Draw(ov).ellipse([cx-r,cy-r,cx+r,cy+r],fill=A(col,46))
    base=Image.alpha_composite(img.convert("RGBA"),ov).convert("RGB"); img.paste(base,(0,0))
    d=ImageDraw.Draw(img,"RGBA")
    d.ellipse([cx-r,cy-r,cx+r,cy+r],outline=A(col,200),width=1)
    d.rectangle([cx-r,cy-r,cx-r+d.textlength(lab,font=mono(9))+8,cy-r+13],fill=A((20,22,26),200)); t(cx-r+4,cy-r+6,lab,mono(9),col,anchor="lm")
# 유닛 마커
def unit(cx,cy,kind,name,sel=False,hdg=0):
    col=STEEL if kind=="scout" else (RED if "B2" in name else GREEN)
    if kind=="scout":
        d.polygon([(cx,cy-7),(cx-6,cy+6),(cx+6,cy+6)],fill=A(col))
        ex=cx+int(16*math.sin(math.radians(hdg))); ey=cy-int(16*math.cos(math.radians(hdg))); d.line([cx,cy,ex,ey],fill=A(col),width=1)
    else:
        d.ellipse([cx-6,cy-6,cx+6,cy+6],fill=A(col))
    if sel: d.ellipse([cx-11,cy-11,cx+11,cy+11],outline=A((235,235,235),200),width=1)
    d.rectangle([cx+9,cy-7,cx+9+d.textlength(name,font=mono(10))+8,cy+7],fill=A((20,22,26),190))
    t(cx+13,cy,name,mono(10),TEXT,anchor="lm")
unit(icx-30,icy+30,"visor","ALPHA-1",sel=True)
unit(icx+70,icy+90,"visor","BRAVO-2")
unit(icx-160,icy+150,"scout","SCOUT-1",hdg=40)
# 북 화살표 + 스케일바
t(MX1-30,MY0+24,"N",segb(13),TEXT,anchor="mm"); d.line([MX1-30,MY0+30,MX1-30,MY0+48],fill=A(TEXT),width=1); d.polygon([(MX1-30,MY0+28),(MX1-34,MY0+36),(MX1-26,MY0+36)],fill=A(TEXT))
d.line([MX0+20,MY1-24,MX0+20+52,MY1-24],fill=A(TEXT),width=2); t(MX0+20,MY1-40,"10 m",mono(10),SUB)
# 맵 상태 스트립
d.rectangle([MX0,MY1-20,MX1,MY1],fill=A(PANEL2)); d.line([MX0,MY1-20,MX1,MY1-20],fill=A(HAIR2))
t(MX0+8,MY1-10,"zoom 1:250   grid 10 m   3 units   4 hazards   sel ALPHA-1   pointer E +14.2 N -3.0",mono(10),SUB,anchor="lm")
t(MX0+540,MY0+10,"TACTICAL MAP — site ENU (top-down)",mono(10),SUB)

out=os.path.abspath(os.path.join(HERE,"..","..","..","FRIZE_cockpit_shot.png"))
img.convert("RGB").save(out); print("saved",out,img.size)
