// frize/cockpit/link.hpp ― 콕핏 측 프로덕션 통신 링크
//
// 목업/시뮬 렌더와 분리된, "진짜 돌아가는" 콕핏 클라이언트.
//  - MQTT(MessageBus) 위에서 디바이스를 발견(하트비트)하고
//  - 고글/드론/벤트를 페어링(PairRequest → PairGrant 핸드셰이크)하며
//  - 라이브 텔레메트리(위치/배터리/가스)를 추적하고
//  - 페어링된 디바이스에만 명령을 내린다.
//
// 헤더 온리 — bus.hpp 프리미티브(subscribe/publish)만으로 구현.
// 콘솔 라이브 앱과 ImGui 콕핏이 동일 코드를 공유한다(SSOT).
#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "frize/bus.hpp"
#include "frize/protocol.hpp"
#include "frize/schemas.hpp"

namespace frize::cockpit {

enum class PairState { Unpaired, Pairing, Paired, Failed };

inline const char* pair_state_str(PairState s) {
    switch (s) {
        case PairState::Unpaired: return "unpaired";
        case PairState::Pairing:  return "pairing";
        case PairState::Paired:   return "paired";
        case PairState::Failed:   return "failed";
    }
    return "?";
}

// 콕핏이 보는 디바이스 1대의 라이브 상태.
struct DeviceView {
    std::string device_id;
    DeviceType  type{DeviceType::Visor};
    DeviceState state{DeviceState::Offline};
    std::string fw_version;
    std::vector<std::string> capabilities;
    PairState   pair{PairState::Unpaired};
    double last_heartbeat{0};
    double last_telemetry{0};

    // 라이브 텔레메트리 스냅샷(텔레메트리 종류 무관 공통 필드)
    bool   has_pose{false};
    Vec3   pos{};
    double battery_pct{-1};
    GasReading gas{};

    bool online() const { return state != DeviceState::Offline; }
    // 최근 N초 내 하트비트가 있었나(워치독)
    bool fresh(double now, double timeout_s = 6.0) const {
        return last_heartbeat > 0 && (now - last_heartbeat) < timeout_s;
    }
};

class CockpitLink {
public:
    CockpitLink(std::string console_id,
                std::string host = "localhost",
                int port = 1883)
        : console_id_(std::move(console_id)),
          session_id_(Envelope::gen_id("sess")),
          bus_(console_id_, host, port) {}

    // 콜백 등록 후 연결. 페어링 승인/디바이스 변화 시 on_change_ 호출.
    void start() {
        bus_.set_device_will(console_id_, DeviceType::Cockpit);

        // 1) 디바이스 발견 ― 모든 하트비트 수신
        bus_.subscribe(Topic::all_heartbeats(), [this](const std::string&, const Envelope& e) {
            Heartbeat hb;
            try { hb = e.as<Heartbeat>(); } catch (...) { return; }
            {
                std::lock_guard<std::mutex> lk(mtx_);
                auto& d = dev_(hb.device_id);
                d.type = hb.device_type;
                d.state = hb.state;
                d.fw_version = hb.fw_version;
                d.last_heartbeat = now_s();
            }
            notify();
        });

        // 2) 라이브 텔레메트리 ― 종류 무관 공통 필드만 추출
        bus_.subscribe(Topic::all_telemetry(), [this](const std::string&, const Envelope& e) {
            ingest_telemetry(e);
            notify();
        });

        // 3) 페어링 승인 ― 내 세션의 PairGrant 만 반영
        bus_.subscribe(Topic::all_pair_grants(), [this](const std::string&, const Envelope& e) {
            if (e.type != MessageType::PairGrant) return;
            PairGrant g;
            try { g = e.as<PairGrant>(); } catch (...) { return; }
            if (!g.session_id.empty() && g.session_id != session_id_) return;  // 다른 콕핏 무시
            {
                std::lock_guard<std::mutex> lk(mtx_);
                auto& d = dev_(g.device_id);
                d.capabilities = g.capabilities;
                d.fw_version   = g.fw_version;
                d.type         = g.device_type;
                d.pair         = g.accepted ? PairState::Paired : PairState::Failed;
            }
            notify();
        });

        bus_.start();
    }

