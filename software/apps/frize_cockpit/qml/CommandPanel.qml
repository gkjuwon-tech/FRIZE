import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Frize.Cockpit

// 하단 명령 패널 ― 콕핏 '딸깍'. 선택 디바이스에 컨텍스트 명령을 보낸다.
// 안전 인터록(코어)을 거쳐 거부/확인필요/발행으로 돌아온다.
Card {
    id: panel

    // 선택 디바이스 정보
    property string target: cockpit.selectedDeviceId
    property var targetDevice: target === "" ? ({}) : cockpit.devices.findById("deviceId", target)
    property string deviceType: targetDevice ? (targetDevice.deviceType || "") : ""
    // 마지막으로 찍은 타겟(감지) ― world 좌표 포함 가능
    property var picked: ({})
    property string pendingConfirmId: ""

    function paramsWithTarget() {
        var p = {};
        if (picked && picked.hasWorld)
            p["world_pos"] = { "x": picked.wx, "y": picked.wy, "z": picked.wz };
        return p;
    }
    function fire(action) {
        if (target === "") { toast("디바이스를 먼저 선택하세요", Theme.warn); return; }
        cockpit.sendCommand(target, action, paramsWithTarget());
    }
    function toast(msg, color) { toastBar.show(msg, color); }

    Connections {
        target: cockpit
        function onCommandResult(result) {
            var st = result.status || "";
            var reason = result.reason || "";
            if (st === "rejected")      { panel.toast("거부됨: " + reason, Theme.critical); panel.pendingConfirmId = ""; }
            else if (st === "needs_confirm") { panel.toast("확인필요: " + reason, Theme.high); panel.pendingConfirmId = result.cmd_id || ""; }
            else if (st === "sent" || st === "queued") { panel.toast("발행됨: " + reason, Theme.ok); panel.pendingConfirmId = ""; }
            else panel.toast(st + " · " + reason, Theme.subtext);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pad
        spacing: 8

        // 헤더 + 타겟
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            Text { text: "지휘 명령 · COMMAND"; color: Theme.subtext; font.bold: true
                   font.pixelSize: Theme.fsSmall; font.family: Theme.mono }
            Rectangle { width: 1; height: 16; color: Theme.border }
            Text {
                text: panel.target === "" ? "대상: 없음"
                      : "대상: " + (panel.targetDevice.callsign || panel.target)
                        + "  (" + (panel.deviceType || "?").toUpperCase() + ")"
                color: panel.target === "" ? Theme.faint : Theme.cyan
                font.pixelSize: Theme.fsSmall; font.family: Theme.mono
            }
            Item { Layout.fillWidth: true }
            Text {
                visible: panel.picked && panel.picked.label !== undefined
                text: "타겟: " + (panel.picked.label || "") + (panel.picked.hasWorld ? "  ⌖좌표" : "")
                color: Theme.high; font.pixelSize: Theme.fsSmall; font.family: Theme.mono
            }
        }

        // 명령 버튼 그리드
        GridLayout {
            Layout.fillWidth: true
            columns: 8; columnSpacing: 8; rowSpacing: 8

            component CmdBtn: Rectangle {
                id: b
                property string label: ""
                property string sub: ""
                property color tone: Theme.cyan
                property bool danger: false
                property bool enabledFor: true
                Layout.fillWidth: true
                implicitHeight: 54
                radius: Theme.radiusSmall
                opacity: enabledFor ? 1.0 : 0.4
                // 테두리 없음 + 그림자(elevation). 호버 시 살짝 눌림.
                color: ma.containsMouse && enabledFor ? "#f0f2f5" : Theme.panel
                border.width: 0
                signal clicked()
                layer.enabled: enabledFor
                layer.effect: MultiEffect {
                    shadowEnabled: true; shadowColor: Theme.shadow
                    shadowBlur: ma.containsMouse ? 0.7 : 0.4; shadowVerticalOffset: ma.containsMouse ? 3 : 2
                }
                ColumnLayout {
                    anchors.centerIn: parent; spacing: 1
                    Text { text: b.label; color: b.danger ? Theme.critical : Theme.text; font.bold: true
                           font.pixelSize: Theme.fsSmall; Layout.alignment: Qt.AlignHCenter }
                    Text { text: b.sub; color: Theme.faint; font.pixelSize: Theme.fsMicro
                           font.family: Theme.mono; Layout.alignment: Qt.AlignHCenter }
                }
                MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true
                            enabled: b.enabledFor; onClicked: b.clicked() }
            }

            // 공통
            CmdBtn { label: "강조"; sub: "HIGHLIGHT"; tone: Theme.cyan
                     onClicked: panel.fire("highlight") }
            CmdBtn { label: "이동"; sub: "MOVE TO"; tone: Theme.cyan
                     onClicked: panel.fire("move_to") }
            // 대원(visor) 전용
            CmdBtn { label: "진입"; sub: "ADVANCE"; tone: Theme.ok
                     enabledFor: panel.deviceType === "visor"; onClicked: panel.fire("advance") }
            CmdBtn { label: "강제진입"; sub: "FORCE ENTRY"; tone: Theme.high; danger: true
                     enabledFor: panel.deviceType === "visor"; onClicked: panel.fire("force_entry") }
            CmdBtn { label: "후퇴"; sub: "RETREAT"; tone: Theme.warn
                     enabledFor: panel.deviceType === "visor"; onClicked: panel.fire("retreat") }
            // 드론(scout) 전용
            CmdBtn { label: "정찰"; sub: "RECON ZONE"; tone: Theme.cyan
                     enabledFor: panel.deviceType === "scout"; onClicked: panel.fire("recon_zone") }
            CmdBtn { label: "호버"; sub: "HOLD"; tone: Theme.cyan
                     enabledFor: panel.deviceType === "scout"; onClicked: panel.fire("hold") }
            CmdBtn { label: "복귀"; sub: "RTL"; tone: Theme.warn
                     enabledFor: panel.deviceType === "scout"; onClicked: panel.fire("rtl") }
            // 소방장비(APPARATUS)
            CmdBtn { label: "방수"; sub: "AIM & SPRAY"; tone: Theme.accent; danger: true
                     enabledFor: panel.deviceType === "apparatus"; onClicked: panel.fire("aim_and_spray") }
            CmdBtn { label: "열기"; sub: "OPEN"; tone: Theme.ok
                     enabledFor: panel.deviceType === "apparatus"; onClicked: panel.fire("open") }
            CmdBtn { label: "닫기"; sub: "CLOSE"; tone: Theme.warn
                     enabledFor: panel.deviceType === "apparatus"; onClicked: panel.fire("close") }
        }

        // 결과 토스트 + 확인 버튼
        Rectangle {
            id: toastBar
            Layout.fillWidth: true
            implicitHeight: 30
            radius: Theme.radiusSmall
            color: Qt.rgba(tone.r, tone.g, tone.b, 0.14)
            border.width: 1; border.color: tone
            visible: false
            property color tone: Theme.subtext
            function show(msg, color) { tone = color; msgT.text = msg; visible = true; t.restart(); }
            Timer { id: t; interval: 7000; onTriggered: toastBar.visible = false }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 8
                Text { id: msgT; color: toastBar.tone; font.pixelSize: Theme.fsSmall
                       font.family: Theme.mono; Layout.fillWidth: true; elide: Text.ElideRight }
                Rectangle {
                    visible: panel.pendingConfirmId !== ""
                    width: cT.implicitWidth + 18; height: 22; radius: Theme.radius; color: Theme.high
                    Text { id: cT; anchors.centerIn: parent; text: "2차 확인 → 발행"
                           color: "black"; font.bold: true; font.pixelSize: Theme.fsMicro; font.family: Theme.mono }
                    MouseArea { anchors.fill: parent
                        onClicked: { cockpit.confirmCommand(panel.pendingConfirmId); panel.pendingConfirmId = ""; } }
                }
            }
        }
    }
}
