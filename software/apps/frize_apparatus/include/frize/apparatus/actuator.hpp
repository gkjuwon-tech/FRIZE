// frize/apparatus/actuator.hpp ― VENT-1 액추에이터 HAL
//
// 콕핏 OPEN/CLOSE → 리니어 액추에이터가 배연구/문을 개폐.
// sim(항상) + 실제(MCU 시리얼: "OPEN"/"CLOSE" 전송, 리미트스위치 피드백).
#pragma once
#include <memory>
#include <string>

namespace frize::apparatus {

struct MechStatus {
    std::string state{"closed"};   // closed/opening/open/closing/fault
    double position_pct{0.0};       // 0=닫힘 100=완전개방
    bool fault{false};
};

class Actuator {
public:
    virtual ~Actuator() = default;
    virtual bool open() = 0;          // 연결/초기화
    virtual void command_open() = 0;
    virtual void command_close() = 0;
    virtual MechStatus poll(double dt) = 0;
};

std::unique_ptr<Actuator> make_actuator_sim();
std::unique_ptr<Actuator> make_actuator_serial(const std::string& port, int baud);

}  // namespace frize::apparatus
