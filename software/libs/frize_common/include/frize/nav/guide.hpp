// frize/nav/guide.hpp ― 유도 엔진: 격자 + 경로탐색 + ArCue(Route) 생성
//
// "트윈에서 목표점 찍고 대원 찍으면" 콕핏이 목표를 등록하고, 이 엔진이 대원
// 실시간 위치마다 경로를 재계산해 해당 고글에 ArCue(Route)를 스트리밍한다.
#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <cstdio>
#include "frize/nav/grid.hpp"
#include "frize/nav/astar.hpp"
#include "frize/schemas.hpp"

namespace frize::nav {

class Guidance {
public:
    Guidance() = default;
    explicit Guidance(NavGrid g, double waypoint_z=0.05)
        : grid_(std::move(g)), z_(waypoint_z) {}

    void set_grid(NavGrid g){ grid_ = std::move(g); have_grid_ = true; }
    bool have_grid() const { return have_grid_; }
    const NavGrid& grid() const { return grid_; }

    // 대원 위치 → 목표 경로(벽 회피, 단순화). 실패 시 빈 벡터.
    std::vector<Vec3> plan_route(const Vec3& wearer, const Vec3& target) const {
        return plan(grid_, wearer, target, z_);
    }

    // 경로 → ArCue(Route). 짧은 ttl 로 계속 갱신(대원 움직이면 새 큐가 덮어씀).
    frize::ArCue route_cue(const std::string& wearer_id,
                           const std::vector<Vec3>& route,
                           const std::string& color = "#36c0ff",
                           const std::string& issued_by = "nav",
                           double ttl_s = 2.5) const {
        frize::ArCue c;
        c.cue_id = next_id();
        c.target_device = wearer_id;
        c.kind = frize::ArCueKind::Route;
        c.route = route;
        c.color = color;
        c.ttl_s = ttl_s;
        c.issued_by = issued_by;
        if (!route.empty()) c.world_to = route.back();
        c.text = "ROUTE";
        return c;
    }

    // 도착 판정(수평거리).
    bool arrived(const Vec3& wearer, const Vec3& target, double tol_m = 1.2) const {
        double dx = wearer.x-target.x, dy = wearer.y-target.y;
        return std::sqrt(dx*dx+dy*dy) < tol_m;
    }

private:
    static std::string next_id(){
        static std::atomic<uint64_t> ctr{0};
        char buf[40];
        std::snprintf(buf, sizeof(buf), "cue-%llx",
                      (unsigned long long)((uint64_t)(frize::now_s()*1000.0) ^ (ctr.fetch_add(1)<<8)));
        return buf;
    }
    NavGrid grid_;
    double z_{0.05};
    bool have_grid_{false};
};

}  // namespace frize::nav
