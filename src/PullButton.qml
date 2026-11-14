import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

// Pulls the folder's repository. Faint while it's up to date; the accent
// colour and a count once the remote has commits we don't; a spinner while
// pulling. Only shown for a repository with a branch to pull from.
AbstractButton {
    id: button
    readonly property var git: backend.git
    readonly property bool hasNew: git.behind > 0
    readonly property color tint: hasNew ? theme.accent
                                : hovered ? theme.colors.foreground
                                : theme.colors.dark_foreground

    visible: git.available
    enabled: !git.pulling
    implicitHeight: 22
    implicitWidth: content.implicitWidth + 10
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    onClicked: git.pull()

    Accessible.name: ToolTip.text
    ToolTip.visible: hovered
    ToolTip.delay: 500
    ToolTip.text: git.pulling ? qsTr("Pulling from %1…").arg(git.upstream)
                : hasNew ? qsTr("Pull %n new commit(s) from %1", "", git.behind).arg(git.upstream)
                : qsTr("Pull from %1").arg(git.upstream)

    HoverHandler { cursorShape: button.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }

    background: Rectangle {
        radius: 4
        color: button.hovered && button.enabled ? Qt.alpha(theme.colors.foreground, 0.08) : "transparent"
    }

    contentItem: Item {
        Row {
            id: content
            anchors.centerIn: parent
            spacing: 4

            Label {
                anchors.verticalCenter: parent.verticalCenter
                visible: button.hasNew && !button.git.pulling
                text: button.git.behind > 99 ? "99+" : button.git.behind
                color: button.tint
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }

            Item {
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: 13
                implicitHeight: 13

                // An arrow into a tray.
                Shape {
                    anchors.fill: parent
                    visible: !button.git.pulling
                    opacity: button.hasNew || button.hovered ? 1 : 0.7
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeWidth: 1.3
                        strokeColor: button.tint
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        joinStyle: ShapePath.RoundJoin
                        startX: 6.5; startY: 1
                        PathLine { x: 6.5; y: 8.5 }
                        PathMove { x: 3.5; y: 5.5 }
                        PathLine { x: 6.5; y: 8.5 }
                        PathLine { x: 9.5; y: 5.5 }
                        PathMove { x: 1.5; y: 9 }
                        PathLine { x: 1.5; y: 12 }
                        PathLine { x: 11.5; y: 12 }
                        PathLine { x: 11.5; y: 9 }
                    }
                }

                Shape {
                    id: spinner
                    anchors.fill: parent
                    visible: button.git.pulling
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeWidth: 1.5
                        strokeColor: theme.accent
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        PathAngleArc { centerX: 6.5; centerY: 6.5; radiusX: 5; radiusY: 5; startAngle: 0; sweepAngle: 270 }
                    }
                    RotationAnimator on rotation {
                        running: spinner.visible
                        from: 0; to: 360
                        duration: 900
                        loops: Animation.Infinite
                    }
                }
            }
        }
    }
}
