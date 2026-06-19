add_executable(frize_visor
    src/main.cpp
    src/sensors.cpp
    src/capture.cpp
    src/ar_display.cpp
)
target_include_directories(frize_visor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(frize_visor PRIVATE cxx_std_17)
target_link_libraries(frize_visor PRIVATE frize_common)

# OpenCV (열화상/RGB)
find_package(OpenCV QUIET)
if(OpenCV_FOUND)
    target_link_libraries(frize_visor PRIVATE ${OpenCV_LIBS})
    target_include_directories(frize_visor PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(frize_visor PRIVATE FRIZE_HAVE_OPENCV)
endif()

# 시리얼 센서팩 (POSIX)
if(UNIX)
    target_compile_definitions(frize_visor PRIVATE FRIZE_HAVE_SERIAL)
endif()
