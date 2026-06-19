import QtQuick
import Frize.Cockpit

// 작은 라벨/값 칩 (가스, 배터리, 링크 등)
Rectangle {
    id: chip
    property string label: ""
    property string value: ""
    property color tone: Theme.subtext
    implicitHeight: 22
    implicitWidth: row.implicitWidth + 16
    radius: Theme.radiusSmall
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.12)
    border.width: 1
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.35)

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 5
        Text { text: chip.label; color: Theme.faint; font.pixelSize: Theme.fsMicro
               font.family: Theme.mono; anchors.verticalCenter: parent.verticalCenter }
        Text { text: chip.value; color: chip.tone; font.pixelSize: Theme.fsSmall
               font.bold: true; font.family: Theme.mono; anchors.verticalCenter: parent.verticalCenter }
    }
}
