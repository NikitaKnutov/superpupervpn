import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Sheet {
    id: dlg
    title: qsTr("Add server")
    property string stage: "choose"
    property var preview: ({})
    property string pendingUrl: ""
    property string pendingName: ""

    function start() { stage = "choose"; urlField.text = ""; open() }
    function fromClipboard() { showPreview(App.importFromClipboard()) }
    function showPreview(p) {
        preview = p
        if (p.error) stage = "error"
        else if (p.needHttpConfirm) { pendingUrl = p.url; pendingName = p.name || ""; stage = "http" }
        else if (p.loading) stage = "loading"
        else stage = "confirm"
        if (!opened) open()
    }
    onRejected: App.cancelImport()

    component Tile: AbstractButton {
        id: tile
        property string subtitle
        Layout.fillWidth: true
        implicitHeight: 128
        contentItem: ColumnLayout {
            spacing: 8
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 48
                implicitHeight: 48
                radius: 24
                color: Theme.accent
                ToolButton {
                    anchors.centerIn: parent
                    enabled: false
                    icon.source: tile.icon.source
                    icon.color: Theme.accentText
                    icon.width: 22
                    icon.height: 22
                    background: null
                }
            }
            Label {
                Layout.fillWidth: true
                text: tile.text
                font.weight: Font.DemiBold
                color: Theme.text
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
            Label {
                Layout.fillWidth: true
                text: tile.subtitle
                color: Theme.muted
                font.pixelSize: Theme.xs
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
        background: Rectangle {
            radius: Theme.radius
            color: tile.down || tile.hovered ? Theme.surface2 : Theme.bg
            border.color: Theme.border
        }
    }
    component InfoRow: RowLayout {
        property string name
        property string value
        Layout.fillWidth: true
        visible: value !== ""
        Label { text: parent.name; color: Theme.muted }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            text: parent.value
            textFormat: Text.PlainText
            elide: Text.ElideMiddle
            color: Theme.text
        }
    }

    contentItem: StackLayout {
        currentIndex: ["choose", "url", "loading", "confirm", "http", "error"].indexOf(dlg.stage)

        RowLayout {
            spacing: Theme.grid * 1.5
            Tile {
                text: qsTr("From clipboard")
                subtitle: qsTr("vless://, several links or base64")
                icon.source: "qrc:/icons/ui/clipboard.svg"
                onClicked: dlg.fromClipboard()
            }
            Tile {
                text: qsTr("Subscription")
                subtitle: qsTr("https:// link")
                icon.source: "qrc:/icons/ui/link.svg"
                onClicked: dlg.stage = "url"
            }
        }

        ColumnLayout {
            spacing: Theme.grid * 1.5
            Field {
                id: urlField
                Layout.fillWidth: true
                iconSource: "qrc:/icons/ui/link.svg"
                placeholderText: "https://"
                inputMethodHints: Qt.ImhUrlCharactersOnly
                onAccepted: if (text !== "") App.previewSubscription(text, false)
            }
            PillButton {
                Layout.fillWidth: true
                accent: true
                text: qsTr("Load")
                enabled: urlField.text !== ""
                onClicked: App.previewSubscription(urlField.text, false)
            }
        }

        ColumnLayout {
            spacing: Theme.grid
            BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: dlg.stage === "loading" }
            Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("Loading %1…").arg(dlg.preview.domain || ""); color: Theme.muted }
        }

        ColumnLayout {
            spacing: Theme.grid * 1.5
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: infoCol.implicitHeight + 28
                radius: Theme.radius
                color: Theme.bg
                border.color: Theme.border
                ColumnLayout {
                    id: infoCol
                    x: 14
                    y: 14
                    width: parent.width - 28
                    spacing: 10
                    InfoRow { name: qsTr("Source"); value: dlg.preview.source || "" }
                    InfoRow { name: qsTr("Address"); value: dlg.preview.domain || "" }
                    InfoRow { name: qsTr("Name"); value: dlg.preview.title || "" }
                    InfoRow { name: qsTr("Found"); value: String(dlg.preview.found || 0) }
                    InfoRow { name: qsTr("Skipped"); value: String(dlg.preview.skipped || 0) }
                }
            }
            AbstractButton {
                id: reasonsToggle
                visible: (dlg.preview.skipped || 0) > 0
                checkable: true
                contentItem: Label {
                    text: reasonsToggle.checked ? qsTr("Hide reasons") : qsTr("Why skipped")
                    color: Theme.accentInk
                    font.pixelSize: Theme.sm
                }
                background: null
            }
            ScrollView {
                visible: reasonsToggle.checked && reasonsToggle.visible
                Layout.fillWidth: true
                Layout.preferredHeight: 110
                Label {
                    width: parent.width
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    color: Theme.muted
                    font.pixelSize: Theme.xs
                    text: (dlg.preview.reasons || []).join("\n")
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.grid
                PillButton { Layout.fillWidth: true; text: qsTr("Cancel"); onClicked: dlg.reject() }
                PillButton {
                    Layout.fillWidth: true
                    accent: true
                    text: qsTr("Add")
                    enabled: (dlg.preview.found || 0) > 0
                    onClicked: { App.confirmImport(); dlg.close() }
                }
            }
        }

        ColumnLayout {
            spacing: Theme.grid * 2
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.text
                text: qsTr("A subscription over http:// is not encrypted: anyone in between can read and change its contents, including server keys.")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.grid
                PillButton { Layout.fillWidth: true; text: qsTr("Cancel"); onClicked: dlg.reject() }
                PillButton { Layout.fillWidth: true; danger: true; text: qsTr("Load anyway"); onClicked: App.previewSubscription(dlg.pendingUrl, true, dlg.pendingName) }
            }
        }

        ColumnLayout {
            spacing: Theme.grid * 2
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                color: Theme.error
                text: dlg.preview.error || ""
            }
            PillButton { Layout.fillWidth: true; text: qsTr("Close"); onClicked: dlg.reject() }
        }
    }
}
