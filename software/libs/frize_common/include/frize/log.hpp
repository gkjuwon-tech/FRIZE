// frize/log.hpp ― 공용 로깅 (spdlog 래퍼)
//
// 실시간 경로에서는 로그도 비용이다. 핫 루프에서는 LOG_TRACE 를 컴파일아웃하거나
// 레벨로 거른다. 기본 패턴은 [시각][컴포넌트][레벨] 메시지.
#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>

namespace frize {

inline std::shared_ptr<spdlog::logger> make_logger(const std::string& name) {
    auto existing = spdlog::get(name);
    if (existing) return existing;
    auto lg = spdlog::stdout_color_mt(name);
    lg->set_pattern("[%H:%M:%S.%e][%n][%^%l%$] %v");
    lg->set_level(spdlog::level::info);
    return lg;
}

}  // namespace frize
