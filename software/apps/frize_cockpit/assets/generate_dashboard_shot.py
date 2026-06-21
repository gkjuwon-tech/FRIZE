# -*- coding: utf-8 -*-
"""FRIZE 지휘 대시보드 합성 샷 (딱딱한 프로페셔널/밀리터리 GCS 톤).

콕핏 QML(Main.qml)이 그리는 화면 ― 전체 건물 3D 디지털 트윈(메인) +
소방관 1인칭 시야(PiP, 실사 외부 에셋) + 유닛/경보/명령 패널 ― 을 1680x980로 합성.
이 PNG는 Blender 제품샷(render_console_product.py)에서 컨트롤러 스크린 텍스처로 쓰인다.

톤 원칙: 무채색 그래파이트 단색조 + 단일 앰버 액센트, 레드는 '치명/중단' 의미로만.
네온·라운드·그라데이션·다색 금지. 고밀도·플랫·하드엣지(실전 관제툴).
중앙 트윈은 frize_recon_sim이 실제로 재구성한 메쉬 렌더.
"""
import os, io, sys, math, argparse, json
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageEnhance
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import trimesh

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 1680, 980

# ── 하드 모노크롬 팔레트 (단일 앰버 액센트, 레드=의미한정) ──
INK0=(18,20,23); INK1=(25,28,32); INK2=(30,34,39); LINE=(46,51,58); LINE2=(38,42,48)
TX=(198,203,209); SUB=(120,126,134); FNT=(78,84,92)
ACC=(178,140,66)           # 앰버 ― 활성/선택/주명령
WARN=(176,138,62)          # 경고(high)
CRIT=(170,58,50)           # 치명/중단(critical, E-STOP 의미)
OKD=(108,124,110)          # 정상 상태 점(채도 낮춤)
STEEL=(104,116,128)        # 데이터/중립 강조

KO="/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"
MONO="/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
MONOB="/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
def F(p,s):
    try: return ImageFont.truetype(p,s)
    except: return ImageFont.load_default()
ko=lambda s:F(KO,s); mono=lambda s:F(MONO,s); monob=lambda s:F(MONOB,s)


# ───────────────────────── 트윈 히어로 렌더 ─────────────────────────
def render_twin(ply, size=(1180,760)):
    m = trimesh.load(ply, process=False)
    V=np.asarray(m.vertices); Fc=np.asarray(m.faces); C=np.asarray(m.visual.vertex_colors)[:,:3]/255.0
    cz=V[Fc][:,:,2].mean(1); Fc=Fc[cz<2.75]; tris=V[Fc]; fc=C[Fc].mean(1)
    n=np.cross(tris[:,1]-tris[:,0],tris[:,2]-tris[:,0]); n/=np.linalg.norm(n,axis=1,keepdims=True)+1e-9
    sh=np.clip(np.abs(n@np.array([0.3,0.45,0.84])),0.30,1)[:,None]
    # 채도 낮춰 차갑게: 회색 베이스, 화점만 살린다(의미상 1포인트)
    grey=fc.mean(1,keepdims=True)
    fc=np.clip((grey*0.55+fc*0.45)*0.4 + (grey*0.55+fc*0.45)*sh*0.95,0,1)
    dpi=100; fig=plt.figure(figsize=(size[0]/dpi,size[1]/dpi),dpi=dpi)
    ax=fig.add_subplot(111,projection="3d")
    ax.add_collection3d(Poly3DCollection(tris,facecolors=fc,linewidths=0))
    ext=[16,12,7]; ax.set_xlim(0,ext[0]);ax.set_ylim(0,ext[1]);ax.set_zlim(0,ext[2])
    ax.set_box_aspect(ext); ax.view_init(elev=46,azim=-58); ax.set_axis_off()
    fig.subplots_adjust(left=0,right=1,bottom=0,top=1)
    buf=io.BytesIO(); fig.savefig(buf,format="png",facecolor=(0.055,0.062,0.072))
    plt.close(fig); buf.seek(0)
    return Image.open(buf).convert("RGB").resize(size)


