import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Go to File (Ctrl+P): fuzzy search over the folder's Markdown files.
Rectangle {
    id: overlay
    visible: false
    color: Qt.alpha("black", 0.35)

    signal done()

    property var files: []
    property var matches: []

    function show() {
        files = backend.markdownFiles()
        field.text = ""
        filter()
        visible = true
        field.forceActiveFocus()
    }
    function hide() {
        visible = false
        done()
    }
    function openSelected() {
        if (list.currentIndex >= 0 && list.currentIndex < matches.length)
            backend.openFile(backend.absolutePath(matches[list.currentIndex].path), false)
        hide()
    }

    // Characters in order, rewarding runs, word starts and the file name.
    function score(path, query) {
        if (query === "") return 1
        const lower = path.toLowerCase()
        const nameStart = lower.lastIndexOf("/") + 1
        let s = 0, at = 0, run = 0
        for (let i = 0; i < query.length; i++) {
            const c = query[i]
            if (c === " ") continue
            const found = lower.indexOf(c, at)
            if (found < 0) return -1
            run = found === at ? run + 1 : 0
            s += 1 + run * 3
            if (found === 0 || "/-_. ".includes(lower[found - 1])) s += 4
            if (found >= nameStart) s += 2
            at = found + 1
        }
        return s - path.length * 0.01
    }
    function filter() {
        const q = field.text.toLowerCase()
        const out = []
        for (const path of files) {
            const s = score(path, q)
            if (s >= 0) out.push({ path: path, score: s })
        }
        if (q !== "") out.sort((a, b) => b.score - a.score)
        matches = out.slice(0, 300)
        list.currentIndex = matches.length > 0 ? 0 : -1
    }

    TapHandler { onTapped: overlay.hide() }

    Rectangle {
        id: panel
        anchors.horizontalCenter: parent.horizontalCenter
        y: 60
        width: Math.min(640, parent.width - 40)
        height: Math.min(field.height + 24 + list.contentHeight, parent.height - 120)
        radius: theme.radius
        color: theme.colors.dark_background
        border.width: 1
        border.color: theme.colors.muted
        TapHandler {}   // clicks on the panel stay on it

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            TextField {
                id: field
                Layout.fillWidth: true
                placeholderText: qsTr("Go to file…")
                color: theme.colors.foreground
                onTextEdited: overlay.filter()
                Keys.onDownPressed: list.incrementCurrentIndex()
                Keys.onUpPressed: list.decrementCurrentIndex()
                Keys.onReturnPressed: overlay.openSelected()
                Keys.onEnterPressed: overlay.openSelected()
                Keys.onEscapePressed: overlay.hide()
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: overlay.matches
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 0
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: item
                    required property var modelData
                    required property int index
                    width: list.width
                    height: 46
                    radius: Math.min(4, theme.radius)
                    color: ListView.isCurrentItem ? theme.colors.selection : "transparent"
                    readonly property string name: modelData.path.split("/").pop()
                    readonly property string folder: modelData.path.slice(0, Math.max(0, modelData.path.length - name.length - 1))

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 10
                        width: parent.width - 20
                        Label {
                            width: parent.width
                            text: item.name
                            elide: Text.ElideRight
                            color: theme.colors.bright_foreground
                            font.pixelSize: 14
                        }
                        Label {
                            width: parent.width
                            text: item.folder
                            visible: text !== ""
                            elide: Text.ElideMiddle
                            color: theme.colors.dark_foreground
                            font.pixelSize: 12
                        }
                    }
                    TapHandler {
                        onTapped: { list.currentIndex = item.index; overlay.openSelected() }
                    }
                }
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                visible: overlay.matches.length === 0
                text: backend.rootFolder === "" ? qsTr("Open a folder first (Ctrl+O).") : qsTr("No Markdown files match.")
                color: theme.colors.dark_foreground
                bottomPadding: 8
            }
        }
    }
}
