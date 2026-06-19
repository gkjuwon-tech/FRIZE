// frize_common 단위 테스트 ― 직렬화 왕복 + 도메인 로직 검증
#include "frize/schemas.hpp"
#include "frize/protocol.hpp"
#include "frize/geo.hpp"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace frize;

static int failures = 0;
#define CHECK(cond) do { if(!(cond)) { std::printf("FAIL: %s (line %d)\n", #cond, __LINE__); ++failures; } } while(0)

int main() {
    // 1) 가스 위험 플래그
    {
        GasReading g; g.co_ppm = 1500; g.o2_vol_pct = 18.0; g.lel_pct = 12;
        auto f = g.danger_flags();
        bool co = false, o2 = false, lel = false;
        for (auto& s : f) { co |= (s=="CO_IDLH"); o2 |= (s=="O2_DEFICIENT"); lel |= (s=="LEL_EXPLOSIVE"); }
        CHECK(co); CHECK(o2); CHECK(lel);
    }

    // 2) VisorTelemetry JSON 왕복
    {
        VisorTelemetry t; t.device_id = "visor-01"; t.battery_pct = 73.5;
        t.gas.co_ppm = 42; t.wearer_callsign = std::string("ALPHA-1");
        t.vitals.heart_rate_bpm = 150;
        json j = t;
        auto t2 = j.get<VisorTelemetry>();
        CHECK(t2.device_id == "visor-01");
        CHECK(std::abs(t2.battery_pct - 73.5) < 1e-9);
        CHECK(t2.gas.co_ppm == 42);
        CHECK(t2.wearer_callsign.has_value() && *t2.wearer_callsign == "ALPHA-1");
        CHECK(t2.vitals.heart_rate_bpm.has_value() && *t2.vitals.heart_rate_bpm == 150);
        CHECK(t2.device_type == DeviceType::Visor);
    }

    // 3) optional 비어있을 때 null 직렬화
    {
        Detection d; d.det_id="x"; d.hazard=HazardClass::DownedPerson; d.severity=Severity::Critical;
        d.confidence=0.91; d.rationale="열화상에서 사람 형상 + 무동작";
        json j = d;
        CHECK(j["bbox"].is_null());
        CHECK(j["world_pos"].is_null());
        CHECK(j["hazard"] == "downed_person");
        CHECK(j["severity"] == "critical");
        auto d2 = j.get<Detection>();
        CHECK(!d2.bbox.has_value());
        CHECK(d2.confidence > 0.9);
    }

    // 4) bbox 직렬화
    {
        Detection d; d.det_id="y"; d.bbox = std::array<double,4>{0.1,0.2,0.3,0.4};
        json j = d;
        auto d2 = j.get<Detection>();
        CHECK(d2.bbox.has_value());
        CHECK(std::abs((*d2.bbox)[2] - 0.3) < 1e-9);
    }

    // 5) Envelope wrap/unwrap
    {
        Command c; c.cmd_id="c1"; c.target_device="scout-01"; c.action=CommandAction::ReconZone;
        c.issued_by="cmd"; c.params["zone_id"]="z3";
        auto env = Envelope::wrap(MessageType::Command, "core", c);
        auto raw = env.to_string();
        auto back = Envelope::from_string(raw);
        CHECK(back.type == MessageType::Command);
        auto c2 = back.as<Command>();
        CHECK(c2.target_device == "scout-01");
        CHECK(c2.action == CommandAction::ReconZone);
        CHECK(c2.params["zone_id"] == "z3");
    }

    // 6) Topic 파싱
    {
        auto id = Topic::device_id_from("frize/device/visor-07/telemetry");
        CHECK(id.has_value() && *id == "visor-07");
        auto id2 = Topic::device_id_from("frize/command/scout-02/ack");
        CHECK(id2.has_value() && *id2 == "scout-02");
        CHECK(!Topic::device_id_from("frize/alert").has_value());
    }

    // 7) geo ENU 왕복 (~서울시청 근처)
    {
        GeoPoint origin{37.5665, 126.9780, 30.0};
        GeoPoint p{37.5670, 126.9788, 45.0};
        auto enu = geo::geo_to_enu(p, origin);
        auto back = geo::enu_to_geo(enu, origin);
        CHECK(std::abs(back.lat - p.lat) < 1e-6);
        CHECK(std::abs(back.lon - p.lon) < 1e-6);
        CHECK(std::abs(back.alt_m - p.alt_m) < 1e-6);
        CHECK(enu.y > 0);  // 북쪽으로 이동
    }

    // 8) bearing
    {
        Vec3 a{0,0,0}, north{0,10,0}, east{10,0,0};
        CHECK(std::abs(geo::bearing_deg(a, north) - 0.0) < 1e-6);
        CHECK(std::abs(geo::bearing_deg(a, east) - 90.0) < 1e-6);
    }

    if (failures == 0) std::printf("ALL TESTS PASSED\n");
    else std::printf("%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
