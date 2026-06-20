#!/usr/bin/env python3
# frize_mission compose ― 풀 시나리오 콕핏 조종 영상 컴포지터
#
# 입력:  <dir>/mission.json  프레임별 에이전트 포즈 + 이벤트(탐지/AR/구조) 타임라인
#        <dir>/frames/twin_XXXX.png  자라나는 디지털 트윈(포인트클라우드) 배경
#        <dir>/cam.bin       프레임별 MVP(월드→화면 투영) → 드론/생존자 마커 정렬
#        <pov dir>/p_XXXX.jpg 실제 소방관 1인칭 영상 프레임(고글 패널)
# 출력:  <dir>/comp/c_XXXX.png  콕핏 합성 프레임 → ffmpeg 로 mp4
#
# 콕핏이 미션에 따라 실제로 반응한다:
#   - 드론들이 트윈을 채우며 이동(벽에 막히면 amber)
#   - 드론 VLM 이 생존자 발견 → 알림 배너 + VLM 카드 + 트윈 핑
#   - 지휘소가 고글에 AR 경로 명령 → 고글 패널에 AR 오버레이
#   - 요구조자 확보(구조 비트)
import json, struct, math, glob, os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

DIR   = sys.argv[1] if len(sys.argv)>1 else "/tmp/mission"
POVD  = sys.argv[2] if len(sys.argv)>2 else "/tmp/povframes"
OUTD  = os.path.join(DIR,"comp"); os.makedirs(OUTD, exist_ok=True)

M = json.load(open(os.path.join(DIR,"mission.json")))
view = json.load(open(os.path.join(DIR,"view.json")))
W,H,NF = view["W"], view["H"], view["nframes"]
FPS = M.get("fps",24)
frames = M["frames"]; events = M["events"]; sem = M.get("semantics",[])

# cam.bin: NF x 16 float (열우선 MVP)
cam = open(os.path.join(DIR,"cam.bin"),"rb").read()
CAM = [struct.unpack_from("<16f", cam, i*64) for i in range(NF)]

# points.bin reveal 히스토그램 → 누적 스캔점 수(콕핏에 "수집 포인트" 표시)
try:
    import numpy as np
    raw = np.fromfile(os.path.join(DIR,"points.bin"), dtype=np.uint8)
    n = raw.shape[0]//18
    rev = raw[:n*18].reshape(n,18)[:,16:18].copy().view(np.uint16).reshape(-1)
    hist = np.bincount(rev, minlength=NF)
    CUM = np.cumsum(hist).tolist()
except Exception as e:
    CUM = [int(M["npoints"]*f/NF) for f in range(NF)]

POV = sorted(glob.glob(os.path.join(POVD,"p_*.jpg")))

# ── 팔레트(모노크롬 콕핏) ──
BG=(8,10,13); TX=(232,237,242); TX2=(150,160,172); DIM=(96,105,116); FNT=(70,78,88)
OK=(96,162,124); WARN=(210,168,86); CRIT=(214,78,64); LINE=(40,46,54); ACC=(120,170,225)

def F(path,sz): return ImageFont.truetype(path,sz)
KO="/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"
RB="/tmp/imgui/misc/fonts/Roboto-Medium.ttf"
CO="/tmp/imgui/misc/fonts/Cousine-Regular.ttf"
f_big=F(RB,30); f_h=F(KO,18); f_ui=F(KO,16); f_sm=F(KO,14); f_xs=F(KO,12)
f_mo=F(CO,15); f_mos=F(CO,12); f_mol=F(CO,20); f_wm=F(RB,26)

def project(mvp,x,y,z):
    cx=mvp[0]*x+mvp[4]*y+mvp[8]*z+mvp[12]
    cy=mvp[1]*x+mvp[5]*y+mvp[9]*z+mvp[13]
    cw=mvp[3]*x+mvp[7]*y+mvp[11]*z+mvp[15]
    if cw<=1e-5: return None
    nx=cx/cw; ny=cy/cw
    return ((nx*0.5+0.5)*W, (1-(ny*0.5+0.5))*H)

