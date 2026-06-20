# -*- coding: utf-8 -*-
"""FRIZE 지휘 대시보드 합성 샷 + 컨트롤러(CONSOLE) 스크린 합성.

콕핏 QML(Main.qml)이 실제로 그리는 화면 ― 전체 건물 3D 디지털 트윈(메인) +
소방관 1인칭 시야(PiP) + 유닛/경보/명령 패널 ― 을 1680x980로 합성하고,
그 화면을 CONSOLE-1 컨트롤러의 스크린에 원근 워프로 박아 "진짜처럼" 보여준다.

Qt를 이 환경에서 못 띄우므로(빌드 타깃 Linux/Jetson), 같은 레이아웃/데이터를
PIL로 합성한다. 중앙 트윈은 frize_recon_sim이 실제로 재구성한 메쉬를 렌더한 것.

생성물:
  dashboard.png          ― 콕핏 대시보드 풀스크린(1680x980)
  console_dashboard.png  ― 컨트롤러 스크린에 대시보드를 합성한 제품 샷
사용:  python3 generate_dashboard_shot.py [--twin /path/twin.ply] [--mesh-dir /hardware/mesh]
"""
import os, io, sys, math, argparse
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import trimesh

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 1680, 980

# ── 유틸리티 GCS 팔레트 (Mission Planner / QGC 톤) ──
TITLE=(26,28,32); TOOL=(34,37,41); PANEL=(30,33,37); PANEL2=(24,27,31); HAIR=(58,63,70)
TWIN_BG=(12,15,19); TEXT=(210,214,219); SUB=(135,141,149); FAINT=(92,98,106)
GREEN=(63,178,118); AMBER=(214,158,58); RED=(224,53,36); CYAN=(53,196,224); BLUE=(74,144,194)

KO="/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"
MONO="/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
MONOB="/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
def F(path,s):
    try: return ImageFont.truetype(path,s)
    except: return ImageFont.load_default()
ko=lambda s:F(KO,s); mono=lambda s:F(MONO,s); monob=lambda s:F(MONOB,s)


# ───────────────────────── 1) 트윈 히어로 렌더 ─────────────────────────
def render_twin(ply, size=(1180,760)):
    """재구성된 전체 건물 트윈을 3D 히어로로 렌더(지붕 컷 → 내부+화점 노출)."""
    m = trimesh.load(ply, process=False)
    V = np.asarray(m.vertices); Fc = np.asarray(m.faces)
    C = np.asarray(m.visual.vertex_colors)[:, :3] / 255.0
    cz = V[Fc][:, :, 2].mean(1)
    keep = cz < 2.75                                  # 지붕/2층 제거 → 1층 내부
    Fc = Fc[keep]; tris = V[Fc]; fc = C[Fc].mean(1)
    n = np.cross(tris[:,1]-tris[:,0], tris[:,2]-tris[:,0])
    n /= np.linalg.norm(n,axis=1,keepdims=True)+1e-9
    sh = np.clip(np.abs(n @ np.array([0.3,0.45,0.84])),0.32,1)[:,None]
    fc = np.clip(fc*0.42 + fc*sh*0.9, 0, 1)
    dpi=100; fig=plt.figure(figsize=(size[0]/dpi,size[1]/dpi),dpi=dpi)
    ax=fig.add_subplot(111,projection="3d")
    ax.add_collection3d(Poly3DCollection(tris,facecolors=fc,linewidths=0))
    ext=[16,12,7]; ax.set_xlim(0,ext[0]);ax.set_ylim(0,ext[1]);ax.set_zlim(0,ext[2])
    ax.set_box_aspect(ext); ax.view_init(elev=48,azim=-58); ax.set_axis_off()
    fig.subplots_adjust(left=0,right=1,bottom=0,top=1)
    buf=io.BytesIO(); fig.savefig(buf,format="png",facecolor=tuple(c/255 for c in TWIN_BG))
    plt.close(fig); buf.seek(0)
    return Image.open(buf).convert("RGB").resize(size)


