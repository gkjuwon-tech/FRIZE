cmake_minimum_required(VERSION 3.21)
project(frize_cockpit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt6: Quick(QML/3D), WebSockets(코어 연결), Quick3D + AssetUtils(glTF RuntimeLoader)
find_package(Qt6 REQUIRED COMPONENTS Quick Qml Gui WebSockets Quick3D Quick3DAssetUtils)

qt_standard_project_setup(REQUIRES 6.5)

# Theme.qml 을 QML 싱글톤으로 등록
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(frize_cockpit
    main.cpp
    src/CockpitClient.cpp
    src/DeviceListModel.cpp
    src/AlertListModel.cpp
    src/DetectionListModel.cpp
)

qt_add_qml_module(frize_cockpit
    URI Frize.Cockpit
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Card.qml
        qml/FeedView.qml
        qml/DeviceRoster.qml
        qml/AlertStack.qml
        qml/CommandPanel.qml
        qml/TacticalMap.qml
        qml/TwinView.qml
        qml/TopBar.qml
        qml/StatChip.qml
    RESOURCES
        assets/fireground.jpg
        assets/frize_logo.svg
)

target_include_directories(frize_cockpit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(frize_cockpit PRIVATE
    Qt6::Quick Qt6::Qml Qt6::Gui Qt6::WebSockets Qt6::Quick3D Qt6::Quick3DAssetUtils
)
