// models.hpp ― 콕핏 QML 노출 모델
//
// 코어가 보내는 WorldSnapshot JSON의 배열들(devices/alerts/detections)을
// QAbstractListModel로 감싸 QML Repeater/ListView에 바인딩한다.
// 공통 베이스(JsonListModel)로 중복 제거: 역할(role)은 JSON 키 이름 그대로.
#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QVector>
#include <QVariantMap>
#include <QString>

// 심각도/상태 → 색 (콕핏 테마와 일치)
inline QString severityColor(const QString& s) {
    if (s == "critical") return "#e0241c";
    if (s == "high")     return "#ff8c00";
    if (s == "low")      return "#f5c518";
    return "#9aa0a6";
}
inline QString stateColor(const QString& s) {
    if (s == "online")   return "#36d399";
    if (s == "degraded") return "#f5c518";
    if (s == "critical") return "#e0241c";
    if (s == "offline")  return "#6b7280";
    return "#9aa0a6";  // booting 등
}

class JsonListModel : public QAbstractListModel {
    Q_OBJECT
public:
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& = QModelIndex()) const override { return items_.size(); }

    QVariant data(const QModelIndex& idx, int role) const override {
        if (idx.row() < 0 || idx.row() >= items_.size()) return {};
        auto it = roles_.constFind(role);
        if (it == roles_.constEnd()) return {};
        return items_[idx.row()].value(QString::fromUtf8(it.value())).toVariant();
    }

    QHash<int, QByteArray> roleNames() const override { return roles_; }

    void setItems(const QJsonArray& arr) {
        beginResetModel();
        items_.clear();
        items_.reserve(arr.size());
        for (const auto& v : arr) items_.push_back(transform(v.toObject()));
        endResetModel();
    }

    Q_INVOKABLE QVariantMap get(int row) const {
        if (row < 0 || row >= items_.size()) return {};
        return items_[row].toVariantMap();
    }

    Q_INVOKABLE int count() const { return items_.size(); }

    // device_id로 항목 찾기(없으면 빈 맵)
    Q_INVOKABLE QVariantMap findById(const QString& key, const QString& value) const {
        for (const auto& o : items_)
            if (o.value(key).toString() == value) return o.toVariantMap();
        return {};
    }

protected:
    void addRole(const QByteArray& name) {
        int id = Qt::UserRole + 1 + roles_.size();
        roles_.insert(id, name);
    }
    virtual QJsonObject transform(QJsonObject o) const { return o; }

    QHash<int, QByteArray> roles_;
    QVector<QJsonObject> items_;
};

// 디바이스 로스터(고글/드론) ― extra 안의 가스/비행정보를 평탄화해 노출
class DeviceListModel : public JsonListModel {
    Q_OBJECT
public:
    explicit DeviceListModel(QObject* parent = nullptr);
protected:
    QJsonObject transform(QJsonObject o) const override;
};

// 경보 스택
class AlertListModel : public JsonListModel {
    Q_OBJECT
public:
    explicit AlertListModel(QObject* parent = nullptr);
protected:
    QJsonObject transform(QJsonObject o) const override;
};

// 선택 디바이스 시야의 VLM 감지(오버레이용)
class DetectionListModel : public JsonListModel {
    Q_OBJECT
public:
    explicit DetectionListModel(QObject* parent = nullptr);
protected:
    QJsonObject transform(QJsonObject o) const override;
};
