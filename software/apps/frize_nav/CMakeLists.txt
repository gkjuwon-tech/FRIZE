# frize_nav ― AR 내비게이션 서비스 + 엔진 데모/테스트
add_executable(frize_nav src/main.cpp)
target_compile_features(frize_nav PRIVATE cxx_std_17)
target_link_libraries(frize_nav PRIVATE frize_common)

# 엔진 데모/단위 테스트(헤더 온리 nav + sim 건물, 의존성 0)
set(_NAV_SIM_INC
    ${CMAKE_SOURCE_DIR}/libs/frize_common/include
    ${CMAKE_SOURCE_DIR}/libs/frize_recon/include
    ${CMAKE_SOURCE_DIR}/apps/frize_recon_sim/include)

add_executable(frize_nav_demo demo.cpp)
target_compile_features(frize_nav_demo PRIVATE cxx_std_17)
target_include_directories(frize_nav_demo PRIVATE ${_NAV_SIM_INC})

# 정적 장애물 JSON 덤퍼(e2e.sh 가 사용: sim 건물 → 점유격자 시드)
add_executable(frize_nav_dump_obstacles dump_obstacles.cpp)
target_compile_features(frize_nav_dump_obstacles PRIVATE cxx_std_17)
target_include_directories(frize_nav_dump_obstacles PRIVATE ${_NAV_SIM_INC})

if(FRIZE_BUILD_TESTS)
    add_executable(frize_nav_test test_nav.cpp)
    target_compile_features(frize_nav_test PRIVATE cxx_std_17)
    target_include_directories(frize_nav_test PRIVATE ${_NAV_SIM_INC})
    add_test(NAME nav_pathfinding COMMAND frize_nav_test)
endif()
