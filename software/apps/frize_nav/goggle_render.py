#!/usr/bin/env python3
# frize_nav goggle_render ― 실제 엔진 출력(scene.json)으로 고글 1인칭 AR 뷰 렌더
#  건물 와이어프레임 + 바닥 + 경로/셰브론(ar_project 출력) + HUD(나침반/회전/거리/바이탈)
#  → 고글 영상. 화면에 보이는 화살표는 손그림이 아니라 실제 내비 엔진의 투영.
import json, math, os, sys, glob
from PIL import Image, ImageDraw, ImageFont, ImageFilter

DIR=sys.argv[1] if len(sys.argv)>1 else "/tmp/goggle"
S=json.load(open(os.path.join(DIR,"scene.json")))
cam=S["cam"]; W,H=cam["W"],cam["H"]; PITCH=cam["pitch"]; HFOV=cam["hfov"]
TGT=S["target"]; BOXES=S["boxes"]; FRAMES=S["frames"]
os.makedirs(os.path.join(DIR,"frames"),exist_ok=True)

CO="/tmp/imgui/misc/fonts/Cousine-Regular.ttf"; KO="/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"
def Fc(s): return ImageFont.truetype(CO,s)
def Fk(s): return ImageFont.truetype(KO,s)
m12,m14,m16,m20,m34=Fc(12),Fc(14),Fc(16),Fc(20),Fc(34); k16,k20,k28=Fk(16),Fk(20),Fk(28)
CYAN=(120,210,235); DIMC=(60,120,140); RED=(235,90,70); YEL=(235,205,90); AMB=(232,180,70); WHT=(225,240,245)

def al(c,a): return (c[0],c[1],c[2],int(max(0,min(1,a))*255))

def basis(yaw,pitch):
    cy,sy,cp,sp=math.cos(yaw),math.sin(yaw),math.cos(pitch),math.sin(pitch)
    return (cy*cp,sy*cp,sp),(-sy,cy,0.0),(-cy*sp,-sy*sp,cp)
def dot(a,b): return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]
def proj(P,eye,fwd,right,up,fx):
    rel=(P[0]-eye[0],P[1]-eye[1],P[2]-eye[2]); dz=dot(rel,fwd)
    if dz<0.25: return None
    return (W/2+fx*dot(rel,right)/dz, H/2-fx*dot(rel,up)/dz, dz)
def clipseg(A,B,eye,fwd,right,up,fx):
    a=proj(A,eye,fwd,right,up,fx); b=proj(B,eye,fwd,right,up,fx)
    if a and b: return a,b
    # 한 점이 뒤면 근평면(dz=0.3)에서 컷
    def dz(P): rel=(P[0]-eye[0],P[1]-eye[1],P[2]-eye[2]); return dot(rel,fwd)
    da,db=dz(A),dz(B)
    if da<0.3 and db<0.3: return None
    t=(0.3-da)/(db-da); M=(A[0]+(B[0]-A[0])*t,A[1]+(B[1]-A[1])*t,A[2]+(B[2]-A[2])*t)
    if da<0.3: A=M
    else: B=M
    a=proj(A,eye,fwd,right,up,fx); b=proj(B,eye,fwd,right,up,fx)
    return (a,b) if (a and b) else None

def box_color(kind):
    return {"fire":RED,"person":YEL,"gas_range":AMB,"range_hood":AMB}.get(kind, DIMC)

