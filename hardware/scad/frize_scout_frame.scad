// =====================================================================
//  FRIZE SCOUT-1  ―  Firefighting Recon Hexacopter  (PRODUCTION-DETAIL)
//  Codename: "Salamander"
//  Units: mm  |  OpenSCAD 2021.01+
//
//  PART_MODE: "ALL"/"PLATE"/"ARM_CLAMP"/"MOTOR_MOUNT"/"PAYLOAD"/"LEG"
// =====================================================================

PART_MODE = "ALL";
SHOW_PROPS = true;
$fn = 40;
sphere_fn = 24;

// ---------------- 파라미터 ----------------
arm_n        = 6;
span         = 650;          // 모터 대각(클래스)
plate_d      = 190;
plate_t      = 4;
stack_h      = 72;
tube_d       = 16;           // 카본 암 튜브 외경
arm_reach    = span/2 - 30;
motor_d      = 42;           // 모터 캔 직경
motor_h      = 26;
prop_d       = 380;          // 프롭 직경
leg_len      = 175;

// 팔레트
C_CARBON = [0.10,0.10,0.11];
C_PLATE  = [0.13,0.13,0.14];
C_ACCENT = [0.82,0.16,0.12];
C_METAL  = [0.74,0.75,0.78];
C_MOTOR  = [0.16,0.16,0.18];
C_BELL   = [0.55,0.12,0.10];
C_PROP   = [0.07,0.07,0.08,0.92];
C_GIMBAL = [0.20,0.20,0.22];
C_LIDAR  = [0.85,0.85,0.88];
C_THERMAL= [0.05,0.05,0.06];
C_BATT   = [0.12,0.13,0.16];
C_GOLD   = [0.80,0.62,0.20];

module rbox(sz,r=2,fn=sphere_fn){
    hull() for(x=[-1,1],y=[-1,1],z=[-1,1])
        translate([x*(sz[0]/2-r),y*(sz[1]/2-r),z*(sz[2]/2-r)]) sphere(r=r,$fn=fn);
}
module ring_holes(pcd,n,d,h=20){ for(i=[0:n-1]) rotate([0,0,i*360/n]) translate([pcd/2,0,0]) cylinder(d=d,h=h,center=true,$fn=20);}
module screw(d=3,L=6){ color(C_METAL){ cylinder(d=d*1.8,h=d*0.7,$fn=6); translate([0,0,-L]) cylinder(d=d,h=L,$fn=18);} }

// ============ 센터 플레이트(상/하 공용) ============
module center_plate(top=false){
    color(C_PLATE) difference(){
        union(){
            cylinder(d=plate_d,h=plate_t,center=true,$fn=120);
            // 암 진입부 보강 러그 6개
            for(i=[0:arm_n-1]) rotate([0,0,i*360/arm_n])
                translate([plate_d/2-10,0,0]) rbox([34,tube_d+12,plate_t+5],3);
        }
        // 경량화 트러스 슬롯
        for(i=[0:5]) rotate([0,0,i*60+30])
            translate([plate_d/4+6,0,0]) hull(){
                cylinder(d=15,h=plate_t+4,center=true,$fn=24);
                translate([26,0,0]) cylinder(d=15,h=plate_t+4,center=true,$fn=24);
            }
        cylinder(d=26,h=plate_t+4,center=true,$fn=48);          // 중앙 케이블 통로
        ring_holes(60,4,3.4,plate_t+6);                          // 스택 스탠드오프
        for(i=[0:arm_n-1]) rotate([0,0,i*360/arm_n])             // 암 클램프 볼트
            translate([plate_d/2-10,0,0]) for(y=[-9,9]) translate([0,y,0]) cylinder(d=3.4,h=plate_t+6,center=true,$fn=18);
    }
    if(top){
        color(C_ACCENT) translate([0,0,plate_t/2+0.4]) cylinder(d=40,h=0.8,$fn=48);
        color(C_ACCENT) translate([0,0,plate_t/2+0.4]) linear_extrude(1.0) rotate([0,0,0]) text("FRIZE",size=10,halign="center",valign="center",font="Arial:style=Bold");
    }
}

