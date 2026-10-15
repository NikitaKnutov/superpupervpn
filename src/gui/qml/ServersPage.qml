import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    signal openProfile(string id)
    signal importRequested()
    spacing: Theme.grid * 1.5

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.topMargin: 4
        Label {
            Layout.fillWidth: true
            text: qsTr("Серверы")
            font.pixelSize: Theme.xl
            font.weight: Font.Bold
            color: Theme.text
        }
        PillButton {
            accent: true
            implicitHeight: 38
            text: App.testing ? qsTr("Проверяю…") : qsTr("Проверить все")
            enabled: !App.testing && App.helperReady && list.count > 0
            onClicked: App.testAll()
        }
    }

    Field {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        iconSource: "qrc:/icons/ui/search.svg"
        placeholderText: qsTr("Поиск по названию или адресу")
        text: App.search
        onTextEdited: App.search = text
        Accessible.name: qsTr("Поиск серверов")
    }

    Segmented {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        implicitHeight: 40
        labels: [qsTr("По имени"), qsTr("По задержке")]
        current: App.sortMode === "delay" ? 1 : 0
        onPicked: index => App.sortMode = index === 1 ? "delay" : "name"
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: App.profiles
        spacing: 8
        reuseItems: true
        cacheBuffer: 800
        bottomMargin: 8
        ScrollBar.vertical: ScrollBar {}
        add: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.listMs } }

        delegate: Item {
            id: cell
            required property var model
            required property int index
            width: ListView.view.width
            height: loader.height
            Loader {
                id: loader
                readonly property var model: cell.model
                x: 16
                width: cell.width - 32
                sourceComponent: cell.model.isHeader ? headerRow : serverRow
            }
        }

        ColumnLayout {
            anchors.centerIn: parent
            visible: list.count === 0
            width: parent.width - 48
            spacing: Theme.grid * 1.5
            Flag { Layout.alignment: Qt.AlignHCenter; size: 64 }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: App.search !== "" ? qsTr("Ничего не найдено") : qsTr("Серверов пока нет")
                font.pixelSize: Theme.md
                font.weight: Font.DemiBold
                color: Theme.text
            }
            PillButton {
                Layout.alignment: Qt.AlignHCenter
                visible: App.search === ""
                accent: true
                text: qsTr("Импортировать")
                onClicked: page.importRequested()
            }
        }
    }

    Component {
        id: headerRow
        Item {
            readonly property var m: parent ? parent.model : ({})
            width: parent ? parent.width : 0
            implicitHeight: col.implicitHeight + 8
            ColumnLayout {
                id: col
                width: parent.width
                y: 4
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    AbstractButton {
                        Layout.fillWidth: true
                        implicitHeight: 32
                        Accessible.name: m.collapsed ? qsTr("Развернуть") : qsTr("Свернуть")
                        onClicked: App.toggleGroup(m.groupId)
                        contentItem: RowLayout {
                            spacing: 6
                            Label {
                                text: m.collapsed ? "▸" : "▾"
                                color: Theme.muted
                            }
                            Label {
                                Layout.fillWidth: true
                                text: m.groupName
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                color: Theme.muted
                                font.pixelSize: Theme.sm
                                font.weight: Font.DemiBold
                                font.capitalization: Font.AllUppercase
                                font.letterSpacing: 0.6
                            }
                            Label {
                                text: m.count
                                color: Theme.muted
                                font.pixelSize: Theme.xs
                            }
                        }
                        background: null
                    }
                    PillButton {
                        visible: m.isSubscription
                        implicitHeight: 32
                        leftPadding: 12
                        rightPadding: 12
                        font.pixelSize: Theme.xs
                        text: m.subUpdating ? qsTr("Обновляю…") : qsTr("Обновить")
                        enabled: !m.subUpdating
                        onClicked: App.refreshSubscription(m.groupId)
                    }
                    PillButton {
                        visible: m.isSubscription
                        implicitHeight: 32
                        implicitWidth: 32
                        leftPadding: 0
                        rightPadding: 0
                        text: "⋯"
                        Accessible.name: qsTr("Действия с подпиской")
                        onClicked: subMenu.open()
                        Menu {
                            id: subMenu
                            MenuItem { text: qsTr("Удалить подписку"); onTriggered: App.removeSubscription(m.groupId) }
                        }
                    }
                }
                Rectangle {
                    visible: m.isSubscription && m.total > 0
                    Layout.fillWidth: true
                    implicitHeight: 6
                    radius: 3
                    color: Theme.surface2
                    Rectangle {
                        width: parent.width * Math.min(1, m.used / Math.max(1, m.total))
                        height: parent.height
                        radius: 3
                        color: Theme.accent
                    }
                }
                Label {
                    visible: m.isSubscription && (m.total > 0 || m.expireDays >= 0)
                    color: Theme.muted
                    font.pixelSize: Theme.xs
                    text: (m.total > 0 ? Theme.bytes(m.used) + qsTr(" из ") + Theme.bytes(m.total) : "")
                        + (m.total > 0 && m.expireDays >= 0 ? "  ·  " : "")
                        + (m.expireDays >= 0 ? qsTr("осталось %1 дн.").arg(m.expireDays) : "")
                }
                Label {
                    visible: m.isSubscription && m.subError !== ""
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    color: Theme.error
                    font.pixelSize: Theme.xs
                    text: m.subError
                }
            }
        }
    }

    Component {
        id: serverRow
        ItemDelegate {
            id: row
            readonly property var m: parent ? parent.model : ({})
            height: 68
            Accessible.name: m.name
            onClicked: { const id = row.m.profileId; App.testProfile(id); App.selectProfile(id) }
            onPressAndHold: page.openProfile(m.profileId)
            background: Rectangle {
                radius: Theme.radius
                color: row.m.selected ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
                     : row.down || row.hovered ? Theme.surface2 : Theme.surface
                border.color: row.m.selected ? Theme.accent : Theme.border
                border.width: row.m.selected ? 1.5 : 1
            }
            contentItem: RowLayout {
                spacing: 12
                Flag { flag: row.m.flag || "" }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        Layout.fillWidth: true
                        text: row.m.name
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: Theme.text
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: row.m.badge + (row.m.insecure ? qsTr(" · небезопасно") : "") + (row.m.nonStandard ? qsTr(" · нестандартный ID") : "")
                        color: row.m.insecure ? Theme.error : Theme.muted
                        font.pixelSize: Theme.xs
                    }
                }
                DelayChip { ms: row.m.delay }
                ToolButton {
                    implicitWidth: 36
                    implicitHeight: 36
                    text: row.m.pinned ? "★" : "☆"
                    font.pixelSize: Theme.lg
                    palette.buttonText: row.m.pinned ? Theme.accentInk : Theme.muted
                    Accessible.name: row.m.pinned ? qsTr("Открепить") : qsTr("Закрепить")
                    onClicked: App.togglePin(row.m.profileId)
                    background: null
                }
                ToolButton {
                    implicitWidth: 32
                    implicitHeight: 36
                    icon.source: "qrc:/icons/ui/chevron.svg"
                    icon.color: Theme.muted
                    icon.width: 18
                    icon.height: 18
                    Accessible.name: qsTr("Карточка профиля")
                    onClicked: page.openProfile(row.m.profileId)
                    background: null
                }
            }
        }
    }
}
