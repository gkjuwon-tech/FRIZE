#include "CockpitClient.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QHash>
#include <QDebug>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>

CockpitClient::CockpitClient(QObject* parent) : QObject(parent) {
    connect(&socket_, &QWebSocket::connected,    this, &CockpitClient::onConnected);
    connect(&socket_, &QWebSocket::disconnected, this, &CockpitClient::onDisconnected);
    connect(&socket_, &QWebSocket::textMessageReceived, this, &CockpitClient::onTextMessage);

    reconnect_.setInterval(2000);
    reconnect_.setSingleShot(true);
    connect(&reconnect_, &QTimer::timeout, this, &CockpitClient::connectToCore);

    // 매핑이 twin.gltf를 다시 구우면 watcher가 알려준다 → URL에 캐시버스터를
    // 붙여 emit → QML Model.source 가 바뀌어 새 메쉬를 다시 로드한다.
    connect(&twinWatcher_, &QFileSystemWatcher::fileChanged, this, [this](const QString& p){
        if (!twinWatcher_.files().contains(p)) twinWatcher_.addPath(p);  // 원자적 교체 대응
        if (QFileInfo::exists(p)) {
            twinMeshUrl_ = QUrl::fromLocalFile(p).toString()
                         + "?v=" + QString::number(QDateTime::currentMSecsSinceEpoch());
            emit twinMeshChanged();
        }
    });
}

void CockpitClient::watchTwinMesh(const QString& path) {
    twinMeshPath_ = path;
    if (path.isEmpty()) return;
    // 디렉터리도 감시(파일이 아직 없거나 교체될 수 있으므로)
    QFileInfo fi(path);
    twinWatcher_.addPath(fi.absolutePath());
    if (fi.exists()) {
        twinWatcher_.addPath(path);
        twinMeshUrl_ = QUrl::fromLocalFile(path).toString()
                     + "?v=" + QString::number(QDateTime::currentMSecsSinceEpoch());
        emit twinMeshChanged();
    }
    connect(&twinWatcher_, &QFileSystemWatcher::directoryChanged, this, [this](const QString&){
        QFileInfo f(twinMeshPath_);
        if (f.exists() && !twinWatcher_.files().contains(twinMeshPath_)) {
            twinWatcher_.addPath(twinMeshPath_);
            twinMeshUrl_ = QUrl::fromLocalFile(twinMeshPath_).toString()
                         + "?v=" + QString::number(QDateTime::currentMSecsSinceEpoch());
            emit twinMeshChanged();
        }
    });
}

void CockpitClient::connectToCore() {
    if (connected_) return;
    qInfo() << "[cockpit] 코어 연결 시도:" << url_.toString();
    socket_.open(url_);
}

void CockpitClient::onConnected() {
    connected_ = true;
    qInfo() << "[cockpit] 코어 연결됨";
    emit connectedChanged();
}

void CockpitClient::onDisconnected() {
    if (connected_) qWarning() << "[cockpit] 코어 연결 끊김 ― 재연결 예약";
    connected_ = false;
    emit connectedChanged();
    reconnect_.start();          // 자동 재연결
}

void CockpitClient::onTextMessage(const QString& msg) {
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    const QJsonObject obj = doc.object();

    // 코어 응답: 명령 결과({kind:...}) 또는 봉투({type:...})
    if (obj.contains("kind")) {
        if (obj.value("kind").toString() == "command_result")
            emit commandResult(obj.value("result").toObject().toVariantMap());
        return;
    }
    const QString type = obj.value("type").toString();
    if (type == "world_snapshot")
        applySnapshot(obj.value("payload").toObject());
    else if (type == "map_patch")
        applyMapPatch(obj.value("payload").toObject());
}

