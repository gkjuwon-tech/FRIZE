#!/usr/bin/env python3
# frize_mission compose v3 ― 브루탈리즘 터미널 콕핏(HN/htop 톤) + 멀티 시나리오
#
# 장식 없음. 모노스페이스 텍스트 테이블 + ASCII 바 + 플랫 패널 + 1px 라인.
# 해저드 스트라이프/볼트/브라켓 같은 "인위적 투박함" 제거 → 진짜 도구처럼.
#
# 6 시나리오: 01 진입·트윈재구성 / 02 생존자탐지·AR구조 / 03 위험대원·AR후퇴
#            04 백드래프트·전대원정지 / 05 가스누출·차단 / 06 2차생존자확보
import json, struct, math, glob, os, sys
from PIL import Image, ImageDraw, ImageFont

DIR  = sys.argv[1] if len(sys.argv)>1 else "/tmp/mission"
POVR = sys.argv[2] if len(sys.argv)>2 else "/tmp"
OUTD = os.path.join(DIR,"comp"); os.makedirs(OUTD, exist_ok=True)

M=json.load(open(DIR+"/mission.json")); view=json.load(open(DIR+"/view.json"))
W,H,NF=view["W"],view["H"],view["nframes"]; FPS=M.get("fps",24)
frames=M["frames"]; events=M["events"]; sem=M.get("semantics",[])
cam=open(DIR+"/cam.bin","rb").read(); CAM=[struct.unpack_from("<16f",cam,i*64) for i in range(NF)]
try:
    import numpy as np
    raw=np.fromfile(DIR+"/points.bin",dtype=np.uint8); n=raw.shape[0]//18
    rev=raw[:n*18].reshape(n,18)[:,16:18].copy().view(np.uint16).reshape(-1)
    CUM=np.cumsum(np.bincount(rev,minlength=NF)).tolist()
except Exception: CUM=[int(M["npoints"]*f/NF) for f in range(NF)]

# ── 터미널 팔레트(거의 모노크롬) ──
BG=(3,4,7); PANEL=(3,4,7); BORD=(44,49,57); RULE=(33,37,43)   # 트윈 배경(검정)과 동일 → 경계 없음
TX=(198,204,210); DIM=(120,128,138); MUT=(78,86,96)
GRN=(116,184,128); AMB=(206,170,84); RED=(222,84,70); CYN=(120,178,224)

# 모노(Cousine) + 한글(WQY zenhei, 고정폭 CJK)
CO="/tmp/imgui/misc/fonts/Cousine-Regular.ttf"; KO="/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"
def Fc(s): return ImageFont.truetype(CO,s)
def Fk(s): return ImageFont.truetype(KO,s)
m13=Fc(13); m14=Fc(14); m15=Fc(15); m12=Fc(12); m17=Fc(17); m22=Fc(22)
k13=Fk(13); k14=Fk(14); k15=Fk(15); k16=Fk(16); k20=Fk(20)

def al(c,a): return (c[0],c[1],c[2],int(a*255))
def project(m,x,y,z):
    cx=m[0]*x+m[4]*y+m[8]*z+m[12]; cy=m[1]*x+m[5]*y+m[9]*z+m[13]; cw=m[3]*x+m[7]*y+m[11]*z+m[15]
    if cw<=1e-5: return None
    return ((cx/cw*0.5+0.5)*W,(1-(cy/cw*0.5+0.5))*H)
def hangul(s): return any('가'<=ch<='힣' for ch in s)
def T(d,xy,s,fill,mono=True,sz=14):
    f=(Fc(sz) if not hangul(s) else Fk(sz)) if False else None
    f=(m14 if sz==14 else Fc(sz)) if (mono and not hangul(s)) else (k14 if sz==14 else Fk(sz))
    d.text(xy,s,font=f,fill=fill)
def tw(d,s,f): return d.textlength(s,font=f)

CHW=tw(ImageDraw.Draw(Image.new("RGB",(4,4))),"M",m14)  # 모노 한 칸 폭
def bar(frac,n=14):
    k=max(0,min(n,int(frac*n+0.5))); return "["+"#"*k+"-"*(n-k)+"]"

def rule(d,x,y,w,label=None,col=DIM):
    # `LABEL --------------------`  (HN/터미널식 섹션 구분)
    if label:
        d.text((x,y),label,font=m13,fill=col)
        lw=tw(d,label,m13)+6; nx=x+lw
    else: nx=x
    dash="-"*int((w-(nx-x))/max(1,CHW))
    d.text((nx,y),dash,font=m13,fill=RULE)

