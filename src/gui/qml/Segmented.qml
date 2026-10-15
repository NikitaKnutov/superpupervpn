import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property var labels: []
    property int current: 0
    signal picked(int index)

    implicitHeight: 48
    radius: height / 2
    color: Theme.surface2
    border.color: Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4
        Repeater {
            model: root.labels
            delegate: AbstractButton {
                id: seg
                required property int index
                required property string modelData
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                text: modelData
                checkable: true
                checked: index === root.current
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData
                onClicked: if (index !== root.current) root.picked(index)
                contentItem: Label {
                    text: seg.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: seg.checked ? Theme.accentText : Theme.muted
                    font.pixelSize: Theme.sm
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                background: Rectangle {
                    radius: height / 2
                    color: seg.checked ? Theme.accent : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.stateMs; easing.type: Easing.OutCubic } }
                }
            }
        }
    }
}
