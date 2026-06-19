// frize/base64.hpp ― 의존성 없는 base64 인코더(영상 키프레임 등)
#pragma once
#include <string>
#include <cstddef>

namespace frize {

inline std::string base64_encode(const unsigned char* data, std::size_t len) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    for (; i + 2 < len; i += 3) {
        unsigned n = (data[i] << 16) | (data[i+1] << 8) | data[i+2];
        out.push_back(T[(n>>18)&63]); out.push_back(T[(n>>12)&63]);
        out.push_back(T[(n>>6)&63]);  out.push_back(T[n&63]);
    }
    if (i < len) {
        unsigned n = data[i] << 16;
        if (i + 1 < len) n |= data[i+1] << 8;
        out.push_back(T[(n>>18)&63]); out.push_back(T[(n>>12)&63]);
        out.push_back(i + 1 < len ? T[(n>>6)&63] : '=');
        out.push_back('=');
    }
    return out;
}

}  // namespace frize
