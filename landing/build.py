# -*- coding: utf-8 -*-
# FRIZE landing site builder — emits static multi-page HTML
import os
HERE=os.path.dirname(os.path.abspath(__file__))

NAV=[("미션","mission.html","mission"),("시스템","system.html","system"),
     ("회사소개","company.html","company"),("뉴스","news.html","news"),("채용","careers.html","careers")]

def head(title,desc):
    return f"""<!doctype html><html lang="ko"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>{title} — FRIZE</title>
<meta name="description" content="{desc}">
<meta property="og:title" content="{title} — FRIZE Command OS">
<meta property="og:description" content="{desc}">
<link rel="preconnect" href="https://cdn.jsdelivr.net" crossorigin>
<link rel="stylesheet" href="styles.css">
</head><body>"""

def header(active):
    links="".join(f'<a href="{u}" class="{ "active" if k==active else "" }">{n}</a>' for n,u,k in NAV)
    return f"""<header class="nav"><div class="nav-in">
<a class="brand" href="index.html"><span class="mk"></span>FRIZE</a>
<nav class="nav-links">{links}</nav>
<a class="nav-cta" href="demo.html">데모 요청 →</a>
<button class="burger" aria-label="menu">MENU</button>
</div></header>"""

FOOTER="""<footer class="ft">
<div class="ft-top">
  <div class="ft-brand">
    <div class="brand" style="font-size:18px"><span class="mk"></span>FRIZE</div>
    <p>연기 속 현장을 실시간으로 보고, 찾고, 안내하고, 지키는 소방 현장 지휘 운영체제(Command&nbsp;OS). 드론·스마트 고글·현장 IoT·지휘 콕핏을 하나의 망으로 묶습니다.</p>
    <div class="addr">FRIZE INC. — 프라이즈<br>충청북도 청주시 흥덕구 가경로 (R&amp;D 센터)<br>36.6424°N · 127.4890°E<br>FRIZE-DOC / WS-0001 · REV 2026.06</div>
  </div>
  <div class="ft-col"><h5>제품</h5>
    <a href="system.html">시스템 개요</a><a href="system.html#twin">디지털 트윈</a>
    <a href="system.html#vlm">VLM 탐지</a><a href="system.html#nav">AR 내비게이션</a>
    <a href="system.html#safety">안전 인터록</a><a href="system.html#hardware">하드웨어</a>
  </div>
  <div class="ft-col"><h5>회사</h5>
    <a href="mission.html">미션</a><a href="company.html">회사소개</a>
    <a href="company.html#values">핵심 가치</a><a href="careers.html">채용 <span class="ext">3</span></a>
    <a href="news.html">뉴스 · 공지</a><a href="company.html#timeline">연혁</a>
  </div>
  <div class="ft-col"><h5>문의</h5>
    <a href="demo.html">데모 요청</a><a href="invest.html">투자 문의</a>
    <a href="demo.html">기관·소방서 협력</a><a href="careers.html">인재 제안</a>
    <a href="mailto:hello@frize.kr">hello@frize.kr</a><a href="mailto:press@frize.kr">언론 · press@frize.kr</a>
  </div>
  <div class="ft-col"><h5>리소스</h5>
    <a href="#">기술 백서 <span class="ext">PDF</span></a><a href="#">제품 브로슈어 <span class="ext">PDF</span></a>
    <a href="#">보도 자료실</a><a href="#">브랜드 가이드</a>
    <a href="#">현장 도입 사례</a><a href="#">상태 페이지 <span class="ext">↗</span></a>
  </div>
</div>

<div class="ft-news">
  <div><h5>현장 브리핑 구독</h5><p>제품 업데이트 · 도입 사례 · 채용 소식. 월 1회, 스팸 없음.</p></div>
  <form data-demo><input type="email" placeholder="name@dept.go.kr" required><button type="submit">구독</button></form>
</div>

<div class="ft-badges">
  <span>ISO 9001 (준비중)</span><span>정보보호 ISMS (준비중)</span><span>조달 등록 예정</span>
  <span>NFPA 호환 설계</span><span>공공 SW 영향평가 대비</span><span>SBOM 제공</span><span data-clock>--:--:--Z</span>
</div>

<div class="ft-legal">
  <div class="links">
    <a href="#">개인정보처리방침</a><a href="#">이용약관</a><a href="#">영상정보 처리방침</a>
    <a href="#">쿠키 설정</a><a href="#">취약점 신고</a><a href="#">사이트맵</a><a href="#">접근성</a>
  </div>
  <div class="copy">© 2026 FRIZE INC. ALL RIGHTS RESERVED · 사업자등록 000-00-00000</div>
</div>
<div class="ft-credit">
사진: 「High-Rise Fire Test / Wind-Driven Fires」(U.S. Gov, Public Domain) · 「강원도 소방공무원」「안산 소방차량」(Wikimedia Commons, CC BY-SA). 하드웨어 비주얼은 FRIZE 자체 3D 렌더(내부 산출물)입니다. 실시간 시스템 화면(디지털 트윈 · 지휘 콕핏 · 고글 AR)은 운용 보안상 일반 공개하지 않으며, 현장 데모 요청 또는 도입 기관에 한해 제공됩니다. 본 페이지의 수치·임계값은 현재 빌드의 검증값으로 사양은 고지 없이 변경될 수 있습니다. 본 사이트는 데모용 정적 페이지로 일부 링크는 비활성입니다.
</div>
</footer>
<script src="app.js"></script></body></html>"""

def write(slug, html):
    with open(os.path.join(HERE, slug),"w",encoding="utf-8") as f: f.write(html)
    print("wrote", slug)

