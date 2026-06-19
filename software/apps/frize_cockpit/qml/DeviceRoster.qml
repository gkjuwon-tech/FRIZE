import QtQuick
import QtQuick.Layouts
import Frize.Cockpit

Card {
    id: roster

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pad
        spacing: Theme.gap

        Text { text: "현장 자원 · UNITS"; color: Theme.subtext; font.bold: true
               font.pixelSize: Theme.fsSmall; font.family: Theme.mono; Layout.bottomMargin: 2 }

        ListView {
            id: list
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; spacing: 8
            model: cockpit.devices

            delegate: Rectangle {
                width: list.width
                height: 78
                radius: Theme.radiusSmall
                property bool selected: cockpit.selectedDeviceId === model.deviceId
                color: selected ? "#eef2f6" : Theme.surfaceAlt
                border.width: 0

                // 선택 표시: 가는 좌측 슬레이트 바
                Rectangle { visible: parent.selected; width: 3; height: parent.height
                            radius: 2; color: Theme.cyan }

                MouseArea { anchors.fill: parent; onClicked: cockpit.selectDevice(model.deviceId) }

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 5

                    RowLayout {
                        spacing: 8; Layout.fillWidth: true
                        // 타입 아이콘
                        Rectangle {
                            width: 26; height: 26; radius: Theme.radius
                            color: "transparent"
                            border.width: 1
                            border.color: model.deviceType === "scout" ? Theme.cyan
                                          : model.deviceType === "apparatus" ? Theme.amber : Theme.subtext
                            Text { anchors.centerIn: parent
                                   text: model.deviceType === "scout" ? "▲"
                                         : model.deviceType === "apparatus" ? "▣" : "●"
                                   color: model.deviceType === "scout" ? Theme.cyan
                                          : model.deviceType === "apparatus" ? Theme.amber : Theme.subtext
                                   font.pixelSize: 12 }
                        }
                        ColumnLayout {
                            spacing: 0
                            Text { text: model.callsign; color: Theme.text; font.bold: true
                                   font.pixelSize: Theme.fsBody; font.family: Theme.mono }
                            Text { text: model.deviceType.toUpperCase() + " · " + model.deviceId
                                   color: Theme.faint; font.pixelSize: Theme.fsMicro }
                        }
                        Item { Layout.fillWidth: true }
                        // 상태 점
                        Row {
                            spacing: 5
                            Rectangle { width: 8; height: 8; radius: 4; color: model.stateColor
                                        anchors.verticalCenter: parent.verticalCenter }
                            Text { text: model.state; color: model.stateColor
                                   font.pixelSize: Theme.fsMicro; font.family: Theme.mono
                                   anchors.verticalCenter: parent.verticalCenter }
                        }
                    }

                    RowLayout {
                        spacing: 6; Layout.fillWidth: true
                        // 배터리 바
                        Rectangle {
                            width: 54; height: 14; radius: Theme.radiusSmall; color: Theme.bg
                            border.color: Theme.borderSoft; border.width: 1
                            Rectangle {
                                anchors.left: parent.left; anchors.margins: 2
                                anchors.verticalCenter: parent.verticalCenter
                                width: (parent.width - 4) * Math.max(0, Math.min(1, model.battery / 100))
                                height: parent.height - 4; radius: 2
                                color: model.battery < 20 ? Theme.critical
                                       : model.battery < 40 ? Theme.warn : Theme.ok
                            }
                            Text { anchors.centerIn: parent; text: model.battery + "%"
                                   color: Theme.text; font.pixelSize: Theme.fsMicro; font.family: Theme.mono }
                        }
                        StatChip { label: "LINK"; value: (model.link + "").toUpperCase()
                                   tone: model.link === "lost" ? Theme.critical : Theme.subtext }
                        // 가스 위험 플래그
                        StatChip {
                            visible: model.gasFlagText !== ""
                            label: "GAS"; value: model.gasFlagText; tone: Theme.critical
                        }
                        // 드론 비행 모드
                        StatChip {
                            visible: model.deviceType === "scout"
                            label: "MODE"; value: (model.flightMode + "").toUpperCase(); tone: Theme.cyan
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // 비어있을 때
            Text {
                anchors.centerIn: parent
                visible: list.count === 0
                text: "연결된 디바이스 없음\n코어/디바이스 기동 대기"
                horizontalAlignment: Text.AlignHCenter
                color: Theme.faint; font.pixelSize: Theme.fsSmall; font.family: Theme.mono
            }
        }
    }
}
