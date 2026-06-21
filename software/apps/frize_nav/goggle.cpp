// frize_nav_goggle ― 실제 내비 엔진으로 "고글 1인칭 AR 뷰" 장면 덤프
//
// 데모처럼 수치만 찍는 게 아니라, A* 경로 + ar_project 투영을 그대로 써서
// 대원 고글 화면에 들어갈 데이터(건물 와이어프레임 + 바닥 셰브론 + 경로 +
// 회전힌트/거리)를 프레임별 JSON 으로 내보낸다. Python(goggle_render.py)이
// 이걸로 고글 영상을 그린다 → 화면에 보이는 화살표가 실제 엔진 출력.
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <array>
#include <fstream>
#include "frize/nav/grid.hpp"
#include "frize/nav/astar.hpp"
#include "frize/nav/guide.hpp"
#include "frize/nav/ar_project.hpp"
#include "frize/sim/building.hpp"
using namespace frize; using namespace frize::nav;

int main(int argc, char** argv){
    std::string out = argc>1?argv[1]:"/tmp/goggle/scene.json";
    int W=1280, H=720; double pitch=-0.30, hfov=1.45, eyeh=1.55;

    sim::Building b;
    // 격자(데모/테스트와 동일)
    std::vector<std::array<double,4>> rects;
    for (auto& B : b.boxes()){
        if (!std::strcmp(B.kind,"slab")||!std::strcmp(B.kind,"roof")) continue;
        if (B.hi.z<0.3||B.lo.z>1.9) continue;
        rects.push_back({B.lo.x,B.lo.y,B.hi.x,B.hi.y});
    }
    NavGrid g = NavGrid::from_rects(-1.0,-1.0,b.W+2.0,b.D+2.0,0.12,rects); g.inflate(2);
    Guidance guide(g);
    Vec3 target{b.victim.x,b.victim.y,0};

    std::ofstream j(out);
    j.setf(std::ios::fixed); j.precision(3);
    j<<"{\"cam\":{\"W\":"<<W<<",\"H\":"<<H<<",\"pitch\":"<<pitch<<",\"hfov\":"<<hfov<<",\"eyeh\":"<<eyeh<<"},";
    j<<"\"target\":["<<target.x<<","<<target.y<<"],";
    // 건물 박스(와이어프레임용): [xmin,ymin,xmax,ymax,ztop,kind]
    j<<"\"boxes\":[";
    bool bf=true;
    for (auto& B : b.boxes()){
        if (!std::strcmp(B.kind,"slab")||!std::strcmp(B.kind,"roof")) continue;
        j<<(bf?"":",")<<"["<<B.lo.x<<","<<B.lo.y<<","<<B.hi.x<<","<<B.hi.y<<","<<B.hi.z<<",\""<<B.kind<<"\"]"; bf=false;
    }
    j<<"],\"frames\":[";

    Vec3 w{0.0f,6.0f,0.0f}; double step=0.20;
    bool ff=true;
    for (int t=0;t<240 && !guide.arrived(w,target);++t){
        auto route = guide.plan_route(w,target);
        if (route.size()<2) break;
        Vec3 nx=route[1]; double yaw=std::atan2(nx.y-w.y,nx.x-w.x);
        Vec3 eye{w.x,w.y,eyeh};
        auto chev = project_route(route, eye, yaw, pitch, hfov, W, H);   // ★ 실제 ar_project
        int th = turn_hint(route, eye, yaw);
        double dist = path_length(route);

        j<<(ff?"":",")<<"{\"eye\":["<<eye.x<<","<<eye.y<<","<<eye.z<<"],\"yaw\":"<<yaw
         <<",\"turn\":"<<th<<",\"dist\":"<<dist<<",\"route\":[";
        for (size_t k=0;k<route.size();++k) j<<(k?",":"")<<"["<<route[k].x<<","<<route[k].y<<"]";
        j<<"],\"chev\":[";
        for (size_t k=0;k<chev.size();++k) j<<(k?",":"")<<"["<<chev[k].u<<","<<chev[k].v<<","<<chev[k].scale<<"]";
        j<<"]}";
        ff=false;

        double dx=nx.x-w.x,dy=nx.y-w.y,L=std::sqrt(dx*dx+dy*dy);
        if(L>1e-3){ w.x+=dx/L*step; w.y+=dy/L*step; }
        w.x+=0.04*std::sin(t*1.1); w.y+=0.04*std::cos(t*0.8);  // 보행 드리프트
    }
    j<<"]}";
    printf("[goggle] scene → %s\n", out.c_str());
    return 0;
}
