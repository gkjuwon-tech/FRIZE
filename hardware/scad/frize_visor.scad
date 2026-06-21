// =====================================================================
//  FRIZE VISOR-1  ―  Firefighter Smart Goggles  (v2, 좌표 정합)
//  Codename: "Salamander Eye"
//  Units: mm  |  OpenSCAD 2021.01+
//
//  좌표계: X=폭, Y=깊이(+Y=머리쪽/뒤, -Y=전방), Z=위.
//  모든 부품은 본체(shell)에 물리적으로 접촉/오버랩한다(공중부양 없음).
// =====================================================================

PART_MODE = "ALL";
$fn = 40; sfn = 16;

face_w = 160;   // 실드 가로 현(chord)
face_h = 76;    // 실드 높이(Z)
wrap_r = 115;   // 곡률 반경
shell_t= 4.0;
ipd    = 64;

C_SHELL=[0.12,0.13,0.15]; C_LENS=[0.10,0.13,0.16,0.55]; C_BEZEL=[0.04,0.04,0.05];
C_ACCENT=[0.72,0.14,0.11]; C_BATT=[0.14,0.15,0.13]; C_METAL=[0.7,0.71,0.74];
C_GOLD=[0.78,0.6,0.2]; C_RUBBER=[0.06,0.06,0.07]; C_GREEN=[0.2,0.8,0.35];

module rbox(s,r=2,fn=sfn){ hull() for(x=[-1,1],y=[-1,1],z=[-1,1])
    translate([x*(s[0]/2-r),y*(s[1]/2-r),z*(s[2]/2-r)]) sphere(r=r,$fn=fn); }
module vent(n=4,len=16,w=2,p=4){ for(i=[0:n-1]) translate([(i-(n-1)/2)*p,0,0]) rbox([w,len,6],0.8,8); }

// 수직축(Z) 곡면 밴드, 전방=-Y, 중심 앞면이 y=0에 오도록 +rad 평행이동
module band(wchord, hz, th, rad){
    translate([0,rad,0]) intersection(){
        difference(){
            cylinder(h=hz, r=rad, center=true, $fn=180);
            cylinder(h=hz+2, r=rad-th, center=true, $fn=180);
        }
        translate([0,-rad,0]) cube([wchord, 2*rad, hz+4], center=true);
    }
}

// ---- 본체 셸 ----
module shell(){
    color(C_SHELL) difference(){
        union(){
            band(face_w, face_h, shell_t, wrap_r);
            for(z=[face_h/2-4, -face_h/2+4]) translate([0,0,z]) band(face_w+4, 8, shell_t+1.6, wrap_r); // 상/하 림
        }
        // 아이포트 2개(Y축 관통)
        for(s=[-1,1]) translate([s*ipd/2, 6, 0]) rotate([90,0,0]) scale([1,0.8,1]) cylinder(d=46,h=60,center=true,$fn=64);
        // 코 컷아웃(하단 중앙)
        translate([0,8,-face_h/2+2]) rotate([90,0,0]) scale([1.5,1,1]) cylinder(d=26,h=30,center=true,$fn=48);
    }
    // 페이스 폼(피부면, 안쪽 +Y, 본체에 접촉)
    color(C_RUBBER) translate([0,0,0]) band(face_w-10, face_h-8, 5, wrap_r-shell_t+0.1);
}

// ---- 바이저 렌즈(셸 앞면에 프레임으로 부착) ----
module lens(){
    color(C_LENS) band(face_w-14, face_h-14, 1.6, wrap_r+2.0);     // 앞면 2mm 띄운 렌즈
    color(C_BEZEL) for(z=[face_h/2-7,-face_h/2+7]) translate([0,0,z]) band(face_w-10, 4, 4.5, wrap_r+1); // 상/하 프레임이 셸과 렌즈를 연결
}

// ---- 상단 센서 포드(셸 윗면 +Z에 얹혀 오버랩) ----
module sensor_pod(){
    pod=[112,30,22]; pz = face_h/2 + pod[2]/2 - 6;     // 6mm 셸에 박힘
    translate([0,-6,pz]){
        color(C_SHELL) difference(){
            rbox(pod,4);
            for(x=[-34,0,34]) translate([x,-pod[1]/2,0]) rotate([90,0,0]) cylinder(d=(x==0?16:19),h=14,center=true,$fn=40);
            translate([44,0,pod[2]/2-1]) vent(4,14,2,4);
        }
        // 렌즈 베젤
        for(x=[-34,0,34]) translate([x,-pod[1]/2+1,0]) rotate([-90,0,0]){
            color(C_BEZEL) cylinder(d1=(x==0?20:23),d2=(x==0?16:19),h=5,$fn=40);
            translate([0,0,5]) color([0.08,0.09,0.12,0.9]) cylinder(d=(x==0?13:15),h=1,$fn=40);
        }
        color(C_GREEN) translate([-46,-pod[1]/2+1,0]) rotate([-90,0,0]) cylinder(d=3,h=3,$fn=16);
        color(C_ACCENT) translate([0,0,pod[2]/2-0.4]) rbox([pod[0]-30,3,1],0.5,10);
    }
    // 센서포드 ↔ 셸 연결 리브(확실히 붙임)
    color(C_SHELL) translate([0,-2,face_h/2-3]) rbox([face_w-30,18,10],3);
}

