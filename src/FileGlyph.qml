import QtQuick
import QtQuick.Shapes

// The sidebar's small icons, drawn so they take the theme's colours: a folder
// (open or closed) or a page with a folded corner.
Item {
    id: glyph
    property bool folder: false
    property bool open: false
    property color color: theme.colors.foreground
    implicitWidth: 15
    implicitHeight: 13

    Shape {
        anchors.fill: parent
        visible: glyph.folder
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: 1.3
            strokeColor: glyph.color
            fillColor: glyph.open ? "transparent" : Qt.alpha(glyph.color, 0.35)
            joinStyle: ShapePath.RoundJoin
            startX: 1; startY: 2
            PathLine { x: 5.5; y: 2 }
            PathLine { x: 7; y: 3.8 }
            PathLine { x: 14; y: 3.8 }
            PathLine { x: 14; y: 12 }
            PathLine { x: 1; y: 12 }
            PathLine { x: 1; y: 2 }
        }
    }
    Shape {
        anchors.fill: parent
        visible: !glyph.folder
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: 1.3
            strokeColor: glyph.color
            fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin
            startX: 3; startY: 0.8
            PathLine { x: 9; y: 0.8 }
            PathLine { x: 12.5; y: 4.3 }
            PathLine { x: 12.5; y: 12.3 }
            PathLine { x: 3; y: 12.3 }
            PathLine { x: 3; y: 0.8 }
            PathMove { x: 9; y: 0.8 }
            PathLine { x: 9; y: 4.3 }
            PathLine { x: 12.5; y: 4.3 }
        }
    }
}