LWX=400; RWX=W-404; TOPH=30; BOTH=92

# ── 시나리오 ──
def sx(name):
    for s in sem:
        if s["kind"]==name: return s
def vget(x,y): return {"x":x,"y":y,"z":0.4}
gas=sx("gas_range") or vget(5.4,1.05)
CH=[
 dict(a=0,  b=80, code="01",title="진입 · 디지털 트윈 재구성",wearer="VISOR-1",pov="pov_v1",telem="normal",action=None,alert=None),
 dict(a=80, b=168,code="02",title="생존자 탐지 · AR 구조 유도",wearer="VISOR-2",pov="pov_v2",telem="normal",action=("ar_route",vget(2.6,9.8)),alert=("SURVIVOR","NW 병실 · 비반응 인원 1")),
 dict(a=168,b=252,code="03",title="위험대원 · AR 후퇴 명령",wearer="VISOR-3",pov="pov_v3",telem="danger",action=("ar_retreat",None),alert=("DANGER","VISOR-3  O2 16%  CO 1100ppm  SCBA 38bar  -> RETREAT")),
 dict(a=252,b=330,code="04",title="백드래프트 경보 · 전대원 정지",wearer="VISOR-1",pov="pov_v1",telem="warn",action=("estop",None),alert=("BACKDRAFT","북측 화점 급팽창 · 플래시오버 위험 -> ALL STOP")),
 dict(a=330,b=410,code="05",title="가스레인지 누출 · 차단 지시",wearer="VISOR-2",pov="pov_v2",telem="warn",action=("gas",gas),alert=("GAS","주방 LEL 28% -> 원격 밸브 CLOSE")),
 dict(a=410,b=NF, code="06",title="2차 생존자 확보 · 상황 정리",wearer="VISOR-1",pov="pov_v3",telem="normal",action=("ar_route",vget(22.8,2.2)),alert=("SURVIVOR","SE 끝방 · 요구조자 확보")),
]
def chap(f):
    for c in CH:
        if c["a"]<=f<c["b"]: return c
    return CH[-1]
POVS={k:sorted(glob.glob(os.path.join(POVR,k,"p_*.jpg"))) for k in ["pov_v1","pov_v2","pov_v3"]}
TEL={
 "normal":[("O2 ","20.8%",0.86,GRN),("HR ","94bpm",0.42,GRN),("AIR","228bar",0.86,GRN),("CO ","40ppm",0.08,GRN)],
 "warn":  [("O2 ","19.2%",0.62,AMB),("HR ","128bpm",0.62,AMB),("AIR","148bar",0.55,AMB),("CO ","310ppm",0.34,AMB)],
 "danger":[("O2 ","16.1%",0.18,RED),("HR ","191bpm",0.96,RED),("AIR","38bar",0.14,RED),("CO ","1100ppm",0.92,RED)],
}
ROSTER=[("SCOUT-1","DRONE"),("SCOUT-2","DRONE"),("SCOUT-3","DRONE"),
        ("VISOR-1","GOGL"),("VISOR-2","GOGL"),("VISOR-3","GOGL")]

def build_log():
    L=[(0,"core boot ok  mesh-link up  dev 6/6",DIM),(1,"scout-1/2/3 enter  autonomous frontier recon",DIM),
       (2,"visor-1/2/3 enter  helmet feed on",DIM)]
    for e in [e for e in events if e["kind"]=="detect"]:
        L.append((e["f"],f"[!] {e['by']} vlm survivor  conf {e['conf']:.2f}",RED))
    for c in CH:
        L.append((c["a"],f"== ch{c['code']}  {c['title']}",CYN))
        if c["action"]:
            t={"ar_route":"cmd> visor ar-route to survivor","ar_retreat":"cmd> visor-3 AR-RETREAT now",
               "estop":"cmd> E-STOP broadcast all units","gas":"cmd> gas valve CLOSE (remote)"}[c["action"][0]]
            L.append((c["a"]+8,t, RED if c["action"][0] in("ar_retreat","estop") else CYN))
    L.sort(key=lambda z:z[0]); return L
LOG=build_log()

