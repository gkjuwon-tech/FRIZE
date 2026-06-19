// 드론 인지 구현 ― base64 + (OpenCV 캡처 | 샘플 프레임) + 가스 시뮬
#include "frize/scout/perception.hpp"
#include "frize/log.hpp"
#include <fstream>
#include <random>
#include <cmath>
#include <vector>
#ifdef FRIZE_HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

namespace frize::scout {

static auto LOG = frize::make_logger("perception");

// ---------- base64 ----------
std::string base64_encode(const unsigned char* data, size_t len) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 2 < len; i += 3) {
        unsigned n = (data[i] << 16) | (data[i+1] << 8) | data[i+2];
        out.push_back(T[(n>>18)&63]); out.push_back(T[(n>>12)&63]);
        out.push_back(T[(n>>6)&63]);  out.push_back(T[n&63]);
    }
    if (i < len) {
        unsigned n = data[i] << 16;
        if (i + 1 < len) n |= data[i+1] << 8;
        out.push_back(T[(n>>18)&63]); out.push_back(T[(n>>12)&63]);
        out.push_back(i + 1 < len ? T[(n>>6)&63] : '=');
        out.push_back('=');
    }
    return out;
}

static std::string read_file_b64(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return base64_encode(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
}

// 가스 시뮬: 화재 근접도에 따라 변동(랜덤워크), 가끔 위험 스파이크
class GasSim {
public:
    frize::GasReading next() {
        co_   = clamp(co_   + n_(rng_)*40,  0, 2500);
        o2_   = clamp(o2_   + n_(rng_)*0.2, 12, 20.9);
        lel_  = clamp(lel_  + n_(rng_)*1.5, 0, 40);
        h2s_  = clamp(h2s_  + n_(rng_)*5,   0, 200);
        frize::GasReading g; g.co_ppm=co_; g.o2_vol_pct=o2_; g.lel_pct=lel_; g.h2s_ppm=h2s_;
        return g;
    }
private:
    static double clamp(double v,double lo,double hi){return v<lo?lo:(v>hi?hi:v);}
    std::mt19937 rng_{42}; std::normal_distribution<double> n_{0,1};
    double co_=120, o2_=20.4, lel_=2, h2s_=4;
};

// ===================== 구현 =====================
class PerceptionImpl : public Perception {
public:
    PerceptionImpl(std::string thermal, std::string rgb, std::string sim)
        : thermal_(std::move(thermal)), rgb_(std::move(rgb)), sim_(std::move(sim)) {}

    bool open() override {
#ifdef FRIZE_HAVE_OPENCV
        if (!thermal_.empty()) cap_.open(thermal_);
        else if (!rgb_.empty()) cap_.open(rgb_);
        if (cap_.isOpened()) { LOG->info("OpenCV 카메라 열림"); return true; }
#endif
        if (!sim_.empty()) { sim_b64_ = read_file_b64(sim_); LOG->info("샘플 프레임 사용: {} ({}B64)", sim_, sim_b64_.size()); }
        else LOG->warn("카메라/샘플 없음 ― 가스 텔레메트리만 송출");
        return true;
    }

    std::optional<CapturedFrame> grab() override {
#ifdef FRIZE_HAVE_OPENCV
        if (cap_.isOpened()) {
            cv::Mat frame;
            if (cap_.read(frame) && !frame.empty()) {
                std::vector<unsigned char> buf;
                cv::imencode(".jpg", frame, buf, {cv::IMWRITE_JPEG_QUALITY, 75});
                CapturedFrame f; f.jpeg_b64 = base64_encode(buf.data(), buf.size());
                f.stream = thermal_.empty() ? "rgb" : "thermal";
                f.width = frame.cols; f.height = frame.rows;
                return f;
            }
        }
#endif
        if (!sim_b64_.empty()) {
            CapturedFrame f; f.jpeg_b64 = sim_b64_; f.stream = "thermal"; f.width=1920; f.height=1080;
            return f;
        }
        return std::nullopt;
    }

    frize::GasReading read_gas() override { return gas_.next(); }

private:
    std::string thermal_, rgb_, sim_, sim_b64_;
    GasSim gas_;
#ifdef FRIZE_HAVE_OPENCV
    cv::VideoCapture cap_;
#endif
};

std::unique_ptr<Perception> make_perception(const std::string& thermal_dev,
                                            const std::string& rgb_dev,
                                            const std::string& sim_frame_path) {
    return std::make_unique<PerceptionImpl>(thermal_dev, rgb_dev, sim_frame_path);
}

}  // namespace frize::scout
