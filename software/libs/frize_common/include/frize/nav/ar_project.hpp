// frize/nav/ar_project.hpp ― 경로(월드 경유점) → 고글 카메라 화면 투영
//
// 데모처럼 화살표를 손으로 그리는 게 아니라, 실제 대원 자세(eye+yaw+pitch)와
// 카메라 내부파라미터로 경로점을 핀홀 투영한다. 대원이 움직이면 자세가 바뀌고
// 투영이 자동으로 갱신 → 화살표가 "계속 바뀌며" 바닥을 따라 목표로 안내.
// 고글 SW(frize_visor)가 이 결과로 바닥 셰브론을 그린다. 헤더 온리/테스트 가능.
#pragma once
#include <vector>
#include <cmath>
#include "frize/schemas.hpp"

namespace frize::nav {

using frize::Vec3;

struct ScreenPt { double u, v; double depth; double scale; };  // 이미지 좌표 + 깊이 + 원근스케일

// 카메라: yaw(rad, +x=0, CCW), pitch(rad, 아래가 음), hfov(rad).
// 월드점을 카메라 좌표로: forward/right/up 직교기저.
inline std::vector<ScreenPt>
project_route(const std::vector<Vec3>& route, const Vec3& eye,
              double yaw, double pitch, double hfov, int W, int H,
              double step_m=0.45, double max_ahead_m=12.0)
{
    std::vector<ScreenPt> out;
    if (route.size()<2) return out;
    double cy=std::cos(yaw), sy=std::sin(yaw), cp=std::cos(pitch), sp=std::sin(pitch);
    Vec3 fwd{ cy*cp, sy*cp, sp };          // 시선(약간 아래면 sp<0)
    Vec3 right{ -sy, cy, 0 };
    Vec3 up{ -cy*sp, -sy*sp, cp };         // right × fwd
    double fx = (W*0.5)/std::tan(hfov*0.5);

    auto proj=[&](const Vec3& P, ScreenPt& s)->bool{
        Vec3 rel{P.x-eye.x, P.y-eye.y, P.z-eye.z};
        double dz = rel.x*fwd.x+rel.y*fwd.y+rel.z*fwd.z;     // 깊이(전방)
        if (dz < 0.25) return false;                          // 뒤/너무 가까움
        double dx = rel.x*right.x+rel.y*right.y+rel.z*right.z;
        double dyv= rel.x*up.x+rel.y*up.y+rel.z*up.z;
        s.u = W*0.5 + fx*dx/dz;
        s.v = H*0.5 - fx*dyv/dz;
        s.depth = dz; s.scale = 1.0/dz;
        return true;
    };

    // 경로를 등간격으로 리샘플(가까운 구간부터 max_ahead 까지)
    double acc=0;
    for (size_t i=1;i<route.size() && acc<max_ahead_m; ++i){
        Vec3 a=route[i-1], b=route[i];
        double seg=std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y)+(b.z-a.z)*(b.z-a.z));
        int n=std::max(1,(int)std::ceil(seg/step_m));
        for (int k=0;k<n && acc<max_ahead_m; ++k){
            double t=(double)k/n;
            Vec3 P{a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t};
            ScreenPt s; if (proj(P,s)){
                // 화면 밖 여유 허용(셰브론 일부 걸침 OK)
                if (s.u>-0.25*W && s.u<1.25*W && s.v>-0.1*H && s.v<1.2*H) out.push_back(s);
            }
            acc += seg/n;
        }
    }
    return out;
}

// 다음 회전 방향 힌트(좌/직/우) ― 고글 상단 큰 화살표용. 대원 진행방향 대비 경로.
inline int turn_hint(const std::vector<Vec3>& route, const Vec3& eye, double yaw, double look_m=3.0){
    if (route.size()<2) return 0;
    // 경로 위 look_m 앞 지점의 방위와 현재 yaw 차이
    double acc=0; Vec3 tgt=route.back();
    for (size_t i=1;i<route.size();++i){ double seg=std::sqrt((route[i].x-route[i-1].x)*(route[i].x-route[i-1].x)+(route[i].y-route[i-1].y)*(route[i].y-route[i-1].y));
        if (acc+seg>=look_m){ tgt=route[i]; break; } acc+=seg; }
    double br=std::atan2(tgt.y-eye.y, tgt.x-eye.x);
    double d=br-yaw; while(d>M_PI)d-=2*M_PI; while(d<-M_PI)d+=2*M_PI;
    if (d> 0.35) return -1;   // 좌회전
    if (d<-0.35) return  1;   // 우회전
    return 0;                 // 직진
}

}  // namespace frize::nav
