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

    void start();   // 연결 시작(비동기, 자동 재연결)
    void stop();
    bool connected() const;

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

}  // namespace frize