def spec(dt,dd): return f'<div class="spec-row"><dt>{dt}</dt><dd>{dd}</dd></div>'
def gate(code,label,chip,redact): return f'''<div class="gate rv"><span class="chip">{chip}</span>
  <span class="corner c1"></span><span class="corner c2"></span><span class="corner c3"></span>
  <span class="redact r1">{redact}</span><span class="redact r2">{redact}</span>
  <div class="gate-body"><div class="lk"></div><h4>{code}</h4><p>{label} · 운영자 전용</p>
  <a class="unlock" href="demo.html">데모 요청 시 공개 →</a></div></div>'''
def devrow(rev,kick,h,p,lis,img,badge,cap1,cap2):
    ul="".join(f"<li>{x}</li>" for x in lis)
    return f'''<div class="frow{' rev' if rev else ''}">
    <div class="ftext rv"><div class="kicker"><span class="ln"></span>{kick}</div><h3>{h}</h3>
      <p>{p}</p><ul>{ul}</ul></div>
    <figure class="fmedia rv"><span class="badge">{badge}</span><img src="{img}" alt="{h}"><figcaption><span>{cap1}</span><span>{cap2}</span></figcaption></figure></div>'''

# ============================ INDEX ============================
index_body = """
<section class="hero">
  <div class="ticks"><span class="tl"></span><span class="tr"></span><span class="bl"></span><span class="br"></span></div>
  <div class="hero-bg"><img src="assets/scene_fire.jpg" alt="화재 현장 — 소방 대응"></div>
  <div class="hero-in">
    <div class="eyebrow rv"><span class="i">●</span>&nbsp;&nbsp;FIRE-RESPONSE INTEGRATED ZONE-CONTROL ENVIRONMENT</div>
    <h1 class="rv">연기 속에서,<br>지휘는 <em>멈추지 않는다.</em></h1>
    <p class="lead rv">FRIZE는 드론·스마트 고글·현장 IoT·지휘 콕핏을 하나의 실시간 망으로 묶는 소방 현장 지휘 운영체제입니다. 보이지 않는 건물 내부를 3D로 재구성하고, 생존자를 자동으로 탐지하며, 대원의 고글에 목표까지의 길을 그립니다.</p>
    <div class="hero-actions rv">
      <a class="btn btn-pri" href="demo.html">현장 데모 요청 <span class="ar">→</span></a>
      <a class="btn btn-ghost" href="system.html">시스템 살펴보기 <span class="ar">→</span></a>
    </div>
    <div class="hero-meta rv">
      <span>STATUS <b>OPERATIONAL</b></span><span>BUILD <b>v2026.06</b></span>
      <span>SITE <b>CHEONGJU · KR</b></span><span data-clock>--:--:--Z</span>
    </div>
  </div>
  <div class="scrollcue">SCROLL</div>
</section>

<section class="sec">
  <div class="wrap rv">
    <p class="statement"><span class="dim">화재 현장에서 가장 위험한 것은 불이 아니라 </span><b>모름</b><span class="dim">입니다. FRIZE는 그 모름을 </span>보임<span class="dim">으로, </span>늦음<span class="dim">을 </span>즉시<span class="dim">로 바꿉니다.</span></p>
  </div>
  <div class="wrap">
    <div class="pillars">
      <div class="pillar rv"><div class="no">01</div><h3>디지털 트윈</h3><p>드론·고글 스캔을 융합해 건물 내부를 실시간 3D 표면으로 재구성. 탐사가 진행될수록 화면이 채워집니다.</p><div class="tag">SITUATIONAL AWARENESS</div></div>
      <div class="pillar rv"><div class="no">02</div><h3>자동 탐지</h3><p>비전 AI가 영상을 장면 단위로 이해해 생존자·화점·위험을 '왜'와 함께 자동 분류합니다.</p><div class="tag">VLM · RATIONALE</div></div>
      <div class="pillar rv"><div class="no">03</div><h3>AR 길안내</h3><p>목표와 대원을 지정하면, 대원이 움직일 때마다 경로를 재계산해 고글 바닥에 화살표로 안내합니다.</p><div class="tag">REAL-TIME REROUTE</div></div>
      <div class="pillar rv"><div class="no">04</div><h3>안전 인터록</h3><p>모든 명령은 생명 직결 검문을 통과하고, 차단에는 사람이 읽는 이유가 붙습니다.</p><div class="tag">HUMAN-IN-THE-LOOP</div></div>
    </div>
  </div>
</section>

<section class="sec" style="padding-top:0;padding-bottom:0;border:0" id="hardware">
  """ + devrow(False,"HARDWARE — VISOR","대원의 눈,<br>스마트 고글",
      "열화상과 저조도 카메라로 연기를 뚫고, 시야 위에 AR 정보를 겹칩니다. UWB·위성 측위로 실내외 위치를 잡고, 가스 4종을 실시간 감시합니다.",
      ["<b>열화상 + 저조도 RGB</b>","<b>UWB · GNSS 측위 융합</b>","<b>AR 디스플레이 · NFPA 헬멧 마운트</b>"],
      "assets/dev_visor.jpg","DEVICE · VISOR-1","SMART VISOR","EDGE AI · THERMAL") + """
  """ + devrow(True,"HARDWARE — SCOUT","먼저 들어가는 눈,<br>정찰 드론",
      "대원보다 먼저 진입해 연기 속을 LiDAR로 훑습니다. 여러 대가 구역을 나눠 자율 비행하며, UWB 측위 비콘을 투하해 실내 측위망을 구성합니다.",
      ["<b>연기 투과 LiDAR · 열화상 짐벌</b>","<b>자율 프런티어 탐사</b>","<b>UWB 앵커 디스펜서 (3발)</b>"],
      "assets/dev_scout.jpg","DEVICE · SCOUT-1","RECON DRONE","LiDAR · AUTONOMY") + """
</section>

<section class="sec" id="operator">
  <div class="wrap">
    <div class="kicker rv" style="margin-bottom:18px"><span class="ln"></span>OPERATOR ACCESS · RESTRICTED</div>
    <h2 class="sec-head" style="padding:0">실시간 트윈·콕핏·고글 화면은<br>운영자에게만 공개됩니다</h2>
    <p class="muted rv" style="max-width:64ch;margin:16px 0 4px">지휘 콕핏의 디지털 트윈, AI 탐지, AR 길안내 화면은 운용 보안상 일반 공개하지 않습니다. 현장 데모 요청 또는 도입 기관에 한해 제공됩니다.</p>
  </div>
  <div class="gate-grid" style="margin-top:clamp(28px,4vh,44px)">
    """ + gate("DIGITAL TWIN","실시간 3D 재구성 화면","TWIN // CLASSIFIED","REDACTED — RECON SURFACE") + """
    """ + gate("COMMAND COCKPIT","콕핏 · 고글 AR 화면","COCKPIT // CLASSIFIED","REDACTED — OPERATOR VIEW") + """
  </div>
</section>

<section class="band">
  <img src="assets/scene_fire.jpg" alt="화재 현장">
  <div class="band-in rv">
    <div class="eyebrow" style="margin-bottom:18px"><span class="i">●</span>&nbsp;&nbsp;THE FIRST 180 SECONDS</div>
    <h2>현장의 모든 1초가<br>판단의 무게를 가진다</h2>
    <p>출동에서 구조까지, FRIZE는 지휘관이 '무엇을·누구에게'만 정하게 합니다. 상황인식·경로·통신·안전은 시스템이 처리합니다.</p>
    <div class="hero-actions" style="margin-top:28px"><a class="btn btn-ghost" href="mission.html">작전 흐름 보기 <span class="ar">→</span></a></div>
  </div>
</section>

<section class="sec" style="padding-bottom:0">
  <div class="wrap" style="margin-bottom:clamp(40px,6vh,72px)">
    <div class="kicker rv" style="margin-bottom:18px"><span class="ln"></span>BY THE NUMBERS</div>
    <h2 class="sec-head" style="padding:0">검증된 현장 수치</h2>
  </div>
  <div class="stats">
    <div class="stat rv"><div class="v">0.1–0.3<small>m</small></div><div class="k">실내 UWB 측위 정밀도 (앵커 3+)</div></div>
    <div class="stat rv"><div class="v">~5<small>Hz</small></div><div class="k">AR 경로 실시간 재계산 주기</div></div>
    <div class="stat rv"><div class="v">5<small>종</small></div><div class="k">실시간 가스 감시 (CO·O₂·LEL·H₂S·HCN)</div></div>
    <div class="stat rv"><div class="v">&lt;55<small>bar</small></div><div class="k">SCBA 잔압 진입 차단 임계</div></div>
  </div>
</section>

<section class="band" style="min-height:clamp(340px,52vh,560px)">
  <img src="assets/kr_truck.jpg" alt="소방 배치" style="object-position:center 60%">
  <div class="band-in rv">
    <div class="eyebrow" style="margin-bottom:18px"><span class="i">●</span>&nbsp;&nbsp;DEPLOYMENT</div>
    <h2>장비는 켜는 순간<br>스스로 연결된다</h2>
    <p>고글·드론·IoT 장비는 전원만 넣으면 지휘 콕핏과 자동 페어링되어 능력을 신고합니다. 설정도, 메뉴도 없습니다.</p>
  </div>
</section>

<section class="cta">
  <div class="wrap rv">
    <div class="eyebrow" style="margin-bottom:22px"><span class="i">●</span>&nbsp;&nbsp;GET STARTED</div>
    <h2>당신의 관할에서<br>FRIZE를 시연합니다</h2>
    <p>소방서·지자체·기관 대상 현장 데모와 기술 브리핑을 제공합니다. 30분이면 충분합니다.</p>
    <div class="hero-actions">
      <a class="btn btn-pri" href="demo.html">데모 요청 <span class="ar">→</span></a>
      <a class="btn btn-ghost" href="invest.html">투자 문의 <span class="ar">→</span></a>
    </div>
  </div>
</section>
"""

