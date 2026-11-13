import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtWebChannel
import QtWebEngine

ApplicationWindow {
    id: win
    width: prefs.width
    height: prefs.height
    minimumWidth: 640
    minimumHeight: 420
    visible: true
    title: backend.currentPath !== "" ? qsTr("%1 — Study").arg(backend.currentRelativePath) : "Study"
    color: theme.colors.background

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    Material.background: theme.colors.background
    Material.foreground: theme.colors.foreground

    Settings {
        id: prefs
        category: "window"
        property int width: 1280
        property int height: 860
        property real zoom: 1.0
        property real sidebarWidth: 280
        property bool sidebarVisible: true
    }
    onWidthChanged: prefs.width = width
    onHeightChanged: prefs.height = height

    readonly property bool overlayOpen: quickOpen.visible || help.visible || confirm.visible
    // Bare-letter shortcuts only where nothing takes typing.
    readonly property bool plainKeys: !backend.editing && !overlayOpen && !findBar.visible
    property bool quitting: false
    property int pendingCloseIndex: -1

    function requestQuit() {
        if (backend.hasDirty) {
            confirm.ask("quit")
        } else {
            quitting = true
            Qt.quit()
        }
    }
    function focusPage() { web.forceActiveFocus() }
    function zoomBy(step) { prefs.zoom = Math.max(0.5, Math.min(3, Math.round((prefs.zoom + step) * 10) / 10)) }

    onClosing: (close) => {
        if (!quitting && backend.hasDirty) {
            close.accepted = false
            confirm.ask("quit")
        }
    }

    Connections {
        target: backend
        function onConfirmClose(index) { win.pendingCloseIndex = index; confirm.ask("close") }
        function onCurrentChanged() { if (!win.overlayOpen && !findBar.visible) win.focusPage() }
        function onRaiseRequested() { win.raise(); win.requestActivate() }
    }

    // ---- Keyboard ----------------------------------------------------------

    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: backend.chooseFolder() }
    Shortcut { sequence: "Ctrl+P"; context: Qt.ApplicationShortcut; onActivated: quickOpen.show() }
    Shortcut { sequence: "Ctrl+Shift+V"; context: Qt.ApplicationShortcut; enabled: !win.overlayOpen; onActivated: backend.toggleMode() }
    Shortcut { sequences: [StandardKey.Save]; context: Qt.ApplicationShortcut; onActivated: backend.saveCurrent() }
    Shortcut { sequence: "Ctrl+W"; context: Qt.ApplicationShortcut; enabled: !win.overlayOpen; onActivated: backend.requestClose(backend.currentIndex) }
    Shortcut { sequence: "Ctrl+Shift+T"; context: Qt.ApplicationShortcut; onActivated: backend.reopenClosed() }
    Shortcut { sequences: ["Ctrl+Tab", "Ctrl+PgDown"]; context: Qt.ApplicationShortcut; onActivated: backend.cycle(1) }
    Shortcut { sequences: ["Ctrl+Shift+Tab", "Ctrl+Backtab", "Ctrl+PgUp"]; context: Qt.ApplicationShortcut; onActivated: backend.cycle(-1) }
    Shortcut { sequence: "Ctrl+K, Ctrl+B"; context: Qt.ApplicationShortcut; onActivated: prefs.sidebarVisible = !prefs.sidebarVisible }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; enabled: !backend.editing && backend.tabs.count > 0 && !win.overlayOpen; onActivated: findBar.open() }
    Shortcut { sequences: ["Ctrl+=", "Ctrl++"]; context: Qt.ApplicationShortcut; onActivated: win.zoomBy(0.1) }
    Shortcut { sequence: "Ctrl+-"; context: Qt.ApplicationShortcut; onActivated: win.zoomBy(-0.1) }
    Shortcut { sequence: "Ctrl+0"; context: Qt.ApplicationShortcut; onActivated: prefs.zoom = 1.0 }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: win.plainKeys; onActivated: help.visible = !help.visible }
    Shortcut { sequence: "F1"; context: Qt.ApplicationShortcut; onActivated: help.visible = !help.visible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: win.plainKeys; autoRepeat: false; onActivated: win.requestQuit() }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; autoRepeat: false; onActivated: win.requestQuit() }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: help.visible || findBar.visible
        onActivated: { help.visible = false; findBar.close() }
    }
    Instantiator {
        model: 9
        delegate: Shortcut {
            required property int index
            sequence: "Alt+" + (index + 1)
            context: Qt.ApplicationShortcut
            onActivated: backend.activate(index === 8 ? backend.tabs.count - 1 : index)
        }
    }

    // ---- Layout --------------------------------------------------------------

    Item {
        anchors.fill: parent
        // Right-to-left languages mirror the window; the Translator sets the
        // application's layout direction.
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        SplitView {
            anchors.fill: parent
            orientation: Qt.Horizontal
            // Remember the sidebar's width once a drag ends (not while laying out).
            onResizingChanged: if (!resizing && sidebar.visible) prefs.sidebarWidth = sidebar.width
            handle: Rectangle {
                implicitWidth: 1
                color: SplitHandle.hovered || SplitHandle.pressed ? theme.accent : theme.colors.darker_background
                // A wider grab area than the 1px line.
                Item { anchors.centerIn: parent; width: 8; height: parent.height }
            }

            Sidebar {
                id: sidebar
                visible: prefs.sidebarVisible
                SplitView.preferredWidth: prefs.sidebarWidth
                SplitView.minimumWidth: 170
                SplitView.maximumWidth: 600
            }

            ColumnLayout {
                SplitView.fillWidth: true
                spacing: 0

                TabStrip {
                    Layout.fillWidth: true
                    visible: backend.tabs.count > 0
                }

                // Asks about a file that changed or vanished under unsaved edits.
                Rectangle {
                    Layout.fillWidth: true
                    visible: backend.currentChangedOnDisk || backend.currentMissing
                    implicitHeight: bannerRow.implicitHeight + 16
                    color: theme.colors.lighter_background
                    RowLayout {
                        id: bannerRow
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 10
                        spacing: 10
                        Label {
                            Layout.fillWidth: true
                            color: theme.colors.foreground
                            wrapMode: Text.WordWrap
                            text: backend.currentMissing
                                ? qsTr("This file was deleted or moved. Save to write it back.")
                                : qsTr("This file changed on disk while you were editing it.")
                        }
                        DialogButton {
                            visible: backend.currentChangedOnDisk
                            text: qsTr("Reload")
                            onClicked: backend.reloadFromDisk(backend.currentIndex)
                        }
                        DialogButton {
                            visible: backend.currentChangedOnDisk
                            text: qsTr("Keep Mine")
                            primary: true
                            onClicked: backend.keepMine(backend.currentIndex)
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    WebChannel {
                        id: channel
                        Component.onCompleted: channel.registerObjects({ "bridge": backend.bridge, "theme": theme })
                    }

                    WebEngineView {
                        id: web
                        anchors.fill: parent
                        visible: backend.tabs.count > 0
                        url: "study://app/index.html"
                        webChannel: channel
                        zoomFactor: prefs.zoom
                        backgroundColor: theme.colors.background
                        settings.localContentCanAccessRemoteUrls: true
                        settings.javascriptCanAccessClipboard: true
                        settings.javascriptCanPaste: true
                        settings.focusOnNavigationEnabled: false
                        settings.showScrollBars: true

                        // The page never navigates: links are handled by the app.
                        onNavigationRequested: (request) => {
                            if (request.url.toString().startsWith("study://app/"))
                                return
                            request.reject()
                            if (/^(https?|mailto):/.test(request.url.toString()))
                                Qt.openUrlExternally(request.url)
                        }
                        onNewWindowRequested: (request) => Qt.openUrlExternally(request.requestedUrl)
                        onContextMenuRequested: (request) => {
                            request.accepted = true
                            pageMenu.flags = request.editFlags
                            pageMenu.popup()
                        }
                        // A crashed renderer comes back with the files from disk.
                        onRenderProcessTerminated: reviveTimer.start()
                        onJavaScriptConsoleMessage: (level, message, line, source) => {
                            if (level >= WebEngineView.WarningMessageLevel)
                                console.warn("page:", message, source + ":" + line)
                        }
                        onFindTextFinished: (result) => findBar.setResult(result.activeMatch, result.numberOfMatches)

                        Timer { id: reviveTimer; interval: 200; onTriggered: web.reload() }
                    }

                    Menu {
                        id: pageMenu
                        property int flags: 0
                        MenuItem {
                            text: qsTr("Copy")
                            enabled: pageMenu.flags & ContextMenuRequest.CanCopy
                            onTriggered: web.triggerWebAction(WebEngineView.Copy)
                        }
                        MenuItem {
                            text: qsTr("Cut")
                            visible: backend.editing
                            height: visible ? implicitHeight : 0
                            enabled: pageMenu.flags & ContextMenuRequest.CanCut
                            onTriggered: web.triggerWebAction(WebEngineView.Cut)
                        }
                        MenuItem {
                            text: qsTr("Paste")
                            visible: backend.editing
                            height: visible ? implicitHeight : 0
                            enabled: pageMenu.flags & ContextMenuRequest.CanPaste
                            onTriggered: web.triggerWebAction(WebEngineView.Paste)
                        }
                        MenuItem {
                            text: qsTr("Select All")
                            onTriggered: web.triggerWebAction(WebEngineView.SelectAll)
                        }
                        MenuSeparator {}
                        MenuItem {
                            text: backend.editing ? qsTr("Show Preview") : qsTr("Edit Source")
                            onTriggered: backend.toggleMode()
                        }
                    }

                    EmptyState {
                        anchors.fill: parent
                        visible: backend.tabs.count === 0
                    }

                    FindBar {
                        id: findBar
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 12
                        onSearch: (text, backward) => web.findText(text, backward ? WebEngineView.FindBackward : 0)
                        onClosed: { web.findText(""); win.focusPage() }
                    }
                }

                StatusLine {
                    Layout.fillWidth: true
                    zoom: prefs.zoom
                }
            }
        }

        QuickOpen {
            id: quickOpen
            anchors.fill: parent
            onDone: win.focusPage()
        }

        HelpOverlay {
            id: help
            anchors.fill: parent
            visible: false
        }

        ConfirmDialog {
            id: confirm
            anchors.fill: parent
            visible: false
            property string kind: ""

            function ask(what) {
                kind = what
                if (what === "quit") {
                    title = qsTr("Save your changes before quitting?")
                    message = qsTr("%n file(s) have unsaved changes.", "", backend.dirtyCount)
                    buttons = [
                        { text: qsTr("Save All"), mnemonic: "S", primary: true },
                        { text: qsTr("Don't Save"), mnemonic: "D" },
                        { text: qsTr("Cancel"), mnemonic: "C" },
                    ]
                } else {
                    title = qsTr("Save changes to this file?")
                    message = qsTr("Your changes are lost if you don't save them.")
                    buttons = [
                        { text: qsTr("Save"), mnemonic: "S", primary: true },
                        { text: qsTr("Don't Save"), mnemonic: "D" },
                        { text: qsTr("Cancel"), mnemonic: "C" },
                    ]
                }
                initialFocus = 0
                visible = true
            }

            onChosen: (index) => {
                visible = false
                if (kind === "quit") {
                    if (index === 0) { win.quitting = true; backend.saveAll(true) }
                    else if (index === 1) { win.quitting = true; backend.discardAllAndQuit() }
                } else {
                    if (index === 0) backend.saveAndClose(win.pendingCloseIndex)
                    else if (index === 1) backend.closeTab(win.pendingCloseIndex)
                    win.pendingCloseIndex = -1
                }
                win.focusPage()
            }
            onDismissed: { visible = false; win.pendingCloseIndex = -1; win.focusPage() }

            Shortcut {
                sequence: "Escape"
                context: Qt.ApplicationShortcut
                enabled: confirm.visible
                onActivated: confirm.dismissed()
            }
        }
    }
}
