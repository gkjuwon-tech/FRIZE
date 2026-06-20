cmake_minimum_required(VERSION 3.21)
project(FRIZE VERSION 0.1.0 LANGUAGES CXX)

# ============================================================
#  FRIZE 소프트웨어 모노레포 (C++17)
#  - libs/frize_common : 공유 프로토콜/스키마/버스/기하
#  - apps/frize_core   : 지휘 커널 (필수)
#  - apps/frize_scout  : 정찰 드론 (Jetson, MAVSDK+OpenCV)   [옵션]
#  - apps/frize_mapping: 3D 맵핑/SLAM (PCL+Eigen)            [옵션]
#  - apps/frize_visor  : 스마트 고글 (Jetson, OpenCV)        [옵션]
#  - apps/frize_cockpit: 콕핏 UI (Qt6) ― 별도 빌드(아래 옵션) [옵션]
#
#  디바이스 앱(scout/visor/mapping)은 무거운 의존성을 요구하므로
#  플랫폼/SDK가 준비된 경우에만 켠다.
# ============================================================

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FRIZE_BUILD_TESTS   "단위 테스트 빌드"        ON)
option(FRIZE_BUILD_CORE    "지휘 커널 빌드"          ON)
option(FRIZE_BUILD_RECON_SIM "트윈 재구성 시뮬 빌드(의존성0)" ON)
option(FRIZE_BUILD_SCOUT   "드론 SW 빌드(MAVSDK)"    OFF)
option(FRIZE_BUILD_MAPPING "3D 맵핑 SW 빌드(PCL)"    OFF)
option(FRIZE_BUILD_VISOR   "고글 SW 빌드(OpenCV)"    OFF)
option(FRIZE_BUILD_APPARATUS "IoT 장비 SW 빌드(VENT-1)" OFF)
option(FRIZE_BUILD_COCKPIT "콕핏 UI 빌드(Qt6)"       OFF)
option(FRIZE_BUILD_COCKPIT_LIVE "콕핏 라이브 통신 데몬 빌드(frize_common)" ON)
option(FRIZE_BUILD_NAV     "AR 내비게이션 서비스 빌드(frize_common)" ON)
option(FRIZE_BUILD_CONSOLE "콘솔 E-STOP 브리지 빌드"  OFF)

enable_testing()

add_subdirectory(libs/frize_common)
add_subdirectory(libs/frize_recon)   # 헤더 온리, 의존성 0 — 항상 사용 가능
if(FRIZE_BUILD_RECON_SIM)
    add_subdirectory(apps/frize_recon_sim)
endif()
if(FRIZE_BUILD_CORE)
    add_subdirectory(apps/frize_core)
endif()
if(FRIZE_BUILD_SCOUT)
    add_subdirectory(apps/frize_scout)
endif()
if(FRIZE_BUILD_MAPPING)
    add_subdirectory(apps/frize_mapping)
endif()
if(FRIZE_BUILD_VISOR)
    add_subdirectory(apps/frize_visor)
endif()
if(FRIZE_BUILD_APPARATUS)
    add_subdirectory(apps/frize_apparatus)
endif()
if(FRIZE_BUILD_COCKPIT)
    add_subdirectory(apps/frize_cockpit)
endif()
if(FRIZE_BUILD_COCKPIT_LIVE)
    add_subdirectory(apps/frize_cockpit_live)
endif()
if(FRIZE_BUILD_NAV)
    add_subdirectory(apps/frize_nav)
endif()
if(FRIZE_BUILD_CONSOLE)
    add_subdirectory(apps/frize_console_estop)
endif()

message(STATUS "FRIZE build:  core=${FRIZE_BUILD_CORE} scout=${FRIZE_BUILD_SCOUT} "
               "mapping=${FRIZE_BUILD_MAPPING} visor=${FRIZE_BUILD_VISOR} "
               "cockpit=${FRIZE_BUILD_COCKPIT} console=${FRIZE_BUILD_CONSOLE} "
               "recon_sim=${FRIZE_BUILD_RECON_SIM}")
