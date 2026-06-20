// =====================================================================
//  FRIZE ANCHOR-1  ―  Air-Dropped UWB Positioning Beacon
//  Codename: "Breadcrumb"
//  Units: mm  |  OpenSCAD 2021.01+
//
//  드론(SCOUT-1)이 매거진에 싣고 다니다 진입구·계단참·복도 분기에 '투하'한다.
//  떨어뜨려도 깨지지 않게 충격흡수 범퍼 + 자기복원(self-righting) 무게추 베이스.
//  상단 UWB 안테나 돔이 항상 위를 보게 굴러서 정렬 → 대원 고글이 삼변측량으로
//  실내 위치를 잡는다. 색: 고시인성 형광(현장에서 보이게).
//
//  PART_MODE: ALL / SHELL_TOP / SHELL_BOT / BUMPER / ALL_NOBUMP
// =====================================================================
PART_MODE = "ALL";
$fn = 64; sfn = 18;

D     = 72;     // 외경
H     = 46;     // 전체 높이
wall  = 3.0;
bump_t= 7;      // 범퍼 두께

C_HI   =[0.95,0.62,0.06];   // 고시인성 오렌지(셸)
C_BLACK=[0.06,0.06,0.07];   // 범퍼/베이스 러버
C_DOME =[0.10,0.13,0.16,0.6];// UWB 안테나 돔(반투명)
C_METAL=[0.7,0.71,0.74];
C_GREEN=[0.2,0.85,0.4];
C_WEIGHT=[0.32,0.33,0.36];

module rbox(s,r=2,fn=sfn){ hull() for(x=[-1,1],y=[-1,1],z=[-1,1])
    translate([x*(s[0]/2-r),y*(s[1]/2-r),z*(s[2]/2-r)]) sphere(r=r,$fn=fn); }

// ---- 하단 셸: 무게추(self-right) + 배터리 + MCU ----
module shell_bot(){
    color(C_HI) difference(){
        union(){
            // 둥근 밑접시(굴러서 똑바로 서게 낮은 무게중심)
            translate([0,0,H*0.30]) scale([1,1,0.62]) sphere(d=D, $fn=96);
            cylinder(d=D-bump_t*2, h=H*0.42, $fn=96);
        }
        translate([0,0,H*0.30]) scale([1,1,0.62]) sphere(d=D-2*wall, $fn=96);
        translate([0,0,-1]) cylinder(d=D-2*wall, h=H, $fn=96);    // 내부 공동
        translate([0,0,H*0.42]) cylinder(d=D-bump_t*2-2, h=H, $fn=96);
    }
    // 바닥 텅스텐/스틸 무게추(self-righting)
    color(C_WEIGHT) translate([0,0,wall+3]) cylinder(d=D-bump_t*2-8, h=7, $fn=64);
    // 21700 단셀 배터리
    color(C_BLACK) translate([0,0,14]) cylinder(d=21.5, h=20, $fn=40);
    // MCU/UWB 보드(ESP32 + DWM3000)
    color(C_METAL) translate([0,0,H*0.42-1]) rbox([D-bump_t*2-10, D-bump_t*2-10, 2], 1, 10);
}

// ---- 상단 셸: UWB 안테나 돔 + 상태 LED ----
module shell_top(){
    color(C_HI) translate([0,0,H*0.42]) difference(){
        cylinder(d1=D-bump_t*2, d2=D-bump_t*2-14, h=H*0.30, $fn=96);
        translate([0,0,-1]) cylinder(d1=D-bump_t*2-2*wall, d2=D-bump_t*2-16, h=H, $fn=96);
    }
    // UWB 안테나 돔(꼭대기, 위를 보게)
    color(C_DOME) translate([0,0,H*0.72]) sphere(d=22, $fn=48);
    color(C_METAL) translate([0,0,H*0.70]) cylinder(d=10, h=4, $fn=32);   // 안테나 베이스
    // 상태 LED 링(투하/측위 fix 표시) ― 4개
    color(C_GREEN) for(a=[0:90:359]) rotate([0,0,a]) translate([(D-bump_t*2)/2-5,0,H*0.55]) cylinder(d=2.4,h=2,$fn=14);
    // 후크 아이(매거진 걸이/회수용)
    color(C_METAL) translate([0,0,H*0.80]) rotate([90,0,0]) difference(){ cylinder(d=8,h=3,center=true,$fn=24); cylinder(d=4,h=4,center=true,$fn=20);}
}

// ---- 충격흡수 범퍼 링(적도, 낙하 보호) ----
module bumper(){
    color(C_BLACK) rotate_extrude($fn=96)
        translate([D/2-bump_t/2,0,0]) circle(d=bump_t+3, $fn=24);
}

module anchor(){ shell_bot(); shell_top(); bumper(); }

if      (PART_MODE=="ALL")       anchor();
else if (PART_MODE=="ALL_NOBUMP"){ shell_bot(); shell_top(); }
else if (PART_MODE=="SHELL_TOP") shell_top();
else if (PART_MODE=="SHELL_BOT") shell_bot();
else if (PART_MODE=="BUMPER")    bumper();
