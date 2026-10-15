import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: pop
    modal: true
    closePolicy: Popup.NoAutoClose
    anchors.centerIn: Overlay.overlay
    width: Math.min(parent ? parent.width - 24 : 400, 460)
    height: Math.min(parent ? parent.height - 48 : 560, 520)
    padding: Theme.grid * 3
    signal importRequested()
    background: Rectangle { color: Theme.surface; radius: 28; border.color: Theme.border }
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }

    function finish() { App.setSetting("onboardingDone", true); close() }

    component Slide: ColumnLayout {
        id: slide
        property string title
        property string body
        property url iconSource
        spacing: Theme.grid * 2
        Rectangle {
            Layout.topMargin: Theme.grid
            implicitWidth: 72
            implicitHeight: 72
            radius: 36
            color: Theme.accent
            ToolButton {
                anchors.centerIn: parent
                enabled: false
                icon.source: slide.iconSource
                icon.color: Theme.accentText
                icon.width: 32
                icon.height: 32
                background: null
            }
        }
        Label { Layout.fillWidth: true; text: slide.title; font.pixelSize: Theme.xl; font.weight: Font.Bold; wrapMode: Text.Wrap; color: Theme.text }
        Label { Layout.fillWidth: true; Layout.fillHeight: true; text: slide.body; wrapMode: Text.Wrap; font.pixelSize: Theme.md; color: Theme.muted; lineHeight: 1.2 }
    }

    contentItem: ColumnLayout {
        spacing: Theme.grid * 2
        SwipeView {
            id: view
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            Slide {
                iconSource: "qrc:/icons/ui/shield.svg"
                title: qsTr("Два режима")
                body: qsTr("«Системный прокси» направляет через сервер браузеры и программы, которые его учитывают.\n\n«TUN» пропускает через сервер весь трафик компьютера, включая игры и терминал.")
            }
            Slide {
                iconSource: "qrc:/icons/ui/settings.svg"
                title: qsTr("Служба")
                body: qsTr("Для режима TUN нужна служба %1: она создаёт сетевой интерфейс и управляет ядром. Права администратора понадобились один раз — при установке.").arg("SuperPuperVPN")
            }
            Slide {
                iconSource: "qrc:/icons/ui/plus.svg"
                title: qsTr("Добавьте сервер")
                body: qsTr("Скопируйте ссылку vless:// или адрес подписки и нажмите «Импортировать». Подключение — одной большой кнопкой.")
            }
        }
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6
            Repeater {
                model: view.count
                Rectangle {
                    required property int index
                    width: index === view.currentIndex ? 22 : 8
                    height: 8
                    radius: 4
                    color: index === view.currentIndex ? Theme.accent : Theme.border
                    Behavior on width { NumberAnimation { duration: Theme.listMs } }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.grid
            PillButton { text: qsTr("Пропустить"); onClicked: pop.finish() }
            Item { Layout.fillWidth: true }
            PillButton {
                visible: view.currentIndex < view.count - 1
                accent: true
                text: qsTr("Далее")
                onClicked: view.incrementCurrentIndex()
            }
            PillButton {
                visible: view.currentIndex === view.count - 1
                accent: true
                text: qsTr("Импортировать")
                onClicked: { pop.finish(); pop.importRequested() }
            }
        }
    }
}
