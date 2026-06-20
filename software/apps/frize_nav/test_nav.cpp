// A* 내비 검증: sim 건물 점유격자에서 경로탐색 + 단순화 + 벽 통과 금지.
#include <cstdio>
#include <vector>
#include <array>
#include <cstring>
#include "frize/nav/grid.hpp"
#include "frize/nav/astar.hpp"
#include "frize/sim/building.hpp"
using namespace frize; using namespace frize::nav;

static NavGrid grid_from_building(const sim::Building& b, double res){
    std::vector<std::array<double,4>> rects;
    for (auto& B : b.boxes()){
        if (!std::strcmp(B.kind,"slab")||!std::strcmp(B.kind,"roof")) continue; // 바닥/천장 제외
        if (B.hi.z < 0.3 || B.lo.z > 1.9) continue;                              // 보행 슬라이스와 겹치는 것만
        rects.push_back({B.lo.x, B.lo.y, B.hi.x, B.hi.y});
    }
    auto g = NavGrid::from_rects(-1.0,-1.0, b.W+2.0, b.D+2.0, res, rects);
    g.inflate(2); // 대원 반경 ~2셀
    return g;
}

static bool path_clear(const NavGrid& g, const std::vector<Vec3>& p){
    for (size_t i=1;i<p.size();++i) if(!g.line_of_sight(p[i-1].x,p[i-1].y,p[i].x,p[i].y)) return false;
    return true;
}

int main(){
    sim::Building b;
    double res=0.12;
    NavGrid g = grid_from_building(b, res);
    printf("[nav] grid %dx%d res=%.2f\n", g.nx, g.ny, g.res);

    int fails=0;
    auto test=[&](const char* name, Vec3 s, Vec3 t, bool expect){
        auto raw = astar(g, s, t);
        auto p = simplify_los(g, raw);
        bool ok = !p.empty();
        double L = path_length(p);
        bool clear = ok ? path_clear(g,p) : true;
        printf("  %-22s start(%.1f,%.1f)->goal(%.1f,%.1f): %s  wp=%zu len=%.1fm clear=%s\n",
               name, s.x,s.y,t.x,t.y, ok?"PATH":"NONE", p.size(), L, clear?"yes":"NO");
        if (ok!=expect){ printf("    !! expected %s\n", expect?"PATH":"NONE"); ++fails; }
        if (ok && !clear){ printf("    !! path crosses wall\n"); ++fails; }
    };

    // 현관(서벽 갭, y~6) → 요구조자1(남동 끝방 x22.8,y2.2): 복도+문 경유 필요
    test("entry->victim1(SE)", {0.0f,6.0f,0}, {22.8f,2.2f,0}, true);
    // 복도 → 요구조자2(북서 병실 x2.6,y9.8)
    test("corridor->victim2(NW)", {8.0f,6.0f,0}, {2.6f,9.8f,0}, true);
    // 복도 → 주방 가스레인지(x5.4,y1.05)
    test("corridor->kitchen", {8.0f,6.0f,0}, {5.4f,1.2f,0}, true);
    // 방 안 → 다른 방(문 두 개 경유)
    test("roomS1->roomN4", {3.2f,2.5f,0}, {22.5f,9.5f,0}, true);

    printf(fails? "\n[FAIL] %d\n" : "\n[OK] all nav tests passed\n", fails);
    return fails?1:0;
}
