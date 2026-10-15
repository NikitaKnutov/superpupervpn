import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool accent: false
    property bool danger: false
    implicitHeight: 44
    leftPadding: 18
    rightPadding: 18
    font.pixelSize: Theme.sm
    font.weight: Font.DemiBold
    opacity: enabled ? 1 : 0.45
    contentItem: Label {
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: control.accent ? Theme.accentText : control.danger ? Theme.error : Theme.text
    }
    background: Rectangle {
        radius: height / 2
        color: control.accent ? Theme.accent : Theme.surface2
        border.color: control.accent ? "transparent" : Theme.border
        opacity: control.down ? 0.8 : 1
    }
}
