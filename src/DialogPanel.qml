import QtQuick

// Every dialog's panel: the theme's dark surface, a quiet border, and the
// corners Hyprland gives windows (sharp on Omarchy's default).
Rectangle {
    color: theme.colors.dark_background
    border.width: 1
    border.color: theme.colors.muted
    radius: theme.radius
}