void CockpitClient::applyMapPatch(const QJsonObject& p) {
    voxelSize_ = p.value("voxel_size_m").toDouble(0.25);
    exploredCells_ = p.value("explored_cells").toInt();
    const QJsonObject origin = p.value("origin").toObject();
    const double ox = origin.value("x").toDouble(), oy = origin.value("y").toDouble(), oz = origin.value("z").toDouble();
    const double v = voxelSize_;
    // 열 복셀 인덱스→온도 맵
    QHash<QString,double> temp;
    for (const auto& tv : p.value("thermal").toArray()) {
        const QJsonArray a = tv.toArray();
        if (a.size()==4) temp.insert(QString("%1,%2,%3").arg(a[0].toInt()).arg(a[1].toInt()).arg(a[2].toInt()), a[3].toDouble());
    }
    twinVoxels_.clear();
    for (const auto& ov : p.value("occupied").toArray()) {
        const QJsonArray a = ov.toArray();
        if (a.size()<4) continue;
        const int i=a[0].toInt(), j=a[1].toInt(), k=a[2].toInt();
        QVariantMap m;
        m["x"]=(i+0.5)*v+ox; m["y"]=(j+0.5)*v+oy; m["z"]=(k+0.5)*v+oz;
        m["occ"]=a[3].toInt();
        m["t"]=temp.value(QString("%1,%2,%3").arg(i).arg(j).arg(k), -999.0);
        twinVoxels_.push_back(m);
        if (twinVoxels_.size()>=6000) break;     // 렌더 상한
    }
    twinFrontiers_.clear();
    for (const auto& fv : p.value("frontiers").toArray()) {
        const QJsonArray a = fv.toArray();
        if (a.size()<3) continue;
        QVariantMap m;
        m["x"]=(a[0].toInt()+0.5)*v+ox; m["y"]=(a[1].toInt()+0.5)*v+oy; m["z"]=(a[2].toInt()+0.5)*v+oz;
        twinFrontiers_.push_back(m);
    }
    // ── 화재 확산(월드 m 좌표로 직접 옴) ──
    // 확산 전선 [x,y,z,temp_c,dT_dt] : 지금 막 달궈지는 표면 = 불이 번지는 가장자리
    fireFront_.clear();
    for (const auto& ff : p.value("fire_front").toArray()) {
        const QJsonArray a = ff.toArray();
        if (a.size()<5) continue;
        QVariantMap m;
        m["x"]=a[0].toDouble(); m["y"]=a[1].toDouble(); m["z"]=a[2].toDouble();
        m["temp"]=a[3].toDouble(); m["rate"]=a[4].toDouble();
        fireFront_.push_back(m);
    }
    // 확산 벡터 [cx,cy,cz,dx,dy,dz,rate,peak] : 화점이 '어느 방향으로' 번지는가
    fireSpread_.clear();
    for (const auto& fs : p.value("fire_spread").toArray()) {
        const QJsonArray a = fs.toArray();
        if (a.size()<8) continue;
        QVariantMap m;
        m["cx"]=a[0].toDouble(); m["cy"]=a[1].toDouble(); m["cz"]=a[2].toDouble();
        m["dx"]=a[3].toDouble(); m["dy"]=a[4].toDouble(); m["dz"]=a[5].toDouble();
        m["rate"]=a[6].toDouble(); m["peak"]=a[7].toDouble();
        fireSpread_.push_back(m);
    }
    // 재정찰 목표 [x,y,z,priority] : 드론이 되돌아가 확산을 재확인할 곳
    revisitTargets_.clear();
    for (const auto& rv : p.value("revisit").toArray()) {
        const QJsonArray a = rv.toArray();
        if (a.size()<4) continue;
        QVariantMap m;
        m["x"]=a[0].toDouble(); m["y"]=a[1].toDouble(); m["z"]=a[2].toDouble();
        m["priority"]=a[3].toDouble();
        revisitTargets_.push_back(m);
    }
    emit twinUpdated();
}

void CockpitClient::applySnapshot(const QJsonObject& payload) {
    devices_.setItems(payload.value("devices").toArray());
    alerts_.setItems(payload.value("active_alerts").toArray());
    allDetections_ = payload.value("detections").toArray();
    refilterDetections();

    // 위험구역(태틱컬 맵)
    hazardZones_.clear();
    for (const auto& v : payload.value("hazard_zones").toArray()) {
        const QJsonObject z = v.toObject();
        QVariantMap m;
        m["zoneId"]   = z.value("zone_id").toString();
        m["hazard"]   = z.value("hazard").toString();
        m["severity"] = z.value("severity").toString();
        const QJsonObject c = z.value("center").toObject();
        m["x"] = c.value("x").toDouble();
        m["y"] = c.value("y").toDouble();
        m["radius"] = z.value("radius_m").toDouble();
        m["rationale"] = z.value("rationale").toString();
        hazardZones_.push_back(m);
    }
    emit snapshotUpdated();
}

void CockpitClient::refilterDetections() {
    if (selectedId_.isEmpty()) {
        detections_.setItems(allDetections_);
        return;
    }
    QJsonArray filtered;
    for (const auto& v : allDetections_)
        if (v.toObject().value("source_device").toString() == selectedId_)
            filtered.push_back(v);
    detections_.setItems(filtered);
}

void CockpitClient::selectDevice(const QString& id) {
    if (id == selectedId_) return;
    selectedId_ = id;
    refilterDetections();
    emit selectedChanged();
}

void CockpitClient::send(const QJsonObject& obj) {
    if (!connected_) { qWarning() << "[cockpit] 미연결 ― 전송 보류"; return; }
    socket_.sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void CockpitClient::sendCommand(const QString& target, const QString& action, const QVariantMap& params) {
    QJsonObject cmd;
    cmd["target_device"] = target;
    cmd["action"]        = action;
    cmd["issued_by"]     = "cockpit";
    cmd["params"]        = QJsonObject::fromVariantMap(params);
    send(QJsonObject{{"kind", "command"}, {"command", cmd}});
}

void CockpitClient::confirmCommand(const QString& cmdId) {
    send(QJsonObject{{"kind", "confirm"}, {"cmd_id", cmdId}, {"by", "cockpit"}});
}

void CockpitClient::ackAlert(const QString& alertId) {
    send(QJsonObject{{"kind", "ack_alert"}, {"alert_id", alertId}});
}
