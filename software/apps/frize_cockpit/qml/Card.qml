import QtQuick
import QtQuick.Effects
import Frize.Cockpit

// 흰색 카드 ― 테두리 없음, Material elevation 그림자.
Rectangle {
    color: Theme.panel
    radius: Theme.radius
    border.width: 0
    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: Theme.shadow
        shadowBlur: 0.5
        shadowVerticalOffset: 2
        shadowHorizontalOffset: 0
    }
}
