# -*- coding: utf-8 -*-
"""control_map.json → console_legend.scad
콕핏과 동일한 컨트롤 맵으로 CONSOLE-1 컨트롤덱을 생성한다(각인 라벨 포함).
이 스크립트가 단일 진실원천(control_map.json)을 SCAD로 컴파일 → 콘솔의 버튼이
콕핏 기능과 1:1로 정확히 일치한다. frize_console.scad가 이 출력을 include한다.
  python3 gen_console_legend.py
"""
import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
MAP  = os.path.join(HERE, "..", "..", "software", "apps", "frize_cockpit_imgui", "control_map.json")
OUT  = os.path.join(HERE, "console_legend.scad")

cm = json.load(open(MAP, encoding="utf-8"))
Hb = 78  # frize_console.scad 데크 상면

# 색 → SCAD 재질그룹 매핑(다중재질 STL 유지)
COL = {"red":"accent","amber":"accent","orange":"accent",
       "cyan":"metal","steel":"metal","green":"body","white":"body"}

def cap(x, y, label, sub, grp, w=44, h=11):
    """라벨 각인 키캡 하나. 데크에 ~3mm 박히고 ~5mm 돌출(실제 키처럼), 라벨은 상면 양각."""
    capcol = "C_ACCENT" if grp=="accent" else ("C_METAL" if grp=="metal" else "C_BODY")
    top = h/2.0                       # 캡 중심(translate) 기준 상면 z
    s = []
    s.append(f'  translate([{x:.1f},{y:.1f},{Hb+2.5}]) {{')   # 중심 Hb+2.5 → 상면 Hb+8, 하면 Hb-3(박힘)
    s.append(f'    m("{grp}", {capcol}) rbox([{w},{w*0.78:.0f},{h}],2,10);')
    fs = max(2.6, min(4.4, (w-6)/max(3,len(label))*1.7))
    ylab = 2.0 if sub else 0
    s.append(f'    m("metal",C_METAL) translate([0,{ylab},{top:.1f}]) linear_extrude(0.7) '
             f'text("{label}", size={fs:.1f}, halign="center", valign="center", font="Liberation Sans:style=Bold");')
    if sub:
        s.append(f'    m("metal",C_METAL) translate([0,{-w*0.78/2+3.4:.1f},{top:.1f}]) linear_extrude(0.6) '
                 f'text("{sub}", size=2.3, halign="center", valign="center", font="Liberation Sans");')
    s.append('  }')
    return "\n".join(s)

lines = ["// AUTO-GENERATED from control_map.json by gen_console_legend.py ― 직접 수정 금지.",
         "// 콕핏(ImGui)과 동일한 컨트롤 맵. 버튼=콕핏 기능 1:1.",
         "module frize_controls(){"]

# ── COMMAND MATRIX 6x3 (중앙) ──
cmd = cm["clusters"]["command"]
mx_sp_x, mx_sp_y = 60, 44
mx_x0 = -(cmd["cols"]-1)/2*mx_sp_x
mx_y0 = 40
lines.append("  // ── COMMAND MATRIX (선택 유닛에 명령) ──")
for c in cmd["controls"]:
    x = mx_x0 + c["col"]*mx_sp_x
    y = mx_y0 - c["row"]*mx_sp_y
    grp = COL.get(c.get("color","steel"), "body")
    lines.append(cap(x, y, c["label"], c.get("sub",""), grp, w=46, h=9))

# ── UNIT SELECT row (앞단) ──
sel = cm["clusters"]["select"]
sl_sp = 62; sl_x0 = -(sel["cols"]-1)/2*sl_sp; sl_y = -118
lines.append("  // ── UNIT SELECT (대상 선택) ──")
for c in sel["controls"]:
    x = sl_x0 + c["col"]*sl_sp
    grp = "accent" if c.get("led")=="red" else "body"
    lines.append(cap(x, sl_y, c["label"], c.get("sub",""), grp, w=50, h=8))
    # 선택 LED
    led = {"cyan":"C_METAL","amber":"C_ACCENT","red":"C_ACCENT","green":"C_GREEN","white":"C_METAL"}.get(c.get("led"),"C_GREEN")
    lines.append(f'  m("metal",{led}) translate([{x:.1f},{sl_y+22},{Hb+1}]) cylinder(d=3,h=2,$fn=14);')

# ── NAV 인코더 2 (좌측) ──
lines.append("  // ── NAV 인코더(트윈 층/줌) ──")
nav = cm["clusters"]["nav"]["controls"]
nav_pos = [(-205,55),(-205,8)]
for (nx,ny),c in zip(nav_pos, nav):
    lines.append(f'  m("metal",C_METAL) translate([{nx},{ny},{Hb-1.2}]) cylinder(d=34,h=15,$fn=36);')
    lines.append(f'  m("body",C_BODY) translate([{nx},{ny},{Hb+13.8}]) cylinder(d=30,h=2,$fn=36);')
    lines.append(f'  m("accent",C_ACCENT) translate([{nx},{ny+11},{Hb+15}]) cube([2,5,2],center=true);')
    lines.append(f'  m("metal",C_METAL) translate([{nx},{ny-24},{Hb+0.5}]) linear_extrude(0.8) '
                 f'text("{c["label"]}", size=4, halign="center", font="Liberation Sans:style=Bold");')

# ── JOG (우측) ──
lines.append("  // ── JOG(카메라 궤도/선택) ──")
jx,jy = 195,30
lines.append(f'  m("metal",C_METAL) translate([{jx},{jy},{Hb-1.2}]) cylinder(d=60,h=20,$fn=48);')
lines.append(f'  m("body",C_BODY) translate([{jx},{jy},{Hb+18.8}]) cylinder(d=52,h=2,$fn=48);')
lines.append(f'  m("accent",C_ACCENT) translate([{jx},{jy+22},{Hb+20}]) cube([2,6,2],center=true);')
lines.append(f'  m("metal",C_METAL) translate([{jx},{jy-38},{Hb+0.5}]) linear_extrude(0.8) '
             f'text("JOG", size=5, halign="center", font="Liberation Sans:style=Bold");')

# ── E-STOP (우상단) ──
lines.append("  // ── E-STOP(전대원 후퇴) ──")
ex,ey = 198,120
lines.append(f'  m("metal",C_METAL) translate([{ex},{ey},{Hb-1.2}]) cylinder(d=46,h=11.2,$fn=40);')
lines.append(f'  m("accent",C_ACCENT) translate([{ex},{ey},{Hb+9.2}]) {{ cylinder(d=40,h=12,$fn=40); translate([0,0,12]) sphere(20,$fn=28); }}')
lines.append(f'  m("metal",C_METAL) translate([{ex},{ey-30},{Hb+0.5}]) linear_extrude(0.8) '
             f'text("E-STOP / EVAC", size=4, halign="center", font="Liberation Sans:style=Bold");')

# ── 상태 LED 줄 ──
lines.append('  m("body",C_BODY) for(i=[0:3]) translate([-140+i*16, 150, '+str(Hb-0.6)+']) cylinder(d=5,h=2.6,$fn=14);')
lines.append("}")

open(OUT, "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("wrote", OUT, "(%d controls)" % (len(cmd["controls"])+len(sel["controls"])+len(nav)+2))
