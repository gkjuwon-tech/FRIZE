# -*- coding: utf-8 -*-
"""control_map.json → 투박한(유틸리티) 운용 가이드 HTML → (puppeteer) PDF.
콕핏·콘솔과 동일한 단일 진실원천을 읽으므로 버튼 설명이 항상 일치한다.
  python3 gen_guide.py        (guide.html 생성)
  node shot_pdf.js            (guide.html → FRIZE_OPERATOR_GUIDE.pdf)
"""
import json, os, html
HERE = os.path.dirname(os.path.abspath(__file__))
MAP  = os.path.join(HERE, "..", "..", "..", "software", "apps", "frize_cockpit_imgui", "control_map.json")
cm = json.load(open(MAP, encoding="utf-8"))
E = html.escape

ACTION_DESC = {
 "advance":"선택 유닛 전진(대원=AR 전진지시 / 드론=전방이동)",
 "retreat":"후퇴 지시(안전 인터록: 항상 허용)",
 "hold":"현 위치 정지/호버", "rally":"집결지로 소집",
 "recon_zone":"드론 자율 프런티어 탐사 개시(빈 공간 우선)",
 "investigate_point":"드론에게 '저기 먼저 조사' ― 지정점 우선비행 후 자율탐사 재개 (트윈에서 점 찍기)",
 "deploy_anchor":"UWB 측위 비콘 투하(매거진 1발). 진입 직후 깔아 실내 측위망 구축",
 "rtl":"드론 발사지점 복귀(Return-To-Launch)", "land":"드론 착륙",
 "open":"VENT-1 배연/진입 포트 개방", "close":"VENT-1 폐쇄",
 "aim_and_spray":"방수포 조준·방수 (지점 지정)",
 "ar_text":"선택 대원 고글에 텍스트 띄우기", "ar_arrow":"고글에 방향 화살표(지점 지정)",
 "ar_route":"고글에 경로 표시(경유점 지정)", "mark_hazard":"위험 지점 마킹(트윈+고글 공유)",
 "force_entry":"강제 진입 지시 (생명직결 → CONFIRM 필요)",
 "confirm":"무장된(CONFIRM) 명령을 실제 발행 — 안전 인터록 통과분만 디바이스로",
 "evac_all":"긴급: 전 대원 즉시 후퇴 브로드캐스트",
}

def table(rows, head):
    h = "<tr>" + "".join(f"<th>{E(c)}</th>" for c in head) + "</tr>"
    b = "".join("<tr>" + "".join(f"<td>{c}</td>" for c in r) + "</tr>" for r in rows)
    return f"<table>{h}{b}</table>"

parts = []
parts.append("""<!doctype html><html lang=ko><head><meta charset=utf-8>
<link rel=stylesheet href="https://cdn.jsdelivr.net/gh/orioncactus/pretendard@v1.3.9/dist/web/static/pretendard.min.css">
<style>
@page{size:A4;margin:14mm 13mm}
*{box-sizing:border-box}
body{font-family:"Pretendard",-apple-system,sans-serif;color:#111;font-size:10.5px;line-height:1.5}
.mono{font-family:"DejaVu Sans Mono",ui-monospace,monospace}
h1{font-size:26px;letter-spacing:-.5px;margin:0 0 2px}
h2{font-size:15px;margin:20px 0 8px;border-bottom:2px solid #111;padding-bottom:3px;text-transform:uppercase;letter-spacing:1px}
h3{font-size:12px;margin:12px 0 5px;background:#111;color:#fff;display:inline-block;padding:2px 8px}
.bar{height:8px;background:repeating-linear-gradient(45deg,#111 0 8px,#fff 8px 16px);margin:6px 0 14px}
table{width:100%;border-collapse:collapse;margin:6px 0 10px}
th,td{border:1px solid #111;padding:4px 6px;text-align:left;vertical-align:top}
th{background:#111;color:#fff;font-size:9.5px;letter-spacing:.5px}
td{font-size:10px}
.key{font-family:"DejaVu Sans Mono",monospace;font-weight:700;background:#eee;border:1px solid #888;border-bottom:2px solid #888;border-radius:3px;padding:0 5px;white-space:nowrap}
.cover{border:3px solid #111;padding:20px;margin-bottom:14px}
.cover .sub{font-size:12px;color:#444;margin-top:4px}
.warn{border:2px solid #c00;color:#900;padding:8px 10px;font-weight:700;margin:10px 0;font-size:10.5px}
.grid{display:grid;grid-template-columns:repeat(6,1fr);gap:4px;margin:8px 0}
.cell{border:1.5px solid #111;padding:5px 4px;min-height:46px}
.cell b{font-size:10px} .cell .k{float:right;font-size:8.5px}
.cell .s{display:block;color:#555;font-size:8.5px;margin-top:2px}
.cell.red{border-color:#c00} .cell.amber{border-color:#c80} .cell.cyan{border-color:#069}
.pg{page-break-before:always}
.small{font-size:9px;color:#555}
footer{position:fixed;bottom:4mm;left:0;right:0;text-align:center;font-size:8px;color:#888}
</style></head><body>""")

