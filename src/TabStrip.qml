import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Sublime Text style tabs: a peek tab's name is italic, an edited tab shows a
// dot that turns into a close button on hover, middle click closes, double
// click keeps a peek, and tabs can be dragged into another order.
Rectangle {
    id: strip
    implicitHeight: 36
    color: theme.colors.dark_background

    ListView {
        id: list
        anchors.fill: parent
        orientation: ListView.Horizontal
        model: backend.tabs
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentWidth > width
        currentIndex: backend.currentIndex
        onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
        ScrollBar.horizontal: ScrollBar { height: 3; policy: ScrollBar.AsNeeded }

        delegate: Item {
            id: tab
            required property int index
            required property string title
            required property string path
            required property bool peek
            required property bool dirty
            required property bool editing
            required property bool missing

            readonly property bool active: index === backend.currentIndex
            width: Math.min(240, Math.max(120, label.implicitWidth + 58))
            height: list.height

            Rectangle {
                anchors.fill: parent
                color: tab.active ? theme.colors.background
                     : hover.hovered ? Qt.alpha(theme.colors.foreground, 0.05) : "transparent"
            }
            Rectangle {
                visible: tab.active
                width: parent.width
                height: 2
                color: theme.accent
            }
            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: theme.colors.darker_background
            }

            Label {
                id: label
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.right: closer.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: tab.title
                elide: Text.ElideMiddle
                font.pixelSize: 13
                font.italic: tab.peek
                font.strikeout: tab.missing
                color: tab.active ? theme.colors.bright_foreground : theme.colors.foreground
                opacity: tab.active ? 1 : 0.8
            }

            // ● when edited; × on hover (or for the active tab).
            Item {
                id: closer
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                readonly property bool showClose: closeHover.hovered || (!tab.dirty && (hover.hovered || tab.active))
                Rectangle {
                    anchors.fill: parent
                    radius: Math.min(4, theme.radius)
                    color: closeHover.hovered ? Qt.alpha(theme.colors.foreground, 0.12) : "transparent"
                }
                Label {
                    anchors.centerIn: parent
                    text: closer.showClose ? "×" : tab.dirty ? "●" : ""
                    color: tab.dirty && !closer.showClose ? theme.accent : theme.colors.foreground
                    font.pixelSize: closer.showClose ? 16 : 10
                }
                HoverHandler { id: closeHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: backend.requestClose(tab.index) }
            }

            HoverHandler { id: hover }
            TapHandler {
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                onTapped: (point, button) => {
                    if (button === Qt.MiddleButton) backend.requestClose(tab.index)
                    else backend.activate(tab.index)
                }
                onDoubleTapped: (point, button) => { if (button === Qt.LeftButton) backend.makePermanent(tab.index) }
            }
            DragHandler {
                id: drag
                target: null
                xAxis.enabled: true
                yAxis.enabled: false
                onTranslationChanged: {
                    const center = tab.x + tab.width / 2 + translation.x
                    const over = list.indexAt(center, tab.height / 2)
                    if (over >= 0 && over !== tab.index) backend.moveTab(tab.index, over)
                }
            }

            ToolTip.visible: hover.hovered && !closeHover.hovered
            ToolTip.delay: 800
            ToolTip.text: tab.path
        }
    }
}
