// frize/mapping/main.cpp ― 3D 맵핑/SLAM 융합 서비스
//
// 입력: 디바이스 텔레메트리(자세) + VLM 감지(world_pos/위험) [+ 실제 LiDAR]
// 출력: MapPatch (점유격자 + 열 복셀) → 콕핏 3D 트윈.
// 실제 운용: Livox Mid-360 포인트클라우드를 integrate_ray로 누적(PCL 경로).
#include <atomic>
#include <array>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <thread>

#include "frize/bus.hpp"
#include "frize/protocol.hpp"
#include "frize/log.hpp"
#include "frize/mapping/voxel_map.hpp"

// 프로덕션 재구성 엔진(헤더 온리): 점유격자에 더해 TSDF 표면 메쉬를 만든다.
// 큐브 복셀(콕핏 Repeater3D)과 별개로, 매끈한 glTF 표면을 콕핏 트윈에 공급.
#include "frize/recon/tsdf.hpp"
#include "frize/recon/marching_cubes.hpp"
#include "frize/recon/mesh.hpp"

using namespace frize;
static auto LOG = make_logger("mapping");
static std::string envs(const char* k, const std::string& d){ const char* v=std::getenv(k); return v?v:d; }
static int envi(const char* k, int d){ const char* v=std::getenv(k); return v?std::atoi(v):d; }

static float temp_for(HazardClass h) {
    switch (h) {
        case HazardClass::Fire:    return 600.f;
        case HazardClass::Hotspot: return 120.f;
        case HazardClass::Backdraft: return 400.f;
        case HazardClass::Person:  return 36.5f;
        case HazardClass::DownedPerson: return 35.f;
        default: return NAN;
    }
}