# ───────────────────────── 2) 1인칭 POV + AR 오버레이 ─────────────────────────
def render_pov(size=(360,224)):
    base_p=os.path.join(HERE,"fireground.jpg")
    img=Image.open(base_p).convert("RGB").resize(size) if os.path.exists(base_p) \
        else Image.new("RGB",size,(40,28,22))
    d=ImageDraw.Draw(img,"RGBA")
    w,hh=size
    # 연기 비네팅
    vg=Image.new("L",size,0); vd=ImageDraw.Draw(vg)
    vd.ellipse([-w*0.3,-hh*0.3,w*1.3,hh*1.3],fill=120)
    img=Image.composite(img,Image.new("RGB",size,(8,10,14)),vg)
    d=ImageDraw.Draw(img,"RGBA")
    # AR: 요구조자 박스 2 + 라벨
    for bx in [(0.46,0.46,0.6,0.74),(0.6,0.5,0.72,0.76)]:
        x0,y0,x1,y1=bx[0]*w,bx[1]*hh,bx[2]*w,bx[3]*hh
        d.rectangle([x0,y0,x1,y1],outline=(255,80,60,255),width=2)
    d.text((0.46*w,0.40*hh),"PERSON ×2",font=monob(13),fill=(255,120,90))
    # 진입 방향 화살표
    ax0,ay=0.2*w,0.84*hh
    d.line([(ax0,ay),(ax0+70,ay)],fill=(90,210,255,255),width=4)
    d.polygon([(ax0+70,ay-8),(ax0+88,ay),(ax0+70,ay+8)],fill=(90,210,255,255))
    d.text((ax0,ay-22),"진입로",font=ko(13),fill=(150,225,255))
    # 생체 HUD (좌상단)
    d.rectangle([6,6,150,52],fill=(0,0,0,150))
    d.text((12,10),"O2  62%",font=mono(13),fill=(120,220,150))
    d.text((12,30),"HR  171 bpm",font=mono(13),fill=(240,170,90))
    return img


# ─────────────── 3) 대시보드 합성(콕핏 Main.qml 레이아웃) ───────────────
def panel(d,x0,y0,x1,y1,title):
    d.rectangle([x0,y0,x1,y1],fill=PANEL,outline=HAIR)
    d.rectangle([x0,y0,x1,y0+22],fill=PANEL2); d.line([x0,y0+22,x1,y0+22],fill=HAIR)
    d.text((x0+9,y0+6),title,font=monob(11),fill=SUB)

def bar(d,x,y,w,frac,col,bg=(52,56,62)):
    d.rectangle([x,y,x+w,y+6],fill=bg)
    d.rectangle([x,y,x+int(w*max(0,min(1,frac))),y+6],fill=col)

