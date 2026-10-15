import QtQuick

Rectangle {
    default property alias content: inner.data
    property int padding: Theme.grid * 2
    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.border
    implicitHeight: inner.childrenRect.height + padding * 2

    Item {
        id: inner
        anchors.fill: parent
        anchors.margins: parent.padding
    }
}
