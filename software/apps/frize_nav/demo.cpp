// frize_nav_demo ― 내비 엔진 동작 증명(MQTT 없이)
//
// sim 건물에서 점유격자를 만들고, 대원이 목표로 한 걸음씩 이동할 때마다
// 경로를 재계산 → 남은 경로/길이/회전힌트/고글 화면 투영(셰브론)이 어떻게
// "계속 바뀌며" 안내하는지 출력한다. (프로덕션 frize_nav 와 동일 엔진)
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <array>
#include "frize/nav/grid.hpp"
#include "frize/nav/astar.hpp"
#include "frize/nav/guide.hpp"
#include "frize/nav/ar_project.hpp"
#include "frize/sim/building.hpp"
using namespace frize; using namespace frize::nav;

static NavGrid grid_from_building(const sim::Building& b, double res, int infl){
    std::vector<std::array<double,4>> rects;
    for (auto& B : b.boxes()){
        if (!std::strcmp(B.kind,"slab")||!std::strcmp(B.kind,"roof")) continue;
        if (B.hi.z < 0.3 || B.lo.z > 1.9) continue;
        rects.push_back({B.lo.x,B.lo.y,B.hi.x,B.hi.y});
    }
    auto g = NavGrid::from_rects(-1.0,-1.0,b.W+2.0,b.D+2.0,res,rects); g.inflate(infl);
    return g;
}

int main(){
    sim::Building b;
    Guidance guide(grid_from_building(b,0.12,2));
    printf("[demo] grid %dx%d  목표=요구조자1(%.1f,%.1f)\n\n",
           guide.grid().nx, guide.grid().ny, b.victim.x, b.victim.y);

    Vec3 target{b.victim.x, b.victim.y, 0};
    Vec3 w{0.0f, 6.0f, 0.0f};               // 현관에서 출발
    double step=0.9;                         // 한 걸음(m)
    printf("%-4s %-12s %-6s %-5s %-6s %-7s %s\n","t","wearer","wp","len","turn","chev","route(앞 3점)");
    printf("--------------------------------------------------------------------------\n");
    for (int t=0; t<40 && !guide.arrived(w,target); ++t){
        auto route = guide.plan_route(w,target);
        if (route.size()<2){ printf("%-4d (%.1f,%.1f)  경로 없음\n",t,w.x,w.y); break; }
        // 대원 머리 자세: 다음 경유점 방향
        Vec3 nx = route[1];
        double yaw = std::atan2(nx.y-w.y, nx.x-w.x);
        Vec3 eye{w.x,w.y,1.5};
        auto chev = project_route(route, eye, yaw, -0.28, 1.40, 1280, 720);
        int th = turn_hint(route, eye, yaw);
        const char* ts = th<0?"<-좌":(th>0?"우->":"직진");
        printf("%-4d (%4.1f,%4.1f)  %-6zu %4.1fm %-6s %-7zu", t,w.x,w.y, route.size(), path_length(route), ts, chev.size());
        for (size_t k=1;k<route.size() && k<=3;++k) printf(" (%.1f,%.1f)", route[k].x, route[k].y);
        printf("\n");
        // 한 걸음 전진(경로 방향 + 약간의 드리프트로 실제 보행 흉내)
        double dx=nx.x-w.x, dy=nx.y-w.y, L=std::sqrt(dx*dx+dy*dy);
        if (L>1e-3){ w.x += dx/L*step; w.y += dy/L*step; }
        w.x += 0.06*std::sin(t*1.3); w.y += 0.06*std::cos(t*0.9);  // 드리프트
    }
    printf("\n[demo] 도착 ― 대원이 움직일 때마다 경로/길이/투영이 갱신됨(실시간 재유도).\n");
    return 0;
}
