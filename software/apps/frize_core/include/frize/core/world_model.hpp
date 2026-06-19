// frize/core/world_model.hpp ― 현장 전체 상태(공유 메모리)
#pragma once

#include <map>
#include <vector>
#include "frize/schemas.hpp"
#include "frize/core/registry.hpp"

namespace frize::core {

class WorldModel {
public:
    WorldModel(Registry& registry, GeoPoint site_origin)
        : registry_(registry), site_origin_(site_origin) {}

    void add_detection(const Detection& det);
    void raise_alert(const Alert& a);
    bool ack_alert(const std::string& alert_id);
    void expire();                       // 오래된 감지/구역/경보 정리
    WorldSnapshot snapshot() const;
    std::vector<Detection> rescue_priority() const;  // '누구부터 빼나'

    // 안전 인터록이 참조
    const std::map<std::string, Detection>& detections() const { return detections_; }
    const std::map<std::string, HazardZone>& hazard_zones() const { return hazard_zones_; }
    GeoPoint site_origin() const { return site_origin_; }
    void set_site_origin(GeoPoint g) { site_origin_ = g; }

    double detection_ttl_s = 12.0;
    double zone_ttl_s = 20.0;

private:
    void merge_into_zone(const Detection& det);

    Registry& registry_;
    GeoPoint site_origin_;
    std::map<std::string, Detection> detections_;
    std::map<std::string, HazardZone> hazard_zones_;
    std::map<std::string, Alert> alerts_;
};

}  // namespace frize::core