def compose(f):
    img=Image.open(os.path.join(DIR,"frames",f"twin_{f:04d}.png")).convert("RGB")
    if img.size!=(W,H): img=img.resize((W,H))
    m=CAM[f]; fr=frames[f]; c=chap(f); flocal=f-c["a"]; aw=c["wearer"]
    agents={a["id"]:a for a in fr["agents"]}
    d=ImageDraw.Draw(img,"RGBA")

    # ── 트윈 마커(미니멀: + 와 라벨) ──
    for s in sem:
        p=project(m,s["x"],s["y"],s["z"])
        if not p: continue
        x,y=p
        if s["kind"]=="fire":
            d.line([x-7,y,x+7,y],fill=RED,width=2); d.line([x,y-7,x,y+7],fill=RED,width=2)
            d.text((x+9,y-7),f"FIRE {int(s['temp'])}C",font=m12,fill=RED)
        elif s["kind"]=="gas_range":
            d.rectangle([x-5,y-5,x+5,y+5],outline=AMB,width=1); d.text((x+9,y-7),"GAS",font=m12,fill=AMB)
    for aid,a in agents.items():
        p=project(m,a["x"],a["y"],a["z"])
        if not p: continue
        x,y=p; scout=a["type"]=="scout"; active=(aid==aw); dng=active and c["telem"]=="danger"
        col=AMB if a.get("blocked") else (CYN if scout else (RED if dng else TX))
        if scout: d.rectangle([x-4,y-4,x+4,y+4],outline=col,width=1)
        else: d.line([x-5,y,x+5,y],fill=col,width=1); d.line([x,y-5,x,y+5],fill=col,width=1)
        if active:
            d.text((x+8,y-7),aid,font=m12,fill=col)
            d.rectangle([x-9,y-9,x+9,y+9],outline=col,width=1)
        else: d.text((x+7,y-6),aid.split("-")[0][0]+aid[-1],font=m12,fill=al(col,0.9))
    for e in [e for e in events if e["kind"]=="detect"]:
        if f<e["f"]: continue
        p=project(m,e["x"],e["y"],e["z"])
        if not p: continue
        x,y=p; bl=(f//4)%2
        if bl: d.rectangle([x-8,y-8,x+8,y+8],outline=RED,width=1)
        d.line([x-5,y,x+5,y],fill=RED,width=2); d.line([x,y-5,x,y+5],fill=RED,width=2)
        d.text((x+10,y-6),"SURVIVOR",font=m12,fill=RED)

    # ── 패널: 트윈 배경과 동일 검정으로 꽉 채움(이음매/경계선 없음) ──
    d.rectangle([0,TOPH,LWX,H-BOTH],fill=PANEL)
    d.rectangle([RWX,TOPH,W,H-BOTH],fill=PANEL)
    d.rectangle([0,0,W,TOPH],fill=PANEL)
    d.rectangle([0,H-BOTH,W,H],fill=PANEL)

    # ── 상단 한 줄(터미널 헤더) ──
    d.text((10,7),"FRIZE",font=m17,fill=TX); d.text((68,9),"command-os",font=m13,fill=DIM)
    d.text((180,9),f"ch{c['code']}",font=m14,fill=CYN); T(d,(220,8),c["title"],TX,sz=15)
    rt=f"mesh:OK  link:6/6  T+{f/FPS:05.1f}s  09:41:22Z"
    d.text((W-tw(d,rt,m14)-10,9),rt,font=m14,fill=DIM)

    # ── 좌 패널 ──
    x=10; y=TOPH+10
    rule(d,x,y,LWX-20,"UNITS"); y+=18
    d.text((x,y),"id        type   state       xy",font=m12,fill=MUT); y+=15
    for i,(aid,typ) in enumerate(ROSTER):
        a=agents.get(aid,{}); blocked=a.get("blocked",False); active=(aid==aw); dng=active and c["telem"]=="danger"
        st="DANGER" if dng else ("BLOCKED" if blocked else ("RECON" if typ=="DRONE" else "SEARCH"))
        col=RED if dng else (AMB if blocked else (TX if active else DIM))
        cur=">" if active else " "
        xy=f"{a.get('x',0):04.1f},{a.get('y',0):04.1f}" if a else "--"
        d.text((x,y),f"{cur}{aid:8} {typ:5}  {st:9}  {xy}",font=m13,fill=col); y+=16
    y+=6; rule(d,x,y,LWX-20,"TWIN"); y+=18
    ex=fr.get("explored",0)
    d.text((x,y),f"recon  {bar(ex,18)} {ex*100:4.0f}%",font=m14,fill=TX); y+=16
    d.text((x,y),f"points {CUM[f]:>10,}",font=m14,fill=DIM); y+=16
    obs=fr.get("observed",0)
    d.text((x,y),f"voxels {obs:>10,} observed",font=m13,fill=MUT); y+=20
    rule(d,x,y,LWX-20,"HAZARDS"); y+=18
    ndet=sum(1 for e in events if e["kind"]=="detect" and f>=e["f"])
    d.text((x,y),f"survivor {ndet}   fire 2   gas 1",font=m14,fill=RED if ndet else DIM); y+=18
    af=c["alert"]
    if af:
        d.text((x,y),f"state    {af[0]}",font=m14,fill={"SURVIVOR":RED,"DANGER":RED,"BACKDRAFT":RED,"GAS":AMB}[af[0]]); y+=16

    # ── 우 패널 ──
    x=RWX+10; y=TOPH+10; rr=W-12
    rule(d,x,y,rr-x,"VLM"); y+=18
    actdet=[e for e in events if e["kind"]=="detect" and 0<=f-e["f"]<FPS*5]
    if actdet:
        e=actdet[-1]
        d.text((x,y),"[!] SURVIVOR DETECTED",font=m14,fill=RED); y+=16
        d.text((x,y),f"by {e['by']}  los-cam",font=m13,fill=TX); y+=15
        d.text((x,y),f"xy {e['x']:.1f},{e['y']:.1f}  conf {e['conf']:.2f}",font=m13,fill=DIM); y+=18
    else:
        d.text((x,y),"nominal  no anomaly",font=m13,fill=GRN); y+=20
    rule(d,x,y,rr-x,f"TELEMETRY {aw}"); y+=18
    for k,v,fv,col in TEL[c["telem"]]:
        d.text((x,y),f"{k} {bar(fv,16)} {v}",font=m14,fill=col); y+=16
    y+=6; rule(d,x,y,rr-x,"LOG"); y+=18
    for (lf,s,col) in [l for l in LOG if l[0]<=f][-12:]:
        ss=f"{lf/FPS:5.1f} {s}"
        while tw(d,ss,m12)>rr-x and len(ss)>4: ss=ss[:-1]
        d.text((x,y),ss,font=m12,fill=col if f-lf<FPS*2 else tuple(int(v*0.7) for v in col)); y+=14

    # ── 고글 POV(플랫 1px 베젤) ──
    pw,ph=int(W*0.33),int(W*0.33*9/16); px=12; py=H-BOTH-ph-10
    plist=POVS.get(c["pov"]) or POVS["pov_v1"]
    if plist:
        seg=max(1,c["b"]-c["a"]); pi=min(len(plist)-1,int(flocal/seg*(len(plist)-1)))
        img.paste(Image.open(plist[pi]).convert("RGB").resize((pw,ph)),(px,py))
    d=ImageDraw.Draw(img,"RGBA")
    d.rectangle([px,py,px+pw,py+ph],outline=BORD,width=1)
    d.rectangle([px,py,px+pw,py+18],fill=al(BG,0.78))
    d.text((px+5,py+3),f"pov {aw.lower()}",font=m12,fill=TX)
    t0=TEL[c["telem"]]
    d.text((px+pw-5-tw(d,"LIVE",m12),py+3),"LIVE",font=m12,fill=RED)
    d.text((px+5,py+ph-16),f"{t0[0][0]}{t0[0][1]}  {t0[3][0]}{t0[3][1]}",font=m12,fill=al((230,230,230),0.9))
    # AR 오버레이(텍스트 위주, 미니멀)
    act=c["action"]
    if act:
        kind=act[0]
        if kind in("ar_route","gas"):
            d.rectangle([px,py+18,px+pw,py+36],fill=al((10,28,44),0.7))
            d.text((px+5,py+20),("AR> gas shutoff point" if kind=="gas" else "AR> route to survivor"),font=m13,fill=CYN)
            tgt=act[1]; ex_,ey_=px+pw*0.66,py+ph*0.42; cxp,cyp=px+pw*0.5,py+ph*0.84
            for k in range(5):
                t=(k+(flocal*0.1)%1)/5; sx=cxp+(ex_-cxp)*t; sy=cyp+(ey_-cyp)*t; s=4+5*t
                d.line([sx-s,sy+s,sx,sy],fill=CYN,width=2); d.line([sx+s,sy+s,sx,sy],fill=CYN,width=2)
            bw=26; tc=RED if kind=="ar_route" else AMB
            for q in [(-1,-1),(1,-1),(-1,1),(1,1)]:
                exx=ex_+q[0]*bw; eyy=ey_+q[1]*bw
                d.line([exx,eyy,exx-q[0]*10,eyy],fill=tc,width=1); d.line([exx,eyy,exx,eyy-q[1]*10],fill=tc,width=1)
            d.text((ex_-bw,ey_-bw-14),"TARGET",font=m12,fill=tc)
        elif kind=="ar_retreat":
            d.rectangle([px,py+18,px+pw,py+36],fill=al((44,10,8),0.78)); d.text((px+5,py+20),"AR> RETREAT — EXIT 8m",font=m13,fill=RED)
            cxp,cyp=px+pw*0.5,py+ph*0.55
            for k in range(4):
                t=(k+(flocal*0.16)%1)/4; sx=cxp-(pw*0.32)*t; s=8+9*t
                d.line([sx+s,cyp-s,sx,cyp],fill=RED,width=3); d.line([sx+s,cyp+s,sx,cyp],fill=RED,width=3)
        elif kind=="estop":
            d.rectangle([px,py+18,px+pw,py+36],fill=al((44,10,8),0.78)); d.text((px+5,py+20),"** ALL STOP — EVACUATE **",font=m13,fill=RED)
    if act and act[0]=="ar_route" and flocal>(c["b"]-c["a"])*0.7:
        d.text((px+pw*0.5,py+ph*0.5),"[ SURVIVOR SECURED ]",font=m14,fill=GRN,anchor="mm")

    # ── 알림: 반전 모노 바(스트라이프 없음) ──
    if c["alert"]:
        ak,atext=c["alert"]; col={"SURVIVOR":RED,"DANGER":RED,"BACKDRAFT":RED,"GAS":AMB}[ak]
        blink = (f//6)%2==0 or ak=="SURVIVOR"
        line=f"*** {ak}: {atext} ***"
        wpx=tw(d,line,k15 if hangul(line) else m15)+24; bx=(W-wpx)/2
        d.rectangle([bx,TOPH+8,bx+wpx,TOPH+32],fill=col if blink else tuple(int(v*0.5) for v in col))
        T(d,(bx+12,TOPH+11),line,(10,10,12),sz=15)

    # ── 하단: 커맨드 라인 + E-STOP ──
    d=ImageDraw.Draw(img,"RGBA")
    by=H-BOTH+10
    hot={"ar_route":"AR-ROUTE","ar_retreat":"AR-RETREAT","estop":"E-STOP","gas":"GAS-OFF"}.get(c["action"][0] if c["action"] else None)
    cmds=["ADVANCE","REROUTE","AR-ROUTE","AR-RETREAT","GAS-OFF","SEARCH","CONFIRM"]
    d.text((10,by),f"cmd {aw.lower()}:",font=m14,fill=DIM)
    x=120
    for i,cmd in enumerate(cmds):
        on=(cmd==hot); key=["1","2","3","4","5","6","ENT"][i]
        s=f"[{key}]" + (f">{cmd}<" if on else cmd)
        col=(RED if c["action"] and c["action"][0] in("ar_retreat","estop") else CYN) if on else DIM
        d.text((x,by),s,font=m14,fill=col); x+=tw(d,s,m14)+14
    # 두번째 줄: 상태
    d.text((10,by+22),f"twin {fr.get('explored',0)*100:3.0f}%  pts {CUM[f]:,}  survivors {sum(1 for e in events if e['kind']=='detect' and f>=e['f'])}/2  scenario {c['code']}/06",font=m13,fill=MUT)
    d.text((10,by+40),"ui = console-1 keys = mouse  ·  control_map single-source  ·  feed: exeter fd (archive.org)",font=m12,fill=MUT)
    # E-STOP (반전 블록)
    es = c["action"] and c["action"][0]=="estop"
    ew=150; exx=W-ew-12; ey=by
    d.rectangle([exx,ey,exx+ew,ey+50],fill=RED if es else (40,16,14),outline=RED,width=1)
    d.text((exx+ew/2,ey+16),"E-STOP",font=m17,fill=(12,10,10) if es else RED,anchor="mm")
    d.text((exx+ew/2,ey+36),"all retreat",font=m12,fill=(12,10,10) if es else AMB,anchor="mm")

    img.save(os.path.join(OUTD,f"c_{f:04d}.png"))

if __name__=="__main__":
    lo=int(sys.argv[3]) if len(sys.argv)>3 else 0; hi=int(sys.argv[4]) if len(sys.argv)>4 else NF
    for f in range(lo,hi):
        compose(f)
        if f%30==0: print("frame",f,flush=True)
    print("done",lo,hi)