# ============================ MISSION ============================
mission_body = """
<section class="band" style="min-height:clamp(420px,64vh,640px)">
  <img src="assets/scene_highrise.jpg" alt="고층 화재">
  <div class="band-in">
    <div class="eyebrow" style="margin-bottom:18px"><span class="i">●</span>&nbsp;&nbsp;OUR MISSION</div>
    <h2>불은 뜨겁지만,<br>지휘는 차가워야 한다</h2>
  </div>
</section>
<section class="sec">
  <div class="prose">
    <p class="lead-xl rv">우리는 현장의 '모름'을 없애기 위해 존재합니다. 지휘관이 상상으로 결정하지 않고, 대원이 연기 속에서 혼자가 아니도록.</p>
    <div class="cols2" style="margin-top:48px">
      <div class="rv">
        <h3>우리가 푸는 문제</h3>
        <p>지금의 현장은 무전 음성과 종이 도면, 그리고 사람의 기억에 의존합니다. 지휘관은 건물 밖에서 내부를 상상하고, 대원은 짙은 연기 속에서 방향 감각을 잃습니다. 위치·상태 공유는 느리고, 가스 누출·백드래프트·공기 고갈 같은 치명적 위험은 늦게 인지됩니다.</p>
        <p>이 네 가지 공백 ― <b>시야, 길, 공유, 위험</b> ― 은 곧 골든타임의 손실이자 대원 안전의 위협입니다.</p>
      </div>
      <div class="rv">
        <h3>우리의 접근</h3>
        <p>FRIZE는 흩어진 장비를 하나의 운영체제로 묶습니다. 각 장비가 모은 정보가 즉시 한 화면에 모이고, 지휘관의 결정이 즉시 각 장비로 전달됩니다.</p>
        <p>핵심 약속은 단순합니다. <b>지휘관은 '무엇을·누구에게'만 정한다.</b> 상황인식·경로·통신·안전은 시스템이 처리합니다.</p>
      </div>
    </div>

    <h3 class="rv">한 번의 작전, 처음부터 끝까지</h3>
  </div>
  <div class="prose" style="margin-top:28px">
    <div class="tl">
      <div class="tl-item rv"><div class="yr">00:00</div><div><h4>도착 · 자동 연결</h4><p>드론·고글·장비 전원 ON. 별도 조작 없이 지휘 콕핏과 자동 페어링되고, 각 장비가 능력을 신고합니다.</p></div></div>
      <div class="tl-item rv"><div class="yr">00:30</div><div><h4>진입 · 트윈 생성</h4><p>드론들이 층·구역을 나눠 자율 비행. 비어 있던 건물 모형이 복도→방 순서로 채워집니다.</p></div></div>
      <div class="tl-item rv"><div class="yr">01:30</div><div><h4>발견 · 자동 탐지</h4><p>드론 카메라가 비반응 인원을 포착. 붉은 경보와 함께 위치·신뢰도·근거가 표시되고, 구조 우선순위가 산정됩니다.</p></div></div>
      <div class="tl-item rv"><div class="yr">02:00</div><div><h4>안내 · AR 길</h4><p>지휘관이 생존자와 대원을 지정. 대원 고글에 바닥을 따라 이어지는 화살표가 그려집니다.</p></div></div>
      <div class="tl-item rv"><div class="yr">02:40</div><div><h4>위험 · 자동 후퇴</h4><p>한 대원의 공기 잔압이 임계에 도달. 시스템이 그 고글에 즉시 후퇴 경로를 띄웁니다.</p></div></div>
      <div class="tl-item rv"><div class="yr">03:30</div><div><h4>구조 · 종료</h4><p>대원이 생존자에 도착하면 안내가 스스로 종료되고, 대피 경로가 다시 그려집니다.</p></div></div>
    </div>
  </div>
  <div class="prose" style="margin-top:40px">
    <h3 class="rv">설계 원칙</h3>
    <div class="cols2" style="margin-top:8px">
      <div class="rv"><p><b>① 모든 자동 판단에는 이유가 붙는다.</b> AI의 분류도, 안전장치의 거부도 '왜'를 말합니다. 블랙박스를 만들지 않습니다.</p>
      <p><b>② 생명을 구하는 명령은 항상 즉시 통과한다.</b> 후퇴·복귀·대피는 어떤 경우에도 막히지 않습니다.</p></div>
      <div class="rv"><p><b>③ 통신 단절을 정상으로 가정한다.</b> 망이 끊겨도 장비는 임무를 잇고, 복구되면 자동 재합류합니다.</p>
      <p><b>④ 훈련이 곧 실전이다.</b> 화면·물리 콘솔·키보드가 완전히 같은 동작을 합니다.</p></div>
    </div>
  </div>
</section>
<section class="cta"><div class="wrap rv"><h2>같은 목표를 향한다면</h2><p>현장과 함께 만드는 제품입니다. 소방서·연구기관과의 실증 협력을 환영합니다.</p><div class="hero-actions"><a class="btn btn-pri" href="demo.html">협력 제안 <span class="ar">→</span></a></div></div></section>
"""

