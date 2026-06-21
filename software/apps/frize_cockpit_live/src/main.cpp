// frize_cockpit_live ― 프로덕션 콕핏 통신 데몬(헤드리스)
//
// 목업/렌더가 아니라 실제 MQTT 위에서 도는 콕핏 백엔드.
//  - 디바이스 발견(하트비트) → 콘솔 로스터
//  - 고글/드론/벤트 페어링(PairRequest → PairGrant) 자동/수동
//  - 라이브 텔레메트리(위치·배터리·가스) 표시
//
// 사용:
//   frize_cockpit_live --host localhost --port 1883 --console cockpit-01 --pair-all
//
// ImGui 콕핏(frize_cockpit_imgui)은 동일한 CockpitLink 를 링크해 이 로스터를
// 그대로 시각화한다(SSOT). 이 데몬은 그 통신 계층을 단독 검증/운용한다.
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <string>
#include <thread>

#include <fstream>

#include "frize/cockpit/link.hpp"
#include "frize/schemas.hpp"

using namespace frize;
using namespace frize::cockpit;

static std::atomic<bool> g_run{true};
static void on_sig(int) { g_run = false; }

static const char* type_str(DeviceType t) {
    switch (t) {
        case DeviceType::Visor:     return "GOGGLE";
        case DeviceType::Scout:     return "DRONE ";
        case DeviceType::Apparatus: return "VENT  ";
        case DeviceType::Cockpit:   return "COCKPIT";
        case DeviceType::Core:      return "CORE  ";
    }
    return "?";
}

static std::string arg(int argc, char** argv, const std::string& key, const std::string& def) {
    for (int i = 1; i + 1 < argc; ++i)
        if (key == argv[i]) return argv[i + 1];
    return def;
}
static bool flag(int argc, char** argv, const std::string& key) {
    for (int i = 1; i < argc; ++i)
        if (key == argv[i]) return true;
    return false;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, on_sig);
    std::signal(SIGTERM, on_sig);

    const std::string host    = arg(argc, argv, "--host", "localhost");
    const int         port    = std::stoi(arg(argc, argv, "--port", "1883"));
    const std::string console = arg(argc, argv, "--console", "cockpit-01");
    const bool        autopair = flag(argc, argv, "--pair-all");
    const int         iters    = std::stoi(arg(argc, argv, "--iters", "0"));  // 0 = 무한
    const std::string snapshot = arg(argc, argv, "--snapshot", "");           // 로스터 JSON 1회 저장

    std::printf("FRIZE 콕핏 라이브 링크\n");
    std::printf("  broker=%s:%d  console=%s  auto-pair=%s\n",
                host.c_str(), port, console.c_str(), autopair ? "ON" : "OFF");

    CockpitLink link(console, host, port);
    link.start();

    // 연결 대기
    for (int i = 0; i < 50 && !link.connected(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::printf("  세션=%s  연결=%s\n\n", link.session_id().c_str(),
                link.connected() ? "OK" : "대기중");

    bool paired_once = false;
    int tick = 0;
    while (g_run) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++tick;

        auto devs = link.devices();

        // 첫 디바이스 발견 후 1회 일괄 페어링
        if (autopair && !paired_once && !devs.empty()) {
            int n = link.pair_all();
            std::printf(">> 페어링 요청 발행: %d 대\n", n);
            paired_once = true;
        }

        // 로스터 출력
        double now = now_s();
        std::printf("\033[H\033[2J");  // 화면 클리어
        std::printf("FRIZE COCKPIT LIVE   broker=%s:%d   t=%ds   연결=%s   페어링=%d/%zu\n",
                    host.c_str(), port, tick, link.connected() ? "OK" : "X",
                    link.paired_count(), devs.size());
        std::printf("─────────────────────────────────────────────────────────────────────────\n");
        std::printf("%-10s %-7s %-9s %-8s %-7s %-6s %-22s\n",
                    "DEVICE", "TYPE", "PAIR", "STATE", "BATT", "LINK", "POS / GAS");
        std::printf("─────────────────────────────────────────────────────────────────────────\n");
        for (auto& d : devs) {
            char pos[48] = "—";
            if (d.has_pose)
                std::snprintf(pos, sizeof(pos), "(%.1f,%.1f,%.1f) CO%.0f O2%.1f",
                              d.pos.x, d.pos.y, d.pos.z, d.gas.co_ppm, d.gas.o2_vol_pct);
            char batt[8] = "—";
            if (d.battery_pct >= 0) std::snprintf(batt, sizeof(batt), "%.0f%%", d.battery_pct);
            std::printf("%-10s %-7s %-9s %-8s %-7s %-6s %-22s\n",
                        d.device_id.c_str(), type_str(d.type), pair_state_str(d.pair),
                        d.fresh(now) ? "online" : "stale", batt,
                        d.fresh(now) ? "live" : "—", pos);
        }
        std::printf("─────────────────────────────────────────────────────────────────────────\n");
        std::printf("(Ctrl-C 종료)\n");
        std::fflush(stdout);

        // 라이브 로스터를 JSON 으로 1회 저장(ImGui 콕핏이 페어링 패널을 그릴 SSOT).
        if (!snapshot.empty() && paired_once && tick >= 4) {
            json arr = json::array();
            for (auto& d : devs) {
                json j;
                j["device_id"]   = d.device_id;
                j["type"]        = d.type;
                j["pair"]        = pair_state_str(d.pair);
                j["online"]      = d.fresh(now);
                j["fw"]          = d.fw_version;
                j["capabilities"]= d.capabilities;
                j["battery_pct"] = d.battery_pct;
                if (d.has_pose) j["pos"] = {d.pos.x, d.pos.y, d.pos.z};
                j["co_ppm"]      = d.gas.co_ppm;
                j["o2_pct"]      = d.gas.o2_vol_pct;
                arr.push_back(j);
            }
            json root;
            root["console_id"] = link.console_id();
            root["session_id"] = link.session_id();
            root["broker"]     = host + ":" + std::to_string(port);
            root["paired"]     = link.paired_count();
            root["ts"]         = now;
            root["devices"]    = arr;
            std::ofstream(snapshot) << root.dump(2);
            std::printf(">> 로스터 스냅샷 저장: %s (%d 대 페어링)\n",
                        snapshot.c_str(), link.paired_count());
            break;
        }

        if (iters > 0 && tick >= iters) break;
    }

    std::printf("\n종료. 최종 페어링 %d 대.\n", link.paired_count());
    return 0;
}
