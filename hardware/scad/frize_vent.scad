// =====================================================================
//  FRIZE VENT-1  ―  IoT 자동 배연/진입 포트 (Auto Vent & Access Port)
//  Codename: "Pry"
//  Units: mm  |  콕핏 OPEN 딸깍 → 리니어 액추에이터가 배연구/문을 민다.
// =====================================================================
PART_MODE="ALL"; $fn=40; sfn=12;
W=200; D=120; H=90; wall=4;
C_BODY=[0.13,0.14,0.16]; C_DECK=[0.10,0.11,0.13]; C_ACCENT=[0.80,0.15,0.12];
C_METAL=[0.72,0.73,0.76]; C_RUBBER=[0.06,0.06,0.07]; C_GREEN=[0.2,0.8,0.35];

module rbox(s,r=3,fn=sfn){ hull() for(x=[-1,1],y=[-1,1],z=[-1,1])
    translate([x*(s[0]/2-r),y*(s[1]/2-r),z*(s[2]/2-r)]) sphere(r=r,$fn=fn); }
module vent(n=5,len=70,w=3,p=8){ for(i=[0:n-1]) translate([(i-(n-1)/2)*p,0,0]) rbox([w,len,8],1,8); }

// 본체(러기드 IP65 박스)
module body(){
    color(C_BODY) difference(){
        rbox([W,D,H],6);
        for(s=[-1,1]) translate([s*(W/2-2),0,H*0.2]) rotate([0,90,0]) vent(4,60,3,9); // 측면 방열
    }
    // 코너 범퍼
    for(x=[-1,1],y=[-1,1],z=[-1,1]) color(C_ACCENT)
        translate([x*(W/2-9),y*(D/2-9),z*(H/2-9)]) rbox([20,20,20],5,10);
    // 상단 핸들
    color(C_METAL) translate([0,0,H/2+6]) rotate([90,0,0])
        difference(){ rbox([90,22,16],8,12); rbox([74,30,16],6,12); }
    // 상태 LED 돔 + 안테나
    color(C_GREEN) translate([W/2-26,-D/2+14,H/2+1]) sphere(5,$fn=16);
    color(C_DECK) translate([W/2-50,-D/2+14,H/2]) { cylinder(d=10,h=6,$fn=18);
        translate([0,0,6]) cylinder(d=6,h=60,$fn=18); translate([0,0,66]) sphere(4,$fn=14); }
    // 배터리 해치(후면)
    color(C_DECK) translate([0,D/2-2,0]) rbox([120,8,H-26],3);
    color(C_GREEN) for(i=[0:3]) translate([-30+i*10,D/2+2,0]) sphere(2,$fn=10);
}

// 리니어 액추에이터(전방 -Y로 밀어 개방), 부분 연장 상태로 표현
module actuator(){
    ext=70;   // 연장량
    color(C_DECK) translate([0,-D/2,0]) rotate([90,0,0]) cylinder(d=46,h=10,$fn=40);     // 하우징 플랜지
    color(C_METAL) translate([0,-D/2-ext/2,0]) rotate([90,0,0]) cylinder(d=22,h=ext,center=true,$fn=36); // 로드
    color(C_DECK) translate([0,-D/2-ext,0]) rotate([90,0,0]) cylinder(d=60,h=12,center=true,$fn=40);     // 푸시 패드
    color(C_RUBBER) translate([0,-D/2-ext-7,0]) rotate([90,0,0]) cylinder(d=64,h=4,center=true,$fn=40);  // 패드 러버
}

// 도어프레임 클램프(후면 2개)
module clamps(){
    for(s=[-1,1]) color(C_METAL) translate([s*(W/2-24), D/2+6, 0])
        difference(){ rbox([24,40,H-30],4,10); translate([0,8,0]) rbox([16,40,H-44],3,10);
            translate([0,-12,0]) cube([10,8,H],center=true); }
}

module vent1(){ body(); actuator(); clamps(); }
if(PART_MODE=="ALL") vent1();
else if(PART_MODE=="BODY") body();