def render(fi, fr):
    eye=fr["eye"]; yaw=fr["yaw"]; fwd,right,up=basis(yaw,PITCH); fx=(W/2)/math.tan(HFOV/2)
    img=Image.new("RGB",(W,H),(6,9,12)); d=ImageDraw.Draw(img,"RGBA")
    # 은은한 열화상 그라데(상단 차갑게, 하단 바닥 따뜻한 노이즈 느낌)
    for y in range(0,H,4):
        t=y/H; d.line([(0,y),(W,y)],fill=al((10,16,20),0.5*(1-t)))
    # ── 바닥 그리드(z=0, 2m 간격) ──
    gx0,gy0,gx1,gy1=-1,-1,27,13
    for gx in range(gx0,gx1+1,2):
        seg=clipseg((gx,gy0,0),(gx,gy1,0),eye,fwd,right,up,fx)
        if seg: d.line([seg[0][0],seg[0][1],seg[1][0],seg[1][1]],fill=al(DIMC,0.22),width=1)
    for gy in range(gy0,gy1+1,2):
        seg=clipseg((gx0,gy,0),(gx1,gy,0),eye,fwd,right,up,fx)
        if seg: d.line([seg[0][0],seg[0][1],seg[1][0],seg[1][1]],fill=al(DIMC,0.22),width=1)
    # ── 건물 와이어프레임(박스 12 엣지) ──
    def edges(b):
        x0,y0,x1,y1,zt=b[0],b[1],b[2],b[3],b[4]
        c=[(x0,y0,0),(x1,y0,0),(x1,y1,0),(x0,y1,0),(x0,y0,zt),(x1,y0,zt),(x1,y1,zt),(x0,y1,zt)]
        E=[(0,1),(1,2),(2,3),(3,0),(4,5),(5,6),(6,7),(7,4),(0,4),(1,5),(2,6),(3,7)]
        return [(c[a],c[bb]) for a,bb in E]
    for b in BOXES:
        kind=b[5]; col=box_color(kind); glow = kind in ("fire","person","gas_range")
        for A,B in edges(b):
            seg=clipseg(A,B,eye,fwd,right,up,fx)
            if not seg: continue
            a,bp=seg; w=2 if glow else 1; aa=0.85 if glow else 0.4
            d.line([a[0],a[1],bp[0],bp[1]],fill=al(col,aa),width=w)
    # 화점/요구조자/가스 라벨(중심 투영)
    for b in BOXES:
        kind=b[5]
        if kind not in ("fire","gas_range"): continue   # person 은 목표 마커가 라벨
        ctr=((b[0]+b[2])/2,(b[1]+b[3])/2,b[4]*0.5); p=proj(ctr,eye,fwd,right,up,fx)
        if not p: continue
        lab={"fire":"화점","gas_range":"가스"}[kind]; col=box_color(kind)
        d.text((p[0]+6,p[1]-8),lab,font=k16,fill=col)

    # ── 경로 리본(z=0.03) ──
    rpts=[proj((x,y,0.03),eye,fwd,right,up,fx) for (x,y) in fr["route"]]
    rpts=[p for p in rpts if p]
    for i in range(len(rpts)-1):
        d.line([rpts[i][0],rpts[i][1],rpts[i+1][0],rpts[i+1][1]],fill=al(CYAN,0.30),width=2)
    # ── 셰브론(★ 엔진 ar_project 출력 그대로) ──
    chev=fr["chev"]
    idxs=list(range(2,len(chev),2))   # 발밑 거대 셰브론 2개 스킵 + 한 칸씩 솎기
    for j,i in enumerate(idxs):
        u,v,sc=chev[i]
        nxt=chev[i+1] if i+1<len(chev) else None
        du,dv=(nxt[0]-u,nxt[1]-v) if nxt else (u-chev[i-1][0],v-chev[i-1][1])
        L=math.hypot(du,dv) or 1; du,dv=du/L,dv/L; px,py=-dv,du
        size=max(7,min(34,sc*520)); aa=min(1,0.34+sc*13)
        apex=(u+du*size*0.95, v+dv*size*0.95)
        l=(u+px*size-du*size*0.18, v+py*size-dv*size*0.18)
        r=(u-px*size-du*size*0.18, v-py*size-dv*size*0.18)
        wln=max(2,int(size*0.16))
        d.line([l[0],l[1],apex[0],apex[1]],fill=al(CYAN,aa),width=wln)
        d.line([r[0],r[1],apex[0],apex[1]],fill=al(CYAN,aa),width=wln)
    # ── 목표 마커 ──
    tp=proj((TGT[0],TGT[1],0.35),eye,fwd,right,up,fx)
    if tp:
        bx,by=tp[0],tp[1]; s=16+6*math.sin(fi*0.3)
        for q in [(-1,-1),(1,-1),(-1,1),(1,1)]:
            ex=bx+q[0]*s; ey=by+q[1]*s
            d.line([ex,ey,ex-q[0]*9,ey],fill=RED,width=2); d.line([ex,ey,ex,ey-q[1]*9],fill=RED,width=2)
        d.text((bx,by-s-16),"요구조자",font=k16,fill=RED,anchor="mm")
        d.text((bx,by+s+10),f"{fr['dist']:.1f} m",font=m16,fill=WHT,anchor="mm")

    # ── 고글 HUD ──
    d=ImageDraw.Draw(img,"RGBA")
    # 나침반 스트립(상단)
    hd=(math.degrees(yaw))%360
    d.rectangle([W/2-180,16,W/2+180,38],outline=al(CYAN,0.4),width=1)
    for deg,lab in [(0,"E"),(90,"N"),(180,"W"),(270,"S")]:
        rel=((deg-hd+540)%360)-180
        if abs(rel)<70: x=W/2+rel*2.4; d.text((x,20),lab,font=m16,fill=al(WHT,0.9),anchor="mm")
    d.polygon([(W/2,40),(W/2-5,33),(W/2+5,33)],fill=CYAN)
    # 회전 지시(큰 화살표)
    turn=fr["turn"]; tcx,tcy=W/2,96
    tcol=YEL if turn!=0 else CYAN
    if turn<0:  pts=[(tcx-26,tcy),(tcx-6,tcy-13),(tcx-6,tcy-5),(tcx+24,tcy-5),(tcx+24,tcy+5),(tcx-6,tcy+5),(tcx-6,tcy+13)]; lab="좌회전"
    elif turn>0:pts=[(tcx+26,tcy),(tcx+6,tcy-13),(tcx+6,tcy-5),(tcx-24,tcy-5),(tcx-24,tcy+5),(tcx+6,tcy+5),(tcx+6,tcy+13)]; lab="우회전"
    else:       pts=[(tcx,tcy-22),(tcx-12,tcy+2),(tcx-5,tcy+2),(tcx-5,tcy+20),(tcx+5,tcy+20),(tcx+5,tcy+2),(tcx+12,tcy+2)]; lab="직진"
    d.polygon(pts,fill=al(tcol,0.92))
    d.text((tcx,tcy+34),lab,font=k20,fill=tcol,anchor="mm")
    # 좌상단: AR NAV 상태
    d.text((22,58),"AR NAV",font=m20,fill=CYAN); d.text((22,82),"목표 · 요구조자",font=k16,fill=WHT)
    d.text((22,104),f"잔여 {fr['dist']:.1f} m",font=m16,fill=al(WHT,0.85))
    # 우상단: 링크/엔진
    d.text((W-22,58),"NAV-1 LINK",font=m14,fill=al(CYAN,0.8),anchor="ra")
    d.text((W-22,76),"A* RE-PLAN 5Hz",font=m12,fill=al(WHT,0.6),anchor="ra")
    # 하단 바이탈
    o2=20.6-0.0*fi; co=40+fi; hr=96+ (fi%7)
    d.rectangle([0,H-30,W,H],fill=al((6,10,12),0.7))
    d.text((22,H-24),f"O2 {o2:.1f}%   CO {co} ppm   HR {hr} bpm   SCBA 214 bar",font=m14,fill=al(WHT,0.85))
    d.text((W-22,H-24),"VISOR-1 · THERMAL+AR",font=m12,fill=al(CYAN,0.7),anchor="ra")
    # 크로스헤어
    d.line([W/2-9,H/2,W/2+9,H/2],fill=al(WHT,0.3),width=1); d.line([W/2,H/2-9,W/2,H/2+9],fill=al(WHT,0.3),width=1)

    # 바이저 비네트 + 스캔라인
    vig=Image.new("L",(W,H),0); vd=ImageDraw.Draw(vig)
    vd.ellipse([-W*0.25,-H*0.25,W*1.25,H*1.25],fill=255); vig=vig.filter(ImageFilter.GaussianBlur(80))
    dark=Image.new("RGB",(W,H),(0,0,0)); img=Image.composite(img,dark,vig)
    sc=ImageDraw.Draw(img,"RGBA")
    for y in range(0,H,3): sc.line([(0,y),(W,y)],fill=al((0,0,0),0.10))
    img.save(os.path.join(DIR,"frames",f"g_{fi:04d}.png"))

if __name__=="__main__":
    for fi,fr in enumerate(FRAMES):
        render(fi,fr)
        if fi%20==0: print("frame",fi,flush=True)
    print("done",len(FRAMES))