// ============ 암(카본 튜브 + 클램프 + 모터마운트 + 모터 + 프롭) ============
module arm_clamp(){
    color(C_PLATE) difference(){
        union(){ rbox([40,tube_d+12,tube_d+8],3); rotate([0,90,0]) cylinder(d=tube_d+8,h=46,center=true,$fn=40);}
        rotate([0,90,0]) cylinder(d=tube_d,h=60,center=true,$fn=40);
        translate([0,0,tube_d/2+4]) cube([60,tube_d+14,8],center=true);   // 분할
        for(x=[-14,14]) translate([x,0,0]) cylinder(d=3.4,h=40,center=true,$fn=18);
    }
}
module motor(){
    color(C_MOTOR) cylinder(d=motor_d,h=motor_h,$fn=48);
    color(C_BELL)  translate([0,0,motor_h]) cylinder(d=motor_d-3,h=5,$fn=48);
    color(C_METAL) translate([0,0,motor_h+5]) cylinder(d=6,h=10,$fn=24);   // 샤프트
    color(C_BELL)  for(i=[0:11]) rotate([0,0,i*30]) translate([motor_d/2-3,0,motor_h*0.5]) cube([2,1.5,motor_h*0.7],center=true); // 방열창
}
module prop(){
    if(SHOW_PROPS) color(C_PROP) for(b=[0,180]) rotate([0,0,b])
        hull(){ cylinder(d=10,h=2,$fn=20); translate([prop_d/2-12,0,0]) scale([1,0.16,1]) cylinder(d=24,h=2,center=true,$fn=24);}
}
module motor_mount(){
    color(C_PLATE) difference(){ cylinder(d=motor_d+10,h=8,$fn=48); translate([0,0,3]) cylinder(d=motor_d+2,h=8,$fn=48);
        ring_holes(19,4,3.2,20); cylinder(d=10,h=20,center=true,$fn=24);}
}
module arm(){
    color(C_CARBON) rotate([0,90,0]) translate([0,0,arm_reach/2]) cylinder(d=tube_d,h=arm_reach,center=true,$fn=40);
    arm_clamp();
    translate([arm_reach,0,0]){ motor_mount(); translate([0,0,8]) motor(); translate([0,0,8+motor_h+10]) prop(); }
    // ESC 시그널 라인(시각)
    color(C_ACCENT) rotate([0,90,0]) translate([0,tube_d/2+1,arm_reach/2]) cube([1.5,1.5,arm_reach],center=true);
}

// ============ 랜딩기어 ============
module landing_leg(){
    color(C_CARBON){
        cylinder(d=18,h=12,$fn=30);
        translate([0,0,-2]) rotate([12,0,0]) translate([0,0,-leg_len/2]) cylinder(d=14,h=leg_len,center=true,$fn=30);
        rotate([12,0,0]) translate([0,0,-leg_len+4]) rotate([90,0,0]) cylinder(d=12,h=70,center=true,$fn=24); // 발(스키드)
    }
}

