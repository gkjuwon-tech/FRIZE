// 고글 센서 구현 ― 시뮬(항상) + 시리얼 센서팩(옵션)
#include "frize/visor/hal.hpp"
#include "frize/geo.hpp"
#include "frize/log.hpp"
#include <random>
#include <cmath>

namespace frize::visor {

static auto LOG = frize::make_logger("vsensors");

// 대원이 현장을 도보 이동하며 생체/공기/가스가 변하는 모델
class SensorsSim : public Sensors {
public:
    explicit SensorsSim(unsigned seed) : rng_(seed) {}
    bool open() override { LOG->info("[sim] 고글 센서팩"); return true; }

    SensorSample sample(double dt) override {
        t_ += dt;
        // 보행: 전진 + 완만한 yaw 스윕
        yaw_ += n_(rng_) * 0.05;
        double speed = 0.6 + 0.3*std::sin(t_*0.2);
        pos_.x += std::sin(yaw_) * speed * dt;
        pos_.y += std::cos(yaw_) * speed * dt;

        // 가스(랜덤워크, 진입 깊어질수록 악화)
        co_  = clamp(co_  + n_(rng_)*30 + dt*3, 0, 2500);
        o2_  = clamp(o2_  + n_(rng_)*0.15 - dt*0.02, 12, 20.9);
        lel_ = clamp(lel_ + n_(rng_)*1.2, 0, 40);
        h2s_ = clamp(h2s_ + n_(rng_)*4, 0, 200);

        // 생체: 심박 상승(운동+스트레스), SCBA 잔압 감소
        hr_ = (int)clamp(hr_ + n_(rng_)*3 + dt*0.5, 70, 195);
        scba_ = clamp(scba_ - dt*0.25, 0, 300);     // bar, 서서히 소모
        battery_ = clamp(battery_ - dt*0.01, 0, 100);

        SensorSample s;
        s.gas.co_ppm=co_; s.gas.o2_vol_pct=o2_; s.gas.lel_pct=lel_; s.gas.h2s_ppm=h2s_;
        s.pose.position = pos_;
        s.pose.orientation = frize::geo::quat_from_yaw(yaw_);
        s.vitals.heart_rate_bpm = hr_;
        s.vitals.air_pressure_bar = scba_;
        s.vitals.motion_state = fall_() ? "fall_detected" : (speed < 0.2 ? "still" : "active");
        s.ambient_temp_c = 30 + co_/50.0;
        s.battery_pct = battery_;
        s.compute_temp_c = 50 + 8*std::sin(t_*0.1);
        return s;
    }
private:
    static double clamp(double v,double lo,double hi){return v<lo?lo:(v>hi?hi:v);}
    bool fall_(){ return dist_(rng_) < 0.0005; }   // 드물게 낙상 이벤트
    std::mt19937 rng_; std::normal_distribution<double> n_{0,1};
    std::uniform_real_distribution<double> dist_{0,1};
    double t_=0, yaw_=0; frize::Vec3 pos_{};
    double co_=80, o2_=20.6, lel_=1, h2s_=2, scba_=300, battery_=100; int hr_=84;
};

#ifdef FRIZE_HAVE_SERIAL
// 시리얼 센서팩: "co,o2,lel,h2s,hr,scba\n" CSV 라인 파싱(POSIX termios)
} // ns
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
namespace frize::visor {
class SensorsSerial : public Sensors {
public:
    explicit SensorsSerial(std::string port) : port_(std::move(port)) {}
    bool open() override {
        fd_ = ::open(port_.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK);
        if (fd_ < 0) { LOG->error("시리얼 열기 실패: {}", port_); return false; }
        termios tio{}; tcgetattr(fd_, &tio); cfsetispeed(&tio, B115200);
        tio.c_cflag |= (CLOCAL | CREAD); tio.c_lflag = 0; tcsetattr(fd_, TCSANOW, &tio);
        LOG->info("시리얼 센서팩: {}", port_); return true;
    }
    SensorSample sample(double) override {
        char buf[256]; int n = ::read(fd_, buf, sizeof(buf)-1);
        if (n > 0) { buf[n]=0; line_ += buf; auto p = line_.find('\n');
            if (p != std::string::npos) { parse(line_.substr(0,p)); line_.erase(0,p+1); } }
        return last_;
    }
private:
    void parse(const std::string& l) {
        double a[6]={0}; if (std::sscanf(l.c_str(), "%lf,%lf,%lf,%lf,%lf,%lf",
            &a[0],&a[1],&a[2],&a[3],&a[4],&a[5]) >= 4) {
            last_.gas.co_ppm=a[0]; last_.gas.o2_vol_pct=a[1]; last_.gas.lel_pct=a[2];
            last_.gas.h2s_ppm=a[3]; last_.vitals.heart_rate_bpm=(int)a[4];
            last_.vitals.air_pressure_bar=a[5];
        }
    }
    std::string port_, line_; int fd_=-1; SensorSample last_{};
};
#endif

std::unique_ptr<Sensors> make_sensors(const std::string& serial_port, const std::string& sim_seed) {
#ifdef FRIZE_HAVE_SERIAL
    if (!serial_port.empty()) return std::make_unique<SensorsSerial>(serial_port);
#else
    (void)serial_port;
#endif
    unsigned seed = sim_seed.empty() ? 7u : (unsigned)std::hash<std::string>{}(sim_seed);
    return std::make_unique<SensorsSim>(seed);
}

}  // namespace frize::visor
