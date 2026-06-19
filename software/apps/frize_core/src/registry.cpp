#include "frize/core/registry.hpp"

namespace frize::core {

DeviceSummary DeviceRecord::summary() const {
    DeviceSummary s;
    s.device_id = device_id; s.device_type = device_type; s.state = state;
    s.callsign = callsign; s.pose = pose; s.battery_pct = battery_pct; s.link = link;
    s.last_seen = std::max(last_heartbeat, last_telemetry); s.extra = extra;
    return s;
}

const DeviceRecord* Registry::get(const std::string& id) const {
    auto it = devices_.find(id);
    return it == devices_.end() ? nullptr : &it->second;
}
DeviceRecord* Registry::get(const std::string& id) {
    auto it = devices_.find(id);
    return it == devices_.end() ? nullptr : &it->second;
}

std::vector<DeviceSummary> Registry::summaries() const {
    std::vector<DeviceSummary> out; out.reserve(devices_.size());
    for (auto& [_, rec] : devices_) out.push_back(rec.summary());
    return out;
}

std::vector<const DeviceRecord*> Registry::of_type(DeviceType t) const {
    std::vector<const DeviceRecord*> out;
    for (auto& [_, rec] : devices_) if (rec.device_type == t) out.push_back(&rec);
    return out;
}

DeviceRecord& Registry::ensure(const std::string& id, DeviceType t) {
    auto it = devices_.find(id);
    if (it == devices_.end()) {
        DeviceRecord rec; rec.device_id = id; rec.device_type = t; rec.state = DeviceState::Online;
        it = devices_.emplace(id, std::move(rec)).first;
    }
    return it->second;
}

DeviceRecord& Registry::on_heartbeat(const Heartbeat& hb) {
    auto& rec = ensure(hb.device_id, hb.device_type);
    rec.last_heartbeat = now_s();
    rec.fw_version = hb.fw_version;
    if (hb.state == DeviceState::Offline) {
        rec.state = DeviceState::Offline;   // LWT 우선
    } else if (rec.state == DeviceState::Booting || rec.state == DeviceState::Offline) {
        rec.state = DeviceState::Online;
    }
    return rec;
}

DeviceRecord& Registry::on_visor_telemetry(const VisorTelemetry& t) {
    auto& rec = ensure(t.device_id, DeviceType::Visor);
    rec.last_telemetry = now_s();
    rec.pose = t.pose; rec.battery_pct = t.battery_pct; rec.link = t.link; rec.state = t.state;
    if (t.wearer_callsign) rec.callsign = t.wearer_callsign;
    json e = json::object();
    e["wearer_id"]    = t.wearer_id;
    e["gas"]          = t.gas;
    e["gas_flags"]    = t.gas.danger_flags();
    e["ambient_temp_c"] = t.ambient_temp_c;
    e["vitals"]       = t.vitals;
    e["compute_temp_c"] = t.compute_temp_c;
    rec.extra = std::move(e);
    return rec;
}

DeviceRecord& Registry::on_scout_telemetry(const ScoutTelemetry& t) {
    auto& rec = ensure(t.device_id, DeviceType::Scout);
    rec.last_telemetry = now_s();
    rec.pose = t.pose; rec.battery_pct = t.battery_pct; rec.link = t.link; rec.state = t.state;
    json e = json::object();
    e["flight_mode"]  = t.flight_mode;
    e["armed"]        = t.armed;
    e["altitude_m"]   = t.altitude_m;
    e["groundspeed_ms"] = t.groundspeed_ms;
    e["gps_fix"]      = t.gps_fix;
    e["satellites"]   = t.satellites;
    e["gas"]          = t.gas;
    e["gas_flags"]    = t.gas.danger_flags();
    e["motor_count_ok"] = t.motor_count_ok;
    e["flight_time_remaining_s"] = t.flight_time_remaining_s;
    rec.extra = std::move(e);
    return rec;
}

DeviceRecord& Registry::on_apparatus_telemetry(const ApparatusTelemetry& t) {
    auto& rec = ensure(t.device_id, DeviceType::Apparatus);
    rec.last_telemetry = now_s();
    rec.pose = t.pose; rec.battery_pct = t.battery_pct; rec.link = t.link; rec.state = t.state;
    json e = json::object();
    e["mech_state"] = t.mech_state;
    e["position_pct"] = t.position_pct;
    e["ambient_temp_c"] = t.ambient_temp_c;
    e["gas"] = t.gas;
    e["gas_flags"] = t.gas.danger_flags();
    rec.extra = std::move(e);
    return rec;
}

std::vector<std::string> Registry::sweep_offline() {
    const double t = now_s();
    std::vector<std::string> newly;
    for (auto& [_, rec] : devices_) {
        if (rec.state == DeviceState::Offline) continue;
        if (t - std::max(rec.last_heartbeat, rec.last_telemetry) > offline_after_s_) {
            rec.state = DeviceState::Offline;
            rec.link = LinkQuality::Lost;
            newly.push_back(rec.device_id);
        }
    }
    return newly;
}

}  // namespace frize::core
