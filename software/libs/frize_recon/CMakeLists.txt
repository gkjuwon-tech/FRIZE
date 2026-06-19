# frize_recon ― 프로덕션 3D 재구성 엔진 (헤더 온리, 의존성 0)
#   TSDF 융합 + 마칭큐브 표면 추출 + glTF/PLY/OBJ export.
#   순수 C++17만 쓰므로 Jetson/Linux/CI 어디서나 빌드된다(무거운 SDK 불필요).
add_library(frize_recon INTERFACE)
target_include_directories(frize_recon INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(frize_recon INTERFACE cxx_std_17)
