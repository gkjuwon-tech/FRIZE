pragma Singleton
import QtQuick

// FRIZE 콕핏 디자인 토큰 ― 라이트 / Material 톤.
// 원칙: 흰색 베이스, 무채색, 테두리 대신 그림자(elevation). 강조색은 치명경보에만.
QtObject {
    // 표면
    readonly property color bg:          "#eceef1"   // 앱 배경(라이트 그레이)
    readonly property color bgElevated:  "#ffffff"
    readonly property color panel:       "#ffffff"   // 카드
    readonly property color panelGlass:  Qt.rgba(1, 1, 1, 0.92)
    readonly property color surfaceAlt:  "#f4f5f7"
    readonly property color border:      "#ffffff"   // 테두리 거의 안 씀
    readonly property color borderSoft:  "#eef0f3"
    readonly property color divider:     "#e6e9ed"

    // 텍스트
    readonly property color text:        "#1f2429"
    readonly property color subtext:     "#5f6671"
    readonly property color faint:       "#9aa1ab"

    // 강조 ― 거의 무채색, 치명만 빨강
    readonly property color accent:      "#33414f"   // 슬레이트(선택/주요 텍스트)
    readonly property color accentDim:   "#dfe3e8"
    readonly property color cyan:        "#4a6572"   // 보조(드론/선택)
    readonly property color amber:       "#9a7b2e"   // 타겟

    // 상태/심각도 (라이트 배경용, 절제)
    readonly property color ok:          "#3f8a5a"
    readonly property color warn:        "#b08a2c"
    readonly property color high:        "#c1762c"
    readonly property color critical:    "#d23b2e"
    readonly property color offline:     "#9aa1ab"

    // 그림자(Material elevation 근사)
    readonly property color shadow:      Qt.rgba(0.10, 0.13, 0.18, 0.18)

    // 치수
    readonly property int radius:        6
    readonly property int radiusSmall:   4
    readonly property int pad:           14
    readonly property int gap:           10

    // 폰트
    readonly property string ui:         "Segoe UI"
    readonly property string mono:       "Consolas"
    readonly property int fsTitle:       19
    readonly property int fsBody:        13
    readonly property int fsSmall:       12
    readonly property int fsMicro:       10
}
