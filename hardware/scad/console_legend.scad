// AUTO-GENERATED from control_map.json by gen_console_legend.py ― 직접 수정 금지.
// 콕핏(ImGui)과 동일한 컨트롤 맵. 버튼=콕핏 기능 1:1.
module frize_controls(){
  // ── COMMAND MATRIX (선택 유닛에 명령) ──
  translate([-150.0,40.0,80.5]) {
    m("accent", C_ACCENT) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,0,4.5]) linear_extrude(0.7) text("ADVANCE", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
  }
  translate([-90.0,40.0,80.5]) {
    m("metal", C_METAL) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,0,4.5]) linear_extrude(0.7) text("RETREAT", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
  }
  translate([-30.0,40.0,80.5]) {
    m("metal", C_METAL) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,0,4.5]) linear_extrude(0.7) text("HOLD", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
  }
  translate([30.0,40.0,80.5]) {
    m("metal", C_METAL) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,0,4.5]) linear_extrude(0.7) text("RALLY", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
  }
  translate([90.0,40.0,80.5]) {
    m("metal", C_METAL) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,0,4.5]) linear_extrude(0.7) text("RECON", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
  }
  translate([150.0,40.0,80.5]) {
    m("metal", C_METAL) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("INVESTG", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("저기조사", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  translate([-150.0,-4.0,80.5]) {
    m("accent", C_ACCENT) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("ANCHOR", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("비콘투하", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  translate([-90.0,-4.0,80.5]) {
    m("metal", C_METAL) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("RTL", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("복귀", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  translate([-30.0,-4.0,80.5]) {
    m("metal", C_METAL) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,0,4.5]) linear_extrude(0.7) text("LAND", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
  }
  translate([30.0,-4.0,80.5]) {
    m("accent", C_ACCENT) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("OPEN", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("개방", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  translate([90.0,-4.0,80.5]) {
    m("metal", C_METAL) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("CLOSE", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("폐쇄", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  translate([150.0,-4.0,80.5]) {
    m("metal", C_METAL) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("SPRAY", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("조준방수", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  translate([-150.0,-48.0,80.5]) {
    m("accent", C_ACCENT) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("AR-TEXT", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("문구", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  translate([-90.0,-48.0,80.5]) {
    m("accent", C_ACCENT) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("AR-ARROW", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("방향", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  translate([-30.0,-48.0,80.5]) {
    m("accent", C_ACCENT) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("AR-ROUTE", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("경로", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  translate([30.0,-48.0,80.5]) {
    m("accent", C_ACCENT) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("MARK", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("위험표시", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  translate([90.0,-48.0,80.5]) {
    m("accent", C_ACCENT) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("FORCE", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("강제진입", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  translate([150.0,-48.0,80.5]) {
    m("body", C_BODY) rbox([46,36,9],2,10);
    m("metal",C_METAL) translate([0,2.0,4.5]) linear_extrude(0.7) text("CONFIRM", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-14.5,4.5]) linear_extrude(0.6) text("확인발행", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  // ── UNIT SELECT (대상 선택) ──
  translate([-186.0,-118.0,80.5]) {
    m("body", C_BODY) rbox([50,39,8],2,10);
    m("metal",C_METAL) translate([0,2.0,4.0]) linear_extrude(0.7) text("SCOUT", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-16.1,4.0]) linear_extrude(0.6) text("드론", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  m("metal",C_METAL) translate([-186.0,-96,79]) cylinder(d=3,h=2,$fn=14);
  translate([-124.0,-118.0,80.5]) {
    m("body", C_BODY) rbox([50,39,8],2,10);
    m("metal",C_METAL) translate([0,2.0,4.0]) linear_extrude(0.7) text("VISOR-1", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-16.1,4.0]) linear_extrude(0.6) text("대원 A", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  m("metal",C_ACCENT) translate([-124.0,-96,79]) cylinder(d=3,h=2,$fn=14);
  translate([-62.0,-118.0,80.5]) {
    m("body", C_BODY) rbox([50,39,8],2,10);
    m("metal",C_METAL) translate([0,2.0,4.0]) linear_extrude(0.7) text("VISOR-2", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-16.1,4.0]) linear_extrude(0.6) text("대원 B", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  m("metal",C_GREEN) translate([-62.0,-96,79]) cylinder(d=3,h=2,$fn=14);
  translate([0.0,-118.0,80.5]) {
    m("body", C_BODY) rbox([50,39,8],2,10);
    m("metal",C_METAL) translate([0,2.0,4.0]) linear_extrude(0.7) text("VISOR-3", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-16.1,4.0]) linear_extrude(0.6) text("대원 C", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  m("metal",C_GREEN) translate([0.0,-96,79]) cylinder(d=3,h=2,$fn=14);
  translate([62.0,-118.0,80.5]) {
    m("body", C_BODY) rbox([50,39,8],2,10);
    m("metal",C_METAL) translate([0,2.0,4.0]) linear_extrude(0.7) text("VENT-1", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-16.1,4.0]) linear_extrude(0.6) text("배연포트", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  m("metal",C_GREEN) translate([62.0,-96,79]) cylinder(d=3,h=2,$fn=14);
  translate([124.0,-118.0,80.5]) {
    m("accent", C_ACCENT) rbox([50,39,8],2,10);
    m("metal",C_METAL) translate([0,2.0,4.0]) linear_extrude(0.7) text("ALL", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-16.1,4.0]) linear_extrude(0.6) text("전체", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  m("metal",C_ACCENT) translate([124.0,-96,79]) cylinder(d=3,h=2,$fn=14);
  translate([186.0,-118.0,80.5]) {
    m("body", C_BODY) rbox([50,39,8],2,10);
    m("metal",C_METAL) translate([0,2.0,4.0]) linear_extrude(0.7) text("SITE", size=4.4, halign="center", valign="center", font="Liberation Sans:style=Bold");
    m("metal",C_METAL) translate([0,-16.1,4.0]) linear_extrude(0.6) text("현장뷰", size=2.3, halign="center", valign="center", font="Liberation Sans");
  }
  m("metal",C_METAL) translate([186.0,-96,79]) cylinder(d=3,h=2,$fn=14);
  // ── NAV 인코더(트윈 층/줌) ──
  m("metal",C_METAL) translate([-205,55,76.8]) cylinder(d=34,h=15,$fn=36);
  m("body",C_BODY) translate([-205,55,91.8]) cylinder(d=30,h=2,$fn=36);
  m("accent",C_ACCENT) translate([-205,66,93]) cube([2,5,2],center=true);
  m("metal",C_METAL) translate([-205,31,78.5]) linear_extrude(0.8) text("LAYER", size=4, halign="center", font="Liberation Sans:style=Bold");
  m("metal",C_METAL) translate([-205,8,76.8]) cylinder(d=34,h=15,$fn=36);
  m("body",C_BODY) translate([-205,8,91.8]) cylinder(d=30,h=2,$fn=36);
  m("accent",C_ACCENT) translate([-205,19,93]) cube([2,5,2],center=true);
  m("metal",C_METAL) translate([-205,-16,78.5]) linear_extrude(0.8) text("ZOOM", size=4, halign="center", font="Liberation Sans:style=Bold");
  // ── JOG(카메라 궤도/선택) ──
  m("metal",C_METAL) translate([195,30,76.8]) cylinder(d=60,h=20,$fn=48);
  m("body",C_BODY) translate([195,30,96.8]) cylinder(d=52,h=2,$fn=48);
  m("accent",C_ACCENT) translate([195,52,98]) cube([2,6,2],center=true);
  m("metal",C_METAL) translate([195,-8,78.5]) linear_extrude(0.8) text("JOG", size=5, halign="center", font="Liberation Sans:style=Bold");
  // ── E-STOP(전대원 후퇴) ──
  m("metal",C_METAL) translate([198,120,76.8]) cylinder(d=46,h=11.2,$fn=40);
  m("accent",C_ACCENT) translate([198,120,87.2]) { cylinder(d=40,h=12,$fn=40); translate([0,0,12]) sphere(20,$fn=28); }
  m("metal",C_METAL) translate([198,90,78.5]) linear_extrude(0.8) text("E-STOP / EVAC", size=4, halign="center", font="Liberation Sans:style=Bold");
  m("body",C_BODY) for(i=[0:3]) translate([-140+i*16, 150, 77.4]) cylinder(d=5,h=2.6,$fn=14);
}
