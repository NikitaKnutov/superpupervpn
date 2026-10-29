import QtQuick

// Emoji flags need a font that has them: Windows has none and shows "NL", Linux without a color emoji
// font shows boxes. So the flag emoji only names the country and the picture comes from assets/flags.
Rectangle {
    property string flag: ""
    property int size: 40
    readonly property string code: flag.length === 4
        ? String.fromCharCode(flag.codePointAt(0) - 0x1F1A5, flag.codePointAt(2) - 0x1F1A5).toLowerCase() : ""
    width: size
    height: size
    radius: size / 2
    color: Theme.surface2
    border.color: Theme.border
    Image {
        id: country
        anchors.fill: parent
        sourceSize: Qt.size(parent.size * 2, parent.size * 2)
        source: parent.code !== "" ? "qrc:/flags/" + parent.code + ".svg" : ""
    }
    Image {
        anchors.fill: parent
        visible: country.status !== Image.Ready
        sourceSize: Qt.size(parent.size * 2, parent.size * 2)
        source: "qrc:/flags/xx.svg"
    }
}
