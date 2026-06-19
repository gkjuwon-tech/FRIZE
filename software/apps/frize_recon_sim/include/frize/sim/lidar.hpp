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
    std::mt19937 rng;

    Lidar(float az_fov_, int az_n_, float el_lo_, float el_hi_, int el_n_,
          float far_, float sigma, float drop, unsigned seed)
        : az_fov(az_fov_), el_lo(el_lo_), el_hi(el_hi_), far(far_),
          range_sigma(sigma), dropout(drop), az_n(az_n_), el_n(el_n_), rng(seed) {}

    // 한 자세에서 한 번 스윕. sensor_pos 반환(엔진 카빙 시작점).
    std::vector<ScanPoint> scan(const Building& b, const Vec3f& pos, float yaw, float pitch){
        std::vector<ScanPoint> out;
        Vec3f fwd, col[3]; rot_zy(yaw,pitch,fwd,col);
        std::uniform_real_distribution<float> uni(0.f,1.f);
        std::normal_distribution<float> noise(0.f, range_sigma);
        bool full = az_fov >= 6.28f;
        for (int ai=0; ai<az_n; ++ai){
            float a = full ? (-3.14159265f + 6.28318f*ai/az_n)
                           : (-az_fov/2 + az_fov*ai/std::max(1,az_n-1));
            for (int ei=0; ei<el_n; ++ei){
                float e = el_lo + (el_hi-el_lo)*ei/std::max(1,el_n-1);
                float ce=std::cos(e);
                // 로컬 방향(+x 전방, +z 위) → 월드
                Vec3f ld{ ce*std::cos(a), ce*std::sin(a), std::sin(e) };
                Vec3f d{ col[0].x*ld.x+col[1].x*ld.y+col[2].x*ld.z,
                         col[0].y*ld.x+col[1].y*ld.y+col[2].y*ld.z,
                         col[0].z*ld.x+col[1].z*ld.y+col[2].z*ld.z };
                float t; int bi;
                if (!b.raycast(pos, d, far, t, bi)) continue;
                if (uni(rng) < dropout) continue;               // 연기 드롭아웃
                t += noise(rng);
                Vec3f p{ pos.x+d.x*t, pos.y+d.y*t, pos.z+d.z*t };
                float temp = b.box_temp(bi);
                if (std::isnan(temp)){                            // 복사열 블리드
                    float dx=p.x-b.fire.x, dy=p.y-b.fire.y, dz=p.z-b.fire.z;
                    float r=std::sqrt(dx*dx+dy*dy+dz*dz);
                    float warm = 240.0f/(1.0f+std::pow(r,1.7f));
                    if (warm>20.0f) temp=std::min(warm,240.0f);
                }
                out.push_back({p, temp});
            }
        }
        return out;
    }
};

inline Lidar drone_lidar(unsigned seed=1){
    return Lidar(6.2832f, 220, -0.46f, 0.46f, 26, 16.0f, 0.02f, 0.04f, seed); }
inline Lidar visor_depth(unsigned seed=2){
    return Lidar(1.5f, 72, -0.6f, 0.6f, 34, 9.0f, 0.03f, 0.10f, seed); }

}  // namespace frize::sim