# ============================ SYSTEM ============================
system_body = f"""
<section class="sec" style="border-bottom:0;padding-bottom:clamp(40px,6vh,64px)">
  <div class="wrap">
    <div class="kicker rv" style="margin-bottom:22px"><span class="ln"></span>SYSTEM OVERVIEW · FRIZE-WS-0001</div>
    <h2 class="sec-head" style="padding:0" >보고 · 찾고 · 안내하고 · 지킨다.<br>네 가지를 끊김 없이 잇는다.</h2>
    <p class="muted rv" style="max-width:64ch;margin-top:18px">FRIZE는 9개 기능 도메인으로 구성됩니다. 모든 구성요소는 단일 실시간 메시지 망 위에서 동작하며, 콕핏은 상태를 통합하고 명령을 안전 검문 후 각 장비로 중계합니다.</p>
  </div>
</section>

<section class="sec" style="padding-top:0;padding-bottom:0;border:0" id="devices">{devrow(False,"HARDWARE — CONSOLE","지휘 콘솔","현장 지휘차의 큰 화면과 전용 물리 콘솔. 화면의 모든 조작이 물리 버튼·키보드와 1:1로 대응하며, 최상위에 물리 E-STOP이 있습니다.",["<b>물리 컨트롤덱 · E-STOP</b>","<b>무정전 전원 · 보조 디스플레이</b>","<b>콘솔 = 화면 = 키보드 1:1</b>"],"assets/dev_console.jpg","DEVICE · CONSOLE-1","COMMAND CONSOLE","CONTROL DECK")}{devrow(True,"HARDWARE — APPARATUS","현장 IoT · 배연/밸브","대원을 위험한 곳에 보내지 않고 지휘소에서 배연구·밸브를 원격 개폐합니다. 원격 가스 차단과 차단지점 AR 표시를 지원합니다.",["<b>원격 개폐 구동</b>","<b>개방도 0–100% 보고</b>","<b>원격 가스 차단</b>"],"assets/dev_vent.jpg","DEVICE · VENT-1","FIELD IoT","REMOTE ACTUATION")}</section>

<section class="sec" style="padding-top:0;padding-bottom:0;border:0" id="twin">
  <div class="frow"><div class="ftext rv"><div class="kicker"><span class="ln"></span>B — 디지털 트윈</div><h3>실시간 3D 재구성</h3>
    <p>드론·고글 스캔(월드좌표 포인트)을 융합해 매끈한 표면 모형을 만듭니다. 시선 카빙으로 자유공간과 미탐사를 구분하고, 미탐사 영역은 트윈에 구멍으로 남아 탐사 목표가 됩니다.</p>
    <ul><li><b>점유격자 + 표면 메쉬</b> 동시 산출 (표준 3D 포맷)</li><li><b>열화상 채널</b> — 화점 발광 시각화</li><li><b>프런티어 환류</b> — 드론 자율 탐사 목표 공급</li></ul></div>
    {gate("DIGITAL TWIN","실시간 3D 재구성 화면","TWIN // CLASSIFIED","REDACTED — RECON SURFACE")}</div>

  <div class="frow rev" id="vlm"><div class="ftext rv"><div class="kicker"><span class="ln"></span>C — 자동 탐지(AI)</div><h3>사람보다 먼저 찾는다</h3>
    <p>비전 AI가 드론·고글 영상을 장면 단위로 이해해 위험을 분류합니다. 픽셀 룰이 아니라 모델이 판단하고, 모든 탐지에 '왜'를 동반합니다. 발견된 인원은 위급도·위험구역 근접·신뢰도로 구조 우선순위가 산정됩니다.</p>
    <ul><li><b>분류</b> — 사람·쓰러진사람·화점·연기·가스통·구조붕괴·백드래프트 외</li><li><b>rationale 필수</b> — 검증 가능한 판단</li><li><b>구조 우선순위</b> — '누구부터' 자동 산정</li></ul></div>
    {gate("AI DETECTION","VLM 자동 탐지 화면","VLM // CLASSIFIED","REDACTED — DETECTION")}</div>

  <div class="frow" id="nav"><div class="ftext rv"><div class="kicker"><span class="ln"></span>D — AR 내비게이션</div><h3>매 순간 다시 그리는 길</h3>
    <p>트윈에서 만든 점유격자 위에서 최단경로를 탐색하고 가시선으로 단순화해, 대원 위치가 갱신될 때마다 경로를 재계산해 고글에 스트리밍합니다. 도달 시 자동 종료, 경로 부재 시 재배치 경고.</p>
    <ul><li><b>벽 회피·문 경유·우회</b></li><li><b>회전 힌트 + 남은 거리</b></li><li><b>위급 시 후퇴 경로 우선</b></li></ul></div>
    {gate("AR NAVIGATION","콕핏 · 고글 길안내 화면","NAV // CLASSIFIED","REDACTED — OPERATOR VIEW")}</div>
</section>

<section class="sec" id="safety">
  <div class="wrap">
    <div class="kicker rv" style="margin-bottom:20px"><span class="ln"></span>E — 명령 · 안전 인터록</div>
    <h2 class="sec-head" style="padding:0">모든 명령은 생명 검문을 통과한다</h2>
    <p class="muted rv" style="max-width:64ch;margin:16px 0 8px">모든 명령은 허용 / 확인필요 / 거부로 판정되고, 항상 사람이 읽는 사유가 붙습니다. 아래는 현재 빌드의 실제 규칙입니다.</p>
  </div>
  <div class="specs" style="margin-top:24px">
    {spec("방수 (조준)","목표 3m 내 사람 → <span class='u'>거부</span> · 4m 내 화점 확인 → 허용 · 화점 미확인 → 확인필요")}
    {spec("대원 진입","SCBA 잔압 &lt;55bar → <span class='u'>거부(후퇴 권고)</span> · 낙상 감지 → 거부 · 주변 IDLH 가스 → 확인필요 · 고글 배터리 &lt;15% → 확인필요")}
    {spec("치명위험 목표","붕괴·백드래프트·가스통 인접 → 지휘관 2차 확인")}
    {spec("항상 허용","후퇴 · 복귀 · 착륙 · 대기 · 집결 · 강조 · 개폐")}
    {spec("가스 임계","CO≥1200 / H₂S≥100 / HCN≥50 ppm = IDLH · O₂&lt;19.5% 결핍 · LEL≥10% 폭발위험")}
  </div>
</section>

<section class="sec" id="hardware">
  <div class="wrap">
    <div class="kicker rv" style="margin-bottom:20px"><span class="ln"></span>HARDWARE</div>
    <h2 class="sec-head" style="padding:0">현장을 견디는 구성</h2>
  </div>
  <div class="specs" style="margin-top:24px">
    {spec("스마트 고글","엣지 AI · 열화상 카메라 · 저조도 RGB · 위성/UWB 측위 · 가스 4종 · AR 디스플레이 · NFPA 헬멧/SCBA 마운트")}
    {spec("정찰 드론","비행 컨트롤러 · 엣지 AI · 연기투과 LiDAR · 열화상 짐벌 · UWB 앵커 디스펜서(3발)")}
    {spec("지휘 콘솔","콕핏 컴퓨트 · 물리 컨트롤덱 · E-STOP · 무정전 전원 · 보조 디스플레이")}
    {spec("측위 인프라","실내 UWB 삼변측량(앵커 3+) · 옥외 위성 · 약신호 시 관성 추측항법 융합")}
    {spec("통신","단일 실시간 메시지 망 · 명령 유실 방지 · 비정상 종료 즉시 인지 · 자동 재연결")}
  </div>
</section>
<section class="cta"><div class="wrap rv"><h2>기술 브리핑을 요청하세요</h2><p>아키텍처·실증 데이터·통합 방식에 대한 30분 기술 세션을 제공합니다.</p><div class="hero-actions"><a class="btn btn-pri" href="demo.html">브리핑 요청 <span class="ar">→</span></a></div></div></section>
"""

