import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The bottom line: where you are, what mode the tab is in, and messages.
Rectangle {
    id: line
    property real zoom: 1
    implicitHeight: 26
    color: theme.colors.dark_background

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 16

        Label {
            Layout.fillWidth: true
            elide: Text.ElideMiddle
            font.pixelSize: 12
            text: backend.message !== "" ? backend.message : backend.currentRelativePath
            color: backend.message !== "" ? theme.colors.yellow : theme.colors.dark_foreground
        }
        Label {
            visible: Math.abs(line.zoom - 1) > 0.01
            text: Math.round(line.zoom * 100) + "%"
            font.pixelSize: 12
            color: theme.colors.dark_foreground
        }
        Label {
            visible: backend.currentPath !== ""
            text: backend.editing ? qsTr("Editing") : qsTr("Preview")
            font.pixelSize: 12
            font.weight: Font.DemiBold
            color: backend.editing ? theme.accent : theme.colors.dark_foreground
            TapHandler { onTapped: backend.toggleMode() }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }
    }
}
