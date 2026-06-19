# frize_recon_sim ― 디지털 트윈 재구성 시뮬레이션 (프로덕션 엔진 end-to-end 검증)
#   합성 건물 + 드론/대원 LiDAR → frize_recon TSDF → 메쉬 export + 품질 리포트.
#   의존성 0 → MQTT/Qt/PCL 없이 단독 빌드·실행. CI 스모크 테스트로도 적합.
add_executable(frize_recon_sim src/main.cpp)
target_include_directories(frize_recon_sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(frize_recon_sim PRIVATE frize_recon)
target_compile_features(frize_recon_sim PRIVATE cxx_std_17)

find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(frize_recon_sim PRIVATE OpenMP::OpenMP_CXX)
endif()

if(FRIZE_BUILD_TESTS)
    # 스모크: 굵은 복셀로 빠르게 돌려 메쉬가 생성되는지(>0 삼각형) 확인
    add_test(NAME recon_sim_smoke
             COMMAND frize_recon_sim ${CMAKE_CURRENT_BINARY_DIR}/recon_out 0.18)
endif()