# ============================ COMPANY ============================
company_body = """
<section class="sec" style="border-bottom:0;padding-bottom:clamp(36px,5vh,56px)">
  <div class="prose">
    <div class="kicker rv" style="margin-bottom:22px"><span class="ln"></span>ABOUT · FRIZE INC.</div>
    <p class="lead-xl rv">우리는 청주에서, 현장 대원과 지휘관이 더 안전하게 더 빨리 결정하도록 만드는 안전 기술 회사입니다.</p>
  </div>
</section>
<section class="band" style="min-height:clamp(360px,54vh,560px)">
  <img src="assets/kr_firefighter.jpg" alt="소방대원" style="object-position:center 30%">
  <div class="band-in rv"><div class="eyebrow" style="margin-bottom:16px"><span class="i">●</span>&nbsp;&nbsp;FOR THE PEOPLE WHO RUN IN</div><h2>먼저 들어가는 사람들을<br>위해 만든다</h2></div>
</section>
<section class="sec" id="values">
  <div class="wrap">
    <div class="kicker rv" style="margin-bottom:20px"><span class="ln"></span>VALUES</div>
    <div class="pillars">
      <div class="pillar rv"><div class="no">01</div><h3>현장 우선</h3><p>제품은 회의실이 아니라 연기 속에서 검증됩니다. 대원의 손과 눈을 기준으로 설계합니다.</p><div class="tag">FIELD-FIRST</div></div>
      <div class="pillar rv"><div class="no">02</div><h3>이유를 말한다</h3><p>자동 판단과 차단은 늘 근거를 남깁니다. 신뢰는 투명함에서 옵니다.</p><div class="tag">EXPLAINABLE</div></div>
      <div class="pillar rv"><div class="no">03</div><h3>안전이 먼저</h3><p>속도보다 생명. 모든 결정 위에 후퇴와 비상 정지가 있습니다.</p><div class="tag">SAFETY-FIRST</div></div>
      <div class="pillar rv"><div class="no">04</div><h3>끝까지 만든다</h3><p>하드웨어부터 펌웨어, AI, 콕핏까지. 통합의 책임을 회피하지 않습니다.</p><div class="tag">FULL-STACK</div></div>
    </div>
  </div>
</section>
<section class="sec" id="timeline" style="padding-top:clamp(40px,6vh,72px)">
  <div class="wrap"><div class="kicker rv" style="margin-bottom:20px"><span class="ln"></span>TIMELINE</div><h2 class="sec-head" style="padding:0">우리의 궤적</h2></div>
  <div class="prose" style="margin-top:24px">
    <div class="tl">
      <div class="tl-item rv"><div class="yr">2026 Q2</div><div><h4>FRIZE 설립 · 청주 R&amp;D 센터</h4><p>소방 현장 지휘 OS 개념 정립 및 핵심 엔진 프로토타입 착수.</p></div></div>
      <div class="tl-item rv"><div class="yr">2026 Q2</div><div><h4>통신·트윈·탐지 통합 검증</h4><p>실시간 메시지 망 위에서 디지털 트윈·자동 탐지·페어링 동작 검증.</p></div></div>
      <div class="tl-item rv"><div class="yr">2026 Q3</div><div><h4>AR 내비게이션 엔진 완성</h4><p>경로탐색·실시간 재유도·고글 투영을 단일 엔진으로 통합. 후퇴/대피 시나리오 대응.</p></div></div>
      <div class="tl-item rv"><div class="yr">2026 H2</div><div><h4>현장 실증 파트너십 (목표)</h4><p>소방서·지자체와의 관할 실증 및 하드웨어 현장 통합.</p></div></div>
    </div>
  </div>
</section>
<section class="sec">
  <div class="wrap"><div class="stats" style="border:0">
    <div class="stat rv"><div class="v">2026</div><div class="k">설립 연도</div></div>
    <div class="stat rv"><div class="v">청주</div><div class="k">본사 · R&amp;D</div></div>
    <div class="stat rv"><div class="v">9<small>도메인</small></div><div class="k">기능 영역</div></div>
    <div class="stat rv"><div class="v">100%</div><div class="k">풀스택 자체 개발</div></div>
  </div></div>
</section>
<section class="cta"><div class="wrap rv"><h2>함께 만들 사람을 찾습니다</h2><p>로보틱스·엣지 AI·임베디드·HCI. 현장을 바꾸는 일에 합류하세요.</p><div class="hero-actions"><a class="btn btn-pri" href="careers.html">채용 보기 <span class="ar">→</span></a><a class="btn btn-ghost" href="invest.html">투자 문의 <span class="ar">→</span></a></div></div></section>
"""

