import QtQuick
import QtQuick.Controls

AbstractButton {
    id: nav
    property bool active: false
    property bool vertical: true
    implicitHeight: vertical ? 56 : 44
    implicitWidth: vertical ? 72 : 160
    Accessible.name: text
    contentItem: Item {
        Column {
            visible: nav.vertical
            anchors.centerIn: parent
            spacing: 2
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 48
                height: 28
                radius: 14
                color: nav.active ? Theme.accent : "transparent"
                ToolButton {
                    anchors.centerIn: parent
                    enabled: false
                    icon.source: nav.icon.source
                    icon.color: nav.active ? Theme.accentText : Theme.muted
                    icon.width: 20
                    icon.height: 20
                    background: null
                }
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: nav.text
                font.pixelSize: 11
                font.weight: nav.active ? Font.DemiBold : Font.Normal
                color: nav.active ? Theme.text : Theme.muted
            }
        }
        Rectangle {
            visible: !nav.vertical
            anchors.fill: parent
            radius: Theme.radiusSmall
            color: nav.active ? Theme.surface2 : "transparent"
            Row {
                anchors.verticalCenter: parent.verticalCenter
                x: 12
                spacing: 10
                ToolButton {
                    anchors.verticalCenter: parent.verticalCenter
                    enabled: false
                    icon.source: nav.icon.source
                    icon.color: nav.active ? Theme.accentInk : Theme.muted
                    icon.width: 20
                    icon.height: 20
                    background: null
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: nav.text
                    font.weight: nav.active ? Font.DemiBold : Font.Normal
                    color: nav.active ? Theme.text : Theme.muted
                }
            }
        }
    }
    background: Item {}
}
