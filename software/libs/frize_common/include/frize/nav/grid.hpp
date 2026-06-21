// frize/nav/grid.hpp ― 2D 점유 격자(층 평면) ― AR 내비게이션의 지도 표현
//
// 디지털 트윈(MapPatch 점유 복셀) 또는 정적 장애물(AABB)에서 한 층의 2D
// 점유 격자를 만든다. 대원 반경만큼 팽창(inflate)해 벽에 붙지 않는 경로를 보장.
// A* 가 이 위에서 도는다(astar.hpp). 의존성 0(헤더 온리).
#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "frize/schemas.hpp"

namespace frize::nav {

using frize::Vec3;

struct NavGrid {
    double x0{0}, y0{0}, res{0.15};
    int nx{0}, ny{0};
    std::vector<uint8_t> occ;   // 0=자유, 1=장애물

    NavGrid() = default;
    NavGrid(double ox, double oy, double r, int w, int h)
        : x0(ox), y0(oy), res(r), nx(w), ny(h), occ((size_t)w*h, 0) {}

    bool inb(int cx, int cy) const { return cx>=0 && cy>=0 && cx<nx && cy<ny; }
    int  idx(int cx, int cy) const { return cy*nx + cx; }
    bool blocked(int cx, int cy) const { return !inb(cx,cy) || occ[idx(cx,cy)]; }
    void set(int cx, int cy, uint8_t v){ if(inb(cx,cy)) occ[idx(cx,cy)]=v; }

    void world_to_cell(double x, double y, int& cx, int& cy) const {
        cx = (int)std::floor((x - x0)/res);
        cy = (int)std::floor((y - y0)/res);
    }
    Vec3 cell_to_world(int cx, int cy, double z=0.0) const {
        return { x0 + (cx+0.5)*res, y0 + (cy+0.5)*res, z };
    }

    // 가장 가까운 자유 셀로 스냅(시작/목표가 벽 속이면 탈출). 반경 R 셀 탐색.
    bool snap_free(int& cx, int& cy, int R=10) const {
        if (inb(cx,cy) && !occ[idx(cx,cy)]) return true;
        for (int r=1; r<=R; ++r){
            for (int dy=-r; dy<=r; ++dy) for (int dx=-r; dx<=r; ++dx){
                if (std::abs(dx)!=r && std::abs(dy)!=r) continue;  // 링만
                int x=cx+dx, y=cy+dy;
                if (inb(x,y) && !occ[idx(x,y)]){ cx=x; cy=y; return true; }
            }
        }
        return false;
    }

    // 장애물 r 셀 팽창(대원 반경 + 여유). 단순 체비셰프 팽창.
    void inflate(int r){
        if (r<=0) return;
        std::vector<uint8_t> out = occ;
        for (int cy=0; cy<ny; ++cy) for (int cx=0; cx<nx; ++cx){
            if (!occ[idx(cx,cy)]) continue;
            for (int dy=-r; dy<=r; ++dy) for (int dx=-r; dx<=r; ++dx){
                int x=cx+dx, y=cy+dy; if (inb(x,y)) out[idx(x,y)]=1;
            }
        }
        occ.swap(out);
    }

    // 두 월드점 사이 가시선(자유공간만 통과?) ― 경로 단순화/리루트 판정.
    bool line_of_sight(double ax, double ay, double bx, double by) const {
        double dx=bx-ax, dy=by-ay; double L=std::sqrt(dx*dx+dy*dy);
        int n = std::max(1, (int)std::ceil(L/(res*0.5)));
        for (int i=0;i<=n;++i){
            double t=(double)i/n; int cx,cy;
            world_to_cell(ax+dx*t, ay+dy*t, cx, cy);
            if (blocked(cx,cy)) return false;
        }
        return true;
    }

    // ── 빌더: 정적 AABB 장애물(xy 풋프린트) ──
    //  bounds: [x0,y0,w,d]. rects: [xmin,ymin,xmax,ymax] (월드 m).
    static NavGrid from_rects(double bx, double by, double bw, double bd, double res,
                              const std::vector<std::array<double,4>>& rects){
        int nx=(int)std::ceil(bw/res), ny=(int)std::ceil(bd/res);
        NavGrid g(bx, by, res, nx, ny);
        for (auto& r : rects){
            int x0c,y0c,x1c,y1c;
            g.world_to_cell(r[0], r[1], x0c, y0c);
            g.world_to_cell(r[2], r[3], x1c, y1c);
            for (int cy=std::max(0,y0c); cy<=std::min(ny-1,y1c); ++cy)
                for (int cx=std::max(0,x0c); cx<=std::min(nx-1,x1c); ++cx)
                    g.occ[g.idx(cx,cy)] = 1;
        }
        return g;
    }

    // ── 빌더: 디지털 트윈 MapPatch 점유 복셀(층 슬라이스 [zlo,zhi]) ──
    static NavGrid from_mappatch(const frize::MapPatch& mp, double zlo, double zhi,
                                 double res, int occ_thresh=128){
        // 패치 xy 범위
        double minx=mp.origin.x, miny=mp.origin.y;
        double maxx=mp.origin.x + mp.dims[0]*mp.voxel_size_m;
        double maxy=mp.origin.y + mp.dims[1]*mp.voxel_size_m;
        int nx=std::max(1,(int)std::ceil((maxx-minx)/res));
        int ny=std::max(1,(int)std::ceil((maxy-miny)/res));
        NavGrid g(minx, miny, res, nx, ny);
        for (auto& v : mp.occupied){
            if (v[3] < occ_thresh) continue;
            double wz = mp.origin.z + (v[2]+0.5)*mp.voxel_size_m;
            if (wz < zlo || wz > zhi) continue;
            double wx = mp.origin.x + (v[0]+0.5)*mp.voxel_size_m;
            double wy = mp.origin.y + (v[1]+0.5)*mp.voxel_size_m;
            int cx,cy; g.world_to_cell(wx,wy,cx,cy); g.set(cx,cy,1);
        }
        return g;
    }
};

}  // namespace frize::nav
