import QtQuick
import Frize.Cockpit

// 상단뷰 택티컬 맵 ― 사이트 ENU(미터)를 위에서 내려다본 미니맵.
// 디바이스 위치 + 위험구역을 한눈에. (원점 = 출동지점)
Rectangle {
    id: map
    width: 240; height: 240
    radius: Theme.radius
    color: Theme.panelGlass
    border.width: 1; border.color: Theme.border
    property real scale_px_per_m: 1.6     // 1m = 1.6px
    function mx(x) { return width / 2 + x * scale_px_per_m }
    function my(y) { return height / 2 - y * scale_px_per_m }   // 북(+y)이 위로

    // 그리드
    Canvas {
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.strokeStyle = Qt.rgba(1,1,1,0.06); ctx.lineWidth = 1;
            const step = 10 * map.scale_px_per_m;  // 10m 격자
            for (let x = width/2 % step; x < width; x += step) { ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,height); ctx.stroke(); }
            for (let y = height/2 % step; y < height; y += step) { ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(width,y); ctx.stroke(); }
            // 원점 십자
            ctx.strokeStyle = Qt.rgba(0.21,0.77,0.88,0.5);
            ctx.beginPath(); ctx.moveTo(width/2-6,height/2); ctx.lineTo(width/2+6,height/2);
            ctx.moveTo(width/2,height/2-6); ctx.lineTo(width/2,height/2+6); ctx.stroke();
        }
    }

    Text { anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 8
           text: "TACTICAL · TOP-DOWN"; color: Theme.subtext; font.pixelSize: Theme.fsMicro; font.family: Theme.mono }
    Text { anchors.bottom: parent.bottom; anchors.right: parent.right; anchors.margins: 6
           text: "10m"; color: Theme.faint; font.pixelSize: Theme.fsMicro; font.family: Theme.mono }

    // 위험구역
    Repeater {
        model: cockpit.hazardZones
        delegate: Rectangle {
            property var z: modelData
            property color tone: severityToColor(z.severity)
            function severityToColor(s) {
                return s === "critical" ? Theme.critical : s === "high" ? Theme.high
                     : s === "low" ? Theme.warn : Theme.subtext;
            }
            width: Math.max(10, z.radius * 2 * map.scale_px_per_m)
            height: width; radius: width / 2
            x: map.mx(z.x) - width / 2
            y: map.my(z.y) - height / 2
            color: Qt.rgba(tone.r, tone.g, tone.b, 0.16)
            border.color: tone; border.width: 1
        }
    }

    // 디바이스
    Repeater {
        model: cockpit.devices
        delegate: Item {
            x: map.mx(model.posX); y: map.my(model.posY)
            Rectangle {
                width: 12; height: 12; radius: model.deviceType === "scout" ? 2 : 6
                x: -6; y: -6
                color: model.deviceType === "scout" ? Theme.cyan : Theme.accent
                border.color: "white"; border.width: model.deviceId === cockpit.selectedDeviceId ? 2 : 0
            }
            Text { text: model.callsign; x: 8; y: -6; color: Theme.text
                   font.pixelSize: Theme.fsMicro; font.family: Theme.mono }
        }
    }
}