    void on_change(std::function<void()> cb) { on_change_ = std::move(cb); }

    // 페어링 요청 발행(콕핏 → 디바이스). 디바이스가 PairGrant 로 응답한다.
    void pair(const std::string& device_id) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            dev_(device_id).pair = PairState::Pairing;
        }
        PairRequest req;
        req.device_id  = device_id;
        req.console_id = console_id_;
        req.session_id = session_id_;
        bus_.publish(Topic::pair(device_id),
                     Envelope::wrap(MessageType::PairRequest, console_id_, req), 1, false);
    }

    // 발견된(하트비트 있는) 모든 디바이스를 일괄 페어링.
    int pair_all() {
        std::vector<std::string> ids;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            for (auto& [id, d] : devs_)
                if (d.last_heartbeat > 0 && d.pair != PairState::Paired) ids.push_back(id);
        }
        for (auto& id : ids) pair(id);
        return static_cast<int>(ids.size());
    }

    // 명령 발행 ― 페어링된 디바이스에만 허용(안전).
    bool command(const std::string& device_id, const Command& c) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = devs_.find(device_id);
            if (it == devs_.end() || it->second.pair != PairState::Paired) return false;
        }
        bus_.publish(Topic::command(device_id),
                     Envelope::wrap(MessageType::Command, console_id_, c), 1, false);
        return true;
    }

    // AR 큐를 고글에 직접 발행(페어링된 visor 한정).
    bool ar_cue(const std::string& device_id, const ArCue& cue) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = devs_.find(device_id);
            if (it == devs_.end() || it->second.pair != PairState::Paired) return false;
        }
        bus_.publish(Topic::ar_cue(device_id),
                     Envelope::wrap(MessageType::ArCue, console_id_, cue), 1, false);
        return true;
    }

    std::vector<DeviceView> devices() const {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<DeviceView> out;
        out.reserve(devs_.size());
        for (auto& [id, d] : devs_) out.push_back(d);
        return out;
    }

    int paired_count() const {
        std::lock_guard<std::mutex> lk(mtx_);
        int n = 0;
        for (auto& [id, d] : devs_) if (d.pair == PairState::Paired) ++n;
        return n;
    }

    bool connected() const { return bus_.connected(); }
    const std::string& session_id() const { return session_id_; }
    const std::string& console_id() const { return console_id_; }

private:
    DeviceView& dev_(const std::string& id) {
        auto& d = devs_[id];
        if (d.device_id.empty()) d.device_id = id;
        return d;
    }

    void notify() { if (on_change_) on_change_(); }

    // 텔레메트리는 디바이스 종류마다 구조가 다르다. 콕핏 로스터엔 공통 필드만
    // 필요하므로 raw JSON 에서 안전하게 긁어온다(스키마 결합도 최소화).
    void ingest_telemetry(const Envelope& e) {
        const json& p = e.payload;
        std::lock_guard<std::mutex> lk(mtx_);
        auto& d = dev_(e.source);
        d.last_telemetry = now_s();
        if (p.contains("device_type")) {
            try { d.type = p.at("device_type").get<DeviceType>(); } catch (...) {}
        }
        if (p.contains("state")) {
            try { d.state = p.at("state").get<DeviceState>(); } catch (...) {}
        }
        if (p.contains("battery_pct") && p["battery_pct"].is_number())
            d.battery_pct = p["battery_pct"].get<double>();
        if (p.contains("pose") && p["pose"].contains("position")) {
            try {
                d.pos = p["pose"]["position"].get<Vec3>();
                d.has_pose = true;
            } catch (...) {}
        }
        if (p.contains("gas")) {
            try { d.gas = p.at("gas").get<GasReading>(); } catch (...) {}
        }
    }

    std::string console_id_;
    std::string session_id_;
    mutable std::mutex mtx_;
    std::map<std::string, DeviceView> devs_;
    std::function<void()> on_change_;
    MessageBus bus_;  // 마지막 멤버: 콜백이 위 멤버를 참조하므로 가장 늦게 소멸
};

}  // namespace frize::cockpit
