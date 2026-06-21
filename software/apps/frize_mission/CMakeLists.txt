# frize_mission ― 작전 시뮬 타임라인 생성기(헤더 온리 recon + sim, OpenMP)
add_executable(frize_mission main.cpp)
target_compile_features(frize_mission PRIVATE cxx_std_17)
target_include_directories(frize_mission PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/frize_recon/include
    ${CMAKE_SOURCE_DIR}/apps/frize_recon_sim/include)
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(frize_mission PRIVATE OpenMP::OpenMP_CXX)
endif()
