// frize/geo.hpp ― 좌표/기하 (위경도 ↔ 사이트 로컬 ENU)
//
// 현장 지휘는 미터 단위 로컬 좌표가 직관적이다. 출동 지점을 원점으로
// 잡고 ENU(동-북-상) 평면 투영. 근거리(수 km)에서 등거리 근사로 충분.
#pragma once

#include <cmath>
#include "frize/schemas.hpp"

namespace frize::geo {

inline constexpr double WGS84_A = 6378137.0;
inline constexpr double DEG2RAD = 3.14159265358979323846 / 180.0;
inline constexpr double RAD2DEG = 180.0 / 3.14159265358979323846;

inline Vec3 geo_to_enu(const GeoPoint& p, const GeoPoint& origin) {
    const double lat0 = origin.lat * DEG2RAD;
    const double dlat = (p.lat - origin.lat) * DEG2RAD;
    const double dlon = (p.lon - origin.lon) * DEG2RAD;
    return Vec3{ dlon * WGS84_A * std::cos(lat0), dlat * WGS84_A, p.alt_m - origin.alt_m };
}

inline GeoPoint enu_to_geo(const Vec3& v, const GeoPoint& origin) {
    const double lat0 = origin.lat * DEG2RAD;
    const double dlat = v.y / WGS84_A;
    const double dlon = v.x / (WGS84_A * std::cos(lat0));
    return GeoPoint{ origin.lat + dlat * RAD2DEG, origin.lon + dlon * RAD2DEG, origin.alt_m + v.z };
}

inline double distance(const Vec3& a, const Vec3& b) {
    const double dx=a.x-b.x, dy=a.y-b.y, dz=a.z-b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}
inline double horizontal_distance(const Vec3& a, const Vec3& b) {
    return std::hypot(a.x-b.x, a.y-b.y);
}

inline double yaw_from_quat(const Quat& q) {
    const double siny = 2.0 * (q.w*q.z + q.x*q.y);
    const double cosy = 1.0 - 2.0 * (q.y*q.y + q.z*q.z);
    return std::atan2(siny, cosy);
}
inline Quat quat_from_yaw(double yaw) {
    return Quat{ std::cos(yaw/2), 0.0, 0.0, std::sin(yaw/2) };
}

// frm에서 to를 향하는 방위각(0=북, 시계방향 deg). 고글 화살표 방향.
inline double bearing_deg(const Vec3& frm, const Vec3& to) {
    double b = std::atan2(to.x - frm.x, to.y - frm.y) * RAD2DEG;
    return std::fmod(b + 360.0, 360.0);
}

}  // namespace frize::geo
