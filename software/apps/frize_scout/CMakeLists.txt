add_executable(frize_scout
    src/main.cpp
    src/flight.cpp
    src/perception.cpp
)
target_include_directories(frize_scout PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(frize_scout PRIVATE cxx_std_17)
target_link_libraries(frize_scout PRIVATE frize_common)

# MAVSDK (실제 비행제어) ― 있으면 활성
find_package(MAVSDK CONFIG QUIET)
if(MAVSDK_FOUND)
    target_link_libraries(frize_scout PRIVATE MAVSDK::mavsdk)
    target_compile_definitions(frize_scout PRIVATE FRIZE_HAVE_MAVSDK)
    message(STATUS "frize_scout: MAVSDK 활성")
else()
    message(STATUS "frize_scout: MAVSDK 없음 ― 비행 시뮬레이터")
endif()

# OpenCV (카메라/열화상 캡처)
find_package(OpenCV QUIET)
if(OpenCV_FOUND)
    target_link_libraries(frize_scout PRIVATE ${OpenCV_LIBS})
    target_include_directories(frize_scout PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(frize_scout PRIVATE FRIZE_HAVE_OPENCV)
    message(STATUS "frize_scout: OpenCV 활성")
endif()
