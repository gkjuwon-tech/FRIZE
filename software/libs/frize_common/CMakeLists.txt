add_library(frize_common STATIC
    src/bus.cpp
)
target_include_directories(frize_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(frize_common PUBLIC cxx_std_17)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(PahoMqttCpp CONFIG REQUIRED)

target_link_libraries(frize_common PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    PahoMqttCpp::paho-mqttpp3
)

# Threads (Paho)
find_package(Threads REQUIRED)
target_link_libraries(frize_common PUBLIC Threads::Threads)

if(FRIZE_BUILD_TESTS)
    add_executable(test_common test/test_schemas.cpp)
    target_link_libraries(test_common PRIVATE frize_common)
    add_test(NAME test_common COMMAND test_common)
endif()
