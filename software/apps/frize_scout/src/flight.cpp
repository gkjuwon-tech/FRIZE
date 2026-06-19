// 비행 제어 구현 ― 시뮬레이터(항상) + MAVSDK(옵션)
#include "frize/scout/flight.hpp"
#include "frize/geo.hpp"
#include "frize/log.hpp"
#include <cmath>
#include <algorithm>
#include <thread>
#include <chrono>

namespace frize::scout {

static auto LOG = frize::make_logger("flight");

// ===================== 시뮬레이터 =====================
// 1차 지연 모델로 목표점을 향해 부드럽게 이동. 배터리 소모 근사.
class FlightSim : public FlightController {
public:
    explicit FlightSim(const frize::Vec3& home) : home_(home) { st_.position = home; }
    bool connect() override { LOG->info("[sim] flight controller 연결"); return true; }
    void arm() override { st_.armed = true; st_.mode = frize::FlightMode::Loiter; }
    void takeoff(double alt) override { arm(); target_ = st_.position; target_.z = alt; st_.mode = frize::FlightMode::Auto; }
    void goto_local(const frize::Vec3& enu, double yaw) override { target_ = enu; target_yaw_ = yaw; st_.mode = frize::FlightMode::Auto; }
    void hold() override { target_ = st_.position; st_.mode = frize::FlightMode::Loiter; }
    void return_to_launch() override { target_ = home_; target_.z = std::max(st_.position.z, 8.0); st_.mode = frize::FlightMode::Rtl; }
    void land() override { target_ = st_.position; target_.z = 0.0; st_.mode = frize::FlightMode::Land; }
    void set_mode(frize::FlightMode m) override { st_.mode = m; }

    FlightState poll(double dt) override {
        // 목표를 향해 최대속도 제한 이동
        const double vmax = 8.0;  // m/s
        frize::Vec3 d{ target_.x - st_.position.x, target_.y - st_.position.y, target_.z - st_.position.z };
        double dist = std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
        double step = std::min(dist, vmax * dt);
        if (dist > 1e-3) {
            st_.position.x += d.x/dist*step;
            st_.position.y += d.y/dist*step;
            st_.position.z += d.z/dist*step;
        }
        st_.groundspeed_ms = (dt > 0) ? std::hypot(d.x, d.y)/dist*std::min(dist,vmax*dt)/dt : 0.0;
        st_.altitude_m = st_.position.z;
        if (target_yaw_) { st_.yaw = *target_yaw_; }
        // 배터리: 비행 중 ~ 0.02%/s + 이동 가산
        if (st_.armed) st_.battery_pct = std::max(0.0, st_.battery_pct - dt*(0.02 + 0.01*st_.groundspeed_ms/vmax));
        st_.flight_time_remaining_s = st_.battery_pct/100.0 * 1500.0;
        if (st_.mode == frize::FlightMode::Land && st_.position.z < 0.05) { st_.armed = false; st_.mode = frize::FlightMode::Disarmed; }
        return st_;
    }
private:
    frize::Vec3 home_, target_{};
    std::optional<double> target_yaw_{};
    FlightState st_{};
};

std::unique_ptr<FlightController> make_flight_sim(const frize::Vec3& home) {
    return std::make_unique<FlightSim>(home);
}

// ===================== MAVSDK (실제 Pixhawk) =====================
#ifdef FRIZE_HAVE_MAVSDK
} // namespace (re-open below after includes to keep top clean)
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mavsdk/plugins/offboard/offboard.h>
namespace frize::scout {

class FlightMavsdk : public FlightController {
public:
    explicit FlightMavsdk(std::string url) : url_(std::move(url)) {}
    bool connect() override {
        if (mavsdk_.add_any_connection(url_) != mavsdk::ConnectionResult::Success) {
            LOG->error("MAVSDK 연결 실패: {}", url_); return false;
        }
        // 첫 시스템 대기
        for (int i = 0; i < 30 && mavsdk_.systems().empty(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (mavsdk_.systems().empty()) { LOG->error("기체 미발견"); return false; }
        system_ = mavsdk_.systems().front();
        action_ = std::make_unique<mavsdk::Action>(system_);
        telemetry_ = std::make_unique<mavsdk::Telemetry>(system_);
        offboard_ = std::make_unique<mavsdk::Offboard>(system_);
        LOG->info("MAVSDK 연결됨: {}", url_);
        return true;
    }
    void arm() override { action_->arm(); }
    void takeoff(double alt) override { action_->set_takeoff_altitude((float)alt); action_->arm(); action_->takeoff(); }
    void goto_local(const frize::Vec3& enu, double yaw) override {
        // ENU → NED(Offboard position). x_n=north=enu.y, y_e=east=enu.x, z_d=-up
        mavsdk::Offboard::PositionNedYaw p{};
        p.north_m = (float)enu.y; p.east_m = (float)enu.x; p.down_m = -(float)enu.z;
        p.yaw_deg = (float)(yaw * frize::geo::RAD2DEG);
        if (!offboard_->is_active()) { offboard_->set_position_ned(p); offboard_->start(); }
        offboard_->set_position_ned(p);
    }
    void hold() override { if (offboard_->is_active()) offboard_->stop(); action_->hold(); }
    void return_to_launch() override { action_->return_to_launch(); }
    void land() override { action_->land(); }
    void set_mode(frize::FlightMode) override {}
    FlightState poll(double) override {
        FlightState st{};
        auto pv = telemetry_->position_velocity_ned();
        st.position = { pv.position.east_m, pv.position.north_m, -pv.position.down_m };  // NED→ENU
        st.altitude_m = telemetry_->position().relative_altitude_m;
        st.groundspeed_ms = std::hypot(pv.velocity.north_m_s, pv.velocity.east_m_s);
        st.battery_pct = telemetry_->battery().remaining_percent * 100.0f;
        st.armed = telemetry_->armed();
        auto gps = telemetry_->gps_info();
        st.satellites = gps.num_satellites; st.gps_fix = (int)gps.fix_type;
        st.yaw = telemetry_->attitude_euler().yaw_deg * frize::geo::DEG2RAD;
        return st;
    }
private:
    std::string url_;
    mavsdk::Mavsdk mavsdk_{mavsdk::Mavsdk::Configuration{mavsdk::Mavsdk::ComponentType::GroundStation}};
    std::shared_ptr<mavsdk::System> system_;
    std::unique_ptr<mavsdk::Action> action_;
    std::unique_ptr<mavsdk::Telemetry> telemetry_;
    std::unique_ptr<mavsdk::Offboard> offboard_;
};

std::unique_ptr<FlightController> make_flight_mavsdk(const std::string& url) {
    return std::make_unique<FlightMavsdk>(url);
}
#else
std::unique_ptr<FlightController> make_flight_mavsdk(const std::string&) {
    LOG->warn("MAVSDK 미빌드 ― 시뮬레이터로 폴백");
    return nullptr;
}
#endif

}  // namespace frize::scout