# Cover
parts.append(f"""<div class=cover>
<div class=mono style="font-size:11px;letter-spacing:3px">FRIZE COMMAND OS</div>
<h1>운용 가이드 · OPERATOR FIELD GUIDE</h1>
<div class=sub>CONSOLE-1 지휘 콘솔 · 콕핏 · 드론(SCOUT) · 고글(VISOR) · 배연포트(VENT) · UWB 앵커</div>
<div class=mono class=sub style="margin-top:8px">control_map v{cm.get('version','1.0')} · 단일 진실원천 동기화 · 현장 인쇄용</div>
</div>
<div class=warn>⚠ 본 장비는 프로토타입이다. 생명직결 명령(빨강 버튼)은 반드시 CONFIRM 후 발행되며 안전 인터록을 통과한 명령만 디바이스로 전달된다. E-STOP은 전 대원 즉시 후퇴다.</div>""")

# 1. 시스템 개요
parts.append("<h2>1. 시스템 개요</h2><div class=bar></div>")
parts.append("<p>FRIZE는 소방 현장을 하나의 운영체제로 묶는다. 커널(CORE)이 MQTT 버스로 디바이스를 연결하고, 지휘관은 CONSOLE-1에서 3D 디지털 트윈을 보며 물리 버튼으로 명령한다. <b>콕핏 화면의 모든 버튼 = 콘솔의 물리 버튼 = 키보드</b>, 셋이 정확히 같은 기능이다.</p>")
parts.append(table([
 ["CORE","지휘 커널(C++)","월드모델·라우터·안전 인터록·VLM 판단"],
 ["SCOUT-1","정찰 드론","자율 프런티어 탐사 + 명시적 태스킹 + UWB 앵커 투하"],
 ["VISOR-1~3","스마트 고글","열화상·가스·AR 표시 + UWB/GPS 측위(NAV 포드)"],
 ["VENT-1","IoT 배연/진입 포트","원격 개폐(인터록 경유)"],
 ["ANCHOR-1","UWB 측위 비콘","드론이 투하 → 실내 삼변측량 측위망"],
 ["CONSOLE-1","지휘 콘솔","물리 컨트롤덱 + 트윈 디스플레이 + E-STOP"],
],["기기","역할","핵심"]))

# 2. 운용 절차
parts.append("<h2>2. 표준 운용 절차 (진입~구조)</h2><div class=bar></div>")
parts.append(table([
 ["①","측위망 구축","SCOUT 선투입 → <span class=key>A</span> ANCHOR로 진입구·계단참·분기에 비콘 투하. 대원 위치가 트윈에 점등."],
 ["②","정찰","<span class=key>T</span> RECON 자율탐사 / 의심구역은 트윈 클릭 후 <span class=key>Y</span> INVESTG('저기 먼저 조사')."],
 ["③","분석","트윈이 TSDF 표면으로 재구성. 화점·연기·요구조자 표시. NAV 인코더로 층/단면 전환."],
 ["④","지휘","대원 선택(<span class=key>1-7</span>) 후 COMMAND. AR로 텍스트/화살표/경로를 고글에 직접."],
 ["⑤","실행/안전","생명직결 명령은 CONFIRM. 위급 시 E-STOP(<span class=key>Space×2</span>)."],
 ["⑥","구조·복귀","요구조자 구조, 드론 <span class=key>S</span> RTL 복귀."],
],["단계","행동","조작"]))

