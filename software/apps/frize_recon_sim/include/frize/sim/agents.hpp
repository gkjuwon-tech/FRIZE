// frize/sim/agents.hpp ― 드론/대원 궤적 생성 (시뮬 전용)
//
// SCOUT-1: 현관으로 진입 → 양 층 각 실을 커버하는 경로(다고도) + 연속 요(yaw) 스핀.
//          핸드오프의 '프런티어 탐사'를 커버리지 웨이포인트로 구체화.
// VISOR-1: 대원이 1층을 도보 진입 → 실 점검. 헬멧이 전방 스캔.
// 둘의 스캔이 융합돼야 천장/바닥/벽이 모두 채워진다(단일 센서로는 음영 발생).
#pragma once
#include <vector>
#include <cmath>
#include "frize/recon/tsdf.hpp"

namespace frize::sim {
using frize::recon::Vec3f;

struct Pose { Vec3f pos; float yaw; float pitch; };

inline std::vector<Vec3f> _interp(const std::vector<Vec3f>& wp, float step){
    std::vector<Vec3f> out;
    for (size_t i=0;i+1<wp.size();++i){
        Vec3f a=wp[i], b=wp[i+1];
        Vec3f d{b.x-a.x,b.y-a.y,b.z-a.z};
        float L=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
        int n=std::max(1,(int)(L/step));
        for (int s=0;s<n;++s){ float t=(float)s/n; out.push_back({a.x+d.x*t,a.y+d.y*t,a.z+d.z*t}); }
    }
    if (!wp.empty()) out.push_back(wp.back());
    return out;
}

// 드론: 현관 진입 후 1층 룸(A/중앙/C/후방) → 계단 상승 → 2층 룸 순회. yaw 연속 회전.
inline std::vector<Pose> drone_path(float step=0.45f){
    std::vector<Vec3f> wp = {
        {6.0f,-2.0f,1.6f},{6.0f,0.6f,1.6f},                   // 현관 진입
        {3.0f,1.8f,1.7f},{2.0f,5.0f,1.7f},{2.0f,9.5f,1.8f},   // 좌측(A/책상실)
        {5.5f,9.5f,2.0f},{5.5f,3.0f,2.0f},                    // 중앙
        {8.5f,2.0f,1.8f},{9.5f,5.0f,1.9f},{8.5f,9.5f,2.0f},   // 우중앙
        {13.5f,9.8f,1.9f},{13.5f,3.0f,1.9f},{13.0f,8.5f,2.2f},// C/우측
        {8.0f,5.5f,2.6f},                                     // 계단 부근 상승
        {8.0f,5.5f,4.8f},{3.0f,5.0f,4.9f},{2.5f,9.0f,4.9f},   // 2층 좌
        {6.0f,9.0f,5.0f},{6.0f,2.0f,5.0f},{9.5f,2.0f,4.9f},   // 2층 중앙
        {13.0f,3.0f,4.9f},{13.5f,9.5f,4.9f},{10.0f,9.0f,5.0f},// 2층 우(요구조자 근처)
    };
    auto pts=_interp(wp,step);
    std::vector<Pose> out; out.reserve(pts.size());
    float yaw=0.f;
    for (auto& p:pts){ yaw+=0.55f; out.push_back({p, yaw, -0.12f}); }
    return out;
}

// 대원: 도보로 1층 진입 → 좌실 → 중앙 → 우실 점검. 진행방향을 바라봄(전방 스캔).
inline std::vector<Pose> firefighter_path(float step=0.40f){
    std::vector<Vec3f> wp = {
        {6.0f,-1.5f,1.5f},{6.0f,1.0f,1.5f},{3.5f,2.5f,1.5f},
        {2.0f,6.0f,1.5f},{2.2f,9.8f,1.5f},{4.5f,9.0f,1.5f},
        {5.8f,5.0f,1.5f},{8.5f,2.5f,1.5f},{9.5f,6.0f,1.5f},
        {12.5f,8.5f,1.5f},{14.0f,10.0f,1.5f},
    };
    auto pts=_interp(wp,step);
    std::vector<Pose> out; out.reserve(pts.size());
    for (size_t i=0;i<pts.size();++i){
        Vec3f a=pts[i], b=pts[std::min(pts.size()-1,i+1)];
        float yaw=std::atan2(b.y-a.y, b.x-a.x);
        out.push_back({a, yaw, -0.05f});
    }
    return out;
}

}  // namespace frize::sim
