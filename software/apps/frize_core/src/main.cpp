// frize/core/main.cpp ― FRIZE Core 커널 엔트리포인트
//
// 스레드 모델:
//   - 메인 스레드: Crow HTTP/WS 서버(콕핏 연결)
//   - 버스 스레드: Paho 콜백(디바이스 메시지 수신) → 짧게 상태 갱신
//   - 워커 스레드: 워치독(오프라인 스윕) + 만료 + 월드 스냅샷 브로드캐스트
//   - VLM 풀  : 영상 프레임 판단(블로킹 HTTP) → 감지/경보 반영
// 공유 상태(registry/world/router)는 state_mtx_ 로 보호한다.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include <crow.h>

#include "frize/bus.hpp"
#include "frize/protocol.hpp"
#include "frize/geo.hpp"
#include "frize/log.hpp"
#include "frize/core/config.hpp"
#include "frize/core/registry.hpp"
#include "frize/core/world_model.hpp"
#include "frize/core/safety.hpp"
#include "frize/core/router.hpp"
#include "frize/core/vlm_client.hpp"

using namespace frize;
using namespace frize::core;
static auto LOG = make_logger("core");

// ----------------- 소형 고정 스레드풀(VLM 동시성 제한) -----------------
class ThreadPool {
public:
    explicit ThreadPool(int n) {
        for (int i = 0; i < n; ++i) workers_.emplace_back([this]{ loop(); });
    }
    ~ThreadPool() {
        { std::lock_guard<std::mutex> lk(m_); stop_ = true; } cv_.notify_all();
        for (auto& t : workers_) if (t.joinable()) t.join();
    }
    void submit(std::function<void()> job) {
        { std::lock_guard<std::mutex> lk(m_); if (q_.size() < 64) q_.push_back(std::move(job)); }
        cv_.notify_one();
    }
private:
    void loop() {
        for (;;) {
            std::function<void()> job;
            { std::unique_lock<std::mutex> lk(m_);
              cv_.wait(lk, [this]{ return stop_ || !q_.empty(); });
              if (stop_ && q_.empty()) return;
              job = std::move(q_.front()); q_.pop_front(); }
            try { job(); } catch (const std::exception& e) { LOG->error("vlm job error: {}", e.what()); }
        }
    }
    std::vector<std::thread> workers_;
    std::deque<std::function<void()>> q_;
    std::mutex m_; std::condition_variable cv_; bool stop_{false};
};

// ----------------- 위치 추정: 디바이스 포즈 + bbox → 세계 좌표(근사) -----------------
// 정밀 위치는 mapping 서비스가 LiDAR로 정제한다. 여기선 초기 추정.
static std::optional<Vec3> estimate_world_pos(const Pose& pose, const Detection& d) {
    double yaw = geo::yaw_from_quat(pose.orientation);   // rad, 북 기준
    double bearing = yaw;
    if (d.bbox) {
        const double FOV = 50.0 * geo::DEG2RAD;           // 열화상 수평 화각 근사
        double cx = (*d.bbox)[0] + (*d.bbox)[2] * 0.5;    // 정규화 중심 x
        bearing += (cx - 0.5) * FOV;
    }
    double range;
    switch (d.hazard) {
        case HazardClass::Person: case HazardClass::DownedPerson: range = 4.0; break;
        case HazardClass::Fire:   case HazardClass::Hotspot:      range = 6.0; break;
        case HazardClass::Structural:                            range = 8.0; break;
        default: range = 5.0;
    }
    Vec3 dir{ std::sin(bearing), std::cos(bearing), 0.0 };
    return Vec3{ pose.position.x + dir.x*range, pose.position.y + dir.y*range, pose.position.z };
}

static std::string gas_summary(const GasReading& g) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "CO %.0fppm, O2 %.1f%%, LEL %.0f%%, H2S %.0fppm",
                  g.co_ppm, g.o2_vol_pct, g.lel_pct, g.h2s_ppm);
    return buf;
}

// ============================ Kernel ============================
class Kernel {
public:
    Kernel(const Config& cfg)
        : cfg_(cfg),
          bus_("frize-core", cfg.mqtt_host, cfg.mqtt_port, cfg.mqtt_user, cfg.mqtt_pass),
          registry_(cfg.device_offline_after_s),
          world_(registry_, GeoPoint{cfg.site_lat, cfg.site_lon, cfg.site_alt}),
          interlock_(registry_, world_, cfg.interlock_enabled),
          router_(bus_, interlock_),
          vlm_(cfg.anthropic_api_key, cfg.vlm_model, cfg.vlm_min_interval_s, cfg.vlm_enabled),
          vlm_pool_(cfg.vlm_max_concurrency) {}

