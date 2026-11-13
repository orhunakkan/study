import QtQuick
import QtQuick.Controls

// From Omacut (MIT, David Heinemeier Hansson), themed.
// A dialog button the way Omarchy's apps draw them (Omawrite's square dialog
// buttons, as Plain uses them): the window's corners, capped small, a bright
// border on the one with focus, and the accent on the primary one. Its
// mnemonic letter is underlined, and that letter presses it, with or without
// Alt: no dialog here has a text field for the bare letter to type into.
Button {
    id: control
    property bool primary: false
    property string mnemonic: ""

    leftPadding: 16
    rightPadding: 16
    topPadding: 8
    bottomPadding: 8
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    // The text with its mnemonic underlined, the first match either case.
    readonly property string formattedText: {
        var at = mnemonic.length ? text.toLowerCase().indexOf(mnemonic.toLowerCase()) : -1
        return at < 0 ? text : text.slice(0, at) + "<u>" + text.charAt(at) + "</u>" + text.slice(at + 1)
    }

    contentItem: Label {
        text: control.formattedText
        textFormat: Text.StyledText
        color: control.primary ? theme.accentForeground : theme.colors.foreground
        font.pixelSize: 13
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        implicitWidth: 88
        implicitHeight: 34
        radius: Math.min(4, theme.radius)
        color: control.primary
            ? Qt.darker(theme.accent, control.down ? 1.2 : control.hovered ? 1.08 : 1)
            : Qt.alpha(theme.colors.foreground, control.down ? 0.16 : control.hovered ? 0.11 : 0.07)
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? theme.colors.bright_foreground
            : control.primary ? Qt.darker(theme.accent, 1.15) : theme.colors.muted
    }
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    Shortcut {
        sequences: [control.mnemonic, "Alt+" + control.mnemonic]
        enabled: control.visible && control.mnemonic !== ""
        context: Qt.ApplicationShortcut
        // Q both asks to quit and answers Quit: holding it must not do both.
        autoRepeat: false
        onActivated: control.clicked()
    }
}
