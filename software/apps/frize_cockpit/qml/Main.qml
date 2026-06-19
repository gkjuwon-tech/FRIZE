import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import Frize.Cockpit

ApplicationWindow {
    id: win
    width: 1680
    height: 980
    visible: true
    title: "FRIZE Cockpit"
    color: Theme.bg

    Material.theme: Material.Light
    Material.accent: Theme.accent
    Material.primary: Theme.panel

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar { Layout.fillWidth: true }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            spacing: 12

            // 좌: 디바이스 로스터
            DeviceRoster {
                Layout.preferredWidth: 330
                Layout.fillHeight: true
            }

            // 중앙: 시야(실사진+오버레이) + 명령 패널
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0d1014"     // 피드는 어두운 영상 패널
                    clip: true

                    FeedView {
                        id: feed
                        anchors.fill: parent
                        onDetectionPicked: (det) => { command.picked = det; }
                        onGroundPicked: (nx, ny) => { /* 향후: 정규화→월드 매핑 후 타겟 설정 */ }
                    }

                    // 미니맵 오버레이(우하단)
                    TacticalMap {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 12
                    }
                }

                CommandPanel {
                    id: command
                    Layout.fillWidth: true
                    Layout.preferredHeight: 232
                }
            }

            // 우: 경보 스택
            AlertStack {
                Layout.preferredWidth: 350
                Layout.fillHeight: true
            }
        }
    }

    // 코어 미연결 안내 배너
    Rectangle {
        visible: !cockpit.connected
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        width: bannerT.implicitWidth + 36; height: 34; radius: 17
        color: Qt.rgba(Theme.warn.r, Theme.warn.g, Theme.warn.b, 0.16)
        border.color: Theme.warn; border.width: 1
        Text { id: bannerT; anchors.centerIn: parent
               text: "⚠ 코어(" + coreUrl + ") 연결 대기 중 ― 자동 재연결"
               color: Theme.warn; font.pixelSize: Theme.fsSmall; font.family: Theme.mono }
    }
}