def compose_dashboard(twin_img,report):
    img=Image.new("RGB",(W,H),PANEL2); d=ImageDraw.Draw(img,"RGBA")
    # 타이틀바
    d.rectangle([0,0,W,30],fill=TITLE)
    d.text((12,8),"FRIZE Cockpit",font=monob(13),fill=TEXT)
    d.text((150,9),"— Site SEOUL-HQ · incident #2026-0620-014 · CONSOLE-1",font=mono(11),fill=SUB)
    d.ellipse([W-150,11,W-142,19],fill=GREEN); d.text((W-136,9),"CORE LINKED",font=mono(11),fill=TEXT)
    # 툴바
    d.rectangle([0,30,W,64],fill=TOOL); d.line([0,64,W,64],fill=HAIR)
    tx=12
    for i,tb in enumerate(["OVERVIEW","TWIN","UNITS","PLAYBACK","SETTINGS"]):
        w=d.textlength(tb,font=monob(12))+26
        if i==1:
            d.rectangle([tx,32,tx+w,63],fill=PANEL); d.rectangle([tx,32,tx+w,34],fill=CYAN)
        d.text((tx+13,40),tb,font=monob(12),fill=TEXT if i==1 else SUB); tx+=w

    pad=12; top=64+pad
    # ── 좌: 유닛 로스터 ──
    lx0,lx1=pad,pad+312
    panel(d,lx0,top,lx1,H-34,"UNITS")
    units=[("SCOUT-1","드론·정찰","online",GREEN,0.71,[("ALT","11.4m"),("SPD","2.1m/s")]),
           ("VISOR-1","대원 α·진입","online",AMBER,0.62,[("O2","62%"),("HR","171")]),
           ("VISOR-2","대원 β·후방","online",GREEN,0.88,[("O2","88%"),("HR","112")]),
           ("VENT-1","배연포트","online",GREEN,1.0,[("POS","open"),("","")])]
    uy=top+34
    for name,role,st,col,batt,stats in units:
        sel = name=="VISOR-1"
        if sel: d.rectangle([lx0+4,uy-6,lx1-4,uy+58],fill=(53,196,224,28),outline=(53,196,224,160))
        d.ellipse([lx0+12,uy+2,lx0+20,uy+10],fill=col)
        d.text((lx0+28,uy-2),name,font=monob(13),fill=TEXT)
        d.text((lx0+150,uy+0),st.upper(),font=mono(10),fill=col)
        d.text((lx0+28,uy+16),role,font=ko(11),fill=SUB)
        bar(d,lx0+28,uy+36,150,batt,col); d.text((lx0+186,uy+32),f"{int(batt*100)}%",font=mono(10),fill=SUB)
        cx=lx0+200
        for k,v in stats:
            if k: d.text((cx,uy+14),k,font=mono(9),fill=FAINT); d.text((cx,uy+26),v,font=monob(11),fill=TEXT)
            cx+=54
        uy+=70; d.line([lx0+8,uy-12,lx1-8,uy-12],fill=(58,63,70,120))

    # ── 우: 경보 + 구조 큐 ──
    rx1=W-pad; rx0=W-pad-320
    panel(d,rx0,top,rx1,top+300,"ALERTS")
    alerts=[("CRITICAL",RED,"붕괴 위험 — 2F 동측 천장 처짐 67%"),
            ("HIGH",AMBER,"VISOR-1 산소 62% · 심박 171"),
            ("HIGH",AMBER,"가스 LEL 11% — 폭발 경계"),
            ("INFO",BLUE,"SCOUT-1 프런티어 4개 탐사 중")]
    ay=top+32
    for sev,col,msg in alerts:
        d.rectangle([rx0+8,ay,rx1-8,ay+50],fill=(col[0],col[1],col[2],26),outline=(col[0],col[1],col[2],150))
        d.rectangle([rx0+8,ay,rx0+11,ay+50],fill=col)
        d.text((rx0+18,ay+6),sev,font=monob(11),fill=col)
        d.text((rx0+18,ay+26),msg,font=ko(12),fill=TEXT); ay+=58
    panel(d,rx0,top+312,rx1,H-34,"RESCUE QUEUE")
    q=[("#1","3F-E 요구조자 ×2","37℃ · 의식불명"),("#2","2F-W 고립대원","SCBA 18%"),
       ("#3","1F-A 잔류 확인","열원 감지")]
    qy=top+346
    for rank,who,note in q:
        d.text((rx0+14,qy),rank,font=monob(14),fill=CYAN)
        d.text((rx0+50,qy-2),who,font=ko(12),fill=TEXT)
        d.text((rx0+50,qy+16),note,font=ko(11),fill=SUB); qy+=46

    # ── 중앙: 트윈(메인) + HUD + PiP + 미니맵 + 명령바 ──
    cx0=lx1+pad; cx1=rx0-pad
    cmd_h=120
    tv_y0=top; tv_y1=H-34-cmd_h-pad
    # 트윈 뷰포트
    d.rectangle([cx0,tv_y0,cx1,tv_y1],fill=TWIN_BG,outline=HAIR)
    tw,th=cx1-cx0-2,tv_y1-tv_y0-2
    ti=twin_img.resize((tw,th)); img.paste(ti,(cx0+1,tv_y0+1))
    d=ImageDraw.Draw(img,"RGBA")
    # HUD 좌상단
    d.rectangle([cx0+10,tv_y0+10,cx0+360,tv_y0+34],fill=(0,0,0,140))
    cov=report.get("gt_surface_coverage_pct",0); fr=report.get("frontier_final",0)
    expl=report.get("explored_fraction",0)*100; tri=report.get("mesh_triangles",0)
    d.text((cx0+18,tv_y0+15),"DIGITAL TWIN",font=monob(12),fill=(207,212,218))
    d.text((cx0+140,tv_y0+15),f"· TSDF surface · {tri//1000}k tris · frontier {fr//1000}k",
           font=mono(11),fill=(124,130,140))
    # HUD 우상단 진행
    d.rectangle([cx1-220,tv_y0+10,cx1-10,tv_y0+58],fill=(0,0,0,140))
    d.text((cx1-210,tv_y0+15),"RECON",font=mono(10),fill=FAINT)
    bar(d,cx1-210,tv_y0+32,150,expl/100,CYAN); d.text((cx1-52,tv_y0+28),f"{expl:.0f}%",font=monob(12),fill=TEXT)
    d.text((cx1-210,tv_y0+44),"explored volume",font=mono(9),fill=FAINT)
    # 화점 마커 라벨
    d.text((cx0+tw*0.30,tv_y0+th*0.62),"● FIRE 620℃",font=monob(11),fill=(255,120,80))
    # PiP(1인칭) 좌하단
    pov=render_pov((360,224)); img.paste(pov,(cx0+14,tv_y1-224-14))
    d=ImageDraw.Draw(img,"RGBA")
    d.rectangle([cx0+14,tv_y1-224-14,cx0+14+360,tv_y1-14],outline=(42,49,58),width=1)
    d.rectangle([cx0+20,tv_y1-224-8,cx0+150,tv_y1-224+12],fill=(0,0,0,150))
    d.ellipse([cx0+26,tv_y1-224-3,cx0+33,tv_y1-224+4],fill=RED)
    d.text((cx0+40,tv_y1-224-6),"VISOR-1 · 1인칭",font=ko(11),fill=(207,212,218))
    # 미니맵 우하단
    mm=148
    mmx,mmy=cx1-mm-14,tv_y1-mm-14
    d.rectangle([mmx,mmy,mmx+mm,mmy+mm],fill=(18,21,26,230),outline=(42,49,58))
    d.text((mmx+8,mmy+6),"TACTICAL",font=mono(9),fill=FAINT)
    for fx,fy,col in [(0.3,0.4,GREEN),(0.55,0.6,AMBER),(0.7,0.35,CYAN),(0.2,0.7,RED)]:
        d.ellipse([mmx+mm*fx-3,mmy+mm*fy-3,mmx+mm*fx+3,mmy+mm*fy+3],fill=col)
    # 명령바
    cmy0=H-34-cmd_h
    panel(d,cx0,cmy0,cx1,H-34,"COMMAND  ·  선택: VISOR-1 (대원 α)")
    btns=[("ADVANCE",GREEN),("RETREAT",AMBER),("RECON",CYAN),("AIM+SPRAY",BLUE),
          ("FORCE ENTRY",RED),("RALLY",SUB)]
    bx=cx0+14; by=cmy0+36
    for label,col in btns:
        bw=d.textlength(label,font=monob(13))+34
        d.rounded_rectangle([bx,by,bx+bw,by+44],radius=5,fill=(col[0],col[1],col[2],32),outline=col,width=1)
        d.text((bx+17,by+15),label,font=monob(13),fill=col); bx+=bw+12
    # 자원 칩
    chips=[("CREW","2/2",GREEN),("DRONE","1",CYAN),("O2 MIN","18%",RED),("WATER","720L",BLUE)]
    chx=cx1-14
    for k,v,col in reversed(chips):
        t=f"{k} {v}"; cw=d.textlength(t,font=mono(12))+22
        d.rounded_rectangle([chx-cw,by,chx,by+44],radius=5,fill=PANEL2,outline=HAIR)
        d.ellipse([chx-cw+10,by+19,chx-cw+16,by+25],fill=col)
        d.text((chx-cw+22,by+14),t,font=mono(12),fill=TEXT); chx-=cw+10

    # 상태바
    d.rectangle([0,H-22,W,H],fill=TITLE)
    d.text((12,H-17),"twin: TSDF surface mesh  ·  drone+visor LiDAR fusion  ·  "
           f"coverage {cov:.0f}%",font=mono(10),fill=SUB)
    d.text((W-220,H-17),"FRIZE — We freeze the chaos.",font=mono(10),fill=FAINT)
    return img


