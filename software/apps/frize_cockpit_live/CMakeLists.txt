# frize_cockpit_live ― 프로덕션 콕핏 통신 데몬(헤드리스, 의존성: frize_common 만)
add_executable(frize_cockpit_live src/main.cpp)
target_compile_features(frize_cockpit_live PRIVATE cxx_std_17)
target_link_libraries(frize_cockpit_live PRIVATE frize_common)
