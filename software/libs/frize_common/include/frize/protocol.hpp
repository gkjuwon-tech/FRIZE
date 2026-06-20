// frize/protocol.hpp ― 메시지 프로토콜(토픽 규약 + 봉투)
#pragma once

#include <string>
#include <optional>
#include <vector>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "frize/schemas.hpp"

namespace frize {

inline constexpr const char* SCHEMA_VERSION = "1.0";
inline constexpr const char* ROOT = "frize";

enum class MessageType {
    Heartbeat, Telemetry, Detection, VideoMeta,
    Command, CommandAck, Alert, WorldSnapshot, MapPatch, Judgment, ArCue
};

NLOHMANN_JSON_SERIALIZE_ENUM(MessageType, {
    {MessageType::Heartbeat,"heartbeat"},{MessageType::Telemetry,"telemetry"},
    {MessageType::Detection,"detection"},{MessageType::VideoMeta,"video_meta"},
    {MessageType::Command,"command"},{MessageType::CommandAck,"command_ack"},
    {MessageType::Alert,"alert"},{MessageType::WorldSnapshot,"world_snapshot"},
    {MessageType::MapPatch,"map_patch"},{MessageType::Judgment,"judgment"},
    {MessageType::ArCue,"ar_cue"}})

// 토픽 빌더 ― 문자열 하드코딩 금지, 전부 여기 경유(오타로 인한 사일런트 유실 방지)
struct Topic {
    // 디바이스 → 코어 (상향)
    static std::string heartbeat(const std::string& id) { return std::string(ROOT)+"/device/"+id+"/heartbeat"; }
    static std::string telemetry(const std::string& id) { return std::string(ROOT)+"/device/"+id+"/telemetry"; }
    static std::string detection(const std::string& id) { return std::string(ROOT)+"/device/"+id+"/detection"; }
    static std::string video_meta(const std::string& id){ return std::string(ROOT)+"/device/"+id+"/video"; }
    // 코어 → 디바이스 (하향)
    static std::string command(const std::string& id)    { return std::string(ROOT)+"/command/"+id; }
    static std::string command_ack(const std::string& id){ return std::string(ROOT)+"/command/"+id+"/ack"; }
    static std::string ar_cue(const std::string& id)      { return std::string(ROOT)+"/command/"+id+"/ar"; }
    // 브로드캐스트
    static std::string alert()    { return std::string(ROOT)+"/alert"; }
    static std::string world()    { return std::string(ROOT)+"/world/state"; }
    static std::string map()      { return std::string(ROOT)+"/map/patch"; }
    static std::string judgment() { return std::string(ROOT)+"/judgment"; }
    // 구독용 와일드카드
    static std::string all_heartbeats(){ return std::string(ROOT)+"/device/+/heartbeat"; }
    static std::string all_telemetry() { return std::string(ROOT)+"/device/+/telemetry"; }
    static std::string all_detections(){ return std::string(ROOT)+"/device/+/detection"; }
    static std::string all_video_meta(){ return std::string(ROOT)+"/device/+/video"; }
    static std::string all_acks()      { return std::string(ROOT)+"/command/+/ack"; }

    // frize/device/<id>/... 또는 frize/command/<id>/... 에서 device_id 추출
    static std::optional<std::string> device_id_from(const std::string& topic) {
        std::vector<std::string> parts;
        size_t start = 0, pos;
        while ((pos = topic.find('/', start)) != std::string::npos) {
            parts.push_back(topic.substr(start, pos - start));
            start = pos + 1;
        }
        parts.push_back(topic.substr(start));
        if (parts.size() >= 3 && parts[0] == ROOT && (parts[1] == "device" || parts[1] == "command"))
            return parts[2];
        return std::nullopt;
    }
};

// 버스 위 모든 메시지의 외피
struct Envelope {
    std::string msg_id;
    MessageType type{MessageType::Heartbeat};
    std::string source;          // device_id 또는 "core"
    double ts{now_s()};
    std::string schema_version{SCHEMA_VERSION};
    json payload = json::object();

    std::string to_string() const {
        json j;
        j["msg_id"] = msg_id;
        j["type"] = type;
        j["source"] = source;
        j["ts"] = ts;
        j["schema_version"] = schema_version;
        j["payload"] = payload;
        return j.dump();
    }

    static Envelope from_string(const std::string& raw) {
        json j = json::parse(raw);
        Envelope e;
        e.msg_id = j.value("msg_id", std::string{});
        e.type = j.at("type").get<MessageType>();
        e.source = j.value("source", std::string{});
        e.ts = j.value("ts", now_s());
        e.schema_version = j.value("schema_version", std::string{SCHEMA_VERSION});
        e.payload = j.value("payload", json::object());
        return e;
    }

    // 모델 → 봉투. T 는 schemas.hpp 의 구조체(to_json 존재).
    template <typename T>
    static Envelope wrap(MessageType mt, const std::string& source, const T& model) {
        Envelope e;
        e.msg_id = gen_id("msg");
        e.type = mt;
        e.source = source;
        e.payload = model;   // ADL to_json
        return e;
    }

    template <typename T>
    T as() const { return payload.get<T>(); }

    // 간단 고유 id 생성기(uuid 의존 회피, 충돌 확률 무시 가능 수준)
    static std::string gen_id(const std::string& prefix) {
        static std::atomic<uint64_t> counter{0};
        uint64_t n = counter.fetch_add(1, std::memory_order_relaxed);
        uint64_t t = static_cast<uint64_t>(now_s() * 1000.0);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%s-%llx%04llx", prefix.c_str(),
                      static_cast<unsigned long long>(t),
                      static_cast<unsigned long long>(n & 0xffff));
        return buf;
    }
};

}  // namespace frize
