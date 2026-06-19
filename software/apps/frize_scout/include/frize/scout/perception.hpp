// frize/scout/perception.hpp ― 드론 인지 HAL (열화상/RGB/가스)
#pragma once

#include <memory>
#include <string>
#include <optional>
#include "frize/schemas.hpp"

namespace frize::scout {

struct CapturedFrame {
    std::string jpeg_b64;     // 비어있으면 프레임 없음
    std::string stream;       // "thermal" / "rgb" / "fused"
    int width{0};
    int height{0};
};

class Perception {
public:
    virtual ~Perception() = default;
    virtual bool open() = 0;
    // 키프레임 캡처(저대역 폴백/ VLM 판단용). nullopt = 이번엔 프레임 없음
    virtual std::optional<CapturedFrame> grab() = 0;
    virtual frize::GasReading read_gas() = 0;
};

// 실제: OpenCV VideoCapture(열화상/RGB). sim: 샘플 프레임/합성 가스.
std::unique_ptr<Perception> make_perception(const std::string& thermal_dev,
                                            const std::string& rgb_dev,
                                            const std::string& sim_frame_path);

// 공용 base64 인코더(의존성 없음)
std::string base64_encode(const unsigned char* data, size_t len);

}  // namespace frize::scout
