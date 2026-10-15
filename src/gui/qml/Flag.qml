import QtQuick
import QtQuick.Controls

Rectangle {
    property string flag: ""
    property int size: 40
    width: size
    height: size
    radius: size / 2
    color: Theme.surface2
    border.color: Theme.border
    clip: true
    Label {
        anchors.centerIn: parent
        text: parent.flag !== "" ? parent.flag : "🌐"
        font.pixelSize: parent.size * 0.62
    }
}