# ─────────────── 4) 컨트롤러 스크린 합성(핀홀 렌더 + 원근 워프) ───────────────
def look_at(eye,tgt,up=(0,0,1)):
    f=np.array(tgt,float)-np.array(eye,float); f/=np.linalg.norm(f)
    up=np.array(up,float); s=np.cross(f,up); s/=np.linalg.norm(s); u=np.cross(s,f)
    R=np.stack([s,u,-f]); return R,np.array(eye,float)

def project(P,R,eye,foc,cx,cy):
    pc=(P-eye)@R.T
    z=-pc[:,2]; z=np.where(z<1e-3,1e-3,z)
    x=foc*pc[:,0]/z+cx; y=cy-foc*pc[:,1]/z
    return np.stack([x,y],1),z

def warp_quad(src,dst_quad,canvas):
    """src 이미지를 dst_quad(4점, TL,TR,BR,BL)에 원근 워프해 canvas에 합성."""
    cw,ch=canvas.size; w,h=src.size
    src_c=np.array([[0,0],[w,0],[w,h],[0,h]],float); dst=np.array(dst_quad,float)
    # 출력→입력 호모그래피(PIL PERSPECTIVE는 output좌표→input좌표)
    A=[]; B=[]
    for (X,Y),(x,y) in zip(dst,src_c):
        A.append([X,Y,1,0,0,0,-x*X,-x*Y]); B.append(x)
        A.append([0,0,0,X,Y,1,-y*X,-y*Y]); B.append(y)
    coef=np.linalg.solve(np.array(A,float),np.array(B,float))
    warped=src.transform((cw,ch),Image.PERSPECTIVE,data=coef,resample=Image.BICUBIC)
    mask=Image.new("L",(cw,ch),0)
    ImageDraw.Draw(mask).polygon([tuple(p) for p in dst],fill=255)
    canvas.paste(warped,(0,0),mask)
    return canvas

