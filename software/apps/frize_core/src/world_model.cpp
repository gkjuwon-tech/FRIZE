#include "frize/core/world_model.hpp"
#include "frize/geo.hpp"
#include <algorithm>

namespace frize::core {

void WorldModel::add_detection(const Detection& det) {
    detections_[det.det_id] = det;
    if (det.world_pos) merge_into_zone(det);
}

void WorldModel::merge_into_zone(const Detection& det) {
    const Vec3& wp = *det.world_pos;
    for (auto& [_, z] : hazard_zones_) {
        if (z.hazard == det.hazard && geo::distance(z.center, wp) <= z.radius_m + 1.5) {
            z.center = Vec3{ (z.center.x + wp.x)/2, (z.center.y + wp.y)/2, (z.center.z + wp.z)/2 };
            if (severity_rank(det.severity) > severity_rank(z.severity)) {
                z.severity = det.severity; z.rationale = det.rationale;
            }
            z.ts = now_s();
            return;
        }
    }
    HazardZone z;
    z.zone_id = Envelope::gen_id("zone");
    z.hazard = det.hazard; z.severity = det.severity; z.center = wp;
    z.radius_m = 3.0; z.rationale = det.rationale;
    hazard_zones_[z.zone_id] = std::move(z);
}

void WorldModel::raise_alert(const Alert& a) { alerts_[a.alert_id] = a; }

bool WorldModel::ack_alert(const std::string& alert_id) {
    auto it = alerts_.find(alert_id);
    if (it == alerts_.end()) return false;
    it->second.acknowledged = true;
    return true;
}

void WorldModel::expire() {
    const double t = now_s();
    for (auto it = detections_.begin(); it != detections_.end(); )
        it = (t - it->second.ts >= detection_ttl_s) ? detections_.erase(it) : std::next(it);
    for (auto it = hazard_zones_.begin(); it != hazard_zones_.end(); )
        it = (t - it->second.ts >= zone_ttl_s) ? hazard_zones_.erase(it) : std::next(it);
    for (auto it = alerts_.begin(); it != alerts_.end(); )
        it = (it->second.acknowledged && t - it->second.ts > 60.0) ? alerts_.erase(it) : std::next(it);
}

WorldSnapshot WorldModel::snapshot() const {
    WorldSnapshot s;
    s.devices = registry_.summaries();
    for (auto& [_, d] : detections_) s.detections.push_back(d);
    for (auto& [_, z] : hazard_zones_) s.hazard_zones.push_back(z);
    for (auto& [_, a] : alerts_) if (!a.acknowledged) s.active_alerts.push_back(a);
    s.site_origin = site_origin_;
    return s;
}

std::vector<Detection> WorldModel::rescue_priority() const {
    std::vector<Detection> people;
    for (auto& [_, d] : detections_)
        if (d.hazard == HazardClass::Person || d.hazard == HazardClass::DownedPerson)
            people.push_back(d);

    auto score = [this](const Detection& d) -> double {
        double s = severity_rank(d.severity) * 10.0 + d.confidence;
        if (d.hazard == HazardClass::DownedPerson) s += 20.0;
        if (d.world_pos) {
            for (auto& [_, z] : hazard_zones_)
                if (severity_rank(z.severity) >= severity_rank(Severity::High) &&
                    geo::distance(z.center, *d.world_pos) < 5.0) s += 5.0;
        }
        return s;
    };
    std::sort(people.begin(), people.end(),
              [&](const Detection& a, const Detection& b){ return score(a) > score(b); });
    return people;
}

}  // namespace frize::core