# 3. 컨트롤 맵(클러스터별)
parts.append("<h2 class=pg>3. CONSOLE-1 컨트롤 맵 (버튼 정의)</h2><div class=bar></div>")
parts.append("<p class=small>콘솔의 모든 컨트롤과 그 기능. 키 = 콕핏 키보드 단축키. 콘솔 키캡에 동일 라벨이 각인되어 있다.</p>")

sel = cm["clusters"]["select"]
parts.append(f"<h3>UNIT SELECT — {E(sel['desc'])}</h3>")
parts.append(table([[f'<span class=key>{E(c["key"])}</span>', E(c["label"]), E(c.get("sub","")), E(c.get("target",""))] for c in sel["controls"]],
                   ["키","라벨","설명","대상 ID"]))

cmd = cm["clusters"]["command"]
parts.append(f"<h3>COMMAND MATRIX — {E(cmd['desc'])}</h3>")
rows=[]
for c in sorted(cmd["controls"], key=lambda c:(c["row"],c["col"])):
    flags=[]
    if c.get("confirm"): flags.append("CONFIRM")
    if c.get("needs_point"): flags.append("지점지정")
    if c.get("needs_route"): flags.append("경로지정")
    rows.append([f'<span class=key>{E(c["key"])}</span>', E(c["label"]),
                 E(ACTION_DESC.get(c.get("action",""), c.get("sub",""))), " · ".join(flags)])
parts.append(table(rows,["키","버튼","기능","비고"]))

for cl in ("nav","jog","estop"):
    cluster=cm["clusters"][cl]
    parts.append(f"<h3>{E(cluster['title'])} — {E(cluster['desc'])}</h3>")
    parts.append(table([[f'<span class=key>{E(c.get("key",""))}</span>', E(c["label"]),
                         E(c.get("detail", ACTION_DESC.get(c.get("action",""), c.get("sub",""))))] for c in cluster["controls"]],
                       ["키","컨트롤","기능"]))

# 4. 퀵 레퍼런스(매트릭스 배열 그대로)
parts.append("<h2 class=pg>4. 퀵 레퍼런스 카드 (COMMAND MATRIX 배열)</h2><div class=bar></div>")
parts.append("<p class=small>콘솔 6×3 물리 매트릭스와 동일 배열. 잘라서 콘솔 옆에 붙여라.</p>")
grid=[None]*(cmd["cols"]*cmd["rows"])
for c in cmd["controls"]: grid[c["row"]*cmd["cols"]+c["col"]]=c
cells=""
for c in grid:
    if not c: cells+="<div class=cell></div>"; continue
    cls=c.get("color","")
    cls=" "+cls if cls in ("red","amber","cyan") else ""
    cells+=f'<div class="cell{cls}"><b>{E(c["label"])}</b><span class=k>{E(c["key"])}</span><span class=s>{E(c.get("sub",""))}</span></div>'
parts.append(f"<div class=grid>{cells}</div>")
parts.append("<p class=small>색 테두리: 빨강=생명직결(CONFIRM) · 황색=주명령 · 청색=지점지정 · 무색=일반.</p>")

parts.append("<footer class=mono>FRIZE — WE FREEZE THE CHAOS · control_map 단일원천 · 무단 현장 적용 금지(프로토타입)</footer>")
parts.append("</body></html>")

open(os.path.join(HERE,"guide.html"),"w",encoding="utf-8").write("\n".join(parts))
print("wrote guide.html")