    void start() {
        // 버스 구독
        bus_.subscribe(Topic::all_heartbeats(), [this](auto& t, auto& e){ on_heartbeat(t,e); });
        bus_.subscribe(Topic::all_telemetry(),  [this](auto& t, auto& e){ on_telemetry(t,e); });
        bus_.subscribe(Topic::all_detections(), [this](auto& t, auto& e){ on_detection(t,e); });
        bus_.subscribe(Topic::all_video_meta(), [this](auto& t, auto& e){ on_video_meta(t,e); });
        bus_.subscribe(Topic::all_acks(),       [this](auto& t, auto& e){ on_ack(t,e); });
        // 3D 디지털 트윈: 맵핑 패치(점유/열/프런티어)를 콕핏 WS로 그대로 포워딩
        bus_.subscribe(Topic::map(),            [this](auto&, auto& e){ ws_broadcast(e.to_string()); });
        bus_.start();
        worker_ = std::thread([this]{ worker_loop(); });
    }
    void stop() {
        running_ = false;
        if (worker_.joinable()) worker_.join();
        bus_.stop();
    }

    // ---- HTTP 핸들러용 접근자(락 관리) ----
    json api_world() {
        std::lock_guard<std::recursive_mutex> lk(state_mtx_);
        return world_.snapshot();
    }
    json api_rescue_priority() {
        std::lock_guard<std::recursive_mutex> lk(state_mtx_);
        json arr = json::array();
        for (auto& d : world_.rescue_priority()) arr.push_back(d);
        return arr;
    }
    json api_submit_command(const json& body) {
        Command c = body.get<Command>();
        if (c.cmd_id.empty()) c.cmd_id = Envelope::gen_id("cmd");
        std::lock_guard<std::recursive_mutex> lk(state_mtx_);
        auto t = router_.submit(std::move(c));
        return t.to_json();
    }
    json api_confirm(const std::string& cmd_id, const std::string& by) {
        std::lock_guard<std::recursive_mutex> lk(state_mtx_);
        auto t = router_.confirm(cmd_id, by);
        return t ? t->to_json() : json{{"error","unknown cmd_id"}};
    }
    void api_set_site_origin(const GeoPoint& g) {
        std::lock_guard<std::recursive_mutex> lk(state_mtx_);
        world_.set_site_origin(g);
    }
    json api_ack_alert(const std::string& alert_id) {
        std::lock_guard<std::recursive_mutex> lk(state_mtx_);
        return json{{"ok", world_.ack_alert(alert_id)}};
    }

    // ---- WS 클라이언트(콕핏) 관리 ----
    void ws_add(crow::websocket::connection* c) { std::lock_guard<std::mutex> lk(ws_mtx_); ws_.insert(c); }
    void ws_remove(crow::websocket::connection* c) { std::lock_guard<std::mutex> lk(ws_mtx_); ws_.erase(c); }
    void ws_on_message(crow::websocket::connection& c, const std::string& data);

private:
    // ---------- 버스 핸들러 ----------
    void on_heartbeat(const std::string&, const Envelope& e) {
        auto hb = e.as<Heartbeat>();
        std::lock_guard<std::recursive_mutex> lk(state_mtx_);
        registry_.on_heartbeat(hb);
    }
    void on_telemetry(const std::string& topic, const Envelope& e) {
        const std::string dt = e.payload.value("device_type", std::string("visor"));
        std::lock_guard<std::recursive_mutex> lk(state_mtx_);
        if (dt == "scout") {
            auto t = e.as<ScoutTelemetry>(); registry_.on_scout_telemetry(t); scout_alerts(t);
        } else if (dt == "apparatus") {
            auto t = e.as<ApparatusTelemetry>(); registry_.on_apparatus_telemetry(t); apparatus_alerts(t);
        } else {
            auto t = e.as<VisorTelemetry>(); registry_.on_visor_telemetry(t); visor_alerts(t);
        }
    }
    void on_detection(const std::string&, const Envelope& e) {
        auto d = e.as<Detection>();
        std::lock_guard<std::recursive_mutex> lk(state_mtx_);
        if (!d.world_pos) {
            if (auto* rec = registry_.get(d.source_device))
                d.world_pos = estimate_world_pos(rec->pose, d);
        }
        world_.add_detection(d);
        if (severity_rank(d.severity) >= severity_rank(Severity::High)) raise_hazard_alert(d);
    }
    void on_video_meta(const std::string&, const Envelope& e) {
        auto vm = e.as<VideoFrameMeta>();
        if (!vm.jpeg_b64 || !vlm_.should_judge(vm.device_id)) return;
        // VLM 판단은 풀에서(블로킹 HTTP). 결과를 락 잡고 반영.
        std::string dev = vm.device_id, jpeg = *vm.jpeg_b64, stream = vm.stream;
        Pose pose = vm.pose;
        std::string gs;
        { std::lock_guard<std::recursive_mutex> lk(state_mtx_);
          if (auto* rec = registry_.get(dev); rec && rec->extra.contains("gas"))
              gs = gas_summary(rec->extra["gas"].get<GasReading>()); }
        vlm_pool_.submit([this, dev, jpeg, stream, pose, gs]{
            VlmContext ctx; ctx.stream = stream; ctx.gas_summary = gs;
            auto dets = vlm_.judge(dev, jpeg, ctx);
            std::lock_guard<std::recursive_mutex> lk(state_mtx_);
            for (auto& d : dets) {
                if (!d.world_pos) d.world_pos = estimate_world_pos(pose, d);
                world_.add_detection(d);
                bus_.publish(Topic::judgment(), Envelope::wrap(MessageType::Detection, "core", d), 0);
                if (severity_rank(d.severity) >= severity_rank(Severity::High)) raise_hazard_alert(d);
            }
        });
    }
    void on_ack(const std::string&, const Envelope& e) {
        auto a = e.as<CommandAck>();
        std::lock_guard<std::recursive_mutex> lk(state_mtx_);
        router_.on_ack(a);
    }

