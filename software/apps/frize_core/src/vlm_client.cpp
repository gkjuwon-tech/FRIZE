#include "frize/core/vlm_client.hpp"
#include "frize/protocol.hpp"
#include "frize/log.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

namespace frize::core {

static auto LOG = make_logger("vlm");

// 허용 enum 값(스키마 고정)
static json hazard_enum() {
    return json::array({"person","downed_person","fire","hotspot","smoke","gas_cylinder",
                        "structural","backdraft","blocked_exit","electrical","unknown"});
}
static json severity_enum() { return json::array({"info","low","high","critical"}); }

static const char* SYSTEM_PROMPT = R"(너는 FRIZE 소방 지휘 시스템의 현장 판단 엔진이다. 소방 전술과 화재역학에 정통하다.
주어진 영상(열화상 또는 저조도 RGB)과 센서 컨텍스트를 보고, 대원·지휘관의 생명에
직결되는 위험요소를 식별한다.

원칙:
- 과소평가보다 과대평가가 낫지만, 거짓 경보를 남발하지 마라. 확신도(confidence)에 정직하라.
- 사람(요구조자/대원)과 쓰러진 사람을 최우선으로 찾아라.
- 화재역학 징후(플래시오버/백드래프트 전조: 고온 연기층, 맥동 연기, 출입구 흡입)를 평가하라.
- 구조물 위험(천장 처짐, 균열, 변형)과 폭발 위험(가스통, 높은 LEL)을 평가하라.
- 모든 감지에는 반드시 '왜 그렇게 판단했는지' rationale 을 한국어로 한 문장 담아라. 블랙박스 금지.
- bbox 는 영상 기준 정규화 좌표 [x,y,w,h] (0~1). 위치를 특정 못 하면 생략.
반드시 report_hazards 도구를 호출해 결과를 보고하라. 위험이 없으면 빈 리스트로 호출하라.)";

VlmClient::VlmClient(std::string api_key, std::string model, double min_interval_s, bool enabled)
    : api_key_(std::move(api_key)), model_(std::move(model)),
      min_interval_s_(min_interval_s), enabled_(enabled && !api_key_.empty()) {
    if (!enabled_)
        LOG->warn("VLM 판단 비활성(API 키 없음). 키 설정 시 실시간 판단 동작.");
}

bool VlmClient::should_judge(const std::string& device_id) {
    if (!enabled_) return false;
    std::lock_guard<std::mutex> lk(mtx_);
    double t = now_s();
    auto it = last_call_.find(device_id);
    if (it != last_call_.end() && t - it->second < min_interval_s_) return false;
    return true;
}

std::vector<Detection> VlmClient::judge(const std::string& device_id,
                                        const std::string& jpeg_b64,
                                        const VlmContext& ctx) {
    if (!enabled_) return {};
    { std::lock_guard<std::mutex> lk(mtx_); last_call_[device_id] = now_s(); }

    // 도구 스키마
    json tool = {
        {"name", "report_hazards"},
        {"description", "영상에서 식별한 소방 위험요소 목록을 보고한다."},
        {"input_schema", {
            {"type", "object"},
            {"properties", {
                {"hazards", {
                    {"type", "array"},
                    {"items", {
                        {"type", "object"},
                        {"properties", {
                            {"hazard", {{"type","string"},{"enum", hazard_enum()}}},
                            {"severity", {{"type","string"},{"enum", severity_enum()}}},
                            {"confidence", {{"type","number"},{"minimum",0},{"maximum",1}}},
                            {"label", {{"type","string"}}},
                            {"rationale", {{"type","string"}}},
                            {"bbox", {{"type","array"},{"items",{{"type","number"}}},{"minItems",4},{"maxItems",4}}}
                        }},
                        {"required", json::array({"hazard","severity","confidence","label","rationale"})}
                    }}
                }}
            }},
            {"required", json::array({"hazards"})}
        }}
    };

    std::string user_text =
        "디바이스: " + device_id + " (스트림: " + ctx.stream + ").\n"
        "센서 컨텍스트: " + (ctx.gas_summary.empty() ? "없음" : ctx.gas_summary) + ".\n"
        "이 영상을 분석해 위험요소를 report_hazards로 보고하라.";

    json body = {
        {"model", model_},
        {"max_tokens", 1024},
        {"system", SYSTEM_PROMPT},
        {"tools", json::array({tool})},
        {"tool_choice", {{"type","tool"},{"name","report_hazards"}}},
        {"messages", json::array({
            {
                {"role","user"},
                {"content", json::array({
                    {{"type","image"},{"source",{{"type","base64"},{"media_type",ctx.media_type},{"data",jpeg_b64}}}},
                    {{"type","text"},{"text",user_text}}
                })}
            }
        })}
    };

    httplib::Client cli("https://api.anthropic.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(20, 0);
    httplib::Headers headers = {
        {"x-api-key", api_key_},
        {"anthropic-version", "2023-06-01"},
        {"content-type", "application/json"}
    };
    auto res = cli.Post("/v1/messages", headers, body.dump(), "application/json");
    if (!res) { LOG->error("VLM 호출 실패({}): 네트워크", device_id); return {}; }
    if (res->status != 200) {
        LOG->error("VLM 호출 실패({}): HTTP {} {}", device_id, res->status, res->body.substr(0, 200));
        return {};
    }
    return parse_response(res->body, device_id);
}

std::vector<Detection> VlmClient::parse_response(const std::string& body, const std::string& device_id) const {
    std::vector<Detection> out;
    json resp;
    try { resp = json::parse(body); }
    catch (const std::exception& e) { LOG->error("VLM 응답 파싱 실패: {}", e.what()); return out; }

    if (!resp.contains("content")) return out;
    for (auto& block : resp["content"]) {
        if (block.value("type", std::string()) != "tool_use") continue;
        if (block.value("name", std::string()) != "report_hazards") continue;
        const json& input = block.value("input", json::object());
        if (!input.contains("hazards")) continue;
        for (auto& h : input["hazards"]) {
            try {
                Detection d;
                d.det_id = Envelope::gen_id("det");
                d.source_device = device_id;
                d.hazard = h.at("hazard").get<HazardClass>();
                d.severity = h.at("severity").get<Severity>();
                d.confidence = h.value("confidence", 0.0);
                d.label = h.value("label", std::string());
                d.rationale = h.value("rationale", std::string());
                if (h.contains("bbox") && h["bbox"].is_array() && h["bbox"].size() == 4) {
                    std::array<double,4> b{ h["bbox"][0], h["bbox"][1], h["bbox"][2], h["bbox"][3] };
                    d.bbox = b;
                }
                out.push_back(std::move(d));
            } catch (const std::exception& e) {
                LOG->warn("감지 파싱 스킵: {}", e.what());
            }
        }
    }
    return out;
}

}  // namespace frize::core
