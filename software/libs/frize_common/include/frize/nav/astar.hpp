// frize/nav/astar.hpp ― 격자 A* 경로탐색 + 가시선 단순화
//
// 8-이웃 A*(옥타일 휴리스틱). 시작/목표는 가장 가까운 자유 셀로 스냅.
// 결과를 가시선 스트링풀링으로 단순화 → 최소 경유점(ArCue.route).
#pragma once
#include <vector>
#include <queue>
#include <cmath>
#include <limits>
#include <cstdint>
#include "frize/nav/grid.hpp"

namespace frize::nav {

// 월드 시작→목표. 실패 시 빈 벡터.
inline std::vector<Vec3> astar(const NavGrid& g, const Vec3& start, const Vec3& goal, double z=0.0){
    int sx,sy,gx,gy;
    g.world_to_cell(start.x,start.y,sx,sy);
    g.world_to_cell(goal.x ,goal.y ,gx,gy);
    if (!g.snap_free(sx,sy) || !g.snap_free(gx,gy)) return {};
    if (sx==gx && sy==gy) return { g.cell_to_world(gx,gy,z) };

    const int N = g.nx*g.ny;
    std::vector<float> gcost(N, std::numeric_limits<float>::infinity());
    std::vector<int>   came(N, -1);
    std::vector<uint8_t> closed(N, 0);
    auto H=[&](int x,int y){ double dx=std::abs(x-gx), dy=std::abs(y-gy);
        return (float)((dx+dy) + (1.41421356-2.0)*std::min(dx,dy)); };   // 옥타일

    typedef std::pair<float,int> PQ;  // (f, cell)
    std::priority_queue<PQ, std::vector<PQ>, std::greater<PQ>> open;
    int sidx=g.idx(sx,sy); gcost[sidx]=0; open.push({H(sx,sy), sidx});

    static const int DX[8]={1,-1,0,0,1,1,-1,-1}, DY[8]={0,0,1,-1,1,-1,1,-1};
    int gidx=g.idx(gx,gy); bool found=false;
    while(!open.empty()){
        int cur=open.top().second; open.pop();
        if (closed[cur]) continue; closed[cur]=1;
        if (cur==gidx){ found=true; break; }
        int cx=cur%g.nx, cy=cur/g.nx;
        for (int k=0;k<8;++k){
            int nxp=cx+DX[k], nyp=cy+DY[k];
            if (g.blocked(nxp,nyp)) continue;
            if (k>=4 && (g.blocked(cx+DX[k],cy) && g.blocked(cx,cy+DY[k]))) continue; // 대각 모서리 컷 방지
            int ni=g.idx(nxp,nyp); if (closed[ni]) continue;
            float step=(k<4)?1.0f:1.41421356f;
            float ng=gcost[cur]+step;
            if (ng<gcost[ni]){ gcost[ni]=ng; came[ni]=cur; open.push({ng+H(nxp,nyp), ni}); }
        }
    }
    if (!found) return {};
    std::vector<Vec3> path;
    for (int c=gidx; c!=-1; c=came[c]) path.push_back(g.cell_to_world(c%g.nx, c/g.nx, z));
    std::reverse(path.begin(), path.end());
    return path;
}

// 가시선 스트링풀링: 연속 직선구간을 한 점으로 합쳐 경유점 최소화.
inline std::vector<Vec3> simplify_los(const NavGrid& g, const std::vector<Vec3>& path){
    if (path.size()<=2) return path;
    std::vector<Vec3> out; out.push_back(path.front());
    size_t anchor=0;
    for (size_t i=2; i<path.size(); ++i){
        if (!g.line_of_sight(path[anchor].x,path[anchor].y, path[i].x,path[i].y)){
            out.push_back(path[i-1]); anchor=i-1;
        }
    }
    out.push_back(path.back());
    return out;
}

// 편의: 경로 + 단순화 한 번에.
inline std::vector<Vec3> plan(const NavGrid& g, const Vec3& start, const Vec3& goal, double z=0.0){
    return simplify_los(g, astar(g, start, goal, z));
}

// 경로 총 길이(m).
inline double path_length(const std::vector<Vec3>& p){
    double L=0; for (size_t i=1;i<p.size();++i){ double dx=p[i].x-p[i-1].x, dy=p[i].y-p[i-1].y; L+=std::sqrt(dx*dx+dy*dy); }
    return L;
}

}  // namespace frize::nav
