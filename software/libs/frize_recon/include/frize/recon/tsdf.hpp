// frize/recon/tsdf.hpp ― 프로덕션 TSDF 융합 볼륨 (의존성 0, 순수 C++17)
//
// KinectFusion 계열의 절단 부호거리장(Truncated Signed Distance Field).
// 드론 LiDAR + 고글 깊이 스캔(월드 좌표 포인트)을 투영형으로 누적한다.
//   • 표면 근방(±trunc): 부호거리 가중평균 → 마칭큐브가 매끈한 메쉬를 뽑음
//   • 센서~표면 사이: 자유공간으로 카빙(약한 가중치) → 미탐사/프런티어 구분
//   • 열 채널: 표면 복셀에 온도 지수이동평균(열화상 융합)
//   • weight==0 셀은 '미탐사' → 마칭큐브가 건너뛰어 구멍(=프런티어)으로 남김
//
// 큐브 복셀(마인크래프트)이 아니라 '진짜 표면'을 재구성한다. 이게 트윈 퀄의 핵심.
#pragma once
#include <vector>
#include <cmath>
#include <cstdint>
#include <limits>
#include <array>
#include <algorithm>

namespace frize::recon {

struct Vec3f { float x{0}, y{0}, z{0}; };
inline Vec3f operator-(Vec3f a, Vec3f b){ return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3f operator+(Vec3f a, Vec3f b){ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3f operator*(Vec3f a, float s){ return {a.x*s,a.y*s,a.z*s}; }
inline float dot(Vec3f a, Vec3f b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
inline float norm(Vec3f a){ return std::sqrt(dot(a,a)); }

// 하나의 스캔 포인트: 월드 좌표 + (선택) 온도. 센서 위치는 스캔 단위로 공유.
struct ScanPoint { Vec3f p; float temp_c{std::numeric_limits<float>::quiet_NaN()}; };

struct TsdfCell {
    float sdf{1.0f};      // 절단 부호거리(+1=빈공간 truncated, 0=표면)
    float w{0.0f};        // 누적 가중치(0=미탐사)
    float temp{std::numeric_limits<float>::quiet_NaN()};
    float temp_w{0.0f};
};

class TsdfVolume {
public:
    // origin = 볼륨 최소 코너(월드 m), dims = 복셀 개수(x,y,z), voxel = 한 변(m)
    TsdfVolume(Vec3f origin, std::array<int,3> dims, float voxel)
        : origin_(origin), nx_(dims[0]), ny_(dims[1]), nz_(dims[2]), vs_(voxel),
          trunc_(voxel * 4.0f), cells_((size_t)nx_*ny_*nz_) {}

    int nx() const { return nx_; } int ny() const { return ny_; } int nz() const { return nz_; }
    float voxel() const { return vs_; } float trunc() const { return trunc_; }
    Vec3f origin() const { return origin_; }
    const TsdfCell& at(int i,int j,int k) const { return cells_[idx(i,j,k)]; }
    bool inb(int i,int j,int k) const { return i>=0&&j>=0&&k>=0&&i<nx_&&j<ny_&&k<nz_; }
    Vec3f world(int i,int j,int k) const {
        return { origin_.x+(i+0.5f)*vs_, origin_.y+(j+0.5f)*vs_, origin_.z+(k+0.5f)*vs_ }; }

    // 한 번의 LiDAR 스윕을 누적. sensor = 센서 위치(월드).
    void integrate_scan(const Vec3f& sensor, const std::vector<ScanPoint>& pts) {
        for (const auto& sp : pts) integrate_ray(sensor, sp.p, sp.temp_c);
    }

    // 센서→히트 레이를 따라 TSDF/열 갱신(투영형 splat).
    void integrate_ray(const Vec3f& s, const Vec3f& hit, float temp) {
        Vec3f ray = hit - s; float d = norm(ray);
        if (d < 1e-4f) return;
        Vec3f u = ray * (1.0f/d);
        // 표면 앞쪽 자유공간 카빙: [r0, d-trunc] 약한 가중치로 sdf=+1 방향.
        // 카빙은 셀당 1샘플(step=vs_)이면 충분 — 성능을 위해 거칠게.
        float carve_step = vs_;
        float carve_end = d - trunc_;
        for (float r = std::max(carve_step, d - max_carve_); r < carve_end; r += carve_step) {
            Vec3f p = s + u * r;
            update(p, +1.0f, 0.25f, std::numeric_limits<float>::quiet_NaN(), 0.f);
        }
        // 표면 밴드 [d-trunc, d+trunc]: 진짜 부호거리, 강한 가중치 + 열.
        // 밴드는 촘촘히(step=vs/2) → 매끈한 등치면.
        float step = vs_ * 0.5f;
        for (float r = d - trunc_; r <= d + trunc_; r += step) {
            Vec3f p = s + u * r;
            float sdf = (d - r) / trunc_;            // [-1,+1] 정규화
            sdf = std::max(-1.0f, std::min(1.0f, sdf));
            float wi = 1.0f - std::min(1.0f, std::fabs(sdf)) * 0.5f;  // 표면 근처 가중↑
            bool surf = std::fabs(r - d) < vs_;       // 표면 바로 그 셀에만 열 융합
            update(p, sdf, wi, surf ? temp : std::numeric_limits<float>::quiet_NaN(),
                   surf ? 1.0f : 0.f);
        }
    }

    // 삼중선형 보간 온도(메쉬 정점 채색용). 없으면 NaN.
    float temp_trilerp(Vec3f w) const {
        float fx=(w.x-origin_.x)/vs_-0.5f, fy=(w.y-origin_.y)/vs_-0.5f, fz=(w.z-origin_.z)/vs_-0.5f;
        int i=(int)std::floor(fx), j=(int)std::floor(fy), k=(int)std::floor(fz);
        float tx=fx-i, ty=fy-j, tz=fz-k, acc=0, wsum=0;
        for (int dz=0;dz<2;++dz)for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
            if(!inb(i+dx,j+dy,k+dz)) continue;
            const TsdfCell& c=at(i+dx,j+dy,k+dz);
            if(c.temp_w<=0||std::isnan(c.temp)) continue;
            float wgt=(dx?tx:1-tx)*(dy?ty:1-ty)*(dz?tz:1-tz);
            acc+=c.temp*wgt; wsum+=wgt;
        }
        return wsum>1e-6f ? acc/wsum : std::numeric_limits<float>::quiet_NaN();
    }

    // 탐사 진행률: 관측된(가중치>0) 셀 비율.
    double explored_fraction() const {
        size_t obs=0; for (const auto& c: cells_) if (c.w>0) ++obs;
        return (double)obs / cells_.size();
    }
    size_t observed_cells() const {
        size_t o=0; for(const auto&c:cells_) if(c.w>0) ++o; return o; }

private:
    size_t idx(int i,int j,int k) const { return ((size_t)k*ny_+j)*nx_+i; }

    void update(Vec3f p, float sdf, float wi, float temp, float temp_wi) {
        int i=(int)std::floor((p.x-origin_.x)/vs_);
        int j=(int)std::floor((p.y-origin_.y)/vs_);
        int k=(int)std::floor((p.z-origin_.z)/vs_);
        if (!inb(i,j,k)) return;
        TsdfCell& c = cells_[idx(i,j,k)];
        float nw = c.w + wi;
        c.sdf = (c.sdf*c.w + sdf*wi) / nw;
        c.w = std::min(nw, W_MAX);
        if (temp_wi>0 && !std::isnan(temp)) {
            if (c.temp_w<=0 || std::isnan(c.temp)) c.temp = temp;
            else c.temp = (c.temp*c.temp_w + temp*temp_wi)/(c.temp_w+temp_wi);
            c.temp_w = std::min(c.temp_w + temp_wi, W_MAX);
        }
    }

    Vec3f origin_; int nx_, ny_, nz_; float vs_, trunc_;
    static constexpr float W_MAX = 64.0f;
    float max_carve_ = 14.0f;          // 자유공간 카빙 최대 거리(m)
    std::vector<TsdfCell> cells_;
};

}  // namespace frize::recon
