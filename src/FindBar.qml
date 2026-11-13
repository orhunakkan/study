import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Find in the rendered document (the source editor has its own).
Rectangle {
    id: bar
    visible: false
    width: 360
    height: 44
    radius: theme.radius
    color: theme.colors.dark_background
    border.width: 1
    border.color: theme.colors.muted

    signal search(string text, bool backward)
    signal closed()

    property int active: 0
    property int total: 0

    function open() {
        visible = true
        field.forceActiveFocus()
        field.selectAll()
        if (field.text !== "") search(field.text, false)
    }
    function close() {
        if (!visible) return
        visible = false
        total = 0
        closed()
    }
    function setResult(activeMatch, count) { active = activeMatch; total = count }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 6
        spacing: 6

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Find")
            selectByMouse: true
            color: theme.colors.foreground
            background: null
            onTextEdited: bar.search(text, false)
            Keys.onReturnPressed: (event) => bar.search(text, event.modifiers & Qt.ShiftModifier)
            Keys.onEnterPressed: (event) => bar.search(text, event.modifiers & Qt.ShiftModifier)
            Keys.onEscapePressed: bar.close()
        }
        Label {
            text: field.text === "" ? "" : bar.total === 0 ? qsTr("No results")
                                                           : qsTr("%1 of %2").arg(bar.active).arg(bar.total)
            color: bar.total === 0 && field.text !== "" ? theme.colors.red : theme.colors.dark_foreground
            font.pixelSize: 12
        }
        ToolButton { text: "↑"; onClicked: bar.search(field.text, true); focusPolicy: Qt.NoFocus }
        ToolButton { text: "↓"; onClicked: bar.search(field.text, false); focusPolicy: Qt.NoFocus }
        ToolButton { text: "×"; onClicked: bar.close(); focusPolicy: Qt.NoFocus }
    }
}
