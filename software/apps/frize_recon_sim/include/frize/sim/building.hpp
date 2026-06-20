// frize/sim/building.hpp ― 합성 그라운드트루스 건물 (시뮬 전용)
//
// 재구성이 '찾아내야 할' 미지의 진실. 재구성 엔진은 이 지오메트리를 절대 못 보고
// 오직 LiDAR 히트만 받는다 → 복원 메쉬가 이 건물과 닮으면 트윈이 작동한 것.
// 프레임: site ENU, z-up (frize::Pose frame="site_enu"와 동일).
// 벽/바닥/가구를 축정렬 박스(AABB)로 표현 → 레이-박스 교차가 정확하고 빠르다.
#pragma once
#include <vector>
#include <cmath>
#include <limits>
#include <random>
#include <array>
#include "frize/recon/tsdf.hpp"   // Vec3f

namespace frize::sim {
using frize::recon::Vec3f;

struct Box { Vec3f lo, hi; const char* kind; float temp_c; };

class Building {
public:
    Vec3f fire{4.0f,2.4f,0.9f};      // 주 화점(1층 좌측방)
    Vec3f fire2{15.0f,11.0f,3.4f};   // 2차 화점(2층 우측방)
    Vec3f victim{17.5f,3.0f,0.0f};   // 요구조자 1(1층 우하 코너방)
    Vec3f victim2{3.0f,12.5f,6.4f};  // 요구조자 2(3층 좌상)
    float W=20.0f, D=15.0f, H=9.6f;  // 더 큰 3층 구조
    std::vector<float> floor_z{0.0f, 3.2f, 6.4f};

    Building(){ build(); }
    const std::vector<Box>& boxes() const { return boxes_; }

    // 단일 레이 vs 모든 박스. 가장 가까운 히트의 (t, box) 반환. miss면 t=inf.
    bool raycast(const Vec3f& o, const Vec3f& d, float far, float& t_out, int& box_out) const {
        float best=far; int bi=-1;
        for (int b=0;b<(int)boxes_.size();++b){
            const Box& B=boxes_[b];
            float tmin=0.0f, tmax=far;
            bool ok=true;
            for (int a=0;a<3;++a){
                float oa=(&o.x)[a], da=(&d.x)[a], lo=(&B.lo.x)[a], hi=(&B.hi.x)[a];
                if (std::fabs(da)<1e-9f){ if (oa<lo||oa>hi){ ok=false; break; } continue; }
                float inv=1.0f/da, t1=(lo-oa)*inv, t2=(hi-oa)*inv;
                if (t1>t2){ float tmp=t1; t1=t2; t2=tmp; }
                tmin=std::max(tmin,t1); tmax=std::min(tmax,t2);
                if (tmin>tmax){ ok=false; break; }
            }
            if (ok && tmin<best && tmin>1e-3f){ best=tmin; bi=b; }
        }
        if (bi<0) return false;
        t_out=best; box_out=bi; return true;
    }

    float box_temp(int b) const { return boxes_[b].temp_c; }

    // 점이 건물 내부 빈공간(걸을 수 있는 곳)인지 — 에이전트 충돌 회피용
    bool free_at(const Vec3f& p, float clearance=0.25f) const {
        for (const auto& B:boxes_){
            if (p.x>B.lo.x-clearance && p.x<B.hi.x+clearance &&
                p.y>B.lo.y-clearance && p.y<B.hi.y+clearance &&
                p.z>B.lo.z-clearance && p.z<B.hi.z+clearance) return false;
        }
        return true;
    }

private:
    std::vector<Box> boxes_;

    void wall(float x0,float y0,float x1,float y1,float z0,float z1,float t=0.18f,
              const char* kind="wall", float temp=std::numeric_limits<float>::quiet_NaN()){
        if (std::fabs(x1-x0)>=std::fabs(y1-y0)){
            float yc=(y0+y1)/2;
            boxes_.push_back({{std::min(x0,x1),yc-t/2,z0},{std::max(x0,x1),yc+t/2,z1},kind,temp});
        } else {
            float xc=(x0+x1)/2;
            boxes_.push_back({{xc-t/2,std::min(y0,y1),z0},{xc+t/2,std::max(y0,y1),z1},kind,temp});
        }
    }

    // 문틈 둔 벽(한 줄에 출입구 갭). horiz=true면 x축 따라.
    void wall_door(bool horiz, float a0, float a1, float fixed, float fz, float zt,
                   float door_at, float door_w=1.1f){
        if (horiz){ wall(a0,fixed,door_at,fixed,fz,zt); wall(door_at+door_w,fixed,a1,fixed,fz,zt); }
        else       { wall(fixed,a0,fixed,door_at,fz,zt); wall(fixed,door_at+door_w,fixed,a1,fz,zt); }
    }

