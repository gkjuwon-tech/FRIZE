#include "models.hpp"

AlertListModel::AlertListModel(QObject* parent) : JsonListModel(parent) {
    for (const char* r : {"alertId","kind","severity","severityColor","title","message","deviceId"})
        addRole(r);
}

QJsonObject AlertListModel::transform(QJsonObject o) const {
    QJsonObject t;
    t["alertId"]  = o.value("alert_id");
    t["kind"]     = o.value("kind");
    QString sev   = o.value("severity").toString();
    t["severity"] = sev;
    t["severityColor"] = severityColor(sev);
    t["title"]    = o.value("title");
    t["message"]  = o.value("message");
    t["deviceId"] = o.value("device_id");
    return t;
}