    // ---------- 경보 생성 ----------
    void raise_alert(AlertKind k, Severity s, const std::string& title, const std::string& msg,
                     std::optional<std::string> dev = std::nullopt,
                     std::optional<Vec3> pos = std::nullopt) {
        Alert a; a.alert_id = Envelope::gen_id("alert"); a.kind = k; a.severity = s;
        a.title = title; a.message = msg; a.device_id = dev; a.world_pos = pos;
        world_.raise_alert(a);
        bus_.publish(Topic::alert(), Envelope::wrap(MessageType::Alert, "core", a), 1);
    }
    void raise_hazard_alert(const Detection& d) {
        raise_alert(AlertKind::Hazard, d.severity,
                    "위험감지: " + d.label, d.rationale, d.source_device, d.world_pos);
    }
    void visor_alerts(const VisorTelemetry& t) {
        for (auto& f : t.gas.danger_flags())
            if (f.size() >= 4 && f.substr(f.size()-4) == "IDLH")
                raise_alert(AlertKind::Gas, Severity::Critical, "치명가스: " + f,
                            t.device_id + " 주변 IDLH 초과", t.device_id, t.pose.position);
        if (t.vitals.motion_state == "fall_detected")
            raise_alert(AlertKind::Vitals, Severity::Critical, "대원 낙상",
                        (t.wearer_callsign?*t.wearer_callsign:t.device_id) + " 낙상 감지", t.device_id, t.pose.position);
        if (t.vitals.air_pressure_bar && *t.vitals.air_pressure_bar < 55.0)
            raise_alert(AlertKind::Vitals, Severity::High, "SCBA 잔압 부족",
                        "잔압 " + std::to_string((int)*t.vitals.air_pressure_bar) + "bar ― 후퇴 검토",
                        t.device_id, t.pose.position);
        if (t.battery_pct < 10.0)
            raise_alert(AlertKind::Device, Severity::High, "고글 배터리 부족",
                        t.device_id + " " + std::to_string((int)t.battery_pct) + "%", t.device_id);
    }
    void scout_alerts(const ScoutTelemetry& t) {
        if (t.motor_count_ok < 6)
            raise_alert(AlertKind::Device, Severity::High, "드론 모터 이상",
                        t.device_id + " 모터 " + std::to_string(t.motor_count_ok) + "/6 동작", t.device_id);
        if (t.battery_pct < 20.0)
            raise_alert(AlertKind::Device, Severity::High, "드론 배터리 부족",
                        t.device_id + " " + std::to_string((int)t.battery_pct) + "% ― 복귀 검토", t.device_id);
        for (auto& f : t.gas.danger_flags())
            if (f == "LEL_EXPLOSIVE")
                raise_alert(AlertKind::Gas, Severity::Critical, "폭발 위험(LEL)",
                            t.device_id + " 측정 폭발하한 초과", t.device_id, t.pose.position);
    }

    void apparatus_alerts(const ApparatusTelemetry& t) {
        if (t.state == DeviceState::Critical || t.mech_state == "fault")
            raise_alert(AlertKind::Device, Severity::High, "장비 이상",
                        t.device_id + " 동작 이상(" + t.mech_state + ")", t.device_id, t.pose.position);
        if (t.battery_pct < 15.0)
            raise_alert(AlertKind::Device, Severity::Low, "장비 배터리 부족",
                        t.device_id + " " + std::to_string((int)t.battery_pct) + "%", t.device_id);
    }

