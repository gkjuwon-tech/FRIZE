// frize/scout/flight.hpp ― 비행 제어 HAL
//
// 추상 인터페이스 + 두 구현:
//   FlightSim     : 하드웨어 없이 물리 근사(개발/통합 테스트, 기본값)
//   FlightMavsdk  : Pixhawk(MAVLink) 실제 제어 (FRIZE_HAVE_MAVSDK 시)
// 코어 명령(recon/move_to/hold/rtl/land)이 여기로 번역된다.
#pragma once

#include <memory>
#include <string>
#include "frize/schemas.hpp"

namespace frize::scout {

struct FlightState {
    frize::Vec3 position{};       // 사이트 ENU(m)
    double yaw{0.0};              // rad
    double altitude_m{0.0};
    double groundspeed_ms{0.0};
    double battery_pct{100.0};
    bool   armed{false};
    int    gps_fix{6};           // 6=RTK
    int    satellites{24};
    int    motors_ok{6};
    frize::FlightMode mode{frize::FlightMode::Disarmed};
    double flight_time_remaining_s{1500.0};
};

class FlightController {
public:
    virtual ~FlightController() = default;
    virtual bool connect() = 0;
    virtual void arm() = 0;
    virtual void takeoff(double alt_m) = 0;
    virtual void goto_local(const frize::Vec3& enu, double yaw) = 0;  // ENU 목표로 이동
    virtual void hold() = 0;                                          // 호버
    virtual void return_to_launch() = 0;
    virtual void land() = 0;
    virtual void set_mode(frize::FlightMode m) = 0;
    virtual FlightState poll(double dt) = 0;     // dt초 진행 후 상태 갱신/반환
};

std::unique_ptr<FlightController> make_flight_sim(const frize::Vec3& home);
std::unique_ptr<FlightController> make_flight_mavsdk(const std::string& conn_url);

}  // namespace frize::scout
