import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What the document area shows with no tab open.
Rectangle {
    color: theme.colors.background

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 14

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Study"
            color: theme.colors.bright_foreground
            font.pixelSize: 34
            font.weight: Font.DemiBold
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: backend.rootFolder === "" ? qsTr("Open a folder to start reading.")
                                            : qsTr("Pick a Markdown file in the sidebar.")
            color: theme.colors.foreground
            font.pixelSize: 15
        }
        GridLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 10
            columns: 2
            columnSpacing: 18
            rowSpacing: 6
            Repeater {
                model: [
                    ["Ctrl+O", qsTr("Open Folder")],
                    ["Ctrl+P", qsTr("Go to File")],
                    ["Ctrl+Shift+V", qsTr("Switch between preview and source")],
                    ["?", qsTr("All shortcuts")],
                ]
                delegate: Item {
                    required property var modelData
                    Layout.columnSpan: 2
                    implicitWidth: row.implicitWidth
                    implicitHeight: row.implicitHeight
                    Row {
                        id: row
                        spacing: 14
                        Label {
                            width: 120
                            horizontalAlignment: Text.AlignRight
                            text: modelData[0]
                            color: theme.accent
                            font.family: "monospace"
                            font.pixelSize: 13
                        }
                        Label { text: modelData[1]; color: theme.colors.dark_foreground; font.pixelSize: 13 }
                    }
                }
            }
        }
    }
}
