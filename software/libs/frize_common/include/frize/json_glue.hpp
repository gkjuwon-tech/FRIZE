// frize/json_glue.hpp ― nlohmann/json 확장 직렬화기
//
// std::optional<T> 직렬화 지원(기본 미지원). 값 없으면 null.
// 이 헤더는 스키마 정의보다 반드시 먼저 포함되어야 한다(ADL 가시성).
#pragma once

#include <optional>
#include <nlohmann/json.hpp>

namespace nlohmann {

template <typename T>
struct adl_serializer<std::optional<T>> {
    static void to_json(json& j, const std::optional<T>& opt) {
        if (opt.has_value()) j = *opt;
        else j = nullptr;
    }
    static void from_json(const json& j, std::optional<T>& opt) {
        if (j.is_null()) opt = std::nullopt;
        else opt = j.get<T>();
    }
};

}  // namespace nlohmann
