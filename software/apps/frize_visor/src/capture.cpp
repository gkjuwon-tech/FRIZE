// 고글 영상 캡처 ― 열화상/RGB (OpenCV 옵션) + 샘플 프레임 폴백
#include "frize/visor/hal.hpp"
#include "frize/base64.hpp"
#include "frize/log.hpp"
#include <fstream>
#include <vector>
#ifdef FRIZE_HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

namespace frize::visor {

static auto LOG = frize::make_logger("vcapture");

static std::string file_b64(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::string b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return frize::base64_encode(reinterpret_cast<const unsigned char*>(b.data()), b.size());
}

class CaptureImpl : public Capture {
public:
    CaptureImpl(std::string dev, std::string sim) : dev_(std::move(dev)), sim_(std::move(sim)) {}
    bool open() override {
#ifdef FRIZE_HAVE_OPENCV
        if (!dev_.empty() && cap_.open(dev_)) { LOG->info("열화상 캡처: {}", dev_); return true; }
#endif
        if (!sim_.empty()) { b64_ = file_b64(sim_); LOG->info("샘플 프레임: {}", sim_); }
        return true;
    }
    std::optional<CapturedFrame> grab() override {
#ifdef FRIZE_HAVE_OPENCV
        if (cap_.isOpened()) {
            cv::Mat m; if (cap_.read(m) && !m.empty()) {
                std::vector<unsigned char> buf;
                cv::imencode(".jpg", m, buf, {cv::IMWRITE_JPEG_QUALITY, 75});
                return CapturedFrame{ frize::base64_encode(buf.data(), buf.size()), "thermal", m.cols, m.rows };
            }
        }
#endif
        if (!b64_.empty()) return CapturedFrame{ b64_, "thermal", 1920, 1080 };
        return std::nullopt;
    }
private:
    std::string dev_, sim_, b64_;
#ifdef FRIZE_HAVE_OPENCV
    cv::VideoCapture cap_;
#endif
};

std::unique_ptr<Capture> make_capture(const std::string& thermal_dev, const std::string& sim_frame) {
    return std::make_unique<CaptureImpl>(thermal_dev, sim_frame);
}

}  // namespace frize::visor
