import QtQuick
import QtQuick.Controls

Row {
    id: root
    property int ms: -1
    property bool showText: true
    readonly property color tone: Theme.delayColor(ms)
    readonly property int level: ms < 0 ? 0 : ms < 150 ? 4 : ms < 300 ? 3 : ms <= 800 ? 2 : 1
    spacing: 8

    Label {
        visible: root.showText
        anchors.verticalCenter: parent.verticalCenter
        text: root.ms === -1 ? qsTr("— мс") : root.ms === -3 ? "…" : root.ms < 0 ? qsTr("нет связи") : qsTr("%1 мс").arg(root.ms)
        color: root.ms === -1 || root.ms === -3 ? Theme.muted : root.tone
        font.pixelSize: Theme.xs
        font.features: { "tnum": 1 }
    }
    Row {
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        Repeater {
            model: 4
            Rectangle {
                required property int index
                anchors.bottom: parent.bottom
                width: 3
                height: 5 + index * 3
                radius: 1.5
                color: index < root.level ? root.tone : Theme.border
            }
        }
    }
}
