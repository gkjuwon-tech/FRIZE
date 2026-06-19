// frize/mapping/main.cpp ― 3D 맵핑/SLAM 융합 서비스
//
// 입력: 디바이스 텔레메트리(자세) + VLM 감지(world_pos/위험) [+ 실제 LiDAR]
// 출력: MapPatch (점유격자 + 열 복셀) → 콕핏 3D 트윈.
// 실제 운용: Livox Mid-360 포인트클라우드를 integrate_ray로 누적(PCL 경로).
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>

#include "frize/bus.hpp"
#include "frize/protocol.hpp"
#include "frize/log.hpp"
#include "frize/mapping/voxel_map.hpp"

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
        map.integrate_ray(Vec3{p.x,p.y,p.z+1.5}, Vec3{p.x,p.y,p.z});
    });

    // 감지(world_pos) → 점유 + 열 (디바이스 감지 + 코어 VLM 판단 둘 다)
    auto on_det = [&](const std::string&, const Envelope& e){
        Detection d = e.as<Detection>();
        if (!d.world_pos) return;
        std::lock_guard<std::mutex> lk(mtx);
        float t = temp_for(d.hazard);
        auto sp = sensor_pos.find(d.source_device);
        if (sp != sensor_pos.end())
            map.integrate_ray(sp->second, *d.world_pos);     // 시선 카빙 + 히트
        map.integrate_hit(*d.world_pos, std::isnan(t) ? std::optional<float>{} : std::optional<float>{t});
    };
    bus.subscribe(Topic::all_detections(), on_det);
    bus.subscribe(Topic::judgment(), on_det);

#ifdef FRIZE_HAVE_PCL
    // 실제 LiDAR 통합 지점:
    //   Livox SDK 콜백에서 각 포인트를 사이트 ENU로 변환 후
    //   map.integrate_ray(sensor_pose, point) 호출. (별도 스레드)
    LOG->info("PCL 활성 ― LiDAR 포인트클라우드 통합 준비");
#endif

    bus.start();

    using clk = std::chrono::steady_clock;
    auto t_pub = clk::now();
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
    }
    return 0;
}
