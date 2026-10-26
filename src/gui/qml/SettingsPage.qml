import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    contentWidth: availableWidth

    component SectionTitle: Label {
        Layout.topMargin: Theme.grid * 2
        Layout.leftMargin: 4
        font.pixelSize: Theme.xs
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.8
        color: Theme.muted
    }
    component Group: Rectangle {
        default property alias rows: groupCol.data
        Layout.fillWidth: true
        implicitHeight: groupCol.implicitHeight + 8
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        ColumnLayout {
            id: groupCol
            x: 16
            y: 4
            width: parent.width - 32
            spacing: 0
        }
    }
    component Row2: RowLayout {
        Layout.fillWidth: true
        Layout.minimumHeight: 56
        spacing: Theme.grid
    }
    component Divider: Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Theme.border
    }
    component Name: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Theme.text
    }
    component Value: Label {
        color: Theme.muted
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        Layout.maximumWidth: 220
    }
    component Combo: ComboBox {
        id: combo
        implicitContentWidthPolicy: ComboBox.WidestText
        implicitHeight: 40
        background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.surface2
            border.color: Theme.border
        }
        delegate: ItemDelegate {
            id: item
            required property var modelData
            required property int index
            width: ListView.view ? ListView.view.width : implicitWidth
            text: modelData
            highlighted: combo.highlightedIndex === index
            contentItem: Label {
                text: item.text
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                color: item.highlighted ? Theme.accentText : Theme.text
                font.weight: combo.currentIndex === item.index ? Font.DemiBold : Font.Normal
            }
            background: Rectangle {
                radius: 10
                color: item.highlighted ? Theme.accent : "transparent"
            }
        }
        popup.background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.surface
            border.color: Theme.border
        }
        popup.padding: 4
    }

    ColumnLayout {
        width: Math.min(page.availableWidth - 32, 640)
        x: (page.availableWidth - width) / 2
        spacing: Theme.grid

        Label {
            Layout.topMargin: 4
            text: qsTr("Settings")
            font.pixelSize: Theme.xl
            font.weight: Font.Bold
            color: Theme.text
        }
        Label {
            visible: App.warning !== ""
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.connecting
            text: App.warning
        }

        SectionTitle { text: qsTr("General") }
        Group {
            Row2 {
                Name { text: qsTr("Language") }
                Combo {
                    readonly property var keys: ["en", "ru"]
                    // Language names stay in their own language so anyone can find theirs.
                    model: ["English", "Русский"]
                    currentIndex: Math.max(0, keys.indexOf(App.settings.language))
                    onActivated: index => App.setSetting("language", keys[index])
                }
            }
            Divider {}
            Row2 {
                Name { text: qsTr("Theme") }
                Combo {
                    readonly property var keys: ["light", "dark"]
                    model: [qsTr("Light"), qsTr("Dark")]
                    currentIndex: App.settings.theme === "light" ? 0 : 1
                    onActivated: index => App.setSetting("theme", keys[index])
                }
            }
            Divider {}
            Row2 {
                Name { text: qsTr("Start with the system") }
                Switch { checked: App.settings.autostart; onToggled: App.setSetting("autostart", checked) }
            }
            Divider {}
            Row2 {
                Name { text: qsTr("Connect on start") }
                Switch { checked: App.settings.connectOnStart; onToggled: App.setSetting("connectOnStart", checked) }
            }
            Divider {}
            Row2 {
                visible: App.trayAvailable
                Name { text: qsTr("Minimize to tray when the window is closed") }
                Switch { checked: App.settings.closeToTray; onToggled: App.setSetting("closeToTray", checked) }
            }
            Row2 {
                visible: !App.trayAvailable
                Name { text: qsTr("When the window is closed") }
                Combo {
                    readonly property var keys: ["ask", "background", "quit"]
                    model: [qsTr("Ask"), qsTr("Keep running"), qsTr("Quit")]
                    currentIndex: Math.max(0, keys.indexOf(App.settings.noTrayClose))
                    onActivated: index => App.setSetting("noTrayClose", keys[index])
                }
            }
        }

        SectionTitle { text: qsTr("Connection") }
        Group {
            Row2 {
                Name { text: qsTr("Default mode") }
                Combo {
                    readonly property var keys: ["tun", "proxy"]
                    model: [qsTr("TUN"), qsTr("System proxy")]
                    currentIndex: keys.indexOf(App.settings.mode)
                    onActivated: index => App.setSetting("mode", keys[index])
                }
            }
            Divider {}
            Row2 {
                Name { text: qsTr("Update subscriptions") }
                Combo {
                    readonly property var hours: [0, 1, 6, 12, 24, 72, 168]
                    model: [qsTr("As the subscription says"), qsTr("Every hour"), qsTr("Every 6 h"), qsTr("Every 12 h"),
                            qsTr("Once a day"), qsTr("Every 3 days"), qsTr("Once a week")]
                    currentIndex: Math.max(0, hours.indexOf(App.settings.subIntervalHours))
                    onActivated: index => App.setSetting("subIntervalHours", hours[index])
                }
            }
        }

        SectionTitle { id: serviceTitle; text: qsTr("Service") }
        Group {
            id: serviceGroup
            readonly property var d: App.diagnostics
            Row2 {
                Rectangle {
                    implicitWidth: 10
                    implicitHeight: 10
                    radius: 5
                    color: App.helperReady ? Theme.connected : Theme.error
                }
                Name {
                    text: App.helperReady ? qsTr("Running") : qsTr("Not running")
                    font.weight: Font.DemiBold
                }
                PillButton {
                    implicitHeight: 36
                    accent: !App.helperReady
                    text: App.helperReady ? qsTr("Restart") : qsTr("Start")
                    onClicked: App.startService(App.helperReady)
                }
            }
            Label {
                visible: App.helperError !== ""
                Layout.fillWidth: true
                Layout.bottomMargin: 8
                wrapMode: Text.Wrap
                color: Theme.error
                font.pixelSize: Theme.xs
                text: App.helperError
            }
            Divider {}
            Row2 { Name { text: qsTr("App") } Value { text: App.version } }
            Divider {}
            Row2 { Name { text: qsTr("Service") } Value { text: serviceGroup.d.helperVersion || "—" } }
            Divider {}
            Row2 { Name { text: qsTr("Core") } Value { text: serviceGroup.d.coreVersion ? "mihomo " + serviceGroup.d.coreVersion : "—" } }
            Divider {}
            Row2 {
                Name { text: qsTr("External IP") }
                Value { text: serviceGroup.d.externalIp || (App.state === "connected" ? qsTr("unknown") : qsTr("not connected")) }
            }
            Divider {}
            Row2 {
                PillButton {
                    Layout.fillWidth: true
                    implicitHeight: 40
                    text: qsTr("Check")
                    enabled: App.helperReady
                    onClicked: App.runDiagnostics()
                }
            }
        }

        SectionTitle { text: qsTr("Advanced") }
        Group {
            Row2 {
                Name { text: qsTr("Core log level") }
                Combo {
                    model: ["silent", "error", "warning", "info", "debug"]
                    currentIndex: model.indexOf(App.settings.logLevel)
                    onActivated: index => App.setSetting("logLevel", model[index])
                }
            }
            Divider {}
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.bottomMargin: 12
                spacing: 8
                Name { text: qsTr("DoH resolver (through the connection)") }
                Field {
                    Layout.fillWidth: true
                    text: App.settings.dohUrl
                    onEditingFinished: if (text !== App.settings.dohUrl) App.setSetting("dohUrl", text)
                    inputMethodHints: Qt.ImhUrlCharactersOnly
                }
            }
            Divider {}
            Row2 {
                Name { text: qsTr("TUN MTU (0 = default)") }
                SpinBox {
                    from: 0
                    to: 9000
                    editable: true
                    value: App.settings.mtu
                    onValueModified: App.setSetting("mtu", value)
                }
            }
            Divider {}
            Row2 {
                Name { text: qsTr("Hide server addresses in logs") }
                Switch { checked: App.settings.maskHosts; onToggled: App.setSetting("maskHosts", checked) }
            }
        }
        Item { Layout.preferredHeight: Theme.grid * 2 }
    }
}
