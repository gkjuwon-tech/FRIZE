// frize_console_estop ― CONSOLE-1 비상정지 브리지 (UI 독립 안전경로)
//
// 물리 E-STOP(evdev) → 버스로 직접 비상 명령 브로드캐스트:
//   전 대원(visor) = retreat(탈출 내비), 전 드론(scout) = rtl(복귀).
// 콕핏(Qt)이 멈춰도 이 경로는 살아있다. 코어 인터록은 후퇴/복귀를 항상 허용.
#include <atomic>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>

#include "frize/bus.hpp"
#include "frize/protocol.hpp"
#include "frize/log.hpp"

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#define FRIZE_HAVE_EVDEV 1
#endif

using namespace frize;
static auto LOG = make_logger("estop");
static std::string envs(const char* k, const std::string& d){ const char* v=std::getenv(k); return v?v:d; }
static int envi(const char* k,int d){ const char* v=std::getenv(k); return v?std::atoi(v):d; }

int main() {
    const std::string host = envs("FRIZE_MQTT_HOST","localhost");
    const int port = envi("FRIZE_MQTT_PORT",1883);
    const std::string dev = envs("FRIZE_ESTOP_DEVICE","");
    const int keycode = envi("FRIZE_ESTOP_KEYCODE", 1 /*KEY_ESC*/);

    MessageBus bus("frize-console-estop", host, port);
    std::mutex mtx; std::map<std::string,DeviceType> units;
    bus.subscribe(Topic::all_heartbeats(), [&](const std::string&, const Envelope& e){
        auto hb = e.as<Heartbeat>();
        std::lock_guard<std::mutex> lk(mtx);
        if (hb.state==DeviceState::Offline) units.erase(hb.device_id);
        else units[hb.device_id]=hb.device_type;
    });
    bus.start();

    auto fire = [&](){
        std::lock_guard<std::mutex> lk(mtx);
        LOG->warn("E-STOP! 비상 브로드캐스트 ― {}개 유닛", units.size());
        for (auto& [id,type] : units) {
            Command c; c.cmd_id=Envelope::gen_id("estop"); c.target_device=id; c.issued_by="ESTOP";
            c.confirmed=true; c.reason="E-STOP";
            c.action = (type==DeviceType::Scout) ? CommandAction::Rtl : CommandAction::Retreat;
            bus.publish(Topic::command(id), Envelope::wrap(MessageType::Command,"console-estop",c), 1);
        }
    };

#ifdef FRIZE_HAVE_EVDEV
    if (!dev.empty()) {
        int fd = ::open(dev.c_str(), O_RDONLY);
        if (fd < 0) { LOG->error("E-STOP 장치 열기 실패: {} ― stdin 폴백", dev); }
        else {
            LOG->info("E-STOP 감시: {} (keycode {})", dev, keycode);
            input_event ev{};
            while (::read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                if (ev.type==EV_KEY && ev.code==keycode && ev.value==1) fire();
            }
            ::close(fd);
            return 0;
        }
    }
#endif
    // 폴백: 표준입력에 Enter → 트리거(테스트/무패널 환경)
    LOG->info("E-STOP 폴백: stdin Enter 로 트리거(테스트)");
    std::string line;
    while (std::getline(std::cin, line)) fire();
    bus.stop();
    return 0;
}
