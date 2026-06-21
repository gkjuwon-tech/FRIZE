// frize/scout/main.cpp ― 정찰 드론 엣지 SW (Jetson 탑재)
//
// 코어와 버스로 연결: ScoutTelemetry + VideoFrameMeta(키프레임) 송출,
// 명령(recon/move_to/hold/rtl/land) 수신·실행·ACK.
// 비행은 FlightController HAL(sim 기본 / MAVSDK 옵션), 인지는 Perception HAL.
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <thread>

#include "frize/bus.hpp"
#include "frize/protocol.hpp"
#include "frize/geo.hpp"
#include "frize/log.hpp"
#include "frize/scout/flight.hpp"
#include "frize/scout/perception.hpp"

using namespace frize;
static auto LOG = make_logger("scout");

static std::string envs(const char* k, const std::string& d){ const char* v=std::getenv(k); return v?v:d; }
static double envd(const char* k, double d){ const char* v=std::getenv(k); return v?std::atof(v):d; }
static int    envi(const char* k, int d){ const char* v=std::getenv(k); return v?std::atoi(v):d; }

static bool flight_armed_ = false;
static int64_t frame_seq_ = 0;

int main() {
    const std::string device_id = envs("FRIZE_DEVICE_ID", "scout-01");
    const std::string host = envs("FRIZE_MQTT_HOST", "localhost");
    const int port = envi("FRIZE_MQTT_PORT", 1883);
    const std::string backend = envs("FRIZE_FLIGHT_BACKEND", "sim");      // sim | mavsdk
    const std::string mav_url = envs("FRIZE_MAVLINK_URL", "udp://:14540");
    const std::string thermal = envs("FRIZE_THERMAL_DEV", "");           // e.g. /dev/video1
    const std::string rgb     = envs("FRIZE_RGB_DEV", "");
    const std::string sim_fr  = envs("FRIZE_SIM_FRAME", "");             // 샘플 jpeg 경로
    Vec3 home{ envd("FRIZE_HOME_X",0), envd("FRIZE_HOME_Y",0), 0 };

    LOG->info("SCOUT {} 시작 (backend={}, mqtt {}:{})", device_id, backend, host, port);

    // 비행 컨트롤러
    std::unique_ptr<scout::FlightController> flight;
    if (backend == "mavsdk") flight = scout::make_flight_mavsdk(mav_url);
    if (!flight) flight = scout::make_flight_sim(home);
    flight->connect();

    // 인지
    auto perception = scout::make_perception(thermal, rgb, sim_fr);
    perception->open();

    // 버스
    MessageBus bus(device_id, host, port);
    bus.set_device_will(device_id, DeviceType::Scout);

    std::mutex qmtx;
    std::deque<Command> cmdq;
    bus.subscribe(Topic::command(device_id), [&](const std::string&, const Envelope& e){
        std::lock_guard<std::mutex> lk(qmtx);
        cmdq.push_back(e.as<Command>());
    });

    // 맵핑 서비스의 프런티어(미탐사 경계) + 재정찰 목표(오래됐고 뜨거운 곳) 수신.
    //  • 프런티어 → '미탐사' 채우기 (지금까지의 유일한 행동)
    //  • 재정찰   → '이미 본' 화재 구역으로 되돌아가 확산 진행을 재확인 (신규)
    std::mutex fmtx;
    std::vector<Vec3> frontiers_w;
    std::vector<std::pair<Vec3,double>> revisit_w;   // (위치, 우선순위)
    bus.subscribe(Topic::map(), [&](const std::string&, const Envelope& e){
        auto mp = e.as<MapPatch>();
        std::lock_guard<std::mutex> lk(fmtx);
        frontiers_w.clear();
        for (auto& f : mp.frontiers)
            frontiers_w.push_back(Vec3{(f[0]+0.5)*mp.voxel_size_m + mp.origin.x,
                                       (f[1]+0.5)*mp.voxel_size_m + mp.origin.y,
                                       (f[2]+0.5)*mp.voxel_size_m + mp.origin.z});
        // 재정찰 목표는 월드 m 좌표로 직접 옴([x,y,z,priority], 우선순위 내림차순)
        revisit_w.clear();
        for (auto& r : mp.revisit) revisit_w.push_back({Vec3{r[0], r[1], r[2]}, r[3]});
    });
    // 콕핏 페어링: 정찰 드론 능력 광고
    bus.enable_pairing(device_id, DeviceType::Scout, "0.1.0",
                       {"thermal", "rgb", "autonomy", "anchor_dispenser", "investigate_point"});
    bus.start();

    // 프런티어 탐사 상태
    bool exploring=false; Vec3 cur_target{}; bool have_target=false;
    bool investigating=false;   // 명시적 태스킹("저기 먼저 조사해") 진행 중 ― 프런티어 가로채기
    // 재정찰 주기: N개 프런티어마다 1번은 '이미 본' 화재 구역으로 되돌아가 확산 재확인.
    //  미탐사만 좇으면 불이 번지는 걸 놓친다 → 주기적으로 뒤를 돌아본다.
    const int  revisit_every = std::max(1, std::atoi(envs("FRIZE_REVISIT_EVERY","3").c_str()));
    int  frontier_streak = 0;     // 마지막 재정찰 이후 채운 프런티어 수
    bool last_was_revisit = false;
    int anchors_left = std::atoi(envs("FRIZE_ANCHOR_MAG","3").c_str());  // 디스펜서 매거진
    Vec3 last_pos = home; int anchor_seq=0;

    auto ack = [&](const Command& c, CommandStatus s, const std::string& d){
        CommandAck a; a.cmd_id=c.cmd_id; a.device_id=device_id; a.status=s; a.detail=d;
        bus.publish(Topic::command_ack(device_id), Envelope::wrap(MessageType::CommandAck, device_id, a), 1);
    };
    auto exec = [&](const Command& c){
        switch (c.action) {
            case CommandAction::ReconZone: {     // 자율 프런티어 탐사 개시(빈곳 위주)
                if (!flight_armed_) { flight->takeoff(12.0); flight_armed_ = true; }
                exploring = true; have_target = false;
                if (c.params.contains("world_pos")) {     // 시작점 시드(옵션)
                    auto w = c.params["world_pos"];
                    flight->goto_local(Vec3{ w.value("x",0.0), w.value("y",0.0), 12.0 }, 0.0);
                }
                ack(c, CommandStatus::Executing, "자율 정찰(프런티어 탐사) 개시");
                break;
            }
            case CommandAction::MoveTo: {
                exploring = false;
                auto& p = c.params;
                if (p.contains("world_pos")) {
                    auto w = p["world_pos"];
                    Vec3 t{ w.value("x",0.0), w.value("y",0.0), w.value("z",home.z+12.0) };
                    if (t.z < 1.0) t.z = 12.0;
                    if (!flight_armed_) { flight->takeoff(t.z); flight_armed_ = true; }
                    flight->goto_local(t, 0.0);
                    ack(c, CommandStatus::Executing, "이동 중");
                } else ack(c, CommandStatus::Failed, "목표 좌표 없음");
                break;
            }
            case CommandAction::InvestigatePoint: {  // 명시적 태스킹: "저기 먼저 조사해!"
                auto& p = c.params;
                if (p.contains("world_pos")) {
                    auto w = p["world_pos"];
                    Vec3 t{ w.value("x",0.0), w.value("y",0.0), w.value("z",0.0) };
                    t.z = std::max(t.z, 3.0) + 2.0;                 // 안전 고도
                    if (!flight_armed_) { flight->takeoff(t.z); flight_armed_ = true; }
                    cur_target = t; have_target = true; investigating = true;
                    // 조사 후 자율탐사 재개 여부(기본 재개)
                    if (!p.value("then_resume", true)) exploring = false; else exploring = true;
                    flight->goto_local(t, geo::bearing_deg(home, t)*geo::DEG2RAD);
                    ack(c, CommandStatus::Executing, "지정 지점 우선 조사 → 자율탐사 재개");
                } else ack(c, CommandStatus::Failed, "목표 좌표 없음");
                break;
            }
            case CommandAction::DeployAnchor: {   // UWB 측위 비콘 투하(실내 측위망 구축)
                if (anchors_left <= 0) { ack(c, CommandStatus::Failed, "앵커 매거진 비었음"); break; }
                Vec3 drop = last_pos;
                if (c.params.contains("world_pos")) { auto w=c.params["world_pos"];
                    drop = Vec3{ w.value("x",last_pos.x), w.value("y",last_pos.y), 0.0 }; }
                else drop.z = 0.0;            // 바닥으로 낙하
                --anchors_left;
                // 투하 위치를 Detection으로 송출 → 월드모델/트윈에 앵커 점으로 표시
                Detection d; d.det_id = device_id + "-anchor-" + std::to_string(++anchor_seq);
                d.source_device = device_id; d.hazard = HazardClass::Unknown; d.severity = Severity::Info;
                d.confidence = 1.0; d.label = "uwb_anchor"; d.rationale = "UWB 측위 비콘 투하";
                d.world_pos = drop;
                bus.publish(Topic::detection(device_id), Envelope::wrap(MessageType::Detection, device_id, d), 1);
                ack(c, CommandStatus::Done, "앵커 투하(잔여 " + std::to_string(anchors_left) + "발)");
                break;
            }
            case CommandAction::Hold: exploring=false; investigating=false; flight->hold(); ack(c, CommandStatus::Done, "호버"); break;
            case CommandAction::Rtl:  exploring=false; flight->return_to_launch(); ack(c, CommandStatus::Executing, "복귀"); break;
            case CommandAction::Land: exploring=false; flight->land(); ack(c, CommandStatus::Executing, "착륙"); break;
            default: ack(c, CommandStatus::Failed, "드론 미지원 명령"); break;
        }
    };

    using clock = std::chrono::steady_clock;
    auto t_prev = clock::now();
    auto t_tel = t_prev, t_hb = t_prev, t_frame = t_prev;
    std::atomic<bool> run{true};

    while (run) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto now = clock::now();
        double dt = std::chrono::duration<double>(now - t_prev).count(); t_prev = now;

        // 명령 처리
        for (;;) {
            Command c;
            { std::lock_guard<std::mutex> lk(qmtx); if (cmdq.empty()) break; c = cmdq.front(); cmdq.pop_front(); }
            ack(c, CommandStatus::Acked, "수신");
            exec(c);
        }

        auto st = flight->poll(dt);
        flight_armed_ = st.armed; last_pos = st.position;

        // 자율 탐사: 목표 도달 or 미설정이면 다음 목표 선택.
        //  핵심 변경 ― 미탐사 프런티어만 좇지 않는다. revisit_every 회마다 한 번은
        //  '이미 본' 화재 구역(재정찰 목표)으로 되돌아가 불이 지금 어떻게 번지는지
        //  재확인한다. 그래야 트윈의 화재 상태가 과거가 아니라 '현재'가 된다.
        if (exploring) {
            if (!have_target || geo::horizontal_distance(st.position, cur_target) < 2.5) {
                if (investigating) investigating = false;   // 지정 지점 조사 완료 → 자율탐사 재개
                if (have_target) { if (last_was_revisit) frontier_streak = 0; else ++frontier_streak; }
                std::lock_guard<std::mutex> lk(fmtx);

                bool found=false; Vec3 bp{}; bool picked_revisit=false;
                // 재정찰 차례 + 재정찰 목표가 있으면, 우선순위 최상위로 되돌아간다.
                if (frontier_streak >= revisit_every && !revisit_w.empty()) {
                    double bestp = -1e18;
                    for (auto& rv : revisit_w) {
                        double d = geo::distance(st.position, rv.first);
                        if (d < 1.0) continue;                 // 바로 코앞은 스킵
                        if (rv.second > bestp) { bestp = rv.second; bp = rv.first; found = true; }
                    }
                    if (found) picked_revisit = true;
                }
                // 평소(또는 재정찰 목표 없음)엔 최근접 미탐사 프런티어.
                if (!found) {
                    double best = 1e18;
                    for (auto& f : frontiers_w) {
                        double d = geo::distance(st.position, f);
                        if (d > 1.5 && d < best) { best=d; bp=f; found=true; }   // 최근접 프런티어(그리디)
                    }
                }
                if (found) {
                    cur_target = bp; cur_target.z = std::max(bp.z, 3.0) + 2.0;   // 안전 고도
                    have_target = true; last_was_revisit = picked_revisit;
                    if (picked_revisit) { frontier_streak = 0;
                        LOG->info("재정찰: 화재 구역 ({:.1f},{:.1f},{:.1f})로 복귀 → 확산 재확인",
                                  bp.x, bp.y, bp.z); }
                    flight->goto_local(cur_target, geo::bearing_deg(st.position, cur_target)*geo::DEG2RAD);
                }
            }
        }

        // 텔레메트리 5Hz
        if (std::chrono::duration<double>(now - t_tel).count() >= 0.2) {
            t_tel = now;
            ScoutTelemetry t;
            t.device_id = device_id;
            t.pose.position = st.position;
            t.pose.orientation = geo::quat_from_yaw(st.yaw);
            t.battery_pct = st.battery_pct;
            t.flight_mode = st.mode; t.armed = st.armed;
            t.altitude_m = st.altitude_m; t.groundspeed_ms = st.groundspeed_ms;
            t.gps_fix = st.gps_fix; t.satellites = st.satellites;
            t.motor_count_ok = st.motors_ok; t.flight_time_remaining_s = st.flight_time_remaining_s;
            t.gas = perception->read_gas();
            t.state = (t.battery_pct < 20 || t.motor_count_ok < 6) ? DeviceState::Degraded : DeviceState::Online;
            bus.publish(Topic::telemetry(device_id), Envelope::wrap(MessageType::Telemetry, device_id, t), 0);
        }
        // 하트비트 1Hz
        if (std::chrono::duration<double>(now - t_hb).count() >= 1.0) {
            t_hb = now;
            Heartbeat hb; hb.device_id=device_id; hb.device_type=DeviceType::Scout;
            bus.publish(Topic::heartbeat(device_id), Envelope::wrap(MessageType::Heartbeat, device_id, hb), 1, true);
        }
        // 키프레임 0.5Hz → 코어 VLM 판단
        if (std::chrono::duration<double>(now - t_frame).count() >= 2.0) {
            t_frame = now;
            if (auto f = perception->grab()) {
                VideoFrameMeta vm;
                vm.device_id = device_id; vm.seq = ++frame_seq_; vm.stream = f->stream;
                vm.width = f->width; vm.height = f->height; vm.codec = "mjpeg";
                vm.pose.position = st.position; vm.pose.orientation = geo::quat_from_yaw(st.yaw);
                vm.jpeg_b64 = f->jpeg_b64;
                bus.publish(Topic::video_meta(device_id), Envelope::wrap(MessageType::VideoMeta, device_id, vm), 0);
            }
        }
    }
    bus.stop();
    return 0;
}