// ============ 페이로드: 3축 짐벌 + 열화상 + RGB + LiDAR + 가스포드 ============
module gimbal(){
    color(C_GIMBAL){
        rbox([70,16,40],3);                                  // 요 모터부
        for(s=[-1,1]) translate([s*40,0,-14]) rotate([0,90,0]) cylinder(d=22,h=8,$fn=36); // 롤 암
        translate([0,0,-30]) rbox([54,46,30],4);             // 카메라 헤드
    }
    // 열화상(검정) + RGB(파랑) 렌즈
    translate([0,-24,-30]) rotate([-90,0,0]){
        color(C_THERMAL) cylinder(d=22,h=8,$fn=40);
        translate([0,0,8]) color([0.1,0.1,0.12,0.9]) cylinder(d=15,h=1.5,$fn=40);
    }
    translate([18,-24,-36]) rotate([-90,0,0]){
        color([0.05,0.05,0.06]) cylinder(d=16,h=7,$fn=36);
        translate([0,0,7]) color([0.2,0.3,0.55,0.7]) cylinder(d=11,h=1.5,$fn=36);
    }
}
module lidar(){    // Livox Mid-360 느낌
    color(C_LIDAR) cylinder(d=70,h=40,$fn=64);
    color([0.15,0.15,0.18]) translate([0,0,8]) cylinder(d=72,h=20,$fn=64);
    color([0.3,0.4,0.6,0.6]) translate([0,0,8]) cylinder(d=74,h=20,$fn=64);  // 스캔 윈도우 밴드
    color(C_ACCENT) translate([0,0,40]) cylinder(d=20,h=2,$fn=40);
}
module gas_pod(){
    color([0.18,0.19,0.21]) rbox([46,28,22],3);
    color(C_GOLD) for(x=[-12,0,12]) translate([x,-14,0]) rotate([90,0,0]) cylinder(d=8,h=4,$fn=24); // 센서 흡입구
    color(C_ACCENT) translate([0,0,11.5]) rbox([30,3,1],0.5,12);
}
module payload_frame(){
    color(C_PLATE) difference(){ rbox([130,96,18],4); translate([0,0,4]) rbox([122,88,18],3);
        ring_holes(70,4,4.2,40);}
}

// ============ 메인 배터리(상부) ============
module battery(){
    color(C_BATT) rbox([150,55,46],4);
    color(C_ACCENT) translate([0,0,23.5]) rbox([120,16,2],1,12);
    color(C_GOLD) translate([75,0,-10]) rotate([0,90,0]) cylinder(d=9,h=12,$fn=24); // XT90
    color([0.9,0.9,0.9]) translate([0,-20,23.6]) linear_extrude(0.6) text("6S 16000mAh",size=6,halign="center",font="Arial:style=Bold");
}

// ============ 비행 컨트롤러 + 컴퓨트 스택(상판 위) ============
module avionics(){
    color([0.05,0.2,0.07]) translate([0,0,0]) rbox([46,46,12],2);  // Pixhawk
    color(C_ACCENT) translate([0,23,0]) rotate([90,0,0]) cylinder(d=3,h=2,$fn=12); // 기수 화살표 마커
    color([0.1,0.1,0.12]) translate([0,-30,8]) rbox([70,40,16],3); // Jetson Orin
    color(C_METAL) translate([0,-30,17]) for(i=[0:7]) translate([-30+i*9,0,0]) rbox([2.4,34,5],1,10); // 히트싱크
    color(C_LIDAR) translate([34,20,18]) cylinder(d=8,h=24,$fn=24);  // GNSS 안테나 마스트
    color(C_ACCENT) translate([34,20,42]) sphere(d=16,$fn=30);       // GNSS 돔
}

// ============ 조립 ============
module scout(){
    // 하판/상판
    translate([0,0,0])             center_plate(false);
    translate([0,0,stack_h])       center_plate(true);
    // 스탠드오프
    color(C_METAL) for(i=[0:3]) rotate([0,0,i*90+45]) translate([30,0,stack_h/2]) cylinder(d=6,h=stack_h,center=true,$fn=20);
    // 6 암
    for(i=[0:arm_n-1]) rotate([0,0,i*360/arm_n]) translate([plate_d/2-10,0,0]) arm();
    // 랜딩기어 4
    for(i=[0:3]) rotate([0,0,i*90+45]) translate([plate_d/2-34,0,-4]) landing_leg();
    // 상부 배터리 + 항전
    translate([0,0,stack_h+plate_t/2+24]) battery();
    translate([0,0,stack_h+plate_t/2+58]) avionics();
    // 하부 페이로드
    translate([0,0,-26]) payload_frame();
    translate([0,10,-44])  gimbal();
    translate([0,-50,-40]) lidar();
    translate([0,52,-40])  gas_pod();
}

if      (PART_MODE=="ALL")         scout();
else if (PART_MODE=="PLATE")       center_plate(true);
else if (PART_MODE=="ARM_CLAMP")   arm_clamp();
else if (PART_MODE=="MOTOR_MOUNT") motor_mount();
else if (PART_MODE=="PAYLOAD")     payload_frame();
else if (PART_MODE=="LEG")         landing_leg();
