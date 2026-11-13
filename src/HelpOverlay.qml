import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Every shortcut, shown with ? or F1.
Rectangle {
    id: overlay
    color: Qt.alpha("black", 0.55)

    readonly property var shortcuts: [
        ["Ctrl+O", qsTr("Open a folder")],
        ["Ctrl+P", qsTr("Go to a Markdown file")],
        ["Ctrl+Shift+V", qsTr("Switch between preview and source")],
        [qsTr("Double click"), qsTr("Edit the source at that spot")],
        ["Ctrl+S", qsTr("Save")],
        ["Ctrl+F", qsTr("Find (in the editor: find and replace)")],
        ["Ctrl+W", qsTr("Close the tab")],
        ["Ctrl+Shift+T", qsTr("Reopen the last closed tab")],
        ["Ctrl+Tab  Ctrl+PgDn", qsTr("Next tab")],
        ["Ctrl+Shift+Tab  Ctrl+PgUp", qsTr("Previous tab")],
        ["Alt+1 … Alt+9", qsTr("Go to tab")],
        ["Ctrl+K Ctrl+B", qsTr("Show or hide the sidebar")],
        ["Ctrl+=  Ctrl+-  Ctrl+0", qsTr("Zoom in, out, reset")],
        ["Ctrl+Z  Ctrl+Shift+Z", qsTr("Undo, redo (editing)")],
        ["? F1", qsTr("This help")],
        ["Q  Ctrl+Q", qsTr("Quit (Q outside the editor)")],
    ]

    TapHandler { onTapped: overlay.visible = false }

    DialogPanel {
        anchors.centerIn: parent
        width: grid.implicitWidth + 64
        height: grid.implicitHeight + titleLabel.implicitHeight + 64

        Label {
            id: titleLabel
            x: 32
            y: 24
            text: qsTr("Shortcuts")
            color: theme.colors.bright_foreground
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }
        GridLayout {
            id: grid
            x: 32
            anchors.top: titleLabel.bottom
            anchors.topMargin: 14
            columns: 2
            columnSpacing: 24
            rowSpacing: 7
            Repeater {
                model: overlay.shortcuts.length * 2
                delegate: Label {
                    required property int index
                    readonly property var entry: overlay.shortcuts[Math.floor(index / 2)]
                    text: entry[index % 2]
                    color: index % 2 === 0 ? theme.accent : theme.colors.foreground
                    font.family: index % 2 === 0 ? "monospace" : ""
                    font.pixelSize: 13
                }
            }
        }
    }
}
