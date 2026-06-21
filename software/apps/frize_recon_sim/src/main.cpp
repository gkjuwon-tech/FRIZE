// frize_recon_sim ― 디지털 트윈 재구성 + 화재 확산 분석 시뮬 (end-to-end 검증)
//
// "실제 건물 만들고 드론/사람 돌려서 3D + 불 번지는 게 트윈에 뜨는지" — 바로 이거.
//   1) 합성 건물 + 시간 진행형 화재장(전선이 번진다)
//   2) SCOUT 드론 + VISOR 대원을 '여러 랩' 돌린다 → 같은 화재 구역을 주기적 재방문
//   3) 매 스윕을 프로덕션 TSDF에 융합(표면+열) + FireAnalyzer에 시계열 융합(확산)
//   4) 랩마다 마칭큐브 스냅샷 → 트윈의 뜨거운 영역이 '자라는지'(=불 번짐) 확인
//   5) 확산 전선/방향벡터/재정찰 목표 + 검증 리포트(JSON) 출력
// 엔진은 건물도 화재장도 절대 못 본다 → 복원 메쉬와 확산 분석이 진실과 닮으면 성공.
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
#include "frize/recon/fire.hpp"
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

// 메쉬 정점 중 t°C 초과인 개수(트윈에서 '불색'으로 보이는 표면 = 화재 영역).
static int hot_verts(const Mesh& m, float t){
    int n=0; for (float v:m.temp) if (!std::isnan(v)&&v>t) ++n; return n;
}

