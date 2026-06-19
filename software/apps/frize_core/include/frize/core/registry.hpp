// frize/core/registry.hpp ― 디바이스 레지스트리 (OS의 프로세스 테이블)
#pragma once

#include <map>
#include <string>
#include <vector>
#include "frize/schemas.hpp"

namespace frize::core {

struct DeviceRecord {
    std::string device_id;
    DeviceType  device_type{DeviceType::Visor};
    DeviceState state{DeviceState::Booting};
    std::optional<std::string> callsign{};
    Pose pose{};
    double battery_pct{100.0};
    LinkQuality link{LinkQuality::G5};
    double last_heartbeat{now_s()};
    double last_telemetry{0.0};
    std::string fw_version;
    json extra = json::object();

    DeviceSummary summary() const;
};

class Registry {
public:
    explicit Registry(double offline_after_s = 8.0) : offline_after_s_(offline_after_s) {}

    const DeviceRecord* get(const std::string& id) const;
    DeviceRecord* get(const std::string& id);
    std::vector<DeviceSummary> summaries() const;
    std::vector<const DeviceRecord*> of_type(DeviceType t) const;

    DeviceRecord& on_heartbeat(const Heartbeat& hb);
    DeviceRecord& on_visor_telemetry(const VisorTelemetry& t);
    DeviceRecord& on_scout_telemetry(const ScoutTelemetry& t);
    DeviceRecord& on_apparatus_telemetry(const ApparatusTelemetry& t);

    // 하트비트 끊긴 디바이스를 OFFLINE 전이. 새로 끊긴 id 반환.
    std::vector<std::string> sweep_offline();

private:
    DeviceRecord& ensure(const std::string& id, DeviceType t);
    std::map<std::string, DeviceRecord> devices_;
    double offline_after_s_;
};

}  // namespace frize::core
