// frize/core/vlm_client.hpp ― VLM 판단 계층 (Anthropic Claude vision)
//
// ★ 설계 원칙: 현장 판단은 VLM 전담. 룰 디텍터(빨간 픽셀=불) 없음.
//    모델이 영상을 보고 위험을 분류하고 '왜'를 말한다(rationale 필수).
//
// 구조화 출력은 tool-use(강제 도구 호출)로 스키마를 고정한다.
// HTTP는 블로킹(cpp-httplib). 호출측이 스레드풀+세마포어로 동시성/빈도 제어.
#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "frize/schemas.hpp"

namespace frize::core {

struct VlmContext {
    std::string stream = "thermal";       // thermal / rgb / fused
    std::string gas_summary;              // "CO 1500ppm, O2 18%, LEL 12%"
    std::string media_type = "image/jpeg";
};

class VlmClient {
public:
    VlmClient(std::string api_key, std::string model = "claude-opus-4-8",
              double min_interval_s = 1.5, bool enabled = true);

    bool enabled() const { return enabled_; }
    // 디바이스당 호출 빈도 제한(비용/지연 관리)
    bool should_judge(const std::string& device_id);

    // 프레임 1장 판단 → Detection 리스트(블로킹). world_pos 는 호출측에서 채움.
    std::vector<Detection> judge(const std::string& device_id,
                                 const std::string& jpeg_b64,
                                 const VlmContext& ctx);

private:
    std::vector<Detection> parse_response(const std::string& body, const std::string& device_id) const;

    std::string api_key_;
    std::string model_;
    double min_interval_s_;
    bool enabled_;
    std::mutex mtx_;
    std::map<std::string, double> last_call_;
};

}  // namespace frize::core
