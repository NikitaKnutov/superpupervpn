pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: App.colors.bg
    readonly property color surface: App.colors.surface
    readonly property color surface2: App.colors.surface2
    readonly property color text: App.colors.text
    readonly property color muted: App.colors.muted
    readonly property color accent: App.colors.accent
    readonly property color accentText: App.colors.accentText
    readonly property color accentInk: App.colors.accentInk
    readonly property color connected: App.colors.connected
    readonly property color connecting: App.colors.connecting
    readonly property color error: App.colors.error
    readonly property color border: Qt.rgba(text.r, text.g, text.b, App.darkTheme ? 0.07 : 0.09)

    readonly property int radius: 20
    readonly property int radiusSmall: 14
    readonly property int grid: 8
    readonly property int touch: 48

    readonly property int xs: 12
    readonly property int sm: 14
    readonly property int md: 16
    readonly property int lg: 20
    readonly property int xl: 28
    readonly property int timer: 44

    readonly property int stateMs: App.reduceMotion ? 0 : 250
    readonly property int listMs: App.reduceMotion ? 0 : 150
    readonly property int pulseMs: 1400

    function stateColor(s) {
        return s === "connected" ? connected : s === "connecting" ? connecting : s === "error" ? error : muted
    }
    function delayColor(ms) {
        return ms === -1 || ms === -3 ? muted : ms < 0 ? error : ms < 300 ? connected : ms <= 800 ? connecting : error
    }
    function bytes(n) {
        if (n === undefined || n < 0) return "—"
        const u = [qsTr("B"), qsTr("KB"), qsTr("MB"), qsTr("GB"), qsTr("TB")]
        let i = 0
        while (n >= 1024 && i < u.length - 1) { n /= 1024; i++ }
        return (i === 0 ? n : n.toFixed(1)) + " " + u[i]
    }
}