# ============================ NEWS ============================
def newscard(date,cat,title,desc): return f'<a class="card rv" href="#"><div class="meta"><span class="cat">{cat}</span><span>{date}</span></div><h3>{title}</h3><p>{desc}</p><div class="more">자세히 <span>→</span></div></a>'
news_body = f"""
<section class="sec" style="border-bottom:0;padding-bottom:clamp(32px,5vh,48px)">
  <div class="wrap"><div class="kicker rv" style="margin-bottom:20px"><span class="ln"></span>NEWSROOM</div>
  <h2 class="sec-head" style="padding:0">뉴스 · 공지사항</h2>
  <p class="muted rv" style="margin-top:14px;max-width:60ch">제품 업데이트, 실증 소식, 채용 공고, 그리고 회사의 발자취.</p></div>
</section>
<section class="sec" style="padding-top:0">
  <div class="wrap"><div class="cards">
    {newscard("2026-06-20","제품 업데이트","AR 내비게이션 엔진 v2 공개","목표·대원 지정만으로 실시간 재유도 경로를 고글에 스트리밍하는 내비게이션 엔진을 통합했습니다.")}
    {newscard("2026-06-18","기술","디지털 트윈 실시간 컷어웨이 렌더링","단층·다층 구조를 실시간으로 채워가는 트윈 시각화 파이프라인을 공개합니다.")}
    {newscard("2026-06-15","회사","FRIZE, 청주 R&D 센터 가동","충청북도 청주에 연구개발 거점을 마련하고 핵심 엔진 개발에 착수했습니다.")}
    {newscard("2026-06-10","채용","로보틱스·엣지 AI 엔지니어 모집","현장을 바꾸는 풀스택 안전 기술 팀에 합류할 동료를 찾습니다.")}
    {newscard("2026-06-05","파트너십","소방서 실증 협력 제안 접수","관할 단위 현장 실증을 위한 기관 협력 제안을 받습니다.")}
    {newscard("2026-06-01","안전","안전 인터록 규칙 공개 검증","명령 안전 검문 규칙을 외부에 공개하고 검증 절차를 시작했습니다.")}
  </div></div>
</section>
<section class="cta"><div class="wrap rv"><h2>소식을 먼저 받아보세요</h2><p>월 1회 현장 브리핑. 제품·실증·채용 소식을 모아서.</p><div class="hero-actions"><a class="btn btn-pri" href="#" onclick="document.querySelector('.ft-news input').focus();return false;">구독하기 <span class="ar">→</span></a></div></div></section>
"""

