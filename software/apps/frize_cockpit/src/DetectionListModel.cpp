#include "models.hpp"
#include <QJsonArray>

DetectionListModel::DetectionListModel(QObject* parent) : JsonListModel(parent) {
    for (const char* r : {"detId","hazard","severity","severityColor","confidence",
                          "label","rationale","hasBox","bx","by","bw","bh","sourceDevice",
                          "hasWorld","wx","wy","wz"})
        addRole(r);
}

QJsonObject DetectionListModel::transform(QJsonObject o) const {
    QJsonObject t;
    t["detId"]      = o.value("det_id");
    t["hazard"]     = o.value("hazard");
    QString sev     = o.value("severity").toString();
    t["severity"]   = sev;
    t["severityColor"] = severityColor(sev);
    t["confidence"] = o.value("confidence");
    t["label"]      = o.value("label");
    t["rationale"]  = o.value("rationale");
    t["sourceDevice"] = o.value("source_device");

    const QJsonValue bb = o.value("bbox");
    if (bb.isArray() && bb.toArray().size() == 4) {
        const QJsonArray a = bb.toArray();
        t["hasBox"] = true;
        t["bx"] = a[0].toDouble();
        t["by"] = a[1].toDouble();
        t["bw"] = a[2].toDouble();
        t["bh"] = a[3].toDouble();
    } else {
        t["hasBox"] = false;
        t["bx"] = 0; t["by"] = 0; t["bw"] = 0; t["bh"] = 0;
    }

    const QJsonValue wp = o.value("world_pos");
    if (wp.isObject()) {
        const QJsonObject w = wp.toObject();
        t["hasWorld"] = true;
        t["wx"] = w.value("x").toDouble();
        t["wy"] = w.value("y").toDouble();
        t["wz"] = w.value("z").toDouble();
    } else {
        t["hasWorld"] = false;
        t["wx"] = 0; t["wy"] = 0; t["wz"] = 0;
    }
    return t;
}
