import QtQuick
import QtQuick.Controls

AbstractButton {
    id: root
    property string connState: "disconnected"
    property int size: 168
    readonly property bool on: connState === "connected"
    readonly property color tone: on ? Theme.accent : connState === "connecting" ? Theme.connecting : connState === "error" ? Theme.error : Theme.muted
    implicitWidth: size * 1.55
    implicitHeight: size * 1.55
    Accessible.name: connState === "connected" || connState === "connecting" ? qsTr("Отключить") : qsTr("Подключить")

    Repeater {
        model: [1.55, 1.32, 1.12]
        Rectangle {
            required property real modelData
            required property int index
            anchors.centerIn: parent
            width: root.size * modelData
            height: width
            radius: width / 2
            color: root.tone
            opacity: (root.on ? 0.07 : 0.035) * (index + 1)
            Behavior on color { ColorAnimation { duration: Theme.stateMs } }
        }
    }

    Rectangle {
        id: pulse
        anchors.centerIn: parent
        width: root.size
        height: width
        radius: width / 2
        color: "transparent"
        border.color: root.tone
        border.width: 3
        opacity: 0
        SequentialAnimation {
            running: root.connState === "connecting" && Theme.stateMs > 0
            loops: Animation.Infinite
            onRunningChanged: if (!running) { pulse.scale = 1; pulse.opacity = 0 }
            ParallelAnimation {
                NumberAnimation { target: pulse; property: "scale"; from: 1; to: 1.55; duration: Theme.pulseMs; easing.type: Easing.OutCubic }
                NumberAnimation { target: pulse; property: "opacity"; from: 0.7; to: 0; duration: Theme.pulseMs }
            }
        }
    }

    background: Item {}

    contentItem: Item {
        Rectangle {
            anchors.centerIn: parent
            width: root.size
            height: width
            radius: width / 2
            color: root.on ? Theme.accent : Theme.surface
            border.color: root.on ? "transparent" : root.tone
            border.width: 2
            scale: root.pressed ? 0.96 : 1
            Behavior on color { ColorAnimation { duration: Theme.stateMs; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: 100 } }

            Canvas {
                id: icon
                anchors.centerIn: parent
                width: 60
                height: 60
                readonly property color ink: root.on ? Theme.accentText : root.tone
                onInkChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = ink
                    ctx.lineWidth = 6
                    ctx.lineCap = "round"
                    ctx.beginPath()
                    ctx.arc(30, 34, 22, -Math.PI / 2 + 0.62, 3 * Math.PI / 2 - 0.62)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(30, 5)
                    ctx.lineTo(30, 30)
                    ctx.stroke()
                }
            }
        }
    }
}