# ───────────────────────── 1인칭 POV (실사 외부 에셋 + 절제된 AR) ─────────────────────────
def render_pov(size=(360,224)):
    for cand in ("goggle_pov.jpg","fireground.jpg"):
        p=os.path.join(HERE,cand)
        if os.path.exists(p): base=Image.open(p).convert("RGB"); break
    else: base=Image.new("RGB",size,(34,30,28))
    # 카메라 크롭(헬멧캠 비율) + 약한 디새추레이션(전술 톤)
    bw,bh=base.size; tr=size[0]/size[1]; cr=bw/bh
    if cr>tr: nw=int(bh*tr); base=base.crop(((bw-nw)//2,0,(bw-nw)//2+nw,bh))
    else: nh=int(bw/tr); base=base.crop((0,(bh-nh)//2,bw,(bh-nh)//2+nh))
    img=base.resize(size); img=ImageEnhance.Color(img).enhance(0.82); img=ImageEnhance.Brightness(img).enhance(0.94)
    d=ImageDraw.Draw(img,"RGBA"); w,hh=size
    # 비네팅
    vg=Image.new("L",size,0); ImageDraw.Draw(vg).ellipse([-w*0.28,-hh*0.28,w*1.28,hh*1.28],fill=130)
    img=Image.composite(img,ImageEnhance.Brightness(img).enhance(0.45),vg); d=ImageDraw.Draw(img,"RGBA")
    # 절제된 AR: 가는 표적 브래킷(코너만) + 모노 라벨 + 가는 진입 화살표
    def bracket(x0,y0,x1,y1,col,g=8,wd=2):
        for (a,b),(dx,dy) in [((x0,y0),(1,1)),((x1,y0),(-1,1)),((x0,y1),(1,-1)),((x1,y1),(-1,-1))]:
            d.line([(a,b),(a+dx*g,b)],fill=col,width=wd); d.line([(a,b),(a,b+dy*g)],fill=col,width=wd)
    bracket(int(0.46*w),int(0.40*hh),int(0.66*w),int(0.78*hh),(214,176,96,255))
    d.text((int(0.46*w),int(0.34*hh)),"TGT · PERSON ×2",font=mono(12),fill=(214,176,96))
    ax0,ay=int(0.16*w),int(0.86*hh)
    d.line([(ax0,ay),(ax0+64,ay)],fill=(196,176,120,235),width=3)
    d.polygon([(ax0+64,ay-6),(ax0+80,ay),(ax0+64,ay+6)],fill=(196,176,120,235))
    d.text((ax0,ay-20),"INGRESS",font=mono(11),fill=(196,176,120))
    # 생체 HUD(모노, 무채색, 위험치만 앰버)
    d.rectangle([6,6,150,50],fill=(0,0,0,150)); d.line([6,6,150,6],fill=(120,126,134,180))
    d.text((12,11),"O2  62%",font=mono(12),fill=(196,176,120))
    d.text((12,29),"HR  171",font=mono(12),fill=(196,176,120))
    # 스캔라인(헬멧 디스플레이 느낌, 아주 약하게)
    sl=Image.new("L",size,0); sd=ImageDraw.Draw(sl)
    for yy in range(0,hh,3): sd.line([(0,yy),(w,yy)],fill=14)
    img=Image.composite(Image.new("RGB",size,(0,0,0)),img,sl)
    return img


# ─────────────── 대시보드 합성 ───────────────
def panel(d,x0,y0,x1,y1,title,right=""):
    d.rectangle([x0,y0,x1,y1],fill=INK1,outline=LINE)
    d.rectangle([x0,y0,x1,y0+22],fill=INK2); d.line([x0,y0+22,x1,y0+22],fill=LINE)
    d.text((x0+9,y0+6),title,font=monob(11),fill=SUB)
    if right: d.text((x1-9-d.textlength(right,font=mono(10)),y0+7),right,font=mono(10),fill=FNT)

def bar(d,x,y,w,frac,col,bg=(44,48,54)):
    d.rectangle([x,y,x+w,y+5],fill=bg); d.rectangle([x,y,x+int(w*max(0,min(1,frac))),y+5],fill=col)

def compose_dashboard(twin_img,report):
    img=Image.new("RGB",(W,H),INK0); d=ImageDraw.Draw(img,"RGBA")
    # 타이틀바
    d.rectangle([0,0,W,28],fill=INK0); d.line([0,28,W,28],fill=LINE2)
    d.text((12,7),"FRIZE COCKPIT",font=monob(12),fill=TX)
    d.text((158,8),"SITE SEOUL-HQ   INCIDENT 2026-0620-014   CONSOLE-1",font=mono(11),fill=SUB)
    d.ellipse([W-150,11,W-143,18],fill=OKD); d.text((W-136,8),"CORE LINKED",font=mono(11),fill=SUB)
    # 툴바(탭, 활성=앰버 언더라인)
    d.rectangle([0,28,W,60],fill=INK1); d.line([0,60,W,60],fill=LINE); tx=8
    for i,tb in enumerate(["OVERVIEW","TWIN","UNITS","PLAYBACK","SETTINGS"]):
        w=d.textlength(tb,font=monob(12))+26
        if i==1: d.rectangle([tx,29,tx+w,59],fill=INK2); d.rectangle([tx,57,tx+w,59],fill=ACC)
        d.text((tx+13,38),tb,font=monob(12),fill=TX if i==1 else SUB); tx+=w

    pad=10; top=60+pad
    # ── 좌: 유닛 로스터 ──
    lx0,lx1=pad,pad+312
    panel(d,lx0,top,lx1,H-26,"UNITS","4 ONLINE")
    units=[("SCOUT-1","드론·정찰","ONLINE",OKD,0.71,[("ALT","11.4"),("SPD","2.1")]),
           ("VISOR-1","대원 A·진입","ONLINE",WARN,0.62,[("O2","62"),("HR","171")]),
           ("VISOR-2","대원 B·후방","ONLINE",OKD,0.88,[("O2","88"),("HR","112")]),
           ("VENT-1","배연포트","ONLINE",OKD,1.0,[("POS","OPN"),("","")])]
    uy=top+32
    for name,role,st,col,batt,stats in units:
        sel=name=="VISOR-1"
        if sel:
            d.rectangle([lx0+1,uy-6,lx1-1,uy+56],fill=(178,140,66,22))
            d.rectangle([lx0+1,uy-6,lx0+3,uy+56],fill=ACC)
        d.rectangle([lx0+12,uy+2,lx0+19,uy+9],fill=col)
        d.text((lx0+28,uy-2),name,font=monob(13),fill=TX)
        d.text((lx0+172,uy+0),st,font=mono(10),fill=SUB)
        d.text((lx0+28,uy+16),role,font=ko(11),fill=SUB)
        bar(d,lx0+28,uy+37,150,batt,col); d.text((lx0+186,uy+33),f"{int(batt*100)}%",font=mono(10),fill=SUB)
        cx=lx0+206
        for k,v in stats:
            if k: d.text((cx,uy+14),k,font=mono(9),fill=FNT); d.text((cx,uy+26),v,font=monob(12),fill=TX)
            cx+=52
        uy+=68; d.line([lx0+8,uy-12,lx1-8,uy-12],fill=LINE2)

    # ── 우: 경보 + 구조 큐 ──
    rx1=W-pad; rx0=W-pad-318
    panel(d,rx0,top,rx1,top+292,"ALERTS","4")
    alerts=[("CRIT",CRIT,"붕괴 위험 — 2F 동측 천장 처짐 67%"),
            ("HIGH",WARN,"VISOR-1 산소 62% · 심박 171"),
            ("HIGH",WARN,"가스 LEL 11% — 폭발 경계"),
            ("INFO",STEEL,"SCOUT-1 프런티어 4 탐사 중")]
    ay=top+30
    for sev,col,msg in alerts:
        d.rectangle([rx0+8,ay,rx1-8,ay+48],fill=INK2,outline=LINE)
        d.rectangle([rx0+8,ay,rx0+10,ay+48],fill=col)
        d.text((rx0+18,ay+6),sev,font=monob(11),fill=col)
        d.text((rx0+18,ay+25),msg,font=ko(12),fill=TX); ay+=56
    panel(d,rx0,top+304,rx1,H-26,"RESCUE QUEUE","3")
    q=[("1","3F-E 요구조자 ×2","37℃ · 의식불명"),("2","2F-W 고립대원","SCBA 18%"),
       ("3","1F-A 잔류 확인","열원 감지")]
    qy=top+338
    for rank,who,note in q:
        d.rectangle([rx0+12,qy,rx0+30,qy+18],outline=ACC); d.text((rx0+17,qy+1),rank,font=monob(13),fill=ACC)
        d.text((rx0+40,qy-2),who,font=ko(12),fill=TX); d.text((rx0+40,qy+16),note,font=ko(11),fill=SUB); qy+=46

    # ── 중앙: 트윈 + HUD + PiP + 미니맵 + 명령바 ──
    cx0=lx1+pad; cx1=rx0-pad; cmd_h=110
    tv_y0=top; tv_y1=H-26-cmd_h-pad
    d.rectangle([cx0,tv_y0,cx1,tv_y1],fill=(14,16,19),outline=LINE)
    tw,th=cx1-cx0-2,tv_y1-tv_y0-2
    img.paste(twin_img.resize((tw,th)),(cx0+1,tv_y0+1)); d=ImageDraw.Draw(img,"RGBA")
    # HUD 좌상단
    d.rectangle([cx0+10,tv_y0+10,cx0+372,tv_y0+32],fill=(0,0,0,150))
    cov=report.get("gt_surface_coverage_pct",0); fr=report.get("frontier_final",0)
    expl=report.get("explored_fraction",0)*100; tri=report.get("mesh_triangles",0)
    d.text((cx0+18,tv_y0+14),"DIGITAL TWIN",font=monob(12),fill=TX)
    d.text((cx0+150,tv_y0+15),f"TSDF SURFACE · {tri//1000}K TRIS · FRONTIER {fr//1000}K",font=mono(10),fill=SUB)
    # HUD 우상단 진행
    d.rectangle([cx1-216,tv_y0+10,cx1-10,tv_y0+54],fill=(0,0,0,150))
    d.text((cx1-206,tv_y0+15),"RECON",font=mono(10),fill=FNT)
    bar(d,cx1-206,tv_y0+31,146,expl/100,ACC); d.text((cx1-52,tv_y0+27),f"{expl:.0f}%",font=monob(12),fill=TX)
    d.text((cx1-206,tv_y0+41),"EXPLORED VOLUME",font=mono(9),fill=FNT)
    d.text((cx0+tw*0.30,tv_y0+th*0.60),"FIRE 620℃",font=monob(11),fill=(190,96,64))
    # PiP(1인칭, 실사)
    pov=render_pov((360,224)); px,py=cx0+12,tv_y1-224-12; img.paste(pov,(px,py)); d=ImageDraw.Draw(img,"RGBA")
    d.rectangle([px,py,px+360,py+224],outline=LINE)
    d.rectangle([px+6,py+6,px+150,py+24],fill=(0,0,0,160))
    d.ellipse([px+12,py+11,px+19,py+18],fill=CRIT); d.text((px+26,py+8),"VISOR-1 · POV",font=mono(11),fill=TX)
    # 미니맵
    mm=146; mmx,mmy=cx1-mm-12,tv_y1-mm-12
    d.rectangle([mmx,mmy,mmx+mm,mmy+mm],fill=(16,18,22,235),outline=LINE)
    d.text((mmx+8,mmy+6),"TACTICAL",font=mono(9),fill=FNT)
    for fx,fy,col in [(0.3,0.4,OKD),(0.55,0.6,WARN),(0.7,0.35,STEEL),(0.2,0.7,CRIT)]:
        d.rectangle([mmx+mm*fx-3,mmy+mm*fy-3,mmx+mm*fx+3,mmy+mm*fy+3],fill=col)
    # 명령바
    cmy0=H-26-cmd_h
    panel(d,cx0,cmy0,cx1,H-26,"COMMAND","선택: VISOR-1")
    btns=[("ADVANCE",ACC),("RETREAT",SUB),("RECON",SUB),("AIM+SPRAY",SUB),("FORCE ENTRY",CRIT),("RALLY",SUB)]
    bx=cx0+12; by=cmy0+34
    for label,col in btns:
        bw=d.textlength(label,font=monob(12))+30
        d.rectangle([bx,by,bx+bw,by+42],fill=INK2,outline=col)
        d.text((bx+15,by+13),label,font=monob(12),fill=TX if col is SUB else col); bx+=bw+10
    chips=[("CREW","2/2",STEEL),("DRONE","1",STEEL),("O2 MIN","18%",CRIT),("WATER","720L",STEEL)]
    chx=cx1-12
    for k,v,col in reversed(chips):
        t=f"{k} {v}"; cw=d.textlength(t,font=mono(12))+22
        d.rectangle([chx-cw,by,chx,by+42],fill=INK1,outline=LINE)
        d.rectangle([chx-cw+9,by+18,chx-cw+13,by+24],fill=col); d.text((chx-cw+20,by+13),t,font=mono(12),fill=TX); chx-=cw+8
    # 상태바
    d.rectangle([0,H-22,W,H],fill=INK0); d.line([0,H-22,W,H-22],fill=LINE2)
    d.text((12,H-16),f"TWIN: TSDF SURFACE MESH · DRONE+VISOR LIDAR FUSION · COVERAGE {cov:.0f}%",font=mono(10),fill=SUB)
    d.text((W-232,H-16),"FRIZE — WE FREEZE THE CHAOS",font=mono(10),fill=FNT)
    return img


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--twin",default="/tmp/dashtwin/twin.ply")
    ap.add_argument("--report",default="/tmp/dashtwin/report.json")
    ap.add_argument("--out",default=HERE)
    a=ap.parse_args()
    report=json.load(open(a.report)) if os.path.exists(a.report) else {}
    print("[dash] 트윈 히어로 렌더…"); twin=render_twin(a.twin,(1180,760))
    print("[dash] 대시보드 합성(하드 프로 톤)…"); dash=compose_dashboard(twin,report)
    dp=os.path.join(a.out,"dashboard.png"); dash.save(dp); print("  →",dp)

if __name__=="__main__":
    main()
