import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The opened folder as a tree. Every file shows; only Markdown files open.
// One click peeks at a file in a temporary tab, a double click keeps it.
Rectangle {
    id: sidebar
    color: theme.colors.dark_background

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.topMargin: 12
            Layout.bottomMargin: 6
            text: qsTr("FOLDERS")
            color: theme.colors.dark_foreground
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 0.8
            visible: backend.rootFolder !== ""
        }

        // The folder itself, like Sublime Text's top node.
        ItemDelegate {
            Layout.fillWidth: true
            visible: backend.rootFolder !== ""
            implicitHeight: 26
            padding: 0
            leftPadding: 12
            onClicked: tree.visible = !tree.visible
            background: Rectangle { color: parent.hovered ? Qt.alpha(theme.colors.foreground, 0.06) : "transparent" }
            contentItem: Row {
                spacing: 6
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 10
                    text: tree.visible ? "▾" : "▸"
                    color: theme.colors.dark_foreground
                    font.pixelSize: 11
                }
                FileGlyph {
                    anchors.verticalCenter: parent.verticalCenter
                    folder: true
                    open: tree.visible
                    color: theme.colors.foreground
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: backend.rootName
                    color: theme.colors.bright_foreground
                    font.pixelSize: 13
                }
            }
        }

        TreeView {
            id: tree
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: backend.rootFolder !== ""
            clip: true
            model: backend.tree
            rootIndex: backend.tree.rootIndex
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            columnWidthProvider: () => tree.width
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Item {
                id: node
                required property TreeView treeView
                required property bool expanded
                required property bool hasChildren
                required property int depth
                required property int row
                required property string name
                required property string path
                required property bool isDir
                required property bool isMarkdown

                readonly property bool openable: isDir || isMarkdown
                readonly property bool active: !isDir && path === backend.currentPath

                implicitWidth: tree.width
                implicitHeight: 26

                Rectangle {
                    anchors.fill: parent
                    color: node.active ? theme.colors.selection
                         : hover.hovered && node.openable ? Qt.alpha(theme.colors.foreground, 0.06)
                         : "transparent"
                }
                Rectangle {
                    visible: node.active
                    width: 2
                    height: parent.height
                    color: theme.accent
                }

                Row {
                    x: 12 + (node.depth + 1) * 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - x - 8
                    spacing: 6
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 10
                        text: node.isDir ? (node.expanded ? "▾" : "▸") : ""
                        color: theme.colors.dark_foreground
                        font.pixelSize: 11
                    }
                    FileGlyph {
                        anchors.verticalCenter: parent.verticalCenter
                        folder: node.isDir
                        open: node.expanded
                        color: node.isMarkdown ? theme.accent
                             : node.isDir ? theme.colors.foreground
                             : theme.colors.muted
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 40
                        text: node.name
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        color: node.active ? theme.colors.bright_foreground
                             : node.openable ? theme.colors.foreground
                             : theme.colors.dark_foreground
                        opacity: node.openable ? 1 : 0.75
                    }
                }

                HoverHandler {
                    id: hover
                    cursorShape: node.openable ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
                TapHandler {
                    onTapped: {
                        if (node.isDir)
                            node.treeView.toggleExpanded(node.row)
                        else if (node.isMarkdown)
                            backend.openFile(node.path, true)
                    }
                    onDoubleTapped: if (node.isMarkdown) backend.openFile(node.path, false)
                }
                ToolTip.visible: hover.hovered && !node.openable && !node.isDir
                ToolTip.delay: 600
                ToolTip.text: qsTr("Study opens Markdown files only")
            }
        }

        // No folder yet.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 20
            visible: backend.rootFolder === ""
            spacing: 12
            Item { Layout.fillHeight: true }
            Label {
                Layout.fillWidth: true
                text: qsTr("No folder open")
                color: theme.colors.foreground
                horizontalAlignment: Text.AlignHCenter
            }
            DialogButton {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Open Folder…")
                primary: true
                onClicked: backend.chooseFolder()
            }
            Item { Layout.fillHeight: true }
        }
    }
}
