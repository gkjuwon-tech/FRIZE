// CockpitClient.hpp ― 코어 연결 + 상태 노출 (QML 백엔드)
#pragma once

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QUrl>
#include <QJsonArray>
#include <QVariantList>
#include <QVariantMap>

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
    Q_PROPERTY(double voxelSize READ voxelSize NOTIFY twinUpdated)
    Q_PROPERTY(int exploredCells READ exploredCells NOTIFY twinUpdated)

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
    double voxelSize() const { return voxelSize_; }
    int exploredCells() const { return exploredCells_; }

    void setCoreUrl(const QString& url) { url_ = QUrl(url); }

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
    double voxelSize_ = 0.25;
    int exploredCells_ = 0;
};