// 그라운드트루스 표면 대비 커버리지(%).
static double coverage(const Building& b, const Mesh& m, float radius){
    if (m.v.empty()) return 0.0;
    float cs = radius;
    std::unordered_set<int64_t> occ;
    auto key=[&](float x,float y,float z)->int64_t{
        int i=(int)std::floor(x/cs),j=(int)std::floor(y/cs),k=(int)std::floor(z/cs);
        return (((int64_t)(i+4096)*8192+(j+4096))*8192)+(k+4096); };
    for (auto& p:m.v) occ.insert(key(p.x,p.y,p.z));
    std::mt19937 rng(7); std::uniform_real_distribution<float> U(0,1);
    int N=40000, hit=0;
    const auto& boxes=b.boxes();
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

static double envf(const char* k, double d){ const char* v=std::getenv(k); return v?std::atof(v):d; }
static int    envi(const char* k, int d){ const char* v=std::getenv(k); return v?std::atoi(v):d; }

int main(int argc, char** argv){
    std::string out = argc>1 ? argv[1] : "out";
    float voxel = argc>2 ? std::atof(argv[2]) : 0.07f;
    int   laps        = envi("FRIZE_SIM_LAPS", 4);           // 재방문 랩 수
    float lap_seconds = (float)envf("FRIZE_SIM_LAP_SEC", 22.0);// 랩당 흐르는 시각(초)
    auto t0 = std::chrono::steady_clock::now();

    Building bld;
    // 시간 진행형 화재장: 남서 주방 + 북측 병실에서 발화, 복도(+x)로 번진다.
    FireSim fire; fire.origins = { bld.fire, bld.fire2 };

    Vec3f origin{-1.0f,-1.0f,-0.6f};
    int nx=(int)std::ceil((bld.W+2.0f)/voxel);
    int ny=(int)std::ceil((bld.D+2.0f)/voxel);
    int nz=(int)std::ceil((bld.H+1.2f)/voxel);
    TsdfVolume vol(origin, {nx,ny,nz}, voxel);
    FireAnalyzer analyzer(0.40f);    // 확산 분석(0.4m 셀)

    printf("[sim] 건물 %d boxes | TSDF %dx%dx%d (%.0fM cells) voxel=%.3fm trunc=%.3fm\n",
           (int)bld.boxes().size(), nx,ny,nz, nx*(double)ny*nz/1e6, voxel, vol.trunc());

    auto dpath = drone_path();
    auto fpath = firefighter_path();
    printf("[sim] SCOUT 자세 %d개, VISOR 자세 %d개 | 재방문 랩 %d개 × %.0fs\n",
           (int)dpath.size(), (int)fpath.size(), laps, lap_seconds);

    auto dlidar = drone_lidar();
    auto vlidar = visor_depth();

    size_t total_pts=0, nposes = std::max(dpath.size(), fpath.size());

    // 랩별 화재 진행 로그(트윈에 불이 자라는지 검증용)
    struct LapRec { float t; int front, burning; float max_temp, max_rate; int hot_verts; };
    std::vector<LapRec> laplog;
    int hot_lap0 = -1;   // 첫 랩의 화재 표면 정점 수(재방문 전)

    for (int lap=0; lap<laps; ++lap){
        float base_t = lap * lap_seconds;
        for (size_t step=0; step<nposes; ++step){
            float t = base_t + (lap_seconds * (float)(step+1)/nposes);   // 이 자세의 시각
            if (step < dpath.size()){
                auto& P=dpath[step];
                auto pts=dlidar.scan(bld, P.pos, P.yaw, P.pitch, &fire, t);
                vol.integrate_scan(P.pos, pts); total_pts+=pts.size();
                for (auto& sp:pts) analyzer.observe(sp.p.x,sp.p.y,sp.p.z,sp.temp_c,t);
            }
            if (step < fpath.size()){
                auto& P=fpath[step];
                auto pts=vlidar.scan(bld, P.pos, P.yaw, P.pitch, &fire, t);
                vol.integrate_scan(P.pos, pts); total_pts+=pts.size();
                for (auto& sp:pts) analyzer.observe(sp.p.x,sp.p.y,sp.p.z,sp.temp_c,t);
            }
        }
        // 랩 종료: 트윈 메쉬 스냅샷 + 확산 분석
        float lap_end_t = base_t + lap_seconds;
        Mesh m = marching_cubes(vol);
        int hv = hot_verts(m, 100.f);
        if (lap==0) hot_lap0 = hv;
        auto fs = analyzer.summary();
        laplog.push_back({lap_end_t, fs.front_cells, fs.burning_cells,
                          fs.max_temp, fs.max_rate, hv});
        char path[256]; snprintf(path,sizeof(path),"%s/twin_lap%d.ply", out.c_str(), lap);
        m.write_ply(path);
        printf("[sim] 랩%d t=%4.0fs | pts=%zu | 화재정점(>100C)=%d | 전선=%d 화점=%d "
               "최대 %.0f°C %.1f°C/s → %s\n",
               lap, lap_end_t, total_pts, hv, fs.front_cells, fs.burning_cells,
               fs.max_temp, fs.max_rate, path);
    }

    // 최종 재구성 + Taubin 스무딩
    Mesh mesh = marching_cubes(vol);
    int smooth_it = std::getenv("FRIZE_SMOOTH") ? std::atoi(std::getenv("FRIZE_SMOOTH")) : 3;
    if (smooth_it>0) mesh.smooth_taubin(smooth_it);
    int frontier_final = count_frontiers(vol);

    mesh.write_obj(out + "/twin.obj");
    mesh.write_ply(out + "/twin.ply");
    mesh.write_gltf(out + "/twin.gltf", "twin.bin");

    // ── 화재 확산 분석 결과(최종 시각 기준) ──
    double now_t = laps * lap_seconds;
    auto front   = analyzer.front();
    auto spread  = analyzer.spread();
    auto revisit = analyzer.revisit_targets(now_t, /*stale_s=*/lap_seconds*0.8);
    auto fsum    = analyzer.summary();

    int hotspots=0; for (float t:mesh.temp) if (!std::isnan(t)&&t>100.f) ++hotspots;
    double cov = coverage(bld, mesh, voxel*2.2f);
    double area = mesh.surface_area();
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();

    // ── 검증: 재방문으로 트윈에 불이 '번지는' 게 실제로 보이는가? ──
    int hot_final = laplog.empty()?0:laplog.back().hot_verts;
    bool twin_shows_spread = hot_lap0>0 && hot_final >= (int)(hot_lap0*1.25);  // 화재면적 성장
    bool spread_detected   = !spread.empty() && fsum.front_cells>0;            // 확산 전선/벡터 검출
    bool has_direction     = false;
    for (auto& s:spread) if (std::fabs(s.dx)+std::fabs(s.dy)+std::fabs(s.dz)>1e-3){ has_direction=true; break; }
    bool revisit_ready     = !revisit.empty();                                 // 재정찰 목표 산출

    printf("\n===== 재구성 + 화재 확산 분석 완료 =====\n");
    printf("스캔 포인트   : %zu\n", total_pts);
    printf("관측 셀       : %zu (%.1f%% of volume)\n", vol.observed_cells(), vol.explored_fraction()*100);
    printf("메쉬 정점/삼각: %zu / %zu | 표면적 %.1f m^2\n", mesh.v.size(), mesh.tris(), area);
    printf("GT 표면 커버리지: %.1f%%\n", cov);
    printf("프런티어(미탐사): %d\n", frontier_final);
    printf("---- 화재 ----\n");
    printf("화재 표면 정점(>100C): 랩0 %d → 최종 %d  (×%.2f 성장)\n",
           hot_lap0, hot_final, hot_lap0>0?(double)hot_final/hot_lap0:0.0);
    printf("확산 전선 셀     : %d (평균 %.1f°C/s)\n", fsum.front_cells, fsum.mean_front_rate);
    printf("화점 핵 셀       : %d (면적 ~%.1f m^2, 최고 %.0f°C)\n",
           fsum.burning_cells, fsum.burning_area_m2, fsum.max_temp);
    printf("확산 클러스터    : %zu개\n", spread.size());
    for (size_t i=0;i<spread.size() && i<4;++i){
        auto& s=spread[i];
        printf("  · (%.1f,%.1f,%.1f) → 방향(%.2f,%.2f,%.2f) %.1f°C/s 최고%.0f°C 셀%d\n",
               s.cx,s.cy,s.cz, s.dx,s.dy,s.dz, s.rate, s.peak, s.cells);
    }
    printf("재정찰 목표      : %zu개 (드론이 되돌아가 확인할 곳)\n", revisit.size());
    printf("---- 검증 ----\n");
    printf("[%s] 트윈에 화재 확산 표시(면적 성장)\n", twin_shows_spread?"PASS":"FAIL");
    printf("[%s] 확산 전선/벡터 검출\n", spread_detected?"PASS":"FAIL");
    printf("[%s] 확산 방향 추정\n", has_direction?"PASS":"FAIL");
    printf("[%s] 재정찰 목표 산출\n", revisit_ready?"PASS":"FAIL");
    printf("소요          : %.1fs\n", secs);
    printf("출력          : %s/twin.{gltf,bin,ply,obj} + twin_lap0..%d.ply\n", out.c_str(), laps-1);

    // ── JSON 리포트 ──
    std::ofstream j(out + "/report.json");
    j << "{\n";
    j << "  \"voxel_m\": " << voxel << ",\n";
    j << "  \"volume_dims\": [" << nx << "," << ny << "," << nz << "],\n";
    j << "  \"building_extent_m\": [" << bld.W << "," << bld.D << "," << bld.H << "],\n";
    j << "  \"laps\": " << laps << ", \"lap_seconds\": " << lap_seconds << ",\n";
    j << "  \"drone_poses\": " << dpath.size() << ", \"visor_poses\": " << fpath.size() << ",\n";
    j << "  \"scan_points\": " << total_pts << ",\n";
    j << "  \"observed_cells\": " << vol.observed_cells() << ",\n";
    j << "  \"explored_fraction\": " << vol.explored_fraction() << ",\n";
    j << "  \"mesh_vertices\": " << mesh.v.size() << ", \"mesh_triangles\": " << mesh.tris() << ",\n";
    j << "  \"surface_area_m2\": " << area << ",\n";
    j << "  \"gt_surface_coverage_pct\": " << cov << ",\n";
    j << "  \"frontier_final\": " << frontier_final << ",\n";
    j << "  \"thermal_hotspot_vertices\": " << hotspots << ",\n";
    // 화재 진행(랩별) — 트윈에 불이 자라는 곡선
    j << "  \"fire_progression\": [";
    for (size_t i=0;i<laplog.size();++i){ auto&L=laplog[i];
        j << "{\"t\":"<<L.t<<",\"front\":"<<L.front<<",\"burning\":"<<L.burning
          << ",\"max_temp\":"<<L.max_temp<<",\"max_rate\":"<<L.max_rate
          << ",\"hot_vertices\":"<<L.hot_verts<<"}" << (i+1<laplog.size()?",":"");
    }
    j << "],\n";
    j << "  \"fire_summary\": {\"front_cells\":"<<fsum.front_cells
      << ",\"burning_cells\":"<<fsum.burning_cells
      << ",\"max_temp_c\":"<<fsum.max_temp<<",\"max_rate_cps\":"<<fsum.max_rate
      << ",\"mean_front_rate_cps\":"<<fsum.mean_front_rate
      << ",\"burning_area_m2\":"<<fsum.burning_area_m2<<"},\n";
    // 확산 벡터(어디로 번지나)
    j << "  \"spread_vectors\": [";
    for (size_t i=0;i<spread.size();++i){ auto&s=spread[i];
        j << "{\"c\":["<<s.cx<<","<<s.cy<<","<<s.cz<<"],\"dir\":["<<s.dx<<","<<s.dy<<","<<s.dz
          << "],\"rate\":"<<s.rate<<",\"peak\":"<<s.peak<<",\"cells\":"<<s.cells<<"}"
          << (i+1<spread.size()?",":"");
    }
    j << "],\n";
    j << "  \"fire_front_count\": " << front.size() << ",\n";
    j << "  \"revisit_targets\": " << revisit.size() << ",\n";
    j << "  \"verification\": {\"twin_shows_spread\":"<<(twin_shows_spread?"true":"false")
      << ",\"spread_detected\":"<<(spread_detected?"true":"false")
      << ",\"has_direction\":"<<(has_direction?"true":"false")
      << ",\"revisit_ready\":"<<(revisit_ready?"true":"false")<<"},\n";
    j << "  \"runtime_s\": " << secs << ",\n";
    j << "  \"fire_xyz\": [" << bld.fire.x << "," << bld.fire.y << "," << bld.fire.z << "],\n";
    j << "  \"victim_xyz\": [" << bld.victim.x << "," << bld.victim.y << "," << bld.victim.z << "]\n";
    j << "}\n";
    printf("리포트        : %s/report.json\n", out.c_str());

    bool all = twin_shows_spread && spread_detected && has_direction && revisit_ready;
    return all ? 0 : 1;
}
