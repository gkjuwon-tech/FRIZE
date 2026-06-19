// frize/visor/hal.hpp ― 스마트 고글 하드웨어 추상화
//
// 세 HAL: Sensors(가스/IMU/GNSS/SCBA·생체), Capture(열화상/RGB),
//         ArDisplay(시각 명령 출력 = '출력장치 = 사람 눈앞').
// 각 인터페이스는 sim/real 구현을 가진다(real은 SDK ifdef).
#pragma once

#include <memory>
#include <optional>
#include <string>
#include "frize/schemas.hpp"

namespace frize::visor {

// ---- 센서 ----
struct SensorSample {
    frize::GasReading gas{};
    frize::Pose pose{};               // 머리 위치/시선(자세)
    frize::WearerVitals vitals{};
    double ambient_temp_c{25.0};
    double battery_pct{100.0};
    double compute_temp_c{52.0};
};
class Sensors {
public:
    virtual ~Sensors() = default;
    virtual bool open() = 0;
    virtual SensorSample sample(double dt) = 0;
};
std::unique_ptr<Sensors> make_sensors(const std::string& serial_port, const std::string& sim_seed);

// ---- 영상 캡처 ----
struct CapturedFrame { std::string jpeg_b64; std::string stream; int width{0}, height{0}; };
class Capture {
public:
    virtual ~Capture() = default;
    virtual bool open() = 0;
    virtual std::optional<CapturedFrame> grab() = 0;
};
std::unique_ptr<Capture> make_capture(const std::string& thermal_dev, const std::string& sim_frame);

// ---- AR 디스플레이 (시각 명령 출력) ----
// 지휘관 명령을 대원 시야의 AR 요소로 렌더(화살표/하이라이트/탈출 내비/경고).
struct ArCommand {
    std::string kind;        // highlight / advance / retreat / annotate / evacuate / rally
    std::string text;        // 표시 문구(한국어)
    double bearing_deg{-1};  // 화살표 방향(0=정면 기준 상대 또는 절대 북). <0이면 없음
    std::string severity;    // info/low/high/critical → 색
};
class ArDisplay {
public:
    virtual ~ArDisplay() = default;
    virtual bool open() = 0;
    virtual void show(const ArCommand& c) = 0;   // 새 명령 표시
    virtual void clear() = 0;
    virtual void tick(double dt) = 0;            // 애니메이션/만료
};
std::unique_ptr<ArDisplay> make_ar_display(bool headless);

}  // namespace frize::visor
