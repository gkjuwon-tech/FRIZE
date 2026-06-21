// FRIZE Cockpit ― 지휘 운영체제 콕핏 UI (Qt6 / QML)
//
// 코어(C++ 커널)와 WebSocket으로 연결되어 실시간 월드 스냅샷을 받고,
// 지휘관의 '딸깍' 명령을 코어로 보낸다. 메인 뷰포트는 대원/드론 시야(실사진/영상)
// 위에 VLM 감지 오버레이와 명령 마커를 얹는다.
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml/qqmlextensionplugin.h>

#include "CockpitClient.hpp"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("FRIZE Cockpit");
    app.setOrganizationName("FRIZE");

    // 코어 URL: 환경변수 FRIZE_CORE_WS 또는 기본값
    QString url = qEnvironmentVariable("FRIZE_CORE_WS", "ws://localhost:8000/ws/cockpit");

    CockpitClient client;
    client.setCoreUrl(url);

    // TSDF 표면 메쉬(glTF) 자동 로드: 매핑 서비스가 FRIZE_TWIN_DIR/twin.gltf를 굽고
    // 콕핏은 그 경로를 감시해 갱신 시마다 매끈한 표면을 다시 로드한다.
    const QString twinDir = qEnvironmentVariable("FRIZE_TWIN_DIR", "");
    if (!twinDir.isEmpty())
        client.watchTwinMesh(twinDir + "/twin.gltf");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("cockpit", &client);
    engine.rootContext()->setContextProperty("coreUrl", url);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("Frize.Cockpit", "Main");
    if (engine.rootObjects().isEmpty()) return -1;

    client.connectToCore();
    return app.exec();
}