# ============================ CAREERS ============================
def job(team,title,loc,typ): return f'<a class="card rv" href="demo.html"><div class="meta"><span class="cat">{team}</span><span>{loc}</span><span>{typ}</span></div><h3>{title}</h3><p>현장을 바꾸는 안전 기술. 자세한 직무 요건은 지원 시 안내합니다.</p><div class="more">지원하기 <span>→</span></div></a>'
careers_body = f"""
<section class="sec" style="border-bottom:0;padding-bottom:clamp(32px,5vh,48px)">
  <div class="prose"><div class="kicker rv" style="margin-bottom:20px"><span class="ln"></span>CAREERS</div>
  <p class="lead-xl rv">연기 속에서 작동하는 제품을 만든다는 것은, 타협 없이 끝까지 만든다는 뜻입니다.</p>
  <p class="rv">FRIZE는 로보틱스, 엣지 AI, 임베디드, 그리고 사람을 위한 인터페이스가 만나는 곳입니다. 우리는 데모가 아니라 현장에서 작동하는 것을 만듭니다.</p></div>
</section>
<section class="sec" style="padding-top:0">
  <div class="wrap"><div class="kicker rv" style="margin-bottom:18px"><span class="ln"></span>OPEN ROLES · 3</div><div class="cards">
    {job("ROBOTICS","자율 비행 · 정찰 드론 엔지니어","청주 / 하이브리드","정규직")}
    {job("AI","엣지 비전 · VLM 엔지니어","청주 / 하이브리드","정규직")}
    {job("EMBEDDED","센서 융합 · 측위(UWB/GNSS) 엔지니어","청주","정규직")}
  </div></div>
</section>
<section class="sec">
  <div class="wrap"><div class="kicker rv" style="margin-bottom:18px"><span class="ln"></span>WHY FRIZE</div>
  <div class="pillars">
    <div class="pillar rv"><div class="no">01</div><h3>의미 있는 난제</h3><p>생명을 구하는 실시간 시스템. 쉬운 문제는 없습니다.</p><div class="tag">HARD · REAL</div></div>
    <div class="pillar rv"><div class="no">02</div><h3>풀스택 오너십</h3><p>하드웨어부터 AI, 콕핏까지 끝에서 끝을 다룹니다.</p><div class="tag">END-TO-END</div></div>
    <div class="pillar rv"><div class="no">03</div><h3>현장 접점</h3><p>대원·지휘관과 직접 일하며 제품을 검증합니다.</p><div class="tag">FIELD</div></div>
    <div class="pillar rv"><div class="no">04</div><h3>빠른 사이클</h3><p>작은 팀, 빠른 결정, 즉각적 임팩트.</p><div class="tag">VELOCITY</div></div>
  </div></div>
</section>
<section class="cta"><div class="wrap rv"><h2>해당 직무가 없어도</h2><p>현장을 바꾸고 싶다면, 당신의 이야기를 들려주세요.</p><div class="hero-actions"><a class="btn btn-pri" href="mailto:join@frize.kr">인재 제안 <span class="ar">→</span></a></div></div></section>
"""