int main() {
    const std::string host = envs("FRIZE_MQTT_HOST", "localhost");
    const int port = envi("FRIZE_MQTT_PORT", 1883);
    const double vs = std::atof(envs("FRIZE_VOXEL_SIZE","0.25").c_str());
    LOG->info("MAPPING 시작 (voxel={}m, mqtt {}:{})", vs, host, port);

    mapping::VoxelMap map(vs);
    std::mutex mtx;

    // ── TSDF 표면 트윈(선택) ──────────────────────────────────────────────────
    // FRIZE_TWIN_MESH=1 이면 점유격자와 병행해 TSDF를 누적하고, 주기적으로
    // 마칭큐브로 twin.gltf를 써서 콕핏이 매끈한 표면 메쉬를 로드하게 한다.
    // 볼륨 bbox/복셀은 현장 크기에 맞춰 env로 조정(기본: 40×40×12m).
    const bool twin_mesh = envi("FRIZE_TWIN_MESH", 0) != 0;
    const std::string twin_dir = envs("FRIZE_TWIN_DIR", ".");
    const double twin_period = std::atof(envs("FRIZE_TWIN_MESH_SEC","3.0").c_str());
    const double tvs = std::atof(envs("FRIZE_TWIN_VOXEL","0.10").c_str());
    recon::Vec3f t_org{ (float)std::atof(envs("FRIZE_TWIN_ORIGIN_X","-20").c_str()),
                        (float)std::atof(envs("FRIZE_TWIN_ORIGIN_Y","-20").c_str()),
                        (float)std::atof(envs("FRIZE_TWIN_ORIGIN_Z","-2").c_str()) };
    std::array<int,3> t_dims{ envi("FRIZE_TWIN_NX",400), envi("FRIZE_TWIN_NY",400), envi("FRIZE_TWIN_NZ",140) };
    recon::TsdfVolume tsdf(t_org, t_dims, (float)tvs);
    if (twin_mesh) LOG->info("TSDF 트윈 활성: {}x{}x{} voxel={}m → {}/twin.gltf (주기 {}s)",
                             t_dims[0],t_dims[1],t_dims[2], tvs, twin_dir, twin_period);

    // 센서→히트 레이를 점유격자 + (켜졌으면) TSDF 양쪽에 누적.
    auto integrate = [&](const Vec3& s, const Vec3& hit, std::optional<float> temp){
        map.integrate_ray(s, hit);
        if (temp) map.integrate_hit(hit, temp);
        if (twin_mesh)
            tsdf.integrate_ray({(float)s.x,(float)s.y,(float)s.z},
                               {(float)hit.x,(float)hit.y,(float)hit.z},
                               temp ? *temp : std::numeric_limits<float>::quiet_NaN());
    };

    MessageBus bus("frize-mapping", host, port);

    // 디바이스 자세 → 센서 위치 기록(자유공간 카빙 시작점)
    std::unordered_map<std::string, Vec3> sensor_pos;
    bus.subscribe(Topic::all_telemetry(), [&](const std::string&, const Envelope& e){
        Vec3 p{ e.payload["pose"]["position"].value("x",0.0),
                e.payload["pose"]["position"].value("y",0.0),
                e.payload["pose"]["position"].value("z",0.0) };
        std::lock_guard<std::mutex> lk(mtx);
        sensor_pos[e.source] = p;
        // 디바이스 발밑은 점유(바닥), 주변은 자유
        integrate(Vec3{p.x,p.y,p.z+1.5}, Vec3{p.x,p.y,p.z}, std::nullopt);
    });

    // 감지(world_pos) → 점유 + 열 (디바이스 감지 + 코어 VLM 판단 둘 다)
    auto on_det = [&](const std::string&, const Envelope& e){
        Detection d = e.as<Detection>();
        if (!d.world_pos) return;
        std::lock_guard<std::mutex> lk(mtx);
        float t = temp_for(d.hazard);
        std::optional<float> ot = std::isnan(t) ? std::optional<float>{} : std::optional<float>{t};
        auto sp = sensor_pos.find(d.source_device);
        if (sp != sensor_pos.end())
            integrate(sp->second, *d.world_pos, ot);          // 시선 카빙 + 히트 + 열
        else
            map.integrate_hit(*d.world_pos, ot);
    };
    bus.subscribe(Topic::all_detections(), on_det);
    bus.subscribe(Topic::judgment(), on_det);

#ifdef FRIZE_HAVE_PCL
    // 실제 LiDAR 통합 지점:
    //   Livox SDK 콜백에서 각 포인트를 사이트 ENU로 변환 후 아래를 호출(별도 스레드):
    //     std::lock_guard<std::mutex> lk(mtx);
    //     integrate(sensor_pose_enu, point_enu, point_temp_or_nullopt);
    //   integrate()가 점유격자(콕핏 복셀)와 TSDF(콕핏 표면 메쉬)를 동시에 갱신한다.
    LOG->info("PCL 활성 ― LiDAR 포인트클라우드 통합 준비(integrate() 훅 사용)");
#endif

    bus.start();

    using clk = std::chrono::steady_clock;
    auto t_pub = clk::now();
    auto t_mesh = clk::now();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (std::chrono::duration<double>(clk::now()-t_pub).count() >= 1.0) {
            t_pub = clk::now();
            MapPatch patch;
            { std::lock_guard<std::mutex> lk(mtx); patch = map.take_patch(); }
            if (!patch.occupied.empty()) {
                patch.dims = {0,0,0};
                bus.publish(Topic::map(), Envelope::wrap(MessageType::MapPatch, "mapping", patch), 0);
                LOG->info("MapPatch 송출: {} 복셀 (총 {})", patch.occupied.size(), map.size());
            }
        }
        // 주기적 TSDF → 표면 메쉬(glTF) 갱신 → 콕핏 트윈이 매끈한 표면을 로드
        if (twin_mesh && std::chrono::duration<double>(clk::now()-t_mesh).count() >= twin_period) {
            t_mesh = clk::now();
            recon::Mesh m;
            { std::lock_guard<std::mutex> lk(mtx); m = recon::marching_cubes(tsdf); }
            if (!m.v.empty()) {
                m.write_gltf(twin_dir + "/twin.gltf", "twin.bin");
                LOG->info("트윈 메쉬 갱신: 정점 {} 삼각 {} 표면적 {:.0f}m² ({}/twin.gltf)",
                          m.v.size(), m.tris(), m.surface_area(), twin_dir);
            }
        }
    }
    return 0;
}
