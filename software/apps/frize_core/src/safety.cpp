#include "frize/core/safety.hpp"
#include "frize/geo.hpp"
#include <set>

namespace frize::core {

static const std::set<HazardClass> LETHAL_FOR_ENTRY = {
    HazardClass::Backdraft, HazardClass::Structural, HazardClass::GasCylinder
};
static const std::set<CommandAction> ADVANCE_ACTIONS = {
    CommandAction::Advance, CommandAction::ForceEntry, CommandAction::MoveTo
};
static const std::set<CommandAction> ALWAYS_SAFE = {
    CommandAction::Retreat, CommandAction::Rtl, CommandAction::Land, CommandAction::Hold,
    CommandAction::Rally, CommandAction::Highlight, CommandAction::Annotate,
    CommandAction::Open, CommandAction::Close   // IoT 배연/진입 포트: 개폐는 허용(인명 위험 낮음)
};

std::optional<Vec3> pos_from_params(const json& params) {
    if (!params.contains("world_pos")) return std::nullopt;
    const json& wp = params["world_pos"];
    if (wp.is_object()) return Vec3{ wp.value("x",0.0), wp.value("y",0.0), wp.value("z",0.0) };
    if (wp.is_array() && wp.size() == 3) return Vec3{ wp[0].get<double>(), wp[1].get<double>(), wp[2].get<double>() };
    return std::nullopt;
}

Verdict SafetyInterlock::evaluate(const Command& cmd) const {
    if (!enabled_) return {Decision::Allow, "interlock disabled"};

    const DeviceRecord* target = reg_.get(cmd.target_device);
    if (!target) return {Decision::Reject, "미등록 디바이스: " + cmd.target_device};
    if (target->state == DeviceState::Offline) return {Decision::Reject, "대상이 오프라인 상태"};

    // 항상 안전한 명령(후퇴/복귀/착륙/강조 등)은 통과
    if (ALWAYS_SAFE.count(cmd.action) && cmd.action != CommandAction::MoveTo)
        return {Decision::Allow, "안전 명령"};

    if (cmd.action == CommandAction::AimAndSpray) return eval_spray(cmd);
    if (ADVANCE_ACTIONS.count(cmd.action))        return eval_advance(cmd, *target);

    return {Decision::Allow, "정책상 제한 없음"};
}

Verdict SafetyInterlock::eval_spray(const Command& cmd) const {
    auto pos = pos_from_params(cmd.params);
    if (!pos) return {Decision::Reject, "방수 목표 좌표 없음"};

    // 목표 3m 내 사람 감지 → 절대 금지
    for (auto& [_, det] : world_.detections()) {
        if ((det.hazard == HazardClass::Person || det.hazard == HazardClass::DownedPerson) &&
            det.world_pos && geo::distance(*det.world_pos, *pos) < 3.0) {
            return {Decision::Reject, "방수 목표 3m 내 사람 감지(" + det.label + ") ― 절대 금지"};
        }
    }
    // 인근 화점 확인되면 허용
    bool near_fire = false;
    for (auto& [_, d] : world_.detections())
        if ((d.hazard == HazardClass::Fire || d.hazard == HazardClass::Hotspot) &&
            d.world_pos && geo::distance(*d.world_pos, *pos) < 4.0) { near_fire = true; break; }
    if (near_fire) return {Decision::Allow, "화점 확인, 방수 허용"};
    return {Decision::NeedsConfirm, "목표 인근 화점 미확인 ― 확인 요망"};
}

Verdict SafetyInterlock::eval_advance(const Command& cmd, const DeviceRecord& target) const {
    auto pos = pos_from_params(cmd.params);
    const bool is_person = target.device_type == DeviceType::Visor;

    if (is_person && (cmd.action == CommandAction::Advance || cmd.action == CommandAction::ForceEntry)) {
        const json& vitals = target.extra.contains("vitals") ? target.extra["vitals"] : json::object();
        if (vitals.contains("air_pressure_bar") && !vitals["air_pressure_bar"].is_null()) {
            double air = vitals["air_pressure_bar"].get<double>();
            if (air < 55.0)
                return {Decision::Reject, "대원 SCBA 잔압 부족(" + std::to_string((int)air) + "bar) ― 진입 금지, 후퇴 권고"};
        }
        if (vitals.value("motion_state", std::string()) == "fall_detected")
            return {Decision::Reject, "대원 낙상 감지 ― 진입 불가"};

        if (target.extra.contains("gas_flags")) {
            for (auto& f : target.extra["gas_flags"]) {
                std::string s = f.get<std::string>();
                if (s.size() >= 4 && s.substr(s.size()-4) == "IDLH")
                    return {Decision::NeedsConfirm, "대원 주변 IDLH 가스 ― 확인 요망"};
            }
        }
        if (target.battery_pct < 15.0)
            return {Decision::NeedsConfirm, "고글 배터리 부족 ― 확인 요망"};
    }

    if (pos) {
        auto haz = worst_hazard_near(*pos, 4.0);
        if (haz) {
            HazardClass hz = haz->first; Severity sev = haz->second;
            if (is_person && LETHAL_FOR_ENTRY.count(hz) && severity_rank(sev) >= severity_rank(Severity::High)) {
                if (cmd.confirmed)
                    return {Decision::Allow, "치명위험 인지 후 지휘관 승인"};
                return {Decision::NeedsConfirm, "진입 목표에 치명위험 ― 지휘관 2차 확인 필요"};
            }
            if (!is_person && hz == HazardClass::GasCylinder)
                return {Decision::Allow, "드론 진입 ― 가스통 인근 주의"};
        }
    }
    return {Decision::Allow, "진입 위험요소 임계 미만"};
}

std::optional<std::pair<HazardClass, Severity>>
SafetyInterlock::worst_hazard_near(const Vec3& pos, double radius) const {
    std::optional<std::pair<HazardClass, Severity>> worst;
    auto consider = [&](HazardClass h, Severity s){
        if (!worst || severity_rank(s) > severity_rank(worst->second)) worst = {h, s};
    };
    for (auto& [_, z] : world_.hazard_zones())
        if (geo::distance(z.center, pos) <= std::max(radius, z.radius_m)) consider(z.hazard, z.severity);
    for (auto& [_, d] : world_.detections())
        if (d.world_pos && geo::distance(*d.world_pos, pos) <= radius) consider(d.hazard, d.severity);
    return worst;
}

}  // namespace frize::core
