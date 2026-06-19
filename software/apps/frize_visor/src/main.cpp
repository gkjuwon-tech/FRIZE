// frize/visor/main.cpp ― 스마트 고글 엣지 SW (Jetson 탑재)
//
// VisorTelemetry(가스/생체/자세) + 키프레임 송출, 지휘 명령 수신 → AR 출력.
// 로컬 안전: IDLH 가스/낙상 시 코어를 기다리지 않고 즉시 대피 AR 표시.
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <thread>
#include <cmath>

#include "frize/bus.hpp"
#include "frize/protocol.hpp"
#include "frize/geo.hpp"
#include "frize/log.hpp"
#include "frize/visor/hal.hpp"

using namespace frize;
static auto LOG = make_logger("visor");

static std::string envs(const char* k, const std::string& d){ const char* v=std::getenv(k); return v?v:d; }
static int    envi(const char* k, int d){ const char* v=std::getenv(k); return v?std::atoi(v):d; }

static int64_t seq_ = 0;

static visor::ArCommand to_ar(const Command& c, const Pose& pose) {
    visor::ArCommand a; a.severity = "high";
    double yaw_deg = geo::yaw_from_quat(pose.orientation) * geo::RAD2DEG;
    auto bearing_to = [&](double x, double y){
        Vec3 t{x,y,pose.position.z};
        double b = geo::bearing_deg(pose.position, t) - yaw_deg;   // 시선 상대 방위
        return std::fmod(b + 360.0, 360.0);
    };
    switch (c.action) {
        case CommandAction::Highlight:
            a.kind="highlight"; a.text="지휘관 표식"; a.severity="info";
            if (c.params.contains("world_pos")) { auto w=c.params["world_pos"]; a.bearing_deg=bearing_to(w.value("x",0.0),w.value("y",0.0)); }
            break;
        case CommandAction::Advance:
            a.kind="advance"; a.text="진입"; a.severity="low";
            if (c.params.contains("world_pos")) { auto w=c.params["world_pos"]; a.bearing_deg=bearing_to(w.value("x",0.0),w.value("y",0.0)); }
            break;
        case CommandAction::ForceEntry:
            a.kind="force_entry"; a.text="강제진입 지점"; a.severity="high";
            if (c.params.contains("world_pos")) { auto w=c.params["world_pos"]; a.bearing_deg=bearing_to(w.value("x",0.0),w.value("y",0.0)); }
            break;
        case CommandAction::Retreat:
            a.kind="retreat"; a.text="후퇴 ― 탈출로"; a.severity="critical";
            a.bearing_deg = bearing_to(0,0);   // 출동지점(home) 방향
            break;
        case CommandAction::Rally:
            a.kind="rally"; a.text="집결"; a.severity="low";
            if (c.params.contains("world_pos")) { auto w=c.params["world_pos"]; a.bearing_deg=bearing_to(w.value("x",0.0),w.value("y",0.0)); }
            break;
        case CommandAction::Annotate:
            a.kind="annotate"; a.text=c.params.value("text", std::string("주석")); a.severity="info"; break;
        default:
            a.kind="info"; a.text="명령"; break;
    }
    return a;
}

