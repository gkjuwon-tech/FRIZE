import QtQuick
import QtQuick.Layouts
import Frize.Cockpit

// 메인 뷰포트: 대원/드론 시야(실사진/영상) 위에 VLM 감지 오버레이 + 명령 타겟.
// '콕핏이 대원의 눈으로 본다' + '판단 근거를 같이 띄운다(블랙박스 금지)'.
Item {
    id: feed
    property string assetBase: "qrc:/qt/qml/Frize/Cockpit/assets/"
    signal detectionPicked(var det)
    signal groundPicked(real nx, real ny)

    // ── 배경 실사진(현장/시야) ──
    Image {
        id: bg
        anchors.fill: parent
        source: feed.assetBase + "fireground.jpg"
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
    }
    // 가독성 그라데이션
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0,0,0,0.45) }
            GradientStop { position: 0.25; color: Qt.rgba(0,0,0,0.0) }
            GradientStop { position: 0.80; color: Qt.rgba(0,0,0,0.0) }
            GradientStop { position: 1.0; color: Qt.rgba(0,0,0,0.65) }
        }
    }
    // 열화상/스캔 느낌의 미세 스캔라인
    Rectangle { anchors.fill: parent; opacity: 0.05
        gradient: Gradient { orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Theme.cyan }
            GradientStop { position: 1.0; color: "transparent" } } }

    // 바닥 클릭 → 명령 타겟(정규화 좌표)
    MouseArea {
        anchors.fill: parent
        onClicked: (m) => feed.groundPicked(m.x / feed.width, m.y / feed.height)
    }

    // ── 감지 바운딩박스 오버레이 ──
    Repeater {
        model: cockpit.detections
        delegate: Item {
            visible: model.hasBox
            x: model.bx * feed.width
            y: model.by * feed.height
            width: model.bw * feed.width
            height: model.bh * feed.height

            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.width: 1
                border.color: model.severityColor
                radius: 0
                // 임계(critical) 박스는 점멸
                SequentialAnimation on border.color {
                    running: model.severity === "critical"
                    loops: Animation.Infinite
                    ColorAnimation { to: Qt.rgba(1,1,1,0.9); duration: 500 }
                    ColorAnimation { to: model.severityColor; duration: 500 }
                }
                // 코너 마커
                Repeater { model: 4
                    Rectangle { width: 10; height: 2; color: parent.border.color
                        x: (index % 2) ? parent.width - width : 0
                        y: (index < 2) ? 0 : parent.height - height } }
            }
            // 라벨 태그
            Rectangle {
                anchors.left: parent.left; anchors.bottom: parent.top; anchors.bottomMargin: 2
                height: 16; width: tag.implicitWidth + 10; radius: 0
                color: Qt.rgba(0.04, 0.045, 0.05, 0.9)
                border.width: 1; border.color: model.severityColor
                Text { id: tag; anchors.centerIn: parent
                    text: model.label + "  " + Math.round(model.confidence * 100) + "%"
                    color: model.severityColor; font.bold: true; font.pixelSize: Theme.fsMicro; font.family: Theme.mono }
            }
            MouseArea { anchors.fill: parent; hoverEnabled: true
                onClicked: feed.detectionPicked(cockpit.detections.get(index))
                onEntered: rationaleBar.show(model.label, model.rationale, model.severityColor)
            }
        }
    }

    // 중앙 레티클
    Item {
        anchors.centerIn: parent; width: 40; height: 40; opacity: 0.5
        Rectangle { anchors.centerIn: parent; width: 1; height: 40; color: Theme.cyan }
        Rectangle { anchors.centerIn: parent; width: 40; height: 1; color: Theme.cyan }
        Rectangle { anchors.centerIn: parent; width: 12; height: 12; radius: 6
                    color: "transparent"; border.color: Theme.cyan; border.width: 1 }
    }

    // 좌상단: 스트림/디바이스 표식
    Column {
        anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 14
        spacing: 4
        Row { spacing: 8
            Rectangle { width: 9; height: 9; radius: 4; color: Theme.accent
                        anchors.verticalCenter: parent.verticalCenter
                        SequentialAnimation on opacity { loops: Animation.Infinite
                            NumberAnimation { to: 0.2; duration: 700 }
                            NumberAnimation { to: 1.0; duration: 700 } } }
            Text { text: "● LIVE · THERMAL"; color: Theme.text; font.bold: true
                   font.pixelSize: Theme.fsSmall; font.family: Theme.mono }
        }
        Text {
            text: cockpit.selectedDeviceId === "" ? "선택된 디바이스 없음 (좌측 로스터에서 선택)"
                                                  : "FEED · " + cockpit.selectedDeviceId
            color: Theme.subtext; font.pixelSize: Theme.fsSmall; font.family: Theme.mono
        }
    }

    // 하단: 판단 근거 바(VLM rationale)
    Rectangle {
        id: rationaleBar
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: 12
        height: visible ? 46 : 0
        radius: Theme.radiusSmall
        color: Theme.panelGlass
        border.width: 1; border.color: tone
        visible: false
        property color tone: Theme.subtext
        function show(label, why, color) {
            tone = color; titleT.text = label; whyT.text = "근거: " + why; visible = true; hideTimer.restart();
        }
        Timer { id: hideTimer; interval: 6000; onTriggered: rationaleBar.visible = false }
        Column {
            anchors.fill: parent; anchors.margins: 8; spacing: 2
            Text { id: titleT; color: rationaleBar.tone; font.bold: true
                   font.pixelSize: Theme.fsSmall; font.family: Theme.mono }
            Text { id: whyT; color: Theme.text; font.pixelSize: Theme.fsSmall
                   width: rationaleBar.width - 16; wrapMode: Text.WordWrap; elide: Text.ElideRight }
        }
    }
}
