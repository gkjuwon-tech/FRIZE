#include "models.hpp"
#include <QJsonValue>

DeviceListModel::DeviceListModel(QObject* parent) : JsonListModel(parent) {
    for (const char* r : {"deviceId","deviceType","state","stateColor","callsign",
                          "battery","link","posX","posY","gasFlags","gasFlagText",
                          "flightMode","armed","altitude"})
        addRole(r);
}

QJsonObject DeviceListModel::transform(QJsonObject o) const {
    QJsonObject t;
    t["deviceId"]   = o.value("device_id");
    t["deviceType"] = o.value("device_type");
    QString st = o.value("state").toString();
    t["state"]      = st;
    t["stateColor"] = stateColor(st);
    t["callsign"]   = o.value("callsign").isNull() ? o.value("device_id") : o.value("callsign");
    t["battery"]    = qRound(o.value("battery_pct").toDouble());
    t["link"]       = o.value("link");

    const QJsonObject pose = o.value("pose").toObject();
    const QJsonObject pos  = pose.value("position").toObject();
    t["posX"] = pos.value("x").toDouble();
    t["posY"] = pos.value("y").toDouble();

    const QJsonObject extra = o.value("extra").toObject();
    // 가스 위험 플래그
    QJsonArray flags = extra.value("gas_flags").toArray();
    t["gasFlags"] = flags;
    QStringList fl;
    for (const auto& f : flags) fl << f.toString();
    t["gasFlagText"] = fl.join(", ");
    // 드론 부가
    t["flightMode"] = extra.value("flight_mode");
    t["armed"]      = extra.value("armed");
    t["altitude"]   = extra.value("altitude_m");
    return t;
}
