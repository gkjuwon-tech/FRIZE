import QtQuick
import QtQuick.Layouts
import Frize.Cockpit

Card {
    id: stack

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pad
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            Text { text: "경보 · ALERTS"; color: Theme.subtext; font.bold: true
                   font.pixelSize: Theme.fsSmall; font.family: Theme.mono }
            Item { Layout.fillWidth: true }
            Rectangle {
                visible: cockpit.alertCount > 0
                width: cntT.implicitWidth + 14; height: 18; radius: Theme.radius
                color: "transparent"; border.color: Theme.accent; border.width: 1
                Text { id: cntT; anchors.centerIn: parent; text: cockpit.alertCount
                       color: Theme.accent; font.bold: true; font.pixelSize: Theme.fsMicro; font.family: Theme.mono }
            }
        }

        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; spacing: 8
            model: cockpit.alerts

            delegate: Rectangle {
                width: ListView.view.width
                height: col.implicitHeight + 18
                radius: Theme.radiusSmall
                color: Theme.surfaceAlt
                border.width: 0

                // 좌측 심각도 바
                Rectangle { width: 4; height: parent.height - 8; radius: 2
                            anchors.left: parent.left; anchors.leftMargin: 4
                            anchors.verticalCenter: parent.verticalCenter; color: model.severityColor }

                ColumnLayout {
                    id: col
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.right: parent.right; anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: model.title; color: model.severityColor; font.bold: true
                               font.pixelSize: Theme.fsSmall; font.family: Theme.mono; Layout.fillWidth: true
                               elide: Text.ElideRight }
                        Text { text: (model.severity + "").toUpperCase(); color: model.severityColor
                               font.pixelSize: Theme.fsMicro; font.family: Theme.mono }
                    }
                    Text { text: model.message; color: Theme.text; font.pixelSize: Theme.fsSmall
                           Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: model.deviceId ? ("→ " + model.deviceId) : ""
                               color: Theme.faint; font.pixelSize: Theme.fsMicro; font.family: Theme.mono }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            width: ackT.implicitWidth + 16; height: 20; radius: Theme.radius
                            color: ackMa.containsMouse ? Theme.accent : "transparent"
                            border.color: Theme.accent; border.width: 1
                            Text { id: ackT; anchors.centerIn: parent; text: "확인 ACK"
                                   color: ackMa.containsMouse ? "white" : Theme.accent
                                   font.pixelSize: Theme.fsMicro; font.bold: true; font.family: Theme.mono }
                            MouseArea { id: ackMa; anchors.fill: parent; hoverEnabled: true
                                        onClicked: cockpit.ackAlert(model.alertId) }
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: parent.count === 0
                text: "활성 경보 없음 ✓"
                color: Theme.ok; font.pixelSize: Theme.fsSmall; font.family: Theme.mono
            }
        }
    }
}
