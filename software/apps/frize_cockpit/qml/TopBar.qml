import QtQuick
import QtQuick.Layouts
import Frize.Cockpit

Rectangle {
    id: bar
    height: 54
    color: Theme.bgElevated
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 14

        // 로고 마크
        Rectangle {
            width: 30; height: 30; radius: Theme.radius; color: Theme.accent
            Text { anchors.centerIn: parent; text: "F"; color: "white"
                   font.bold: true; font.pixelSize: 18; font.family: Theme.mono }
        }
        ColumnLayout {
            spacing: 0
            Text { text: "FRIZE COCKPIT"; color: Theme.text; font.bold: true
                   font.pixelSize: Theme.fsTitle; font.family: Theme.mono }
            Text { text: "Firefighting Real-time Integrated Zone-control Environment"
                   color: Theme.faint; font.pixelSize: Theme.fsMicro }
        }

        Item { Layout.fillWidth: true }

        // 경보 카운트
        Rectangle {
            visible: cockpit.alertCount > 0
            height: 26; width: alertRow.implicitWidth + 20; radius: Theme.radius
            color: "transparent"
            border.color: Theme.critical; border.width: 1
            Row {
                id: alertRow; anchors.centerIn: parent; spacing: 6
                Rectangle { width: 8; height: 8; radius: 4; color: Theme.critical
                            anchors.verticalCenter: parent.verticalCenter
                            SequentialAnimation on opacity { loops: Animation.Infinite
                                NumberAnimation { to: 0.2; duration: 600 }
                                NumberAnimation { to: 1.0; duration: 600 } } }
                Text { text: cockpit.alertCount + " ACTIVE ALERTS"; color: Theme.critical
                       font.bold: true; font.pixelSize: Theme.fsSmall; font.family: Theme.mono
                       anchors.verticalCenter: parent.verticalCenter }
            }
        }

        // 시계
        Text {
            id: clock
            color: Theme.subtext; font.pixelSize: Theme.fsBody; font.family: Theme.mono
            Timer { interval: 1000; running: true; repeat: true
                    onTriggered: clock.text = Qt.formatDateTime(new Date(), "yyyy-MM-dd  HH:mm:ss") }
            Component.onCompleted: text = Qt.formatDateTime(new Date(), "yyyy-MM-dd  HH:mm:ss")
        }

        // 연결 상태
        Row {
            spacing: 7
            Rectangle { width: 10; height: 10; radius: 5; anchors.verticalCenter: parent.verticalCenter
                        color: cockpit.connected ? Theme.ok : Theme.offline }
            Text { text: cockpit.connected ? "CORE LINK" : "OFFLINE"
                   color: cockpit.connected ? Theme.ok : Theme.offline
                   font.pixelSize: Theme.fsSmall; font.bold: true; font.family: Theme.mono
                   anchors.verticalCenter: parent.verticalCenter }
        }
    }
}
