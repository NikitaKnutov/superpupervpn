import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ColumnLayout {
    id: page
    spacing: Theme.grid * 1.5
    property int tab: 0
    readonly property var logModel: tab === 0 ? App.appLogs : App.coreLogs
    readonly property var levels: ["all", "info", "warning", "error"]

    Label {
        Layout.leftMargin: 16
        Layout.topMargin: 4
        text: qsTr("Логи")
        font.pixelSize: Theme.xl
        font.weight: Font.Bold
        color: Theme.text
    }

    Segmented {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        labels: [qsTr("Приложение"), qsTr("Ядро")]
        current: page.tab
        onPicked: index => page.tab = index
    }

    Flow {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        spacing: Theme.grid
        Repeater {
            model: [qsTr("Все"), qsTr("Info+"), qsTr("Warning+"), qsTr("Ошибки")]
            PillButton {
                required property int index
                required property string modelData
                implicitHeight: 34
                leftPadding: 14
                rightPadding: 14
                font.pixelSize: Theme.xs
                text: modelData
                accent: page.levels.indexOf(page.logModel.level) === index
                onClicked: page.logModel.level = page.levels[index]
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        spacing: Theme.grid
        Field {
            Layout.fillWidth: true
            iconSource: "qrc:/icons/ui/search.svg"
            placeholderText: qsTr("Поиск")
            text: page.logModel.search
            onTextEdited: page.logModel.search = text
        }
        PillButton {
            implicitHeight: 46
            text: page.logModel.paused ? qsTr("Продолжить") : qsTr("Пауза")
            onClicked: page.logModel.paused = !page.logModel.paused
        }
        PillButton {
            implicitHeight: 46
            text: qsTr("Экспорт")
            onClicked: saveDialog.open()
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.bottomMargin: 4
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 12
            clip: true
            model: page.logModel
            spacing: 4
            ScrollBar.vertical: ScrollBar {}
            onCountChanged: if (!page.logModel.paused) positionViewAtEnd()
            delegate: Text {
                required property string time
                required property string level
                required property string msg
                width: ListView.view.width
                wrapMode: Text.WrapAnywhere
                textFormat: Text.PlainText
                font.family: "monospace"
                font.pixelSize: Theme.xs
                color: level === "error" || level === "fatal" ? Theme.error : level === "warning" ? Theme.connecting : Theme.text
                text: time + "  " + level.toUpperCase().padEnd(7) + " " + msg
            }
            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("Пока пусто")
                color: Theme.muted
            }
        }
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "zip"
        nameFilters: [qsTr("Архив zip (*.zip)")]
        onAccepted: App.exportLogs(selectedFile)
    }
}
