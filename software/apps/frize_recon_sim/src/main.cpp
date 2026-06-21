// frize_recon_sim ― 디지털 트윈 재구성 시뮬레이션 (프로덕션 엔진 end-to-end 검증)
//
// "실제 건물 만들고 내부에 드론이랑 사람 돌려서 3D 만들어지는지" — 바로 이거.
//   1) 합성 건물(미지의 진실)을 세운다
//   2) SCOUT 드론 + VISOR 대원을 내부로 굴리며 매 자세 LiDAR 스윕
//   3) 스윕을 '프로덕션 엔진'(frize::recon TSDF)에 융합
//   4) 마칭큐브로 표면 메쉬 추출 → glTF/PLY/OBJ로 내보냄
//   5) 진행 스냅샷 + 품질 리포트(JSON) 출력
// 엔진은 건물 지오메트리를 절대 못 본다 → 복원 메쉬가 건물과 닮으면 성공.
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <chrono>
#include <fstream>
#include <random>

#include "frize/recon/tsdf.hpp"
#include "frize/recon/marching_cubes.hpp"
#include "frize/recon/mesh.hpp"
#include "frize/sim/building.hpp"
#include "frize/sim/lidar.hpp"
#include "frize/sim/agents.hpp"

using namespace frize::recon;
using namespace frize::sim;

