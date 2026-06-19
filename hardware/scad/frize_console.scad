// =====================================================================
//  FRIZE CONSOLE-1  ―  Field Command Console (v2: 재질그룹 + 정면 스크린)
//  Codename: "Command Deck"
//  Units: mm
//
//  MAT 변수로 재질그룹만 골라 STL 익스포트(Blender 다중재질):
//    "all"(프리뷰) / "body" / "screen" / "metal" / "accent" / "rubber"
//  예) openscad -D MAT=\"screen\" -o console_screen.stl frize_console.scad
// =====================================================================
MAT = "all";
$fn=40; sfn=12;

W=520; D=360; Hb=78; wall=4;
Ws=W-40; Hs=270; St=22;

C_BODY=[0.16,0.17,0.19]; C_SCR=[0.03,0.04,0.06]; C_METAL=[0.72,0.73,0.76];
C_ACCENT=[0.78,0.13,0.10]; C_RUBBER=[0.06,0.06,0.07]; C_GREEN=[0.2,0.8,0.35];

// 재질그룹 래퍼: 선택된 그룹만 렌더(없으면 all)
module m(g,c){ if(MAT=="all"||MAT==g) color(c) children(); }
module rbox(s,r=3,fn=sfn){ hull() for(x=[-1,1],y=[-1,1],z=[-1,1])
    translate([x*(s[0]/2-r),y*(s[1]/2-r),z*(s[2]/2-r)]) sphere(r=r,$fn=fn); }
module vent(n=6,len=70,w=3,p=8){ for(i=[0:n-1]) translate([0,(i-(n-1)/2)*p,0]) rbox([len,w,8],1,8); }

// ---- 데크 본체 ----
module deck(){
    translate([0,0,Hb/2]) {  // rbox 중심좌표→바닥좌표 보정: 데크 0~Hb
        m("body",C_BODY) difference(){
            rbox([W,D,Hb],6);
            translate([0,-D/2-2,-8]) rotate([32,0,0]) cube([W+8,60,46],center=true);  // 앞 손목 베벨
            for(s=[-1,1]) translate([s*(W/2-2),40,Hb/2]) rotate([0,90,0]) vent(4,80,3,9);
        }
        m("rubber",C_RUBBER) translate([0,-D/2+16,Hb-12]) rotate([32,0,0]) rbox([W-40,40,6],3,10); // 손목받침
    }
}

// ---- 스크린: 데크 뒤에서 조작자(전방 -Y)를 향해 거의 수직(뒤로 12°) ----
module screen(){
    translate([0, D/2-26, Hb-6]) rotate([-12,0,0]) translate([0,0,Hs/2]) {
        m("body",C_BODY) rbox([Ws+8, St, Hs+8],6);                 // 베젤(뒤판 포함)
        m("screen",C_SCR) translate([0,-St/2-0.5,2]) rbox([Ws, 3, Hs-30],1.5); // 디스플레이(-Y 정면)
        m("metal",C_METAL) translate([0,-St/2-1, Hs/2-16]) rbox([110,2.5,7],1,8);  // 상단 센서바(정면)
        m("accent",C_ACCENT) translate([0,-St/2-1.2,-Hs/2+16]) rbox([60,2,3],1,8); // 하단 레드 라인(정면)
        m("metal",C_METAL) translate([0,St/2-1,0]) rotate([90,0,0]) vent(7,Ws-40,4,16); // 후면 방열(뒤)
    }
}

// ---- 컨트롤덱: 버튼 위주(대형 매트릭스 + 전용키 + 인코더/조그) ----
module key(c=C_BODY,d=24,h=8){ m(c==C_ACCENT?"accent":"metal",c){ rbox([d,d,h],2,10);} }
module knob(d=34,h=16){ m("metal",C_METAL) cylinder(d=d,h=h,$fn=36);
    m("body",C_BODY) translate([0,0,h]) cylinder(d=d-4,h=2,$fn=36);
    m("accent",C_ACCENT) translate([0,d/2-4,h+1]) cube([2,5,2],center=true); }

module controls(){
    z=Hb+0.4;
    // 기능 버튼 매트릭스 6 x 3 (조작 대부분을 물리버튼으로)
    for(i=[-2.5:1:2.5], j=[0:2]) translate([i*34, -36+j*30, z]) key(C_BODY,26,8);
    // 유닛 선택 전용키 행(앞단, 레드 1개 강조)
    for(i=[-3:1:3]) translate([i*44, -D/2+54, z]) key(i==0?C_ACCENT:C_BODY,30,9);
    // 좌측 인코더 2 + 우측 조그다이얼
    translate([-W/2+58, 50, z]) knob(34,16);
    translate([-W/2+58, 6, z]) knob(30,14);
    translate([ W/2-70, 30, z]) knob(60,20);
    // E-STOP
    m("metal",C_METAL) translate([W/2-62,D/2-66,z]) cylinder(d=46,h=10,$fn=40);
    m("accent",C_ACCENT) translate([W/2-62,D/2-66,z+8]) { cylinder(d=40,h=12,$fn=40); translate([0,0,12]) sphere(20,$fn=28);}
    // 상태 LED 줄
    m("body",C_BODY) for(i=[0:3]) translate([-W/2+120+i*16, D/2-30, z]) cylinder(d=5,h=2,$fn=14);
}

// ---- 측면 핸들 / 범퍼 / 후면포트 / 안테나 / 배터리 / 발 ----
module handles(){ for(s=[-1,1]) m("metal",C_METAL)
    translate([s*(W/2+8),0,Hb*0.55]) rotate([90,0,0])
        difference(){ rbox([26,Hb*0.8,90],8,12); rbox([15,Hb,94],6,12);} }
module bumpers(){
    for(x=[-1,1],y=[-1,1]) m("accent",C_ACCENT) translate([x*(W/2-10),y*(D/2-10),6]) rbox([32,32,16],6,10);
    for(x=[-1,1],y=[-1,1]) m("rubber",C_RUBBER) translate([x*(W/2-10),y*(D/2-10),Hb-6]) rbox([32,32,12],6,10);
}
module rear(){
    m("body",C_BODY) translate([0,D/2-3,Hb*0.45]) difference(){ rbox([W-140,10,Hb*0.55],4);
        rotate([90,0,0]){ translate([-130,0,0]) cylinder(d=15,h=20,center=true,$fn=24);
            for(i=[-1,1]) translate([-80+i*26,0,0]) cube([15,11,20],center=true);
            for(i=[0:2]) translate([30+i*22,5,0]) cube([12,5,20],center=true);
            translate([120,-5,0]) cube([22,4,20],center=true);} }
    for(i=[-1,0,1]) m("rubber",C_RUBBER) translate([i*70,D/2-16,Hb+6]){ cylinder(d=10,h=6,$fn=18);
        translate([0,0,6]) cylinder(d=6,h=66,$fn=18); translate([0,0,72]) sphere(4,$fn=14);} }
module battery(){ m("body",C_BODY) translate([W/2-6,-40,Hb*0.4])
        difference(){ rbox([18,120,Hb*0.5],3); translate([4,0,0]) rbox([18,108,Hb*0.5-6],2);}
    m("metal",C_METAL) for(i=[0:3]) translate([W/2+2,-80+i*12,Hb*0.4]) sphere(2,$fn=10); }
module feet(){ for(x=[-1,1],y=[-1,1]) m("rubber",C_RUBBER)
    translate([x*(W/2-40),y*(D/2-40),-2]) cylinder(d=30,h=6,$fn=24); }

module console(){ deck(); screen(); controls(); handles(); bumpers(); rear(); battery(); feet(); }
console();