# ============================ DEMO ============================
demo_body = """
<section class="sec" style="border-bottom:0;padding-bottom:clamp(28px,4vh,40px)">
  <div class="prose"><div class="kicker rv" style="margin-bottom:20px"><span class="ln"></span>REQUEST A DEMO</div>
  <p class="lead-xl rv">당신의 관할에서 30분 안에 FRIZE를 시연합니다.</p>
  <p class="rv">소방서·지자체·기관 대상 현장 데모와 기술 브리핑. 디지털 트윈, 자동 탐지, AR 길안내를 실제 시나리오로 보여드립니다.</p></div>
</section>
<section class="sec" style="padding-top:0">
  <div class="wrap" style="max-width:980px">
    <form class="form" data-demo>
      <div class="field"><label>기관 · 회사 <span class="req">*</span></label><input required placeholder="○○소방서 / ○○시청"></div>
      <div class="field"><label>담당자 성함 <span class="req">*</span></label><input required placeholder="홍길동"></div>
      <div class="field"><label>이메일 <span class="req">*</span></label><input type="email" required placeholder="name@dept.go.kr"></div>
      <div class="field"><label>연락처</label><input placeholder="010-0000-0000"></div>
      <div class="field"><label>역할</label><select><option>현장 지휘</option><option>안전 담당</option><option>장비·기획</option><option>연구·기술</option><option>기타</option></select></div>
      <div class="field"><label>관심 영역</label><select><option>전체 시스템</option><option>디지털 트윈</option><option>AR 내비게이션</option><option>자동 탐지(AI)</option><option>하드웨어 통합</option></select></div>
      <div class="field full"><label>메시지</label><textarea placeholder="관할 규모, 도입 시점, 기대 효과 등을 알려주시면 맞춤 시연을 준비합니다."></textarea></div>
      <div class="actions"><button class="btn btn-pri" type="submit">요청 보내기 <span class="ar">→</span></button><span class="note">제출 시 개인정보처리방침에 동의하는 것으로 간주됩니다.</span></div>
    </form>
  </div>
</section>
<section class="sec">
  <div class="wrap"><div class="stats" style="border:0">
    <div class="stat rv"><div class="v">30<small>분</small></div><div class="k">표준 시연 시간</div></div>
    <div class="stat rv"><div class="v">2<small>일</small></div><div class="k">영업일 내 회신</div></div>
    <div class="stat rv"><div class="v">현장</div><div class="k">관할 방문 시연 가능</div></div>
    <div class="stat rv"><div class="v">무료</div><div class="k">기술 브리핑</div></div>
  </div></div>
</section>
"""

# ============================ INVEST ============================
invest_body = """
<section class="sec" style="border-bottom:0;padding-bottom:clamp(28px,4vh,40px)">
  <div class="prose"><div class="kicker rv" style="margin-bottom:20px"><span class="ln"></span>INVESTOR RELATIONS</div>
  <p class="lead-xl rv">재난 안전은 미루는 시장이 아닙니다.</p>
  <p class="rv">FRIZE는 드론·엣지 AI·AR·측위를 하나의 현장 지휘 OS로 통합한 딥테크 회사입니다. 우리는 '정보를 한 줄 더 보여주는 도구'가 아니라, 보기→찾기→안내하기→지키기를 잇는 시스템을 만듭니다.</p></div>
</section>
<section class="sec" style="padding-top:0">
  <div class="wrap">
    <div class="stats">
      <div class="stat rv"><div class="v">딥테크</div><div class="k">드론 · AI · AR · 측위 융합</div></div>
      <div class="stat rv"><div class="v">B2G</div><div class="k">소방·지자체·기관 시장</div></div>
      <div class="stat rv"><div class="v">풀스택</div><div class="k">HW·FW·AI·콕핏 자체 개발</div></div>
      <div class="stat rv"><div class="v">청주</div><div class="k">R&amp;D 거점 · 비수도권 강점</div></div>
    </div>
  </div>
</section>
<section class="sec" style="padding-top:clamp(40px,6vh,72px)">
  <div class="wrap" style="max-width:980px">
    <div class="kicker rv" style="margin-bottom:20px"><span class="ln"></span>CONTACT IR</div>
    <h2 class="sec-head" style="padding:0;margin-bottom:24px">투자 문의</h2>
    <form class="form" data-demo>
      <div class="field"><label>투자사 · 기관 <span class="req">*</span></label><input required placeholder="○○벤처스 / ○○캐피탈"></div>
      <div class="field"><label>담당자 <span class="req">*</span></label><input required placeholder="성함"></div>
      <div class="field"><label>이메일 <span class="req">*</span></label><input type="email" required placeholder="name@fund.com"></div>
      <div class="field"><label>관심 단계</label><select><option>Pre-Seed / Seed</option><option>Pre-A</option><option>Series A</option><option>전략적 투자</option></select></div>
      <div class="field full"><label>메시지</label><textarea placeholder="펀드 규모, 투자 영역, 미팅 가능 일정 등을 알려주세요. 데이터룸은 NDA 후 제공됩니다."></textarea></div>
      <div class="actions"><button class="btn btn-pri" type="submit">문의 보내기 <span class="ar">→</span></button><span class="note">IR 자료(데크·데이터룸)는 NDA 체결 후 제공됩니다.</span></div>
    </form>
    <p class="muted" style="margin-top:24px;font-size:13.5px">IR 직접 문의: <a href="mailto:ir@frize.kr" style="color:var(--ember)">ir@frize.kr</a></p>
  </div>
</section>
"""

PAGES=[
 ("index.html","FRIZE — 소방 현장 지휘 OS","연기 속 현장을 실시간 3D로 보고, 생존자를 자동 탐지하고, 대원을 AR로 안내하는 현장 지휘 운영체제.","",index_body),
 ("mission.html","미션","FRIZE의 미션과 작전 흐름, 설계 원칙.","mission",mission_body),
 ("system.html","시스템","디지털 트윈·VLM 탐지·AR 내비게이션·안전 인터록·하드웨어.","system",system_body),
 ("company.html","회사소개","청주 기반 안전 기술 회사 FRIZE.","company",company_body),
 ("news.html","뉴스","FRIZE 뉴스와 공지사항.","news",news_body),
 ("careers.html","채용","FRIZE와 함께할 동료를 찾습니다.","careers",careers_body),
 ("demo.html","데모 요청","관할 현장 데모와 기술 브리핑 요청.","",demo_body),
 ("invest.html","투자 문의","FRIZE 투자 문의 · IR.","",invest_body),
]
for slug,title,desc,active,body in PAGES:
    write(slug, head(title,desc)+header(active)+body+FOOTER)
print("done")
