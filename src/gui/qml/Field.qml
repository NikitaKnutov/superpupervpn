import QtQuick
import QtQuick.Controls

TextField {
    id: control
    property url iconSource: ""
    implicitHeight: 46
    leftPadding: iconSource != "" ? 42 : 16
    rightPadding: 16
    color: Theme.text
    placeholderTextColor: Theme.muted
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentText
    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.surface2
        border.color: control.activeFocus ? Theme.accent : Theme.border
        border.width: control.activeFocus ? 1.5 : 1
        ToolButton {
            visible: control.iconSource != ""
            enabled: false
            anchors.verticalCenter: parent.verticalCenter
            x: 6
            icon.source: control.iconSource
            icon.color: Theme.muted
            icon.width: 18
            icon.height: 18
            background: null
        }
    }
}
