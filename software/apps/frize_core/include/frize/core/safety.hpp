// frize/core/safety.hpp ― 안전 인터록 (커널의 보호 모드)
//
// 생명 직결이므로 명령은 무조건 여기를 통과해야 디바이스로 나간다.
//   Allow / NeedsConfirm(휴먼 인 더 루프) / Reject
// 모든 판정에 사람이 읽는 reason 을 붙인다(블랙박스 금지).
#pragma once

#include <string>
#include "frize/schemas.hpp"
#include "frize/core/registry.hpp"
#include "frize/core/world_model.hpp"

namespace frize::core {

enum class Decision { Allow, NeedsConfirm, Reject };

struct Verdict {
    Decision decision;
    std::string reason;
};

class SafetyInterlock {
public:
    SafetyInterlock(Registry& reg, WorldModel& world, bool enabled = true)
        : reg_(reg), world_(world), enabled_(enabled) {}

    Verdict evaluate(const Command& cmd) const;

private:
    Verdict eval_spray(const Command& cmd) const;
    Verdict eval_advance(const Command& cmd, const DeviceRecord& target) const;
    // pos 인근 최악 위험(있으면 {hazard,severity})
    std::optional<std::pair<HazardClass, Severity>> worst_hazard_near(const Vec3& pos, double radius) const;

    Registry& reg_;
    WorldModel& world_;
    bool enabled_;
};

// params["world_pos"] → Vec3
std::optional<Vec3> pos_from_params(const json& params);

}  // namespace frize::core
