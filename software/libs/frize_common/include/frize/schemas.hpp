// frize/schemas.hpp ― FRIZE 데이터 스키마 (단일 진실원천)
//
// 소방 도메인 모델. 모든 디바이스/콕핏이 이 구조체로 직렬화한다.
// nlohmann/json 직렬화는 파일 하단에 정의(NLOHMANN_* 매크로).
//
// 실시간성: 이 구조체들은 값 타입(POD에 가깝게)으로 설계해 복사/이동이 싸다.
// 핫 경로에서는 JSON 변환을 피하고 구조체 자체로 다룬다(직렬화는 버스 경계에서만).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <array>
#include <chrono>

#include <nlohmann/json.hpp>
#include "frize/json_glue.hpp"  // std::optional 직렬화 (스키마보다 먼저)

namespace frize {

using json = nlohmann::json;

inline double now_s() {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

// ===================== 열거형 =====================
enum class DeviceType { Visor, Scout, Apparatus, Cockpit, Core };
enum class DeviceState { Booting, Online, Degraded, Critical, Offline };
enum class LinkQuality { G5, Wifi, Mesh, Lost };
enum class FlightMode { Disarmed, Manual, Loiter, Auto, Recon, Rtl, Land };

enum class Severity { Info, Low, High, Critical };

inline int severity_rank(Severity s) { return static_cast<int>(s); }
inline const char* severity_color(Severity s) {
    switch (s) {
        case Severity::Info:     return "#9aa0a6";
        case Severity::Low:      return "#f5c518";
        case Severity::High:     return "#ff8c00";
        case Severity::Critical: return "#e0241c";
    }
    return "#9aa0a6";
}

enum class HazardClass {
    Person, DownedPerson, Fire, Hotspot, Smoke, GasCylinder,
    Structural, Backdraft, BlockedExit, Electrical, Unknown
};

enum class CommandAction {
    MoveTo, Highlight, Annotate,
    Advance, Retreat, Rally, ForceEntry,
    ReconZone, InvestigatePoint,    // 드론 명시적 태스킹: "저기 먼저 조사해!"(자율탐사 가로채기)
    DeployAnchor,                   // 드론이 UWB 측위 비콘을 현 위치/지정점에 투하
    Hold, Rtl, Land,
    AimAndSpray,
    Open, Close            // IoT 소방장비(VENT-1): 포트/배연구/문 열기·닫기
};

// ── AR 큐(고글에 띄우는 시각 명령): 텍스트 / 화살표 / 마커 / 경로 / 강조 ──
//   (구조체 본문은 Vec3 정의 이후, 명령 섹션에 둔다)
enum class ArCueKind { Text, Arrow, Marker, Route, Highlight, Warning };

enum class CommandStatus {
    Queued, Rejected, NeedsConfirm, Sent, Acked, Executing, Done, Failed
};

enum class AlertKind { Gas, Vitals, Structural, Device, Hazard, Evacuate };

// ===================== 기하 =====================
struct Vec3 { double x{0}, y{0}, z{0}; };
struct Quat { double w{1}, x{0}, y{0}, z{0}; };
struct GeoPoint { double lat{0}, lon{0}, alt_m{0}; };

struct Pose {
    Vec3 position{};
    Quat orientation{};
    std::optional<GeoPoint> geo{};
    std::string frame{"site_enu"};
};

// ===================== 센서/텔레메트리 =====================
struct GasReading {
    double co_ppm{0};
    double o2_vol_pct{20.9};
    double lel_pct{0};
    double h2s_ppm{0};
    double hcn_ppm{0};
    double ts{now_s()};

    // 현장 안전 기준(IDLH 근사) 위험 플래그
    std::vector<std::string> danger_flags() const {
        std::vector<std::string> f;
        if (co_ppm >= 1200) f.push_back("CO_IDLH");
        else if (co_ppm >= 35) f.push_back("CO_HIGH");
        if (o2_vol_pct < 19.5) f.push_back("O2_DEFICIENT");
        if (o2_vol_pct > 23.5) f.push_back("O2_ENRICHED");
        if (lel_pct >= 10) f.push_back("LEL_EXPLOSIVE");
        else if (lel_pct >= 5) f.push_back("LEL_WARN");
        if (h2s_ppm >= 100) f.push_back("H2S_IDLH");
        if (hcn_ppm >= 50) f.push_back("HCN_IDLH");
        return f;
    }
};

struct WearerVitals {
    std::optional<int> heart_rate_bpm{};
    std::optional<double> skin_temp_c{};
    std::string motion_state{"active"};        // active / still / fall_detected
    std::optional<double> air_pressure_bar{};  // SCBA 잔압
};

struct Heartbeat {
    std::string device_id;
    DeviceType device_type{DeviceType::Visor};
    std::string fw_version{"0.1.0"};
    DeviceState state{DeviceState::Online};
    double ts{now_s()};
};

struct VisorTelemetry {
    std::string device_id;
    DeviceType device_type{DeviceType::Visor};
    double ts{now_s()};
    DeviceState state{DeviceState::Online};
    LinkQuality link{LinkQuality::G5};
    Pose pose{};
    double battery_pct{100.0};

    std::optional<std::string> wearer_id{};
    std::optional<std::string> wearer_callsign{};
    GasReading gas{};
    double ambient_temp_c{25.0};
    WearerVitals vitals{};
    double compute_temp_c{0.0};

    // 측위(NAV 포드): 실내 UWB / 옥외 GNSS / IMU 추측항법 융합
    std::string pos_source{"none"};   // none / uwb / gnss / fused / dead_reckon
    double pos_accuracy_m{99.0};       // 위치 추정 오차(작을수록 정확)
};

struct ScoutTelemetry {
    std::string device_id;
    DeviceType device_type{DeviceType::Scout};
    double ts{now_s()};
    DeviceState state{DeviceState::Online};
    LinkQuality link{LinkQuality::G5};
    Pose pose{};
    double battery_pct{100.0};

    FlightMode flight_mode{FlightMode::Disarmed};
    bool armed{false};
    double altitude_m{0};
    double groundspeed_ms{0};
    int gps_fix{0};
    int satellites{0};
    GasReading gas{};
    int motor_count_ok{6};
    double flight_time_remaining_s{0};
};

// IoT 소방장비(VENT-1 등). state: closed/opening/open/closing/fault, position 0~100%.
struct ApparatusTelemetry {
    std::string device_id;
    DeviceType device_type{DeviceType::Apparatus};
    double ts{now_s()};
    DeviceState state{DeviceState::Online};
    LinkQuality link{LinkQuality::Mesh};
    Pose pose{};
    double battery_pct{100.0};

    std::string mech_state{"closed"};   // closed/opening/open/closing/fault
    double position_pct{0.0};            // 개방도 0(닫힘)~100(완전개방)
    double ambient_temp_c{25.0};
    GasReading gas{};
};

// ===================== 감지/판단 =====================
struct Detection {
    std::string det_id;
    std::string source_device;
    HazardClass hazard{HazardClass::Unknown};
    Severity severity{Severity::Info};
    double confidence{0.0};
    std::string label;
    std::string rationale;                       // ★ VLM 판단 근거(블랙박스 금지)
    std::optional<std::array<double,4>> bbox{};  // [x,y,w,h] 정규화 0~1
    std::optional<Vec3> world_pos{};
    double ts{now_s()};
};

// ===================== 명령 =====================
struct Command {
    std::string cmd_id;
    std::string target_device;
    CommandAction action{CommandAction::Highlight};
    json params = json::object();
    std::string issued_by;
    double issued_at{now_s()};
    bool requires_confirm{false};
    bool confirmed{false};
    std::string reason;
};

struct CommandAck {
    std::string cmd_id;
    std::string device_id;
    CommandStatus status{CommandStatus::Acked};
    std::string detail;
    double ts{now_s()};
};

// ── 페어링(콕핏 ↔ 디바이스 핸드셰이크) ──
struct PairRequest {
    std::string device_id;       // 페어링 대상
    std::string console_id;      // 요청한 콘솔
    std::string session_id;      // 세션 토큰
    double ts{now_s()};
};
struct PairGrant {
    std::string device_id;
    std::string console_id;
    std::string session_id;
    bool accepted{true};
    DeviceType device_type{DeviceType::Visor};
    std::string fw_version{"0.1.0"};
    std::vector<std::string> capabilities;   // 예: ["thermal","gas","ar","uwb"]
    double ts{now_s()};
};

// 유도 요청(콕핏 → 내비 서비스): 대원에게 목표점 설정 또는 해제.
//  트윈에서 목표 찍고 대원 찍으면 콕핏이 이걸 발행 → 내비가 경로를 계속 갱신.
struct GuideRequest {
    std::string wearer_id;                 // 유도 대상 대원(고글)
    std::optional<Vec3> target{};          // 목표점(site_enu). 없으면 해제.
    std::string label{"목표"};             // 표시 라벨(요구조자/출구/가스밸브…)
    std::string color{"#36c0ff"};          // 경로 색
    bool active{true};                     // false=유도 중지
    std::string issued_by{"console"};
    double ts{now_s()};
};

// 고글 AR 시각 명령(텍스트/화살표/마커/경로/강조/경고)
struct ArCue {
    std::string cue_id;
    std::string target_device;                 // 어느 대원 고글에
    ArCueKind kind{ArCueKind::Text};
    std::string text;                          // Text/Warning 내용
    std::optional<Vec3> world_pos{};           // Marker/Arrow 시작 또는 앵커(site_enu)
    std::optional<Vec3> world_to{};            // Arrow 끝점
    std::vector<Vec3> route{};                 // Route 경유점들
    std::string color{"#e0a83a"};              // 표시 색(헥스)
    Severity severity{Severity::Info};
    double ttl_s{8.0};                         // 표시 지속(초), 0=수동 해제까지
    std::string issued_by{"console"};
    double ts{now_s()};
};

// ===================== 경보 =====================
struct Alert {
    std::string alert_id;
    AlertKind kind{AlertKind::Device};
    Severity severity{Severity::Info};
    std::string title;
    std::string message;
    std::optional<std::string> device_id{};
    std::optional<Vec3> world_pos{};
    double ts{now_s()};
    bool acknowledged{false};
};

// ===================== 월드 모델 =====================
struct DeviceSummary {
    std::string device_id;
    DeviceType device_type{DeviceType::Visor};
    DeviceState state{DeviceState::Booting};
    std::optional<std::string> callsign{};
    Pose pose{};
    double battery_pct{100.0};
    LinkQuality link{LinkQuality::G5};
    double last_seen{now_s()};
    json extra = json::object();   // 타입별 부가(가스/비행모드 등)
};

struct HazardZone {
    std::string zone_id;
    HazardClass hazard{HazardClass::Unknown};
    Severity severity{Severity::Info};
    Vec3 center{};
    double radius_m{3.0};
    std::string rationale;
    double ts{now_s()};
};

struct WorldSnapshot {
    double ts{now_s()};
    std::vector<DeviceSummary> devices;
    std::vector<Detection> detections;
    std::vector<HazardZone> hazard_zones;
    std::vector<Alert> active_alerts;
    std::optional<GeoPoint> site_origin{};
};

// ===================== 맵/영상 =====================
struct MapPatch {
    std::string patch_id;
    Vec3 origin{};
    double voxel_size_m{0.25};
    std::array<int,3> dims{0,0,0};
    std::vector<std::array<int,4>> occupied;        // [ix,iy,iz,occ0_255]
    std::vector<std::array<double,4>> thermal;       // [ix,iy,iz,temp_c]
    std::vector<std::array<int,3>> frontiers;        // 탐사 경계(미탐사와 맞닿은 자유공간) [ix,iy,iz]
    int explored_cells{0};                            // 누적 탐사 셀 수(진행률)
    double ts{now_s()};
};

struct VideoFrameMeta {
    std::string device_id;
    int64_t seq{0};
    std::string stream{"thermal"};      // thermal / rgb / fused
    int width{0};
    int height{0};
    std::string codec{"h264"};
    Pose pose{};
    std::optional<std::string> jpeg_b64{};   // 저대역 폴백 키프레임
    double ts{now_s()};
};

// ===================== JSON 직렬화 =====================
// 열거형 ↔ 문자열 (와이어 포맷은 Python/TS와 동일한 snake/lower 문자열)
NLOHMANN_JSON_SERIALIZE_ENUM(DeviceType, {
    {DeviceType::Visor,"visor"},{DeviceType::Scout,"scout"},
    {DeviceType::Apparatus,"apparatus"},{DeviceType::Cockpit,"cockpit"},{DeviceType::Core,"core"}})
NLOHMANN_JSON_SERIALIZE_ENUM(DeviceState, {
    {DeviceState::Booting,"booting"},{DeviceState::Online,"online"},
    {DeviceState::Degraded,"degraded"},{DeviceState::Critical,"critical"},{DeviceState::Offline,"offline"}})
NLOHMANN_JSON_SERIALIZE_ENUM(LinkQuality, {
    {LinkQuality::G5,"5g"},{LinkQuality::Wifi,"wifi"},{LinkQuality::Mesh,"mesh"},{LinkQuality::Lost,"lost"}})
NLOHMANN_JSON_SERIALIZE_ENUM(FlightMode, {
    {FlightMode::Disarmed,"disarmed"},{FlightMode::Manual,"manual"},{FlightMode::Loiter,"loiter"},
    {FlightMode::Auto,"auto"},{FlightMode::Recon,"recon"},{FlightMode::Rtl,"rtl"},{FlightMode::Land,"land"}})
NLOHMANN_JSON_SERIALIZE_ENUM(Severity, {
    {Severity::Info,"info"},{Severity::Low,"low"},{Severity::High,"high"},{Severity::Critical,"critical"}})
NLOHMANN_JSON_SERIALIZE_ENUM(HazardClass, {
    {HazardClass::Person,"person"},{HazardClass::DownedPerson,"downed_person"},{HazardClass::Fire,"fire"},
    {HazardClass::Hotspot,"hotspot"},{HazardClass::Smoke,"smoke"},{HazardClass::GasCylinder,"gas_cylinder"},
    {HazardClass::Structural,"structural"},{HazardClass::Backdraft,"backdraft"},
    {HazardClass::BlockedExit,"blocked_exit"},{HazardClass::Electrical,"electrical"},{HazardClass::Unknown,"unknown"}})
NLOHMANN_JSON_SERIALIZE_ENUM(CommandAction, {
    {CommandAction::MoveTo,"move_to"},{CommandAction::Highlight,"highlight"},{CommandAction::Annotate,"annotate"},
    {CommandAction::Advance,"advance"},{CommandAction::Retreat,"retreat"},{CommandAction::Rally,"rally"},
    {CommandAction::ForceEntry,"force_entry"},{CommandAction::ReconZone,"recon_zone"},
    {CommandAction::InvestigatePoint,"investigate_point"},{CommandAction::DeployAnchor,"deploy_anchor"},
    {CommandAction::Hold,"hold"},
    {CommandAction::Rtl,"rtl"},{CommandAction::Land,"land"},{CommandAction::AimAndSpray,"aim_and_spray"},
    {CommandAction::Open,"open"},{CommandAction::Close,"close"}})
NLOHMANN_JSON_SERIALIZE_ENUM(ArCueKind, {
    {ArCueKind::Text,"text"},{ArCueKind::Arrow,"arrow"},{ArCueKind::Marker,"marker"},
    {ArCueKind::Route,"route"},{ArCueKind::Highlight,"highlight"},{ArCueKind::Warning,"warning"}})
NLOHMANN_JSON_SERIALIZE_ENUM(CommandStatus, {
    {CommandStatus::Queued,"queued"},{CommandStatus::Rejected,"rejected"},{CommandStatus::NeedsConfirm,"needs_confirm"},
    {CommandStatus::Sent,"sent"},{CommandStatus::Acked,"acked"},{CommandStatus::Executing,"executing"},
    {CommandStatus::Done,"done"},{CommandStatus::Failed,"failed"}})
NLOHMANN_JSON_SERIALIZE_ENUM(AlertKind, {
    {AlertKind::Gas,"gas"},{AlertKind::Vitals,"vitals"},{AlertKind::Structural,"structural"},
    {AlertKind::Device,"device"},{AlertKind::Hazard,"hazard"},{AlertKind::Evacuate,"evacuate"}})

// 구조체 직렬화 (WITH_DEFAULT: 누락 필드는 기본값 → 버전 호환성↑)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Vec3, x, y, z)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Quat, w, x, y, z)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GeoPoint, lat, lon, alt_m)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Pose, position, orientation, geo, frame)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GasReading, co_ppm, o2_vol_pct, lel_pct, h2s_ppm, hcn_ppm, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WearerVitals, heart_rate_bpm, skin_temp_c, motion_state, air_pressure_bar)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Heartbeat, device_id, device_type, fw_version, state, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(VisorTelemetry, device_id, device_type, ts, state, link, pose,
    battery_pct, wearer_id, wearer_callsign, gas, ambient_temp_c, vitals, compute_temp_c,
    pos_source, pos_accuracy_m)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ArCue, cue_id, target_device, kind, text, world_pos, world_to,
    route, color, severity, ttl_s, issued_by, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GuideRequest, wearer_id, target, label, color, active, issued_by, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PairRequest, device_id, console_id, session_id, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PairGrant, device_id, console_id, session_id, accepted,
    device_type, fw_version, capabilities, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ScoutTelemetry, device_id, device_type, ts, state, link, pose,
    battery_pct, flight_mode, armed, altitude_m, groundspeed_ms, gps_fix, satellites, gas,
    motor_count_ok, flight_time_remaining_s)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApparatusTelemetry, device_id, device_type, ts, state, link, pose,
    battery_pct, mech_state, position_pct, ambient_temp_c, gas)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Detection, det_id, source_device, hazard, severity, confidence,
    label, rationale, bbox, world_pos, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Command, cmd_id, target_device, action, params, issued_by,
    issued_at, requires_confirm, confirmed, reason)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CommandAck, cmd_id, device_id, status, detail, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Alert, alert_id, kind, severity, title, message, device_id,
    world_pos, ts, acknowledged)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DeviceSummary, device_id, device_type, state, callsign, pose,
    battery_pct, link, last_seen, extra)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HazardZone, zone_id, hazard, severity, center, radius_m, rationale, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WorldSnapshot, ts, devices, detections, hazard_zones,
    active_alerts, site_origin)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MapPatch, patch_id, origin, voxel_size_m, dims, occupied, thermal, frontiers, explored_cells, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(VideoFrameMeta, device_id, seq, stream, width, height, codec,
    pose, jpeg_b64, ts)

}  // namespace frize