def al(c,a): return (c[0],c[1],c[2],int(a*255))

def text(d,xy,s,font,fill,anchor=None):
    d.text(xy,s,font=font,fill=fill,anchor=anchor)
def tw(d,s,font): return d.textlength(s,font=font)

# ── 이벤트 인덱싱 ──
detects=[e for e in events if e["kind"]=="detect"]
ar_cmds=[e for e in events if e["kind"]=="ar_cmd"]
ar_cues=[e for e in events if e["kind"]=="ar_cue"]
rescues=[e for e in events if e["kind"]=="rescue"]

# ── 로그 피드 빌드(프레임 → 누적 줄) ──
SEMNAME={"gas_range":"가스레인지","range_hood":"렌지후드","fire":"화점","person":"요구조자","furniture":"가구"}
def build_log():
    L=[]  # (f, text, color)
    L.append((0,"CORE 부팅 · 메시 링크 OK · 디바이스 4",DIM))
    L.append((1,"SCOUT-1·2·3 진입 · 자율 프런티어 탐사 개시",TX2))
    L.append((2,"VISOR-1 대원 현관 진입 · 헬멧 피드 ON",TX2))
    # blocked 로그(가끔)
    last_block={}
    for fr in frames:
        f=fr["f"]
        for a in fr["agents"]:
            if a.get("blocked") and a["type"]=="scout":
                if f-last_block.get(a["id"],-99)>40:
                    L.append((f,f"{a['id']} 벽면 차단 · 경로 재계획",WARN)); last_block[a["id"]]=f
        if f==int(NF*0.12): L.append((f,"트윈 재구성 진행 · TSDF 융합 중",DIM))
        if f==int(NF*0.30): L.append((f,"1층 평면 윤곽 확보 · 복도/실 분리",DIM))
        if f==int(NF*0.55): L.append((f,"2층 진입 · 우측 화점 열징후 상승",WARN))
        if f==int(NF*0.78): L.append((f,"3층 스캔 · 미탐사 경계 감소",DIM))
    for e in detects:
        L.append((e["f"],f"⚠ {e['by']} VLM 생존자 탐지 · {e['floor']}F · conf {e['conf']:.2f}",CRIT))
    for e in ar_cmds:
        L.append((e["f"],"지휘소 → VISOR-1 AR 경로 명령 송출",ACC))
    for e in ar_cues:
        L.append((e["f"],f"고글 AR 큐: {e['text']}",ACC))
    for e in rescues:
        L.append((e["f"],f"✔ {e['text']}",OK))
    L.sort(key=lambda x:x[0])
    return L
LOG=build_log()

# 폰트로 한글/기호 폭 보정용 truncate
def fit(d,s,font,maxw):
    while s and d.textlength(s,font=font)>maxw: s=s[:-1]
    return s

def frosted(base, box, blur=14, dark=0.62):
    x0,y0,x1,y1=box
    x0=max(0,int(x0)); y0=max(0,int(y0)); x1=min(W,int(x1)); y1=min(H,int(y1))
    if x1<=x0 or y1<=y0: return
    reg=base.crop((x0,y0,x1,y1)).filter(ImageFilter.GaussianBlur(blur))
    base.paste(reg,(x0,y0))
    ov=Image.new("RGBA",(x1-x0,y1-y0),al(BG,dark))
    base.paste(Image.alpha_composite(base.crop((x0,y0,x1,y1)).convert("RGBA"),ov).convert("RGB"),(x0,y0))

LWX=372; RWX=W-396   # 좌/우 패널 경계
TOPH=52; BOTH=104