// 프런티어: 관측된 자유공간이면서 6이웃 중 미탐사가 있는 셀(드론이 가야 할 곳).
static int count_frontiers(const TsdfVolume& v){
    int n=0; static const int N[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (int k=1;k<v.nz()-1;++k)for(int j=1;j<v.ny()-1;++j)for(int i=1;i<v.nx()-1;++i){
        const auto& c=v.at(i,j,k);
        if (c.w<=0 || c.sdf<0.3f) continue;            // 관측된 자유공간만
        for (auto& d:N) if (v.at(i+d[0],j+d[1],k+d[2]).w<=0){ ++n; break; }
    }
    return n;
}

// 그라운드트루스 표면 대비 커버리지(%): GT 표본점이 복원 메쉬 정점 근처에 있는 비율.
static double coverage(const Building& b, const Mesh& m, float radius){
    if (m.v.empty()) return 0.0;
    float cs = radius;                                  // 공간 해시 셀 = 탐색반경
    std::unordered_set<int64_t> occ;
    auto key=[&](float x,float y,float z)->int64_t{
        int i=(int)std::floor(x/cs),j=(int)std::floor(y/cs),k=(int)std::floor(z/cs);
        return (((int64_t)(i+4096)*8192+(j+4096))*8192)+(k+4096); };
    for (auto& p:m.v) occ.insert(key(p.x,p.y,p.z));
    std::mt19937 rng(7); std::uniform_real_distribution<float> U(0,1);
    int N=40000, hit=0;
    const auto& boxes=b.boxes();
    // 면적가중 표본
    std::vector<double> w; double tot=0;
    for (auto& B:boxes){ Vec3f e{B.hi.x-B.lo.x,B.hi.y-B.lo.y,B.hi.z-B.lo.z};
        double a=2*(e.x*e.y+e.y*e.z+e.x*e.z); w.push_back(a); tot+=a; }
    std::discrete_distribution<int> pick(w.begin(),w.end());
    for (int s=0;s<N;++s){
        const auto& B=boxes[pick(rng)];
        Vec3f e{B.hi.x-B.lo.x,B.hi.y-B.lo.y,B.hi.z-B.lo.z};
        Vec3f p{B.lo.x+U(rng)*e.x,B.lo.y+U(rng)*e.y,B.lo.z+U(rng)*e.z};
        int face=rng()%6; float* pa=&p.x; float lo=(&B.lo.x)[face/2], hi=(&B.hi.x)[face/2];
        pa[face/2]=(face%2)?hi:lo;
        bool found=false;
        for (int dx=-1;dx<=1&&!found;++dx)for(int dy=-1;dy<=1&&!found;++dy)for(int dz=-1;dz<=1&&!found;++dz)
            if (occ.count(key(p.x+dx*cs,p.y+dy*cs,p.z+dz*cs))) found=true;
        if (found) ++hit;
    }
    return 100.0*hit/N;
}

int main(int argc, char** argv){
    std::string out = argc>1 ? argv[1] : "out";
    float voxel = argc>2 ? std::atof(argv[2]) : 0.07f;
    auto t0 = std::chrono::steady_clock::now();

    Building bld;
    // 볼륨: 건물 bbox + 여유. ENU z-up.
    Vec3f origin{-1.0f,-1.0f,-0.6f};
    int nx=(int)std::ceil((bld.W+2.0f)/voxel);
    int ny=(int)std::ceil((bld.D+2.0f)/voxel);
    int nz=(int)std::ceil((bld.H+1.2f)/voxel);
    TsdfVolume vol(origin, {nx,ny,nz}, voxel);
    printf("[sim] 건물 %d boxes | TSDF %dx%dx%d (%.0fM cells) voxel=%.3fm trunc=%.3fm\n",
           (int)bld.boxes().size(), nx,ny,nz, nx*(double)ny*nz/1e6, voxel, vol.trunc());

    auto dpath = drone_path();
    auto fpath = firefighter_path();
    printf("[sim] SCOUT 자세 %d개, VISOR 자세 %d개\n", (int)dpath.size(), (int)fpath.size());

    auto dlidar = drone_lidar();
    auto vlidar = visor_depth();

    size_t total_pts=0;
    size_t nposes = std::max(dpath.size(), fpath.size());
    std::vector<std::pair<double,int>> frontier_log;   // (progress, frontier_count)

    // 진행 스냅샷 마일스톤 (메쉬가 자라는 걸 보여줌)
    std::vector<double> snaps = {0.25, 0.5, 0.75};
    size_t snap_i=0;
    std::ofstream rep; // report later

    for (size_t step=0; step<nposes; ++step){
        if (step < dpath.size()){
            auto& P=dpath[step];
            auto pts=dlidar.scan(bld, P.pos, P.yaw, P.pitch);
            vol.integrate_scan(P.pos, pts); total_pts+=pts.size();
        }
        if (step < fpath.size()){
            auto& P=fpath[step];
            auto pts=vlidar.scan(bld, P.pos, P.yaw, P.pitch);
            vol.integrate_scan(P.pos, pts); total_pts+=pts.size();
        }
        double prog = (double)(step+1)/nposes;
        if (snap_i<snaps.size() && prog>=snaps[snap_i]){
            Mesh m = marching_cubes(vol);
            int fr = count_frontiers(vol);
            frontier_log.push_back({prog, fr});
            char path[256]; snprintf(path,sizeof(path),"%s/twin_%02d.ply", out.c_str(), (int)(snaps[snap_i]*100));
            m.write_ply(path);
            printf("[sim] %3.0f%% | pts=%zu | verts=%zu tris=%zu | frontier=%d → %s\n",
                   snaps[snap_i]*100, total_pts, m.v.size(), m.tris(), fr, path);
            ++snap_i;
        }
    }

    // 최종 재구성 + Taubin 스무딩(찰흙 → 매끈한 실제 표면)
    Mesh mesh = marching_cubes(vol);
    int smooth_it = std::getenv("FRIZE_SMOOTH") ? std::atoi(std::getenv("FRIZE_SMOOTH")) : 3;
    if (smooth_it>0) mesh.smooth_taubin(smooth_it);
    int frontier_final = count_frontiers(vol);
    frontier_log.push_back({1.0, frontier_final});

    mesh.write_obj(out + "/twin.obj");
    mesh.write_ply(out + "/twin.ply");
    mesh.write_gltf(out + "/twin.gltf", "twin.bin");

    int hotspots=0; for (float t:mesh.temp) if (!std::isnan(t)&&t>100.f) ++hotspots;
    double cov = coverage(bld, mesh, voxel*2.2f);
    double area = mesh.surface_area();
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();

    printf("\n===== 재구성 완료 =====\n");
    printf("스캔 포인트   : %zu\n", total_pts);
    printf("관측 셀       : %zu (%.1f%% of volume)\n", vol.observed_cells(), vol.explored_fraction()*100);
    printf("메쉬 정점/삼각: %zu / %zu\n", mesh.v.size(), mesh.tris());
    printf("표면적        : %.1f m^2\n", area);
    printf("열 핫스팟(>100C) 정점: %d\n", hotspots);
    printf("GT 표면 커버리지: %.1f%%\n", cov);
    printf("프런티어(미탐사 경계): %d\n", frontier_final);
    printf("소요          : %.1fs\n", secs);
    printf("출력          : %s/twin.{gltf,bin,ply,obj} + twin_25/50/75.ply\n", out.c_str());

    // ── JSON 리포트 ──
    std::ofstream j(out + "/report.json");
    j << "{\n";
    j << "  \"voxel_m\": " << voxel << ",\n";
    j << "  \"trunc_m\": " << vol.trunc() << ",\n";
    j << "  \"volume_dims\": [" << nx << "," << ny << "," << nz << "],\n";
    j << "  \"building_extent_m\": [" << bld.W << "," << bld.D << "," << bld.H << "],\n";
    j << "  \"drone_poses\": " << dpath.size() << ", \"visor_poses\": " << fpath.size() << ",\n";
    j << "  \"scan_points\": " << total_pts << ",\n";
    j << "  \"observed_cells\": " << vol.observed_cells() << ",\n";
    j << "  \"explored_fraction\": " << vol.explored_fraction() << ",\n";
    j << "  \"mesh_vertices\": " << mesh.v.size() << ", \"mesh_triangles\": " << mesh.tris() << ",\n";
    j << "  \"surface_area_m2\": " << area << ",\n";
    j << "  \"thermal_hotspot_vertices\": " << hotspots << ",\n";
    j << "  \"gt_surface_coverage_pct\": " << cov << ",\n";
    j << "  \"frontier_final\": " << frontier_final << ",\n";
    j << "  \"runtime_s\": " << secs << ",\n";
    j << "  \"frontier_curve\": [";
    for (size_t i=0;i<frontier_log.size();++i)
        j << "[" << frontier_log[i].first << "," << frontier_log[i].second << "]" << (i+1<frontier_log.size()?",":"");
    j << "],\n";
    j << "  \"fire_xyz\": [" << bld.fire.x << "," << bld.fire.y << "," << bld.fire.z << "],\n";
    j << "  \"victim_xyz\": [" << bld.victim.x << "," << bld.victim.y << "," << bld.victim.z << "]\n";
    j << "}\n";
    printf("리포트        : %s/report.json\n", out.c_str());
    return 0;
}
