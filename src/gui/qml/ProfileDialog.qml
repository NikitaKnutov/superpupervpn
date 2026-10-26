import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Sheet {
    id: dlg
    height: Math.min(parent ? parent.height - 48 : 600, 680)
    property string profileId: ""
    property bool reveal: false
    property var d: ({})
    readonly property var fields: [
        [QT_TR_NOOP("Subscription"), "subscription"], [QT_TR_NOOP("Address"), "host"], [QT_TR_NOOP("Port"), "port"],
        [QT_TR_NOOP("ID"), "uuid"], [QT_TR_NOOP("Transport"), "network"], [QT_TR_NOOP("Security"), "security"],
        [QT_TR_NOOP("SNI"), "sni"], [QT_TR_NOOP("Fingerprint"), "fp"], [QT_TR_NOOP("Flow"), "flow"],
        [QT_TR_NOOP("Encryption"), "encryption"], [QT_TR_NOOP("Public key"), "pbk"], [QT_TR_NOOP("Short ID"), "sid"],
        [QT_TR_NOOP("spx"), "spx"], [QT_TR_NOOP("Path"), "path"], [QT_TR_NOOP("Host"), "hostHeader"],
        [QT_TR_NOOP("serviceName"), "serviceName"], [QT_TR_NOOP("mode"), "mode"], [QT_TR_NOOP("ALPN"), "alpn"]]

    function show(id) { profileId = id; reveal = false; reload(); open() }
    function reload() { d = App.profileDetails(profileId, reveal); nameField.text = d.name || "" }
    onRevealChanged: reload()
    onClosed: { reveal = false; d = ({}) }
    title: d.badge || ""

    contentItem: ScrollView {
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: Theme.grid * 1.5

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.grid
                Field { id: nameField; Layout.fillWidth: true; Accessible.name: qsTr("Name") }
                PillButton {
                    implicitHeight: 46
                    accent: true
                    text: qsTr("Save")
                    enabled: nameField.text !== dlg.d.name
                    onClicked: { App.renameProfile(dlg.profileId, nameField.text); dlg.reload() }
                }
            }
            Label {
                visible: !!dlg.d.nonStandardId
                text: qsTr("Non-standard ID: passed to the core as is")
                color: Theme.connecting
                font.pixelSize: Theme.xs
            }
            Rectangle {
                visible: !!dlg.d.insecure
                Layout.fillWidth: true
                implicitHeight: insecureCol.implicitHeight + 24
                radius: Theme.radius
                color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.10)
                border.color: Theme.error
                ColumnLayout {
                    id: insecureCol
                    x: 12
                    y: 12
                    width: parent.width - 24
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: Theme.error
                        text: qsTr("Insecure: the link turns off certificate verification (allowInsecure). The connection can be intercepted.")
                    }
                    Switch {
                        text: qsTr("I understand the risk, allow connecting")
                        checked: !!dlg.d.insecureConsent
                        onToggled: { App.setInsecureConsent(dlg.profileId, checked); dlg.reload() }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: fieldCol.implicitHeight + 16
                radius: Theme.radius
                color: Theme.bg
                border.color: Theme.border
                ColumnLayout {
                    id: fieldCol
                    x: 14
                    y: 8
                    width: parent.width - 28
                    spacing: 0
                    Repeater {
                        model: dlg.fields
                        RowLayout {
                            required property var modelData
                            readonly property string val: String(dlg.d[modelData[1]] || "")
                            Layout.fillWidth: true
                            Layout.minimumHeight: 36
                            visible: val !== ""
                            spacing: Theme.grid
                            Label { text: qsTr(parent.modelData[0]); color: Theme.muted }
                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignRight
                                text: parent.val
                                textFormat: Text.PlainText
                                wrapMode: Text.WrapAnywhere
                                color: Theme.text
                                font.family: ["uuid", "pbk", "sid"].indexOf(parent.modelData[1]) >= 0 ? "monospace" : ""
                            }
                        }
                    }
                }
            }
            Label {
                visible: (dlg.d.unknownKeys || []).length > 0
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.muted
                font.pixelSize: Theme.xs
                text: qsTr("Unknown parameters (kept): %1").arg((dlg.d.unknownKeys || []).join(", "))
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: Theme.grid
                columnSpacing: Theme.grid
                PillButton { Layout.fillWidth: true; text: dlg.reveal ? qsTr("Hide keys") : qsTr("Show keys"); onClicked: dlg.reveal = !dlg.reveal }
                PillButton { Layout.fillWidth: true; text: qsTr("Duplicate"); onClicked: { App.duplicateProfile(dlg.profileId); dlg.close() } }
                PillButton { Layout.fillWidth: true; danger: true; text: qsTr("Delete"); onClicked: confirmDelete.open() }
                PillButton { Layout.fillWidth: true; accent: true; text: qsTr("Done"); onClicked: dlg.close() }
            }
        }
    }

    Sheet {
        id: confirmDelete
        title: qsTr("Delete this server?")
        contentItem: RowLayout {
            spacing: Theme.grid
            PillButton { Layout.fillWidth: true; text: qsTr("Cancel"); onClicked: confirmDelete.close() }
            PillButton {
                Layout.fillWidth: true
                danger: true
                text: qsTr("Delete")
                onClicked: { confirmDelete.close(); App.deleteProfile(dlg.profileId); dlg.close() }
            }
        }
    }
}
