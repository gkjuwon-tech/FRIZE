add_executable(frize_core
    src/main.cpp
    src/registry.cpp
    src/world_model.cpp
    src/safety.cpp
    src/router.cpp
    src/vlm_client.cpp
)

target_include_directories(frize_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(frize_core PRIVATE cxx_std_17)

target_link_libraries(frize_core PRIVATE frize_common)

# Crow (HTTP/WS 서버) ― 콕핏 연결
find_package(Crow CONFIG QUIET)
if(Crow_FOUND)
    target_link_libraries(frize_core PRIVATE Crow::Crow)
else()
    # vcpkg 포트명이 다를 때 헤더-온리 fallback
    find_path(CROW_INCLUDE_DIR crow.h)
    target_include_directories(frize_core PRIVATE ${CROW_INCLUDE_DIR})
endif()

# cpp-httplib (Anthropic VLM HTTPS 클라이언트)
find_package(httplib CONFIG QUIET)
if(httplib_FOUND)
    target_link_libraries(frize_core PRIVATE httplib::httplib)
else()
    find_path(HTTPLIB_INCLUDE_DIR httplib.h)
    target_include_directories(frize_core PRIVATE ${HTTPLIB_INCLUDE_DIR})
endif()

# OpenSSL (httplib HTTPS)
find_package(OpenSSL REQUIRED)
target_link_libraries(frize_core PRIVATE OpenSSL::SSL OpenSSL::Crypto)

if(WIN32)
    target_link_libraries(frize_core PRIVATE ws2_32 crypt32)
endif()