def compose(f):
    twin=Image.open(os.path.join(DIR,"frames",f"twin_{f:04d}.png")).convert("RGB")
    if twin.size!=(W,H): twin=twin.resize((W,H))
    img=twin
    mvp=CAM[f]; fr=frames[f]
    agents={a["id"]:a for a in fr["agents"]}

    # ── 트윈 위 마커(패널 가리기 전에) ──
    d=ImageDraw.Draw(img,"RGBA")
    # 시맨틱: 가스레인지/화점/가구
    for s in sem:
        p=project(mvp,s["x"],s["y"],s["z"])
        if not p: continue
        x,y=p
        if not (LWX-40<x<RWX+40): pass
        k=s["kind"]
        if k=="fire":
            d.ellipse([x-9,y-9,x+9,y+9],outline=al(CRIT,0.7),width=2)
            d.ellipse([x-3,y-3,x+3,y+3],fill=CRIT)
            text(d,(x+12,y-8),f"화점 {int(s['temp'])}℃",f_mos,CRIT)
        elif k=="gas_range":
            d.rectangle([x-7,y-7,x+7,y+7],outline=al(WARN,0.8),width=2)
            text(d,(x+11,y-8),"가스레인지",f_xs,WARN)
        elif k=="furniture":
            d.rectangle([x-3,y-3,x+3,y+3],outline=al(DIM,0.5),width=1)
    # 에이전트
    for aid,a in agents.items():
        p=project(mvp,a["x"],a["y"],a["z"])
        if not p: continue
        x,y=p
        scout=a["type"]=="scout"
        col=WARN if a.get("blocked") else (ACC if scout else TX)
        # 헤딩 틱
        hp=project(mvp,a["x"]+math.cos(a["yaw"])*0.9,a["y"]+math.sin(a["yaw"])*0.9,a["z"])
        if hp: d.line([x,y,hp[0],hp[1]],fill=al(col,0.8),width=2)
        if scout:
            d.polygon([(x,y-7),(x-6,y+5),(x+6,y+5)],outline=col,width=2)
        else:
            d.ellipse([x-6,y-6,x+6,y+6],outline=col,width=2)
            d.ellipse([x-2,y-2,x+2,y+2],fill=col)
        text(d,(x+9,y-7),aid,f_mos,col)
        if a.get("blocked"):
            d.ellipse([x-12,y-12,x+12,y+12],outline=al(WARN,0.55),width=1)
    # 생존자 핑(탐지 후 펄스)
    for e in detects:
        if f<e["f"]: continue
        p=project(mvp,e["x"],e["y"],e["z"])
        if not p: continue
        x,y=p
        ph=(f-e["f"])
        rr=10+(ph%18)*1.6
        aa=max(0.0,0.7-(ph%18)/26)
        d.ellipse([x-rr,y-rr,x+rr,y+rr],outline=al(CRIT,aa),width=2)
        d.ellipse([x-4,y-4,x+4,y+4],fill=CRIT)
        text(d,(x+13,y-6),"요구조자",f_xs,(255,210,205))
        text(d,(x+13,y+8),f"conf {e['conf']:.2f}",f_mos,al(CRIT,0.9))

    # ── 프로스티드 패널 ──
    frosted(img,(0,TOPH-6,LWX,H-BOTH+2))
    frosted(img,(RWX,TOPH-6,W,H-BOTH+2))
    d=ImageDraw.Draw(img,"RGBA")
    # 상/하 스크림
    d.rectangle([0,0,W,TOPH],fill=al(BG,0.92))
    d.rectangle([0,H-BOTH,W,H],fill=al(BG,0.92))

    # ── 상단 바 ──
    d.regular_polygon((24,26,9),6,rotation=0,outline=TX,width=2)
    d.regular_polygon((24,26,3),6,rotation=0,outline=TX,width=1)
    x=42
    for ch in "FRIZE":
        text(d,(x,12),ch,f_wm,TX); x+=tw(d,ch,f_wm)+5
    text(d,(x+10,18),"COMMAND OS",f_mo,DIM)
    text(d,(LWX+18,10),"작전 시연 · 3층 구조화재",f_sm,TX)
    text(d,(LWX+18,30),"SITE SEOUL-HQ   INCIDENT 2026-0620-014",f_mos,TX2)
    # 우상단 인디케이터
    rx=W-16
    for s,c in [("09:41:22Z",TX),("UWB MESH",TX2),("4 LINKED",TX2)]:
        wsz=tw(d,s,f_mo); text(d,(rx-wsz,16),s,f_mo,c); rx-=wsz+20
    d.ellipse([rx-8,20,rx-2,26],fill=OK); rx-=18

    # ── 좌 패널: 유닛 로스터 ──
    x=18; y=TOPH+14
    text(d,(x,y),"UNITS · 4 ONLINE",f_mos,FNT); text(d,(LWX-46,y),"[1-4]",f_mos,FNT); y+=24
    roster=[("SCOUT-1","scout"),("SCOUT-2","scout"),("SCOUT-3","scout"),("VISOR-1","visor")]
    for i,(aid,typ) in enumerate(roster):
        a=agents.get(aid,{})
        blocked=a.get("blocked",False)
        st = "차단·재경로" if blocked else ("탐사중" if typ=="scout" else "수색중")
        led = WARN if blocked else OK
        sel = (typ=="visor")  # 고글 선택 강조
        if sel: d.rectangle([x-6,y-3,LWX-10,y+40],fill=al(TX,0.06))
        d.rectangle([x,y+3,x+5,y+9],fill=led)
        nm = "GOGGLE" if typ=="visor" else "DRONE"
        text(d,(x+14,y-1),aid,f_ui,TX if sel else (TX2 if not blocked else WARN))
        text(d,(x+14,y+19),f"{nm} · {st}",f_sm,DIM)
        text(d,(LWX-12,y+1),f"[{i+1}]",f_mos,FNT,anchor="ra")
        # 미니 배터리/좌표
        if a:
            text(d,(LWX-12,y+20),f"{a['x']:.0f},{a['y']:.0f},{a['z']:.0f}",f_mos,FNT,anchor="ra")
        y+=48
    # 탐사율
    y+=6; ex=fr.get("explored",0)
    text(d,(x,y),"트윈 재구성",f_mos,FNT); text(d,(LWX-12,y),f"{ex*100:.0f}%",f_mo,TX,anchor="ra"); y+=18
    d.rectangle([x,y,LWX-12,y+4],fill=al(LINE,1)); d.rectangle([x,y,x+(LWX-12-x)*ex,y+4],fill=ACC); y+=16
    text(d,(x,y),"수집 포인트",f_mos,FNT); text(d,(LWX-12,y),f"{CUM[f]:,}",f_mo,TX2,anchor="ra"); y+=24
    text(d,(x,y),"DETECTIONS",f_mos,FNT); y+=18
    ndet=sum(1 for e in detects if f>=e["f"])
    text(d,(x+14,y),f"생존자 {ndet} · 화점 2 · 가스 1",f_sm, CRIT if ndet else DIM)

    # ── 우 패널 ──
    x=RWX+16; y=TOPH+14; rr=W-16
    text(d,(x,y),"MISSION CLOCK",f_mos,FNT); text(d,(rr,y),f"T+{f/FPS:5.1f}s",f_mo,TX,anchor="ra"); y+=26
    # VLM 탐지 카드(활성 시)
    active_det=[e for e in detects if 0<=f-e["f"]<FPS*5]
    if active_det:
        e=active_det[-1]
        d.rectangle([x-6,y-4,rr+2,y+86],outline=al(CRIT,0.8),width=2)
        d.rectangle([x-6,y-4,rr+2,y+20],fill=al(CRIT,0.22))
        text(d,(x+4,y-1),"⚠ VLM 생존자 탐지",f_ui,(255,225,222)); y+=24
        text(d,(x+4,y),f"{e['by']} 카메라 · {e['floor']}F",f_sm,TX); y+=20
        text(d,(x+4,y),f"위치 ({e['x']:.1f}, {e['y']:.1f}, {e['z']:.1f})",f_mos,TX2); y+=18
        text(d,(x+4,y),f"신뢰도 {e['conf']:.2f} · 비반응 자세",f_mos,WARN); y+=26
    else:
        text(d,(x,y),"VLM 분석",f_mos,FNT); text(d,(rr,y),"정상 감시",f_mo,OK,anchor="ra"); y+=26
    # 텔레메트리(VISOR-1)
    text(d,(x,y),"VISOR-1 텔레메트리",f_mos,FNT); y+=20
    rows=[("산소 O₂","18.4%",0.62,WARN),("심박","168 bpm",0.84,WARN),
          ("SCBA 잔압","152 bar",0.50,TX2),("가스 CO","420 ppm",0.7,CRIT)]
    for k,v,fv,c in rows:
        text(d,(x,y),k,f_sm,DIM); text(d,(rr,y),v,f_mo,TX,anchor="ra"); y+=15
        d.rectangle([x,y,rr,y+3],fill=al(LINE,1)); d.rectangle([x,y,x+(rr-x)*fv,y+3],fill=c); y+=15
    y+=10
    # 이벤트 로그(스크롤)
    text(d,(x,y),"EVENT LOG",f_mos,FNT); y+=18
    shown=[l for l in LOG if l[0]<=f][-11:]
    for (lf,s,c) in shown:
        fade = 1.0 if f-lf<6 else 0.82
        cc = c if f-lf<FPS*2 else tuple(int(v*0.8) for v in c)
        text(d,(x,y),fit(d,s,f_xs,rr-x),f_xs,cc); y+=16

    # ── 고글 POV 패널(좌하단, 크게) + AR 오버레이 ──
    pw,ph=int(W*0.345),int(W*0.345*9/16)
    px=18; py=H-BOTH-ph-14
    if POV:
        pi=min(len(POV)-1,int(f/NF*(len(POV)-1)))
        pov=Image.open(POV[pi]).convert("RGB").resize((pw,ph))
        img.paste(pov,(px,py))
    d=ImageDraw.Draw(img,"RGBA")
    d.rectangle([px-2,py-2,px+pw+2,py+ph+2],outline=al(TX,0.3),width=1)
    d.rectangle([px,py,px+pw,py+26],fill=al(BG,0.6))
    d.ellipse([px+8,py+9,px+16,py+17],fill=CRIT)
    text(d,(px+22,py+6),"POV · VISOR-1 고글 1인칭",f_mo,TX)
    text(d,(px+pw-8,py+7),"LIVE",f_mos,CRIT,anchor="ra")
    # 고글 자체 AR HUD(상시): 나침반/크로스헤어
    cx=px+pw//2
    d.line([cx-10,py+ph-30,cx+10,py+ph-30],fill=al(TX,0.4),width=1)
    text(d,(px+10,py+ph-22),"O₂ 18.4%",f_mos,al((255,255,255),0.8))
    text(d,(px+pw-10,py+ph-22),"CO 420ppm",f_mos,al((255,180,170),0.9),anchor="ra")
    # 지휘소 AR 명령(ar_cmd 후) → 경로 셰브론 + 타깃 브라켓
    ar_on=[e for e in ar_cmds if f>=e["f"]]
    if ar_on:
        e=ar_on[-1]
        ph2=f-e["f"]
        # 상단 AR 배너
        d.rectangle([px,py+28,px+pw,py+50],fill=al((20,40,60),0.55))
        text(d,(px+10,py+30),"▲ 지휘소 AR · 요구조자 경로",f_sm,(150,210,255))
        # 경로 셰브론(중앙 하단 → 우상단 타깃)
        tx_,ty_=px+pw*0.66, py+ph*0.42
        for k in range(5):
            t=(k+ (ph2*0.12)%1)/5
            sx=px+pw*0.5+(tx_-(px+pw*0.5))*t
            sy=py+ph*0.86+(ty_-(py+ph*0.86))*t
            sz=4+6*t
            d.line([sx-sz,sy+sz,sx,sy],fill=al((150,210,255),0.9),width=2)
            d.line([sx+sz,sy+sz,sx,sy],fill=al((150,210,255),0.9),width=2)
        # 타깃 브라켓
        bw=34
        for c0 in [(-1,-1),(1,-1),(-1,1),(1,1)]:
            ex=tx_+c0[0]*bw; ey=ty_+c0[1]*bw
            d.line([ex,ey,ex-c0[0]*12,ey],fill=(255,120,110),width=2)
            d.line([ex,ey,ex,ey-c0[1]*12],fill=(255,120,110),width=2)
        text(d,(tx_-bw,ty_-bw-16),"비반응 인원",f_sm,(255,150,140))
        dist=max(2.0,12.0-ph2*0.05)
        text(d,(tx_-bw,ty_+bw+4),f"{dist:.1f} m",f_mo,(255,200,195))
        # 구조 완료 스탬프
        if any(f>=r["f"] for r in rescues):
            d.rectangle([px+pw*0.3,py+ph*0.5-16,px+pw*0.7,py+ph*0.5+16],fill=al((20,60,40),0.6))
            text(d,(px+pw*0.5,py+ph*0.5),"✔ 요구조자 확보",f_ui,(170,240,200),anchor="mm")

    # ── 알림 배너(탐지 순간 플래시) ──
    for e in detects:
        if 0<=f-e["f"]<FPS*1.4:
            a=0.7*(1-(f-e["f"])/(FPS*1.4))
            d.rectangle([0,TOPH,W,TOPH+4],fill=al(CRIT,0.9))
            bw_=tw(d,"⚠ 생존자 탐지 — 드론 VLM",f_h)+40
            d.rectangle([(W-bw_)/2,TOPH+10,(W+bw_)/2,TOPH+44],fill=al((40,12,10),0.6+a*0.3))
            text(d,(W/2,TOPH+27),"⚠ 생존자 탐지 — 드론 VLM",f_h,(255,210,205),anchor="mm")

    # ── 하단: 명령 스트립 + AR 명령 카드 + E-STOP ──
    d=ImageDraw.Draw(img,"RGBA")
    by=H-BOTH
    text(d,(18,by+10),"COMMAND · VISOR-1",f_mos,FNT)
    cmds=["ADVANCE","REROUTE","AR-경로","AR-마커","수색","후퇴","CONFIRM"]
    x=18; cy=by+34
    ar_active = any(f>=e["f"] for e in ar_cmds) and not any(f>=r["f"] for r in rescues)
    for i,c in enumerate(cmds):
        hot = (c=="AR-경로" and ar_active)
        col = ACC if hot else TX
        if hot: d.rectangle([x-5,cy-3,x+tw(d,c,f_ui)+8,cy+40],fill=al(ACC,0.16))
        text(d,(x,cy),c,f_ui,col)
        key=["1","2","3","4","5","6","↵"][i]
        d.rectangle([x,cy+26,x+13,cy+40],fill=al(TX,0.08)); text(d,(x+3,cy+27),key,f_mos,DIM)
        x+=tw(d,c,f_ui)+34
    # E-STOP
    ew=168; exx=W-ew-16; ey=by+22
    d.rectangle([exx,ey,exx+ew,ey+50],outline=CRIT,width=2,fill=al(CRIT,0.12))
    d.ellipse([exx+12,ey+13,exx+38,ey+39],fill=CRIT)
    text(d,(exx+48,ey+8),"E-STOP",f_ui,CRIT); text(d,(exx+48,ey+30),"전대원 후퇴",f_sm,(235,150,145))
    text(d,(18,H-18),"화면 컨트롤 = CONSOLE-1 물리버튼 = 키보드 · control_map 단일원천",f_mos,FNT)

    img.save(os.path.join(OUTD,f"c_{f:04d}.png"))

if __name__=="__main__":
    lo=int(sys.argv[3]) if len(sys.argv)>3 else 0
    hi=int(sys.argv[4]) if len(sys.argv)>4 else NF
    for f in range(lo,hi):
        compose(f)
        if f%30==0: print("frame",f,flush=True)
    print("done",lo,hi)
