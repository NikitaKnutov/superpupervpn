import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    signal openServers()
    signal openDiagnostics()
    contentWidth: availableWidth

    readonly property string st: App.state
    property double now: Date.now()
    Timer { interval: 1000; repeat: true; running: page.st === "connected"; onTriggered: page.now = Date.now() }

    function elapsed() {
        if (page.st !== "connected") return "00:00:00"
        const s = Math.max(0, Math.floor((page.now - App.connectedSince) / 1000))
        const pad = n => (n < 10 ? "0" : "") + n
        return pad(Math.floor(s / 3600)) + ":" + pad(Math.floor(s / 60) % 60) + ":" + pad(s % 60)
    }

    component StatTile: Rectangle {
        id: tile
        property string label
        property string value
        property url iconSource
        Layout.fillWidth: true
        implicitHeight: 76
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10
            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: 18
                color: Theme.surface2
                ToolButton {
                    anchors.centerIn: parent
                    enabled: false
                    icon.source: tile.iconSource
                    icon.color: Theme.accentInk
                    icon.width: 18
                    icon.height: 18
                    background: null
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label { text: tile.label; color: Theme.muted; font.pixelSize: Theme.xs }
                Label {
                    text: tile.value
                    color: Theme.text
                    font.pixelSize: Theme.md
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }
    }

    ColumnLayout {
        width: Math.min(page.availableWidth, 520)
        x: (page.availableWidth - width) / 2
        spacing: Theme.grid * 2

        Card {
            visible: !App.helperReady
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 4
            ColumnLayout {
                width: parent.width
                spacing: Theme.grid
                Label {
                    text: qsTr("The %1 service is not running").arg("SuperPuperVPN")
                    font.pixelSize: Theme.md
                    font.weight: Font.DemiBold
                    color: Theme.error
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.muted
                    text: App.helperError !== "" ? App.helperError : qsTr("You cannot connect without the service. It is needed for TUN and runs the core.")
                }
                PillButton { text: qsTr("Start service"); accent: true; onClicked: App.startService(false) }
            }
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4
            implicitWidth: statusRow.implicitWidth + 28
            implicitHeight: 34
            radius: height / 2
            color: Theme.surface2
            border.color: Theme.border
            Row {
                id: statusRow
                anchors.centerIn: parent
                spacing: 8
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8
                    height: 8
                    radius: 4
                    color: Theme.stateColor(page.st)
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.st === "connected" ? qsTr("Protected")
                        : page.st === "connecting" ? qsTr("Connecting…")
                        : page.st === "error" ? qsTr("Error") : qsTr("Not protected")
                    color: page.st === "disconnected" ? Theme.muted : Theme.stateColor(page.st)
                    font.pixelSize: Theme.sm
                    font.weight: Font.DemiBold
                }
            }
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: page.elapsed()
            font.pixelSize: Theme.timer
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
            color: page.st === "connected" ? Theme.text : Theme.muted
        }

        ConnectButton {
            Layout.alignment: Qt.AlignHCenter
            connState: page.st
            size: page.height < 700 ? 140 : 160
            enabled: App.helperReady
            onClicked: App.toggleConnection()
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            color: Theme.muted
            font.pixelSize: Theme.sm
            text: page.st === "connected" || page.st === "connecting" ? qsTr("Click to disconnect") : qsTr("Click to connect")
        }

        ColumnLayout {
            visible: page.st === "error"
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: Theme.grid
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                color: Theme.error
                text: App.errorText
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: Theme.grid
                PillButton { text: qsTr("Retry"); accent: true; onClicked: App.connectNow() }
                PillButton { text: qsTr("Diagnostics"); onClicked: page.openDiagnostics() }
            }
        }

        Segmented {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            labels: [qsTr("System proxy"), qsTr("TUN")]
            current: App.settings.mode === "proxy" ? 0 : 1
            onPicked: index => App.setSetting("mode", index === 0 ? "proxy" : "tun")
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: Theme.grid * 1.5
            StatTile {
                label: qsTr("Download")
                value: page.st === "connected" ? Theme.bytes(App.traffic.down || 0) + qsTr("/s") : "—"
                iconSource: "qrc:/icons/ui/down.svg"
            }
            StatTile {
                label: qsTr("Upload")
                value: page.st === "connected" ? Theme.bytes(App.traffic.up || 0) + qsTr("/s") : "—"
                iconSource: "qrc:/icons/ui/up.svg"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            implicitHeight: 76
            radius: Theme.radius
            color: serverTap.pressed ? Theme.surface2 : Theme.surface
            border.color: Theme.border
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Server: %1").arg(App.current.name || qsTr("not selected"))
            TapHandler { id: serverTap; onTapped: page.openServers() }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10
                spacing: 12
                Flag { flag: App.current.flag || "" }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        Layout.fillWidth: true
                        text: App.current.name || qsTr("Choose a server")
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                        font.pixelSize: Theme.md
                        font.weight: Font.DemiBold
                        color: Theme.text
                    }
                    Label {
                        text: App.current.badge || qsTr("Click to open the list")
                        color: Theme.muted
                        font.pixelSize: Theme.xs
                    }
                }
                DelayChip { visible: !!App.current.id; ms: App.current.delay !== undefined ? App.current.delay : -1 }
                ToolButton {
                    enabled: false
                    icon.source: "qrc:/icons/ui/chevron.svg"
                    icon.color: Theme.muted
                    icon.width: 18
                    icon.height: 18
                    background: null
                }
            }
        }

        Label {
            visible: page.st === "connected"
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("This session: %1").arg(Theme.bytes((App.traffic.upTotal || 0) + (App.traffic.downTotal || 0)))
            font.features: { "tnum": 1 }
            font.pixelSize: Theme.xs
            color: Theme.muted
        }

        Item { Layout.preferredHeight: Theme.grid }
    }
}
