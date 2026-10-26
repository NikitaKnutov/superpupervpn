import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 420
    height: 760
    minimumWidth: 380
    minimumHeight: 640
    visible: !App.startHidden
    title: "SuperPuperVPN"
    color: Theme.bg

    readonly property bool wide: width >= 900
    property int page: 0
    readonly property var pages: [
        { t: qsTr("Home"), i: "qrc:/icons/ui/home.svg" },
        { t: qsTr("Servers"), i: "qrc:/icons/ui/globe.svg" },
        { t: qsTr("Logs"), i: "qrc:/icons/ui/logs.svg" },
        { t: qsTr("Settings"), i: "qrc:/icons/ui/settings.svg" }
    ]

    palette {
        window: Theme.bg
        windowText: Theme.text
        base: Theme.surface2
        alternateBase: Theme.surface
        text: Theme.text
        button: Theme.surface2
        buttonText: Theme.text
        highlight: Theme.accent
        highlightedText: Theme.accentText
        placeholderText: Theme.muted
        mid: Theme.border
        midlight: Theme.surface2
        dark: Theme.accent
        brightText: Theme.accentText
        light: Theme.surface
        toolTipBase: Theme.surface2
        toolTipText: Theme.text
    }

    function showServers() { page = 1 }
    function showDiagnostics() { diagDialog.open() }

    Connections {
        target: App
        function onShowWindowRequested() { win.show(); win.raise(); win.requestActivate() }
        function onImportPreviewReady(p) { importDialog.showPreview(p) }
        function onConfirmConflicts(text) { conflictDialog.details = text; conflictDialog.open() }
        function onConfirmInsecure(id) { profileDialog.show(id) }
        function onToast(text) { toast.show(text) }
    }

    onClosing: close => {
        close.accepted = false
        if (App.trayAvailable) {
            if (App.settings.closeToTray) win.hide(); else App.quit()
            return
        }
        const b = App.settings.noTrayClose
        if (b === "background") win.hide()
        else if (b === "quit") App.quit()
        else closeDialog.open()
    }

    Shortcut {
        sequences: [StandardKey.Paste]
        enabled: !(win.activeFocusItem && win.activeFocusItem.hasOwnProperty("selectByMouse"))
        onActivated: importDialog.fromClipboard()
    }

    header: Item {
        implicitHeight: 64
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 16
            spacing: 10
            Image {
                source: "qrc:/icons/superpupervpn.svg"
                sourceSize: Qt.size(32, 32)
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
            }
            Label {
                Layout.fillWidth: true
                text: "SuperPuperVPN"
                font.pixelSize: Theme.lg
                font.weight: Font.Bold
                color: Theme.text
                elide: Text.ElideRight
            }
            RoundButton {
                implicitWidth: 44
                implicitHeight: 44
                icon.source: "qrc:/icons/ui/plus.svg"
                icon.color: Theme.text
                icon.width: 22
                icon.height: 22
                Accessible.name: qsTr("Import")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Import")
                onClicked: importDialog.start()
                background: Rectangle {
                    radius: width / 2
                    color: Theme.surface2
                    border.color: Theme.border
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.bottomMargin: win.wide ? 0 : navBar.height + 16
        spacing: 0

        Rectangle {
            visible: win.wide
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            Layout.margins: 12
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            Column {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 4
                Repeater {
                    model: [0, 1, 2, 3]
                    Tab {
                        required property int modelData
                        width: parent.width
                        vertical: false
                        text: win.pages[modelData].t
                        icon.source: win.pages[modelData].i
                        active: win.page === modelData
                        onClicked: win.page = modelData
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: win.page

            HomePage {
                onOpenServers: win.showServers()
                onOpenDiagnostics: win.showDiagnostics()
            }
            ServersPage {
                onOpenProfile: id => profileDialog.show(id)
                onImportRequested: importDialog.start()
            }
            LogsPage {}
            SettingsPage { id: settingsPage }
        }
    }

    Rectangle {
        id: navBar
        visible: !win.wide
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        width: parent.width - 24
        height: 68
        radius: 26
        color: Theme.surface
        border.color: Theme.border
        RowLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 0
            Repeater {
                model: win.pages
                Tab {
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: modelData.t
                    icon.source: modelData.i
                    active: win.page === index
                    onClicked: win.page = index
                }
            }
        }
    }

    ImportDialog { id: importDialog }
    ProfileDialog { id: profileDialog }
    Onboarding {
        id: onboarding
        Component.onCompleted: if (!App.settings.onboardingDone) open()
        onImportRequested: importDialog.start()
    }

    Sheet {
        id: closeDialog
        title: qsTr("Quit or keep running in the background?")
        contentItem: ColumnLayout {
            spacing: Theme.grid * 2
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.muted
                text: qsTr("There is no tray icon. In the background the connection stays up, and launching %1 again opens the window.").arg("SuperPuperVPN")
            }
            CheckBox { id: remember; text: qsTr("Remember my choice") }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.grid
                PillButton {
                    Layout.fillWidth: true
                    text: qsTr("Quit")
                    onClicked: { if (remember.checked) App.setSetting("noTrayClose", "quit"); closeDialog.close(); App.quit() }
                }
                PillButton {
                    Layout.fillWidth: true
                    accent: true
                    text: qsTr("Keep running")
                    onClicked: { if (remember.checked) App.setSetting("noTrayClose", "background"); closeDialog.close(); win.hide() }
                }
            }
        }
    }

    Sheet {
        id: conflictDialog
        property string details: ""
        title: qsTr("Possible conflict")
        contentItem: ColumnLayout {
            spacing: Theme.grid * 2
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.muted
                text: qsTr("Detected:\n%1\n\nThis may break the connection or let part of the traffic bypass the tunnel.").arg(conflictDialog.details)
            }
            PillButton {
                Layout.fillWidth: true
                visible: (App.diagnostics.conflictingAdapters || []).length > 0
                accent: true
                text: qsTr("Turn off other VPNs and connect")
                onClicked: { conflictDialog.close(); App.stopOtherVpns(true) }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.grid
                PillButton { Layout.fillWidth: true; text: qsTr("Cancel"); onClicked: conflictDialog.close() }
                PillButton {
                    Layout.fillWidth: true
                    accent: (App.diagnostics.conflictingAdapters || []).length === 0
                    text: qsTr("Connect anyway")
                    onClicked: { conflictDialog.close(); App.acknowledgeConflicts() }
                }
            }
        }
    }

    Sheet {
        id: diagDialog
        readonly property var d: App.diagnostics
        readonly property var other: d.conflictingAdapters || []
        title: qsTr("Diagnostics")
        onOpened: App.runDiagnostics()
        contentItem: ColumnLayout {
            spacing: Theme.grid * 1.5
            Repeater {
                model: [
                    [qsTr("Service"), App.helperReady ? qsTr("running") + (diagDialog.d.helperVersion ? " · " + diagDialog.d.helperVersion : "") : qsTr("not running")],
                    [qsTr("Core"), diagDialog.d.coreVersion ? "mihomo " + diagDialog.d.coreVersion : "—"],
                    [qsTr("Other VPNs"), diagDialog.other.length > 0 ? diagDialog.other.join(", ") : qsTr("none found")],
                    [qsTr("External IP"), diagDialog.d.externalIp || (App.state === "connected" ? qsTr("unknown") : qsTr("not connected"))]
                ]
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Theme.grid
                    Label { text: parent.modelData[0]; color: Theme.muted }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        text: parent.modelData[1]
                        color: Theme.text
                    }
                }
            }
            Label {
                visible: App.settings.mode === "tun" && (!!diagDialog.d.ipForwarding || !!diagDialog.d.hotspot)
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.error
                font.pixelSize: Theme.xs
                text: diagDialog.d.hotspot ? qsTr("Windows Mobile Hotspot is on and may interfere with TUN mode")
                                          : qsTr("IP forwarding is on and may interfere with TUN mode")
            }
            PillButton {
                Layout.fillWidth: true
                visible: !App.helperReady
                accent: true
                text: qsTr("Start service")
                onClicked: App.startService(false)
            }
            PillButton {
                Layout.fillWidth: true
                visible: App.helperReady && diagDialog.other.length > 0
                accent: true
                text: qsTr("Turn off other VPNs")
                onClicked: App.stopOtherVpns(false)
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.grid
                PillButton { Layout.fillWidth: true; text: qsTr("Check again"); enabled: App.helperReady; onClicked: App.runDiagnostics() }
                PillButton { Layout.fillWidth: true; text: qsTr("Close"); onClicked: diagDialog.close() }
            }
        }
    }

    Rectangle {
        id: toast
        function show(t) { toastText.text = t; opacity = 1; toastTimer.restart() }
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: win.wide ? 24 : navBar.height + 24
        width: Math.min(toastText.implicitWidth + 40, win.width - 32)
        height: toastText.implicitHeight + 24
        radius: height / 2 > 24 ? 18 : height / 2
        color: Theme.surface2
        border.color: Theme.border
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.listMs } }
        Label {
            id: toastText
            anchors.centerIn: parent
            width: parent.width - 40
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            color: Theme.text
        }
        Timer { id: toastTimer; interval: 3500; onTriggered: toast.opacity = 0 }
    }
}
