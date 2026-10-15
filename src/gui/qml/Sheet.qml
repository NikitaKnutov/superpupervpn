import QtQuick
import QtQuick.Controls

Dialog {
    id: sheet
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(parent ? parent.width - 24 : 420, 480)
    padding: Theme.grid * 2.5
    topPadding: Theme.grid * 2
    background: Rectangle {
        color: Theme.surface
        radius: 24
        border.color: Theme.border
    }
    header: Label {
        visible: sheet.title !== ""
        text: sheet.title
        padding: Theme.grid * 2.5
        bottomPadding: 0
        font.pixelSize: Theme.lg
        font.weight: Font.DemiBold
        color: Theme.text
        elide: Text.ElideRight
    }
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }
}