int main() {
    const std::string device_id = envs("FRIZE_DEVICE_ID", "visor-01");
    const std::string callsign  = envs("FRIZE_CALLSIGN", "ALPHA-1");
    const std::string wearer    = envs("FRIZE_WEARER_ID", "ff-1024");
    const std::string host = envs("FRIZE_MQTT_HOST", "localhost");
    const int port = envi("FRIZE_MQTT_PORT", 1883);
    const std::string serial = envs("FRIZE_SENSOR_SERIAL", "");
    const std::string thermal= envs("FRIZE_THERMAL_DEV", "");
    const std::string sim_fr = envs("FRIZE_SIM_FRAME", "");
    const bool headless = envi("FRIZE_AR_HEADLESS", 1) != 0;

    LOG->info("VISOR {} ({}) 시작", device_id, callsign);

    auto sensors = visor::make_sensors(serial, device_id);
    auto capture = visor::make_capture(thermal, sim_fr);
    auto ar      = visor::make_ar_display(headless);
    sensors->open(); capture->open(); ar->open();

    MessageBus bus(device_id, host, port);
    bus.set_device_will(device_id, DeviceType::Visor);

    std::mutex qmtx; std::deque<Command> cmdq;
    bus.subscribe(Topic::command(device_id), [&](const std::string&, const Envelope& e){
        std::lock_guard<std::mutex> lk(qmtx); cmdq.push_back(e.as<Command>());
    });
    bus.start();

    auto ack = [&](const Command& c, CommandStatus s, const std::string& d){
        CommandAck a; a.cmd_id=c.cmd_id; a.device_id=device_id; a.status=s; a.detail=d;
        bus.publish(Topic::command_ack(device_id), Envelope::wrap(MessageType::CommandAck, device_id, a), 1);
    };

    using clk = std::chrono::steady_clock;
    auto t0=clk::now(); auto t_tel=t0,t_hb=t0,t_frame=t0;
    visor::SensorSample last{};
    bool evac_latched=false;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto now=clk::now();
        double dt=std::chrono::duration<double>(now - t0).count(); t0=now;

        last = sensors->sample(dt);
        ar->tick(dt);

        // 로컬 안전: 코어 대기 없이 즉시 대피 표시
        auto flags = last.gas.danger_flags();
        bool idlh=false; for (auto&f:flags) if (f.size()>=4 && f.substr(f.size()-4)=="IDLH") idlh=true;
        bool fall = last.vitals.motion_state=="fall_detected";
        if ((idlh || fall) && !evac_latched) {
            visor::ArCommand a; a.kind="evacuate"; a.severity="critical";
            a.text = fall ? "낙상 감지 ― 응답하라" : "치명가스 ― 즉시 대피";
            a.bearing_deg = std::fmod(geo::bearing_deg(last.pose.position,{0,0,0})
                              - geo::yaw_from_quat(last.pose.orientation)*geo::RAD2DEG + 360.0, 360.0);
            ar->show(a); evac_latched=true;
        } else if (!idlh && !fall) evac_latched=false;

        // 명령 처리
        for (;;) {
            Command c; { std::lock_guard<std::mutex> lk(qmtx); if (cmdq.empty()) break; c=cmdq.front(); cmdq.pop_front(); }
            ack(c, CommandStatus::Acked, "수신");
            ar->show(to_ar(c, last.pose));
            ack(c, CommandStatus::Done, "AR 표시");
        }

        // 텔레메트리 5Hz
        if (std::chrono::duration<double>(now-t_tel).count()>=0.2) {
            t_tel=now;
            VisorTelemetry t;
            t.device_id=device_id; t.wearer_id=wearer; t.wearer_callsign=callsign;
            t.pose=last.pose; t.battery_pct=last.battery_pct;
            t.gas=last.gas; t.ambient_temp_c=last.ambient_temp_c;
            t.vitals=last.vitals; t.compute_temp_c=last.compute_temp_c;
            t.state = (idlh||fall) ? DeviceState::Critical
                     : (last.battery_pct<15 ? DeviceState::Degraded : DeviceState::Online);
            bus.publish(Topic::telemetry(device_id), Envelope::wrap(MessageType::Telemetry, device_id, t), 0);
        }
        // 하트비트 1Hz
        if (std::chrono::duration<double>(now-t_hb).count()>=1.0) {
            t_hb=now; Heartbeat hb; hb.device_id=device_id; hb.device_type=DeviceType::Visor;
            bus.publish(Topic::heartbeat(device_id), Envelope::wrap(MessageType::Heartbeat, device_id, hb), 1, true);
        }
        // 키프레임 0.5Hz → 코어 VLM
        if (std::chrono::duration<double>(now-t_frame).count()>=2.0) {
            t_frame=now;
            if (auto f = capture->grab()) {
                VideoFrameMeta vm; vm.device_id=device_id; vm.seq=++seq_; vm.stream=f->stream;
                vm.width=f->width; vm.height=f->height; vm.codec="mjpeg";
                vm.pose=last.pose; vm.jpeg_b64=f->jpeg_b64;
                bus.publish(Topic::video_meta(device_id), Envelope::wrap(MessageType::VideoMeta, device_id, vm), 0);
            }
        }
    }
    return 0;
}
