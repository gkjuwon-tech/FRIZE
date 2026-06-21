// frize/sim/lidar.hpp ― 합성 깊이/열 센서 (시뮬 전용)
//
// 핸드오프가 명세한 실제 하드웨어를 대역한다:
//   • SCOUT-1 드론  → Livox Mid-360 류 스피닝 LiDAR(광각, 전방위)
//   • VISOR-1 고글   → 헬멧 전방 깊이 + 열화상 카메라(좁은 원뿔 FoV)
// 자세를 주면 월드좌표 히트 + 포인트별 온도를 돌려준다. 거리노이즈 + 랜덤
// 드롭아웃으로 연기 속 실센서처럼 거동 → 융합이 강건해야 함(엔진의 일).
#pragma once
#include <vector>
#include <cmath>
#include <random>
#include "frize/recon/tsdf.hpp"
#include "frize/sim/building.hpp"

namespace frize::sim {
using frize::recon::Vec3f;
using frize::recon::ScanPoint;

inline void rot_zy(float yaw, float pitch, Vec3f& fwd, Vec3f col[3]){
    float cy=std::cos(yaw), sy=std::sin(yaw), cp=std::cos(pitch), sp=std::sin(pitch);
    // R = Rz(yaw) * Ry(pitch); 컬럼 벡터
    col[0]={cy*cp, sy*cp, -sp};
    col[1]={-sy,   cy,    0};
    col[2]={cy*sp, sy*sp, cp};
    fwd=col[0];
}

struct Lidar {
    float az_fov, el_lo, el_hi, far, range_sigma, dropout;
    int az_n, el_n;
    unsigned seed;

    Lidar(float az_fov_, int az_n_, float el_lo_, float el_hi_, int el_n_,
          float far_, float sigma, float drop, unsigned seed_)
        : az_fov(az_fov_), el_lo(el_lo_), el_hi(el_hi_), far(far_),
          range_sigma(sigma), dropout(drop), az_n(az_n_), el_n(el_n_), seed(seed_) {}

    // 한 자세에서 한 번 스윕. OpenMP로 레이를 병렬 캐스팅(결정론적 노이즈).
    //  fire/sim_t를 주면 시간 진행형 화재장으로 온도를 덮어쓴다(번지는 불 시뮬).
    std::vector<ScanPoint> scan(const Building& b, const Vec3f& pos, float yaw, float pitch,
                                const FireSim* fire=nullptr, float sim_t=0.f){
        Vec3f fwd, col[3]; rot_zy(yaw,pitch,fwd,col);
        bool full = az_fov >= 6.28f;
        const int R = az_n*el_n;
        std::vector<ScanPoint> tmp(R); std::vector<uint8_t> ok(R,0);
        uint32_t base = seed*2654435761u
            ^ (uint32_t)(pos.x*131.1f) ^ (uint32_t)(pos.y*977.3f) ^ (uint32_t)(yaw*613.7f);
        #pragma omp parallel for schedule(static)
        for (int k=0;k<R;++k){
            int ai=k/el_n, ei=k%el_n;
            float a = full ? (-3.14159265f + 6.28318f*ai/az_n)
                           : (-az_fov/2 + az_fov*ai/std::max(1,az_n-1));
            float e = el_lo + (el_hi-el_lo)*ei/std::max(1,el_n-1);
            float ce=std::cos(e);
            Vec3f ld{ ce*std::cos(a), ce*std::sin(a), std::sin(e) };
            Vec3f d{ col[0].x*ld.x+col[1].x*ld.y+col[2].x*ld.z,
                     col[0].y*ld.x+col[1].y*ld.y+col[2].y*ld.z,
                     col[0].z*ld.x+col[1].z*ld.y+col[2].z*ld.z };
            float t; int bi;
            if (!b.raycast(pos, d, far, t, bi)) continue;
            uint32_t h=base+ (uint32_t)k*2246822519u;
            float r0=( (h^(h>>15))*0x2C1B3C6Du & 0xFFFFFF)/float(0x1000000);
            if (r0 < dropout) continue;
            float r1=( ((h*0x9E3779B1u)^(h>>13)) & 0xFFFFFF)/float(0x1000000);
            t += (r1*2.f-1.f)*range_sigma*1.7f;           // 결정론적 거리노이즈
            Vec3f p{ pos.x+d.x*t, pos.y+d.y*t, pos.z+d.z*t };
            float temp = b.box_temp(bi);
            if (fire){
                // 시간 진행형 화재장: 전선이 번지며 표면(벽/가구)을 달군다.
                float ft = fire->temp_at(p, sim_t);
                if (!std::isnan(ft)) temp = std::isnan(temp) ? ft : std::max(temp, ft);
            } else if (std::isnan(temp)){
                // (폴백) 정적 방사형 온기 ― fire 미지정 시 기존 거동 유지.
                float dx=p.x-b.fire.x, dy=p.y-b.fire.y, dz=p.z-b.fire.z;
                float r=std::sqrt(dx*dx+dy*dy+dz*dz);
                float warm = 240.0f/(1.0f+std::pow(r,1.7f));
                if (warm>20.0f) temp=std::min(warm,240.0f);
            }
            tmp[k]={p,temp}; ok[k]=1;
        }
        std::vector<ScanPoint> out; out.reserve(R/2);
        for (int k=0;k<R;++k) if(ok[k]) out.push_back(tmp[k]);
        return out;
    }
};

inline Lidar drone_lidar(unsigned seed=1){
    return Lidar(6.2832f, 220, -0.46f, 0.46f, 26, 16.0f, 0.02f, 0.04f, seed); }
inline Lidar visor_depth(unsigned seed=2){
    return Lidar(1.5f, 72, -0.6f, 0.6f, 34, 9.0f, 0.03f, 0.10f, seed); }

}  // namespace frize::sim