// ---- AR 디스플레이 포드 x2 (셸 뒤 +Y, 눈 위치) ----
module display_pod(){ color(C_SHELL) difference(){ rbox([34,26,24],3); translate([0,-6,0]) rbox([28,20,20],2);}
    color([0.5,0.7,0.9,0.5]) translate([0,-13,0]) rotate([90,0,0]) cylinder(d=22,h=1,$fn=40); }

// ---- 측면 배터리 포드 x2 (셸 측면에 접촉) ----
module battery_pod(){
    body=[42,54,30];
    color(C_BATT) difference(){ rbox(body,3); translate([0,6,0]) rbox([body[0]-2*shell_t,body[1],body[2]-2*shell_t],2);
        translate([0,body[1]/2-3,0]) rbox([14,6,body[2]+2],2,10); }
    color(C_BATT) for(i=[0:4]) translate([0,-12+i*9,body[2]/2-0.4]) rbox([body[0]-6,2.2,3],1,8);
    color(C_ACCENT) translate([0,body[1]/2-2,0]) rbox([12,4,body[2]-8],1.5,10);
    color(C_GOLD) for(x=[-7,0,7]) translate([x,-body[1]/2+1,-6]) cylinder(d=2,h=3,$fn=14);
    color(C_GREEN) for(i=[0:3]) translate([-12+i*4,body[1]/2-2,body[2]/2-3]) cylinder(d=1.6,h=2,$fn=10);
}

// ---- 후방 컴퓨트 포드(셸 뒤쪽 상단에 접촉) ----
module compute_pod(){ color(C_SHELL) difference(){ rbox([88,58,20],4); translate([0,0,3]) rbox([80,50,18],3);
        translate([0,0,9]) vent(7,46,3,7); }
    color(C_METAL) for(i=[0:8]) translate([-36+i*9,0,11]) rbox([2.2,50,5],1,8);
    color(C_ACCENT) translate([0,-28,0]) rbox([36,3,14],1,10); }

// ---- 헬멧 마운트 레일(센서포드 위에 라이저로 연결) ----
module helmet_rail(){
    rz = face_h/2 + 22;
    // 라이저(센서포드 상단 → 레일): 두 기둥으로 물리 연결
    color(C_SHELL) for(x=[-60,60]) translate([x,2,face_h/2+8]) rbox([14,16,24],3);
    color(C_SHELL) translate([0,2,rz]) difference(){
        rbox([150,26,12],3);
        translate([0,0,5]) rotate([90,0,0]) hull(){ translate([-9,0,0]) cylinder(d=6,h=30,center=true,$fn=6);
                                                     translate([9,0,0]) cylinder(d=6,h=30,center=true,$fn=6); }
    }
    color(C_ACCENT) translate([0,2,rz+6.4]) rbox([40,10,1],0.5,10);
}

// ---- 측면 스트랩 마운트 ----
module strap_mount(){ color(C_SHELL) difference(){ rbox([10,24,16],2); rotate([0,90,0]) cylinder(d=8,h=14,center=true,$fn=20);} }

// ---- 측위 포드(NAV): UWB(DWM3000) 정밀 실내측위 + GPS(u-blox) 옥외측위 ----
//   헬멧 레일 위에 라이저로 물리 접촉. 상단에 GPS 패치 안테나, 전방에 UWB 모노폴.
//   지휘 트윈에서 대원의 위치(실내 UWB, 옥외 GPS 융합)를 실시간 표시한다.
module nav_pod(){
    rz = face_h/2 + 22;            // 헬멧 레일 높이와 정합
    // 엔클로저(모듈 PCB: UWB + GPS 수신부)
    color(C_SHELL) translate([0,14,rz+9]) difference(){
        rbox([48,30,15],3); translate([0,0,3]) rbox([42,24,13],2);
        translate([0,0,8]) vent(5,22,2.2,6);
    }
    // GPS 패치 안테나(상단 금속 패치) ― 하늘 보이게 위로
    color(C_METAL) translate([0,14,rz+17]) rbox([24,24,2],1,12);
    color(C_GOLD)  translate([0,14,rz+18.2]) cylinder(d=2,h=2,$fn=12);
    // UWB 모노폴 안테나(전방 돌출) + 골드 팁
    color(C_ACCENT) translate([18,4,rz+10]) cylinder(d=4,h=12,$fn=18);
    color(C_GOLD)   translate([18,4,rz+22]) sphere(2.4,$fn=16);
    // 상태 LED(측위 fix 표시)
    color(C_GREEN) for(i=[0:1]) translate([-8+i*6,28,rz+9]) cylinder(d=1.8,h=2,$fn=12);
    // 라이저: 레일에 물리 접촉(공중부양 방지)
    color(C_SHELL) translate([0,14,rz+2]) rbox([16,16,10],2);
}

module assembly(){
    shell(); lens(); sensor_pod(); helmet_rail(); nav_pod();
    for(s=[-1,1]) translate([s*ipd/2, 20, 0]) display_pod();
    for(s=[-1,1]) translate([s*(face_w/2 - 4), 26, 2]) rotate([0,0,s*10]) battery_pod();
    translate([0, 48, 12]) compute_pod();
    for(s=[-1,1]) translate([s*(face_w/2 + 2), 18, -8]) strap_mount();
}

if (PART_MODE=="ALL") assembly();
else if (PART_MODE=="SHELL") shell();
else if (PART_MODE=="SENSOR_POD") sensor_pod();
else if (PART_MODE=="NAV_POD") nav_pod();
else if (PART_MODE=="BATTERY_POD") battery_pod();
