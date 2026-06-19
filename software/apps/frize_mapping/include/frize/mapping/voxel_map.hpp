// frize/mapping/voxel_map.hpp ― 점유격자 + 열화상 융합 복셀 맵
//
// 희소 해시 복셀맵. 로그-오즈 점유 갱신(센서모델), 열은 지수이동평균.
// LiDAR 히트로 점유↑, 센서→히트 레이로 자유공간↓(카빙). 연기 속에서도
// 누적되며 현장 3D를 재구성한다. MapPatch(희소)로 콕핏에 증분 송출.
#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include "frize/schemas.hpp"
#include "frize/protocol.hpp"

namespace frize::mapping {

struct Cell {
    float log_odds{0.f};     // 점유 로그-오즈
    float temp_c{NAN};       // 융합 온도(없으면 NaN)
};

class VoxelMap {
public:
    explicit VoxelMap(double voxel_size_m = 0.25) : vs_(voxel_size_m) {}

    int64_t key(int i, int j, int k) const {
        // 21비트씩 패킹(±100만 복셀 범위)
        auto enc = [](int v){ return (int64_t)(v + (1<<20)) & 0x1FFFFF; };
        return (enc(i) << 42) | (enc(j) << 21) | enc(k);
    }
    void idx(double x, double y, double z, int& i, int& j, int& k) const {
        i = (int)std::floor(x / vs_); j = (int)std::floor(y / vs_); k = (int)std::floor(z / vs_);
    }
    static int di(int64_t k){ return (int)((k >> 42) & 0x1FFFFF) - (1<<20); }
    static int dj(int64_t k){ return (int)((k >> 21) & 0x1FFFFF) - (1<<20); }
    static int dk(int64_t k){ return (int)( k        & 0x1FFFFF) - (1<<20); }

    void integrate_hit(const frize::Vec3& p, std::optional<float> temp = std::nullopt) {
        int i,j,k; idx(p.x,p.y,p.z,i,j,k);
        Cell& c = cells_[key(i,j,k)];
        c.log_odds = std::min(c.log_odds + 0.85f, 4.0f);          // 점유 증가(상한)
        if (temp) c.temp_c = std::isnan(c.temp_c) ? *temp : 0.7f*c.temp_c + 0.3f*(*temp);
        dirty_.insert(key(i,j,k));
    }

    // 센서 위치 sensor → 히트 hit 사이를 자유공간으로 카빙(DDA 근사)
    void integrate_ray(const frize::Vec3& sensor, const frize::Vec3& hit) {
        const int steps = std::max(1, (int)(distance(sensor,hit) / vs_));
        for (int s = 0; s < steps; ++s) {
            double t = (double)s / steps;
            int i,j,k; idx(sensor.x+(hit.x-sensor.x)*t, sensor.y+(hit.y-sensor.y)*t,
                           sensor.z+(hit.z-sensor.z)*t, i,j,k);
            Cell& c = cells_[key(i,j,k)];
            c.log_odds = std::max(c.log_odds - 0.4f, -2.0f);      // 자유공간
            dirty_.insert(key(i,j,k));
        }
        integrate_hit(hit);
    }

    bool occupied(int64_t key) const {
        auto it = cells_.find(key); return it != cells_.end() && it->second.log_odds > 0.5f;
    }
    double voxel_size() const { return vs_; }
    size_t size() const { return cells_.size(); }

    // 프런티어 = '알려진 자유공간'이면서 6이웃 중 '미탐사(맵에 없음)'와 맞닿은 셀.
    // 드론이 여기로 가면 미탐사 영역이 채워진다(프런티어 탐사).
    std::vector<std::array<int,3>> frontiers(size_t maxn = 1500) const {
        std::vector<std::array<int,3>> out;
        static const int N[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        for (auto& [k, c] : cells_) {
            if (c.log_odds > -0.3f) continue;          // 자유공간만
            int i=di(k), j=dj(k), kk=dk(k);
            for (auto& n : N)
                if (cells_.find(key(i+n[0], j+n[1], kk+n[2])) == cells_.end()) { out.push_back({i,j,kk}); break; }
            if (out.size() >= maxn) break;
        }
        return out;
    }
    int known_count() const {
        int c=0; for (auto& [k,v] : cells_) if (std::abs(v.log_odds) > 0.5f) ++c; return c;
    }

    // 변경분만 MapPatch로 추출 후 dirty 비움
    frize::MapPatch take_patch() {
        frize::MapPatch p; p.patch_id = frize::Envelope::gen_id("patch");
        p.voxel_size_m = vs_; p.origin = {0,0,0};
        for (int64_t kk : dirty_) {
            auto it = cells_.find(kk); if (it == cells_.end()) continue;
            int i = (int)((kk >> 42) & 0x1FFFFF) - (1<<20);
            int j = (int)((kk >> 21) & 0x1FFFFF) - (1<<20);
            int k = (int)(kk & 0x1FFFFF) - (1<<20);
            float lo = it->second.log_odds;
            if (lo > 0.5f) {
                int occ = (int)std::min(255.0f, (lo/4.0f)*255.0f);
                p.occupied.push_back({i,j,k,occ});
                if (!std::isnan(it->second.temp_c))
                    p.thermal.push_back({(double)i,(double)j,(double)k,(double)it->second.temp_c});
            }
        }
        p.frontiers = frontiers();
        p.explored_cells = known_count();
        dirty_.clear();
        return p;
    }

private:
    static double distance(const frize::Vec3&a,const frize::Vec3&b){
        return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)); }
    double vs_;
    std::unordered_map<int64_t, Cell> cells_;
    std::unordered_set<int64_t> dirty_;
};

}  // namespace frize::mapping