    // ---------- 워커 루프 ----------
    void worker_loop() {
        using namespace std::chrono;
        while (running_) {
            std::this_thread::sleep_for(milliseconds(500));
            json snap;
            {
                std::lock_guard<std::recursive_mutex> lk(state_mtx_);
                for (auto& id : registry_.sweep_offline())
                    raise_alert(AlertKind::Device, Severity::High, "디바이스 연결 끊김",
                                id + " 오프라인(통신 두절)", id);
                world_.expire();
                snap = world_.snapshot();
            }
            // 콕핏 WS 브로드캐스트
            auto env = Envelope{}; env.type = MessageType::WorldSnapshot; env.source="core"; env.payload = snap;
            const std::string msg = env.to_string();
            std::lock_guard<std::mutex> lk(ws_mtx_);
            for (auto* c : ws_) c->send_text(msg);
        }
    }

    Config cfg_;
    MessageBus bus_;
    Registry registry_;
    WorldModel world_;
    SafetyInterlock interlock_;
    CommandRouter router_;
    VlmClient vlm_;
    ThreadPool vlm_pool_;
    std::recursive_mutex state_mtx_;
    std::thread worker_;
    std::atomic<bool> running_{true};

    std::set<crow::websocket::connection*> ws_;
    std::mutex ws_mtx_;
};

void Kernel::ws_on_message(crow::websocket::connection& c, const std::string& data) {
    json j;
    try { j = json::parse(data); } catch (...) { return; }
    const std::string kind = j.value("kind", std::string());
    if (kind == "command") {
        auto r = api_submit_command(j.at("command"));
        c.send_text(json{{"kind","command_result"},{"result",r}}.dump());
    } else if (kind == "confirm") {
        auto r = api_confirm(j.value("cmd_id",std::string()), j.value("by",std::string("cockpit")));
        c.send_text(json{{"kind","command_result"},{"result",r}}.dump());
    } else if (kind == "ack_alert") {
        api_ack_alert(j.value("alert_id", std::string()));
    } else if (kind == "set_site_origin") {
        api_set_site_origin(j.at("origin").get<GeoPoint>());
    }
}

// ============================ main ============================
int main() {
    Config cfg;
    LOG->info("FRIZE Core 시작 ― MQTT {}:{}, HTTP {}:{}, VLM {}",
              cfg.mqtt_host, cfg.mqtt_port, cfg.http_host, cfg.http_port,
              cfg.vlm_enabled && !cfg.anthropic_api_key.empty() ? "ON" : "OFF");

    Kernel kernel(cfg);
    kernel.start();

    crow::SimpleApp app;
    app.loglevel(crow::LogLevel::Warning);

    CROW_ROUTE(app, "/healthz")([]{ return crow::response(200, "ok"); });

    CROW_ROUTE(app, "/api/world")([&]{
        return crow::response(kernel.api_world().dump());
    });
    CROW_ROUTE(app, "/api/rescue_priority")([&]{
        return crow::response(kernel.api_rescue_priority().dump());
    });
    CROW_ROUTE(app, "/api/command").methods("POST"_method)([&](const crow::request& req){
        try { return crow::response(kernel.api_submit_command(json::parse(req.body)).dump()); }
        catch (const std::exception& e) { return crow::response(400, std::string("bad request: ")+e.what()); }
    });
    CROW_ROUTE(app, "/api/command/<string>/confirm").methods("POST"_method)
        ([&](const crow::request& req, const std::string& id){
            std::string by = "cockpit";
            try { by = json::parse(req.body).value("by", by); } catch (...) {}
            return crow::response(kernel.api_confirm(id, by).dump());
        });
    CROW_ROUTE(app, "/api/site_origin").methods("POST"_method)([&](const crow::request& req){
        try { kernel.api_set_site_origin(json::parse(req.body).get<GeoPoint>());
              return crow::response(200, "ok"); }
        catch (const std::exception& e) { return crow::response(400, e.what()); }
    });

    CROW_WEBSOCKET_ROUTE(app, "/ws/cockpit")
        .onopen([&](crow::websocket::connection& c){ kernel.ws_add(&c);
                LOG->info("콕핏 연결"); })
        .onclose([&](crow::websocket::connection& c, const std::string&){ kernel.ws_remove(&c); })
        .onmessage([&](crow::websocket::connection& c, const std::string& data, bool /*is_bin*/){
                kernel.ws_on_message(c, data); });

    app.bindaddr(cfg.http_host).port(cfg.http_port).multithreaded().run();

    kernel.stop();
    return 0;
}
