// frize/bus.hpp ― 메시지 버스 (Eclipse Paho MQTT C++ 래퍼)
//
// 소방 현장은 망이 끊긴다. 그래서:
//  - 자동 재연결(Paho 내장 + 백오프)
//  - QoS 1 기본(명령 유실 방지)
//  - LWT(Last Will): 디바이스 비정상 종료 시 코어가 즉시 OFFLINE 인지
//
// 스레드 모델: Paho 콜백 스레드에서 핸들러가 호출된다. 핸들러는 짧게 끝내고
// 무거운 작업은 큐로 넘겨라(핸들러 안에서 블로킹 금지).
#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>
#include <memory>

#include "frize/protocol.hpp"
#include "frize/schemas.hpp"

namespace frize {

using BusHandler = std::function<void(const std::string& topic, const Envelope&)>;

// MQTT 토픽 매칭(+, # 지원) ― 로컬 디스패치용
bool topic_matches(const std::string& filter, const std::string& topic);

class MessageBus {
public:
    MessageBus(std::string client_id,
               std::string host = "localhost",
               int port = 1883,
               std::string username = "",
               std::string password = "",
               int keepalive_s = 10);
    ~MessageBus();

    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;

    // LWT 설정: 비정상 종료 시 OFFLINE 하트비트가 retained 로 남는다.
    void set_device_will(const std::string& device_id, DeviceType type);

    // 토픽 필터(와일드카드 허용)와 핸들러 등록. connect 전/후 모두 가능.
    void subscribe(const std::string& topic_filter, BusHandler handler);

    void publish(const std::string& topic, const Envelope& env, int qos = 1, bool retain = false);

    // 페어링 응답 자동화: 콕핏이 frize/pair/<id> 로 PairRequest 를 보내면
    // 디바이스가 스스로 PairGrant 를 frize/pair/<id>/grant 로 retained 발행한다.
    // (헤더 인라인 — subscribe/publish 프리미티브만으로 구현, 디바이스 main 에서 1줄 호출)
    void enable_pairing(const std::string& device_id,
                        DeviceType type,
                        const std::string& fw_version = "0.1.0",
                        std::vector<std::string> capabilities = {}) {
        subscribe(Topic::pair(device_id),
                  [this, device_id, type, fw_version, capabilities](const std::string&, const Envelope& env) {
            if (env.type != MessageType::PairRequest) return;
            PairRequest req;
            try { req = env.as<PairRequest>(); } catch (...) { return; }
            PairGrant grant;
            grant.device_id   = device_id;
            grant.console_id  = req.console_id;
            grant.session_id  = req.session_id;
            grant.accepted    = true;
            grant.device_type = type;
            grant.fw_version  = fw_version;
            grant.capabilities = capabilities;
            // retained: 늦게 들어온 콕핏도 마지막 페어링 상태를 즉시 본다.
            publish(Topic::pair_grant(device_id),
                    Envelope::wrap(MessageType::PairGrant, device_id, grant),
                    1, true);
        });
    }

    void start();   // 연결 시작(비동기, 자동 재연결)
    void stop();
    bool connected() const;

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

}  // namespace frize
