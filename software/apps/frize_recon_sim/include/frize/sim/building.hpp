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
    // ── 영상(Exeter FD 내부수색) 역설계: 단층 복도+병실형 institutional 건물 ──
    //  서쪽 현관 → 중앙 동서 복도 → 양옆 4+4 병실(침대/캐비닛/창) → 동쪽 밝은 출구.
    Vec3f fire{3.6f,2.2f,0.8f};       // 주 화점(남서 주방, 가스레인지 옆)
    Vec3f fire2{16.0f,9.2f,0.8f};     // 2차 화점(북측 병실)
    Vec3f victim{22.8f,2.2f,0.0f};    // 요구조자1(남동 끝방 바닥)
    Vec3f victim2{2.6f,9.8f,0.0f};    // 요구조자2(북서 병실 바닥)
    float W=26.0f, D=12.0f, H=3.2f;   // 단층, 천장 3.0m
    std::vector<float> floor_z{0.0f};

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
        float st=0.20f, zt=3.0f;
        // 바닥/천장
        boxes_.push_back({{0,0,-st},{W,D,0},"slab",NaN});
        boxes_.push_back({{0,0,H-st},{W,D,H},"roof",NaN});

        float cy0=5.0f, cy1=7.0f;     // 중앙 동서 복도(y 5.0~7.0, 폭 2m)
        // 외벽: 서쪽 현관 갭 + 동쪽 출구 갭(밝은 끝), 남/북 외벽
        wall_door(false, 0,D, 0.0f, 0,zt, 5.2f, 1.6f);   // 서벽 현관(복도로)
        wall_door(false, 0,D, W,    0,zt, 5.2f, 1.6f);   // 동벽 출구(밝은 끝)
        // 남/북 외벽(병실 창 갭 몇 개)
        wall(0,0,3.0f,0,0,zt); wall(4.2f,0,9.5f,0,0,zt); wall(10.7f,0,16.0f,0,0,zt); wall(17.2f,0,W,0,0,zt);
        wall(0,D,6.0f,D,0,zt); wall(7.2f,D,13.0f,D,0,zt); wall(14.2f,D,W,D,0,zt);
        // 복도 양벽(병실 출입구 갭 4개씩)
        const float doors[4]={3.25f,9.75f,16.0f,22.5f};
        auto corridor=[&](float yy){ float x=0; for(int i=0;i<4;++i){ float dl=doors[i]-0.55f;
            if(dl>x) wall(x,yy,dl,yy,0,zt); x=doors[i]+0.55f; } if(x<W) wall(x,yy,W,yy,0,zt); };
        corridor(cy0); corridor(cy1);
        // 병실 칸막이(남측 4실 / 북측 4실) ― x=6.5,13,19.5
        for (float xx : {6.5f,13.0f,19.5f}){ wall(xx,0,xx,cy0,0,zt); wall(xx,cy1,xx,D,0,zt); }

        // ── 가구(병실마다 침대/캐비닛) ──
        struct F{float x0,y0,z0,x1,y1,z1;};
        F furn[]={
            // 남측 병실 침대들
            {0.6f,0.5f,0, 2.4f,3.2f,0.6f}, {7.0f,0.5f,0, 8.8f,3.2f,0.6f},
            {13.5f,0.5f,0,15.3f,3.2f,0.6f},{20.0f,0.5f,0,21.8f,3.2f,0.6f},
            // 북측 병실 침대/캐비닛
            {0.6f,9.0f,0, 2.4f,11.6f,0.6f},{7.0f,9.0f,0, 8.8f,11.6f,0.6f},
            {17.0f,9.0f,0,18.8f,11.6f,0.6f},
            // 캐비닛/사물함
            {5.4f,0.4f,0, 6.2f,1.6f,1.7f}, {18.6f,9.0f,0,19.3f,10.6f,1.7f},
            {12.0f,9.2f,0,12.8f,10.8f,1.5f},
        };
        for (auto&f:furn) boxes_.push_back({{f.x0,f.y0,f.z0},{f.x1,f.y1,f.z1},"furniture",NaN});

        // 주방(남서 끝방): 가스레인지(발화원) + 후드
        boxes_.push_back({{4.6f,0.5f,0.0f},{6.2f,1.6f,0.9f},"gas_range",115.0f});
        boxes_.push_back({{4.6f,0.5f,1.9f},{6.2f,1.6f,2.4f},"range_hood",NaN});
        // 화점 2 + 요구조자 2 (영상 위치 반영: 남서 주방화재, 북측 병실, 남동/북서 바닥)
        boxes_.push_back({{2.8f,1.4f,0},{4.6f,3.4f,1.7f},"fire",640.0f});           // 남서 주방 화점
        boxes_.push_back({{15.0f,8.4f,0},{16.8f,10.0f,1.5f},"fire",520.0f});        // 북측 병실 화점
        boxes_.push_back({{22.2f,1.6f,0},{23.4f,2.8f,0.7f},"person",36.5f});        // 요구조자1(남동 끝방)
        boxes_.push_back({{2.0f,9.2f,0},{3.2f,10.4f,0.7f},"person",35.0f});         // 요구조자2(북서 병실)
    }
};

}  // namespace frize::sim
