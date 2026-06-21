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

            // 중앙: 3D 디지털 트윈(메인) + 1인칭 시야 PiP + 명령 패널
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0c0f13"     // 트윈은 어두운 3D 뷰포트
                    clip: true

                    // ── 메인: 전체 건물 디지털 트윈(드론 LiDAR + 고글 열화상 융합) ──
                    TwinView {
                        id: twin
                        anchors.fill: parent
                    }

                    // ── PiP: 선택 대원 1인칭 시야(실영상 + VLM 감지 오버레이) ──
                    Rectangle {
                        id: pip
                        width: 372; height: 232; radius: 6
                        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 12
                        color: "#0d1014"; border.color: "#2a313a"; border.width: 1
                        clip: true
                        FeedView {
                            id: feed
                            anchors.fill: parent
                            anchors.margins: 1
                            onDetectionPicked: (det) => { command.picked = det; }
                            onGroundPicked: (nx, ny) => { /* 향후: 정규화→월드 매핑 후 타겟 설정 */ }
                        }
                        // PiP 라벨
                        Rectangle {
                            anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 6
                            width: pipLbl.implicitWidth + 14; height: 20; radius: 3
                            color: Qt.rgba(0,0,0,0.55)
                            Row { anchors.centerIn: parent; spacing: 6
                                Rectangle { width: 7; height: 7; radius: 4; color: "#e0241c"; anchors.verticalCenter: parent.verticalCenter }
                                Text { id: pipLbl
                                    text: (cockpit.selectedDeviceId.length ? cockpit.selectedDeviceId : "VISOR-1") + " · 1인칭"
                                    color: "#cfd4da"; font.pixelSize: 10; font.family: Theme.mono }
                            }
                        }
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