def render_console(mesh_dir,size=(1180,900)):
    groups={'body':(58,62,68),'metal':(176,182,190),'accent':(210,55,40),
            'rubber':(20,21,24),'screen':(10,12,16)}
    allV=[];allF=[];allC=[];off=0
    for g,c in groups.items():
        p=os.path.join(mesh_dir,f"console_{g}.stl")
        if not os.path.exists(p): continue
        m=trimesh.load(p,process=False)
        allV.append(np.asarray(m.vertices)); allF.append(np.asarray(m.faces)+off)
        allC.append(np.tile(c,(len(m.faces),1))); off+=len(m.vertices)
    V=np.vstack(allV); Fc=np.vstack(allF); FC=np.vstack(allC).astype(float)
    # 카메라: 정면 상방(콘솔 스튜디오 샷과 유사)
    ctr=V.mean(0); ctr[2]=80
    eye=ctr+np.array([40,-560,360.0])
    R,eye=look_at(eye,ctr+np.array([0,40,30]))
    cw,ch=size; foc=ch*1.5
    P2,_=project(V,R,eye,foc,cw/2,ch/2)
    tri=P2[Fc]; depth=((V[Fc]-eye)**2).sum(2).mean(1)
    nrm=np.cross(V[Fc][:,1]-V[Fc][:,0],V[Fc][:,2]-V[Fc][:,0])
    nrm/=np.linalg.norm(nrm,axis=1,keepdims=True)+1e-9
    sh=np.clip(np.abs(nrm@np.array([0.2,-0.5,0.84])),0.32,1)
    col=np.clip(FC*sh[:,None],0,255).astype(int)
    order=np.argsort(-depth)
    canvas=Image.new("RGB",size,(22,24,28)); dd=ImageDraw.Draw(canvas)
    for i in order:
        dd.polygon([tuple(tri[i,0]),tuple(tri[i,1]),tuple(tri[i,2])],
                   fill=tuple(col[i]))
    # 스크린 디스플레이 4코너(SCAD 좌표) 투영
    a=math.radians(-12)
    def rotx(p): x,y,z=p; return np.array([x,y*math.cos(a)-z*math.sin(a),y*math.sin(a)+z*math.cos(a)])
    T1=np.array([0,154,72])
    corners_local=[(-240,-12.0,257),(240,-12.0,257),(240,-12.0,17),(-240,-12.0,17)]
    quad3d=np.array([rotx(p)+T1 for p in corners_local])
    quad2d,_=project(quad3d,R,eye,foc,cw/2,ch/2)
    return canvas,[tuple(p) for p in quad2d]


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--twin",default="/tmp/dashtwin/twin.ply")
    ap.add_argument("--report",default="/tmp/dashtwin/report.json")
    ap.add_argument("--mesh-dir",default=os.path.join(HERE,"../../../../hardware/mesh"))
    ap.add_argument("--out",default=HERE)
    a=ap.parse_args()
    import json
    report=json.load(open(a.report)) if os.path.exists(a.report) else {}
    print("[dash] 트윈 히어로 렌더…")
    twin=render_twin(a.twin,(1180,760))
    print("[dash] 대시보드 합성…")
    dash=compose_dashboard(twin,report)
    dp=os.path.join(a.out,"dashboard.png"); dash.save(dp); print("  →",dp)
    print("[dash] 컨트롤러 렌더 + 스크린 합성…")
    console,quad=render_console(os.path.abspath(a.mesh_dir))
    warp_quad(dash,quad,console)
    cp=os.path.join(a.out,"console_dashboard.png"); console.save(cp); print("  →",cp)

if __name__=="__main__":
    main()
