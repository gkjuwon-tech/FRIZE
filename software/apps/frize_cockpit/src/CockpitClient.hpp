// CockpitClient.hpp ― 코어 연결 + 상태 노출 (QML 백엔드)
#pragma once

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QUrl>
#include <QJsonArray>
#include <QVariantList>
#include <QVariantMap>
#include <QFileSystemWatcher>

#include "models.hpp"

class CockpitClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString selectedDeviceId READ selectedDeviceId WRITE selectDevice NOTIFY selectedChanged)
    Q_PROPERTY(DeviceListModel*    devices    READ devices    CONSTANT)
    Q_PROPERTY(AlertListModel*     alerts     READ alerts     CONSTANT)
    Q_PROPERTY(DetectionListModel* detections READ detections CONSTANT)
    Q_PROPERTY(QVariantList hazardZones READ hazardZones NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantList rescueQueue READ rescueQueue NOTIFY snapshotUpdated)
    Q_PROPERTY(int alertCount READ alertCount NOTIFY snapshotUpdated)
    // 3D 디지털 트윈
    Q_PROPERTY(QVariantList twinVoxels READ twinVoxels NOTIFY twinUpdated)
    Q_PROPERTY(QVariantList twinFrontiers READ twinFrontiers NOTIFY twinUpdated)
    // 화재 확산: 전선(번지는 가장자리) / 확산 벡터(어디로) / 재정찰 목표
    Q_PROPERTY(QVariantList fireFront READ fireFront NOTIFY twinUpdated)
    Q_PROPERTY(QVariantList fireSpread READ fireSpread NOTIFY twinUpdated)
    Q_PROPERTY(QVariantList revisitTargets READ revisitTargets NOTIFY twinUpdated)
    Q_PROPERTY(double voxelSize READ voxelSize NOTIFY twinUpdated)
    Q_PROPERTY(int exploredCells READ exploredCells NOTIFY twinUpdated)
    // 매핑 서비스가 굽는 TSDF 표면 메쉬(glTF). 파일이 갱신되면 자동 리로드.
    Q_PROPERTY(QString twinMeshUrl READ twinMeshUrl NOTIFY twinMeshChanged)

public:
    explicit CockpitClient(QObject* parent = nullptr);

    bool connected() const { return connected_; }
    QString selectedDeviceId() const { return selectedId_; }
    DeviceListModel*    devices()    { return &devices_; }
    AlertListModel*     alerts()     { return &alerts_; }
    DetectionListModel* detections() { return &detections_; }
    QVariantList hazardZones() const { return hazardZones_; }
    QVariantList rescueQueue() const { return rescueQueue_; }
    int alertCount() const { return alerts_.rowCount(); }
    QVariantList twinVoxels() const { return twinVoxels_; }
    QVariantList twinFrontiers() const { return twinFrontiers_; }
    QVariantList fireFront() const { return fireFront_; }
    QVariantList fireSpread() const { return fireSpread_; }
    QVariantList revisitTargets() const { return revisitTargets_; }
    double voxelSize() const { return voxelSize_; }
    int exploredCells() const { return exploredCells_; }
    QString twinMeshUrl() const { return twinMeshUrl_; }

    void setCoreUrl(const QString& url) { url_ = QUrl(url); }
    // 트윈 메쉬 glTF 경로 감시 시작(매핑이 갱신 시 자동 리로드). 빈 경로면 비활성.
    void watchTwinMesh(const QString& path);

public slots:
    void connectToCore();
    void selectDevice(const QString& id);
    // 명령 발행: target에게 action(+params). 코어 인터록을 거친다.
    void sendCommand(const QString& target, const QString& action, const QVariantMap& params);
    void confirmCommand(const QString& cmdId);
    void ackAlert(const QString& alertId);

signals:
    void connectedChanged();
    void selectedChanged();
    void snapshotUpdated();
    void twinUpdated();
    void twinMeshChanged();
    void commandResult(const QVariantMap& result);   // 거부/확인필요/발행 결과 → 토스트

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessage(const QString& msg);

private:
    void send(const QJsonObject& obj);
    void applySnapshot(const QJsonObject& payload);
    void applyMapPatch(const QJsonObject& payload);
    void refilterDetections();

    QWebSocket socket_;
    QUrl url_;
    QTimer reconnect_;
    bool connected_ = false;

    DeviceListModel    devices_;
    AlertListModel     alerts_;
    DetectionListModel detections_;
    QJsonArray allDetections_;     // 전체 감지(선택 디바이스로 필터링)
    QVariantList hazardZones_;
    QVariantList rescueQueue_;
    QString selectedId_;
    // 3D 트윈
    QVariantList twinVoxels_;
    QVariantList twinFrontiers_;
    QVariantList fireFront_;
    QVariantList fireSpread_;
    QVariantList revisitTargets_;
    double voxelSize_ = 0.25;
    int exploredCells_ = 0;
    // TSDF 표면 메쉬(glTF) 자동 리로드
    QFileSystemWatcher twinWatcher_;
    QString twinMeshPath_;     // 디스크 경로
    QString twinMeshUrl_;      // QML용 file:// URL(+캐시버스터)
};
