#!/usr/bin/env python3
# 타이틀/아웃트로 카드 생성(1920x1080) ― 콕핏 시연 영상 앞뒤
import json, sys
from PIL import Image, ImageDraw, ImageFont
DIR=sys.argv[1] if len(sys.argv)>1 else "/tmp/mission"
M=json.load(open(DIR+"/mission.json"))
W,Hh=1920,1080
BG=(8,10,13); TX=(232,237,242); TX2=(150,160,172); DIM=(96,105,116); ACC=(120,170,225)
OK=(96,162,124); CRIT=(214,78,64)
KO="/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"; RB="/tmp/imgui/misc/fonts/Roboto-Medium.ttf"; CO="/tmp/imgui/misc/fonts/Cousine-Regular.ttf"
def Ft(p,s): return ImageFont.truetype(p,s)
def card():
    im=Image.new("RGB",(W,Hh),BG); d=ImageDraw.Draw(im,"RGBA")
    return im,d
def cx_text(d,y,s,font,fill):
    w=d.textlength(s,font=font); d.text(((W-w)/2,y),s,font=font,fill=fill); return w

# ── 타이틀 ──
im,d=card()
# 헥사 마크
d.regular_polygon((W/2,360,46),6,outline=TX,width=4); d.regular_polygon((W/2,360,18),6,outline=TX,width=2)
x=W/2-150; f=Ft(RB,86)
word="FRIZE"; tot=sum(d.textlength(c,font=f)+14 for c in word)-14; x=(W-tot)/2
for c in word: d.text((x,440),c,font=f,fill=TX); x+=d.textlength(c,font=f)+14
cx_text(d,560,"지휘 콕핏 — 작전 시연",Ft(KO,40),TX)
cx_text(d,624,"디지털 트윈 실시간 재구성 · 드론 VLM 생존자 탐지 · 고글 AR 구조",Ft(KO,24),ACC)
d.rectangle([W/2-330,694,W/2+330,695],fill=(60,70,82))
cx_text(d,716,"SITE SEOUL-HQ   ·   INCIDENT 2026-0620-014   ·   3F STRUCTURE FIRE",Ft(CO,20),TX2)
cx_text(d,760,"SCOUT-1 · SCOUT-2 · SCOUT-3 · VISOR-1   |   UWB MESH",Ft(CO,17),DIM)
im.save(DIR+"/title.png")

# ── 아웃트로 ──
ex=M["frames"][-1].get("explored",0)
det=[e for e in M["events"] if e["kind"]=="detect"]
im,d=card()
cx_text(d,300,"작전 요약",Ft(KO,40),TX)
rows=[
 (f"디지털 트윈 재구성   {ex*100:.0f}%   ·   드론 3대 자율 정찰",TX),
 (f"드론 VLM 생존자 탐지   {len(det)}명   (LOS 카메라)",CRIT),
 ("지휘소 → 고글 AR 경로 명령   ·   요구조자 1명 유도 구조",OK),
 ("실시간 컷어웨이 트윈 · 프로스티드 HUD · CONSOLE-1 1:1 매핑",TX2),
]
y=400
for s,c in rows:
    cx_text(d,y,s,Ft(KO,26),c); y+=58
d.rectangle([W/2-420,y+20,W/2+420,y+21],fill=(50,58,68))
cx_text(d,y+44,"Goggle POV: Exeter Fire Dept. Search & Rescue (public domain, archive.org)",Ft(CO,17),DIM)
cx_text(d,y+70,"Digital twin · drones · VLM · HUD: FRIZE engine (this repo)",Ft(CO,17),DIM)
im.save(DIR+"/outro.png")
print("cards written")
