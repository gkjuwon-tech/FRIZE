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
    Vec3f fire{3.0f,2.0f,0.9f};
    Vec3f victim{13.5f,9.5f,3.4f};
    float W=16.0f, D=12.0f, H=6.4f;
    std::array<float,2> floor_z{0.0f, 3.2f};

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

    void build(){
        const float NaN=std::numeric_limits<float>::quiet_NaN();
        float st=0.22f;
        for (float fz:floor_z) boxes_.push_back({{0,0,fz-st},{W,D,fz},"slab",NaN});
        boxes_.push_back({{0,0,H-st},{W,D,H},"roof",NaN});
        float door=1.1f;
        for (float fz:floor_z){
            float zt=fz+2.8f;
            wall(0,0,5.5f,0,fz,zt);              // 남벽(현관문 좌)
            wall(5.5f+door,0,W,0,fz,zt);         // 남벽(현관문 우)
            wall(0,D,W,D,fz,zt);                 // 북벽
            wall(0,0,0,D,fz,zt);                 // 서벽
            wall(W,0,W,D,fz,zt);                 // 동벽
            wall(6.4f,0,6.4f,4.0f,fz,zt);        // A실 칸막이(문틈)
            wall(6.4f,5.1f,6.4f,D,fz,zt);
            wall(6.4f,7.2f,11.0f,7.2f,fz,zt);    // 후방 복도
            wall(12.1f,7.2f,W,7.2f,fz,zt);
            wall(10.5f,0,10.5f,4.0f,fz,zt);      // C실 칸막이
        }
        boxes_.push_back({{1.0f,8.5f,0},{3.4f,10.8f,0.9f},"furniture",NaN});  // 책상
        boxes_.push_back({{8.0f,1.0f,0},{9.6f,2.6f,1.4f},"furniture",NaN});   // 로커
        boxes_.push_back({{12.5f,9.0f,0},{14.6f,11.0f,1.1f},"furniture",NaN});// 캐비닛
        boxes_.push_back({{2.0f,4.4f,3.2f},{4.0f,6.0f,4.0f},"furniture",NaN});// 2층 박스
        boxes_.push_back({{2.4f,1.4f,0},{3.6f,2.6f,1.6f},"fire",620.0f});     // 화점
        boxes_.push_back({{13.1f,9.1f,3.2f},{13.9f,9.9f,3.9f},"person",36.5f});// 요구조자
    }
};

}  // namespace frize::sim