    void build(){
        const float NaN=std::numeric_limits<float>::quiet_NaN();
        float st=0.22f;
        for (float fz:floor_z) boxes_.push_back({{0,0,fz-st},{W,D,fz},"slab",NaN});
        boxes_.push_back({{0,0,H-st},{W,D,H},"roof",NaN});

        float cy0=6.6f, cy1=8.4f;       // 중앙 동서 복도(y 6.6~8.4)
        for (size_t fl=0; fl<floor_z.size(); ++fl){
            float fz=floor_z[fl], zt=fz+2.8f;
            // 외벽(남벽에 현관 갭은 1층만)
            if (fl==0) wall_door(true, 0,W, 0, fz,zt, 8.0f, 1.4f); else wall(0,0,W,0,fz,zt);
            wall(0,D,W,D,fz,zt); wall(0,0,0,D,fz,zt); wall(W,0,W,D,fz,zt);
            // 중앙 복도 양벽(여러 출입구 갭)
            wall_door(true, 0,W, cy0, fz,zt, 3.0f); wall_door(true, 3.0f+1.1f,W, cy0, fz,zt, 9.0f);
            wall_door(true, 0,W, cy1, fz,zt, 6.0f); wall_door(true, 6.0f+1.1f,W, cy1, fz,zt, 14.0f);
            // 남측 방 칸막이(3개 방)
            wall_door(false, 0,cy0, 6.5f, fz,zt, 2.0f);
            wall_door(false, 0,cy0, 13.0f, fz,zt, 2.5f);
            // 북측 방 칸막이(3개 방)
            wall_door(false, cy1,D, 5.0f, fz,zt, cy1+1.0f);
            wall_door(false, cy1,D, 11.5f, fz,zt, D-1.5f);
            // 계단실 코어(좌측, 층 관통 ― 벽 3면 + 개구부)
            wall(0.0f,1.5f,2.2f,1.5f,fz,zt); wall(2.2f,1.5f,2.2f,4.0f,fz,zt);
            wall_door(true,0,2.2f,4.0f,fz,zt,0.6f,1.0f);
            // 계단 단(시각적 디테일, 1·2층)
            if (fl<2) for(int s=0;s<8;++s) boxes_.push_back({{0.3f+s*0.22f,2.0f,fz+s*0.34f},{2.0f,3.6f,fz+s*0.34f+0.18f},"stair",NaN});
        }
        // 가구(층별 다수)
        struct F{float x0,y0,z0,x1,y1,z1;};
        F furn[]={
            {1.0f,9.5f,0, 3.6f,11.8f,0.9f},{8.5f,1.0f,0, 10.4f,2.8f,1.5f},{16.5f,9.5f,0, 18.8f,11.4f,1.1f},
            {12.0f,1.2f,0, 13.0f,3.2f,1.9f},{4.5f,10.0f,3.2f, 6.5f,12.0f,1.0f+3.2f},
            {15.0f,2.0f,3.2f, 16.6f,4.0f,3.2f+1.4f},{9.0f,10.5f,6.4f, 11.0f,12.5f,6.4f+0.9f},
            {2.5f,2.0f,6.4f, 4.0f,3.6f,6.4f+1.6f},
        };
        for (auto&f:furn) boxes_.push_back({{f.x0,f.y0,f.z0},{f.x1,f.y1,f.z1},"furniture",NaN});
        // 주방: 가스레인지(발화원). 1층 좌측방 ― 화점 바로 옆(누출/과열 시나리오).
        boxes_.push_back({{2.8f,1.0f,0.0f},{4.4f,1.9f,0.9f},"gas_range",110.0f}); // 레인지 상판 가열
        boxes_.push_back({{2.8f,1.0f,0.9f},{4.4f,1.9f,1.6f},"range_hood",NaN});   // 후드
        // 화점 2개 + 요구조자 2개
        boxes_.push_back({{3.2f,1.6f,0},{4.8f,3.2f,1.7f},"fire",640.0f});        // 1층 좌 화점
        boxes_.push_back({{14.2f,10.2f,3.2f},{15.8f,11.8f,3.2f+1.5f},"fire",520.0f}); // 2층 우 화점
        boxes_.push_back({{17.0f,2.4f,0},{17.9f,3.6f,0.8f},"person",36.5f});     // 요구조자1(1층)
        boxes_.push_back({{2.5f,12.0f,6.4f},{3.4f,12.9f,6.4f+0.8f},"person",35.0f}); // 요구조자2(3층)
    }
};

}  // namespace frize::sim
