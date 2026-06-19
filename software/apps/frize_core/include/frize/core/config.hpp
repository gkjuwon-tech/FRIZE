// frize/core/config.hpp ― 코어 설정 (환경변수 주입, FRIZE_ 접두)
#pragma once

#include <cstdlib>
#include <string>

namespace frize::core {

inline std::string env_s(const char* key, const std::string& def) {
    const char* v = std::getenv(key);
    return v ? std::string(v) : def;
}
inline int env_i(const char* key, int def) {
    const char* v = std::getenv(key); return v ? std::atoi(v) : def;
}
inline double env_d(const char* key, double def) {
    const char* v = std::getenv(key); return v ? std::atof(v) : def;
}
inline bool env_b(const char* key, bool def) {
    const char* v = std::getenv(key);
    if (!v) return def;
    std::string s(v);
    return s=="1"||s=="true"||s=="TRUE"||s=="yes"||s=="on";
}

struct Config {
    // 버스
    std::string mqtt_host = env_s("FRIZE_MQTT_HOST", "localhost");
    int         mqtt_port = env_i("FRIZE_MQTT_PORT", 1883);
    std::string mqtt_user = env_s("FRIZE_MQTT_USER", "");
    std::string mqtt_pass = env_s("FRIZE_MQTT_PASS", "");

    // HTTP/WS
    std::string http_host = env_s("FRIZE_HTTP_HOST", "0.0.0.0");
    int         http_port = env_i("FRIZE_HTTP_PORT", 8000);

    // 사이트 원점(출동 지점)
    double site_lat = env_d("FRIZE_SITE_LAT", 37.5665);
    double site_lon = env_d("FRIZE_SITE_LON", 126.9780);
    double site_alt = env_d("FRIZE_SITE_ALT", 30.0);

    // VLM 판단 계층
    std::string anthropic_api_key = env_s("FRIZE_ANTHROPIC_API_KEY", env_s("ANTHROPIC_API_KEY", ""));
    std::string vlm_model        = env_s("FRIZE_VLM_MODEL", "claude-opus-4-8");
    bool        vlm_enabled      = env_b("FRIZE_VLM_ENABLED", true);
    double      vlm_min_interval_s = env_d("FRIZE_VLM_MIN_INTERVAL_S", 1.5);
    int         vlm_max_concurrency = env_i("FRIZE_VLM_MAX_CONCURRENCY", 4);

    double device_offline_after_s = env_d("FRIZE_DEVICE_OFFLINE_AFTER_S", 8.0);
    bool   interlock_enabled = env_b("FRIZE_INTERLOCK_ENABLED", true);
};

}  // namespace frize::core
