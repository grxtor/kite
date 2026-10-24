// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One destination in the sidebar. The selected fill itself is a single
 * shared shape SidebarNav slides between items (NavSelectionIndicator) --
 * this control only ever draws its own transient hover/press state, so the
 * shape shows through untouched wherever it sits. Selected, its icon and
 * label take accentText over that neutral shape, whose nose is lit in the
 * accent.
 *
 * A destination can carry a count (`badge`, hidden at 0): a pill at the
 * icon's top-right corner on the rail, at the row's right end in the
 * labelled sidebar. `badgeTone` says what the number is: "accent" for
 * something that wants an answer, "success" for good news.
 */
AbstractButton {
    id: control

    property string iconName
    property string label
    property bool selected: false
    // Icon rail mode: drops the label, which shows as a tooltip to the
    // right instead (Modrinth's rail). The icon itself never moves: it sits
    // on the same axis in both modes (iconAxis), and the collapsed rail is
    // exactly twice that wide (SidebarNav.railWidth), so it is centred there
    // without re-anchoring anything -- swapping anchors at runtime left the
    // icon behind.
    property bool collapsed: false
    // The icon's centre, from the row's left edge. SidebarNav sets its own
    // (half its rail square); Settings' section list keeps this default.
    property int iconAxis: Theme.space.md + Theme.icon.md / 2
    // Where the label starts, from the row's left edge: right after the
    // icon by default; SidebarNav lines it up with its recent instances'
    // names instead, whose picture is wider than a glyph.
    property int labelX: control.iconAxis + Theme.icon.md / 2 + Theme.space.md
    // Collapsed: a compact square centred on the icon rather than the
    // whole stretched row -- the same square SidebarNav's selection
    // indicator draws for the selected item (NavSelectionIndicator), so
    // hover and selection read as the same shape in the rail.
    property int railSquare: Theme.control.height
    property int cornerRadius: Theme.radius.md
    property int badge: 0
    property string badgeTone: "accent"
    // The most a pill shows; past it, "99+".
    readonly property int badgeCap: 99
    // What the rail is drawn on (SidebarNav.surround), so the pill's ring can
    // cut it clear of whatever it overlaps: the icon, and the kite's nose on
    // a selected item.
    property color surround: "transparent"

    implicitHeight: Theme.control.height
    implicitWidth: 200
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    Accessible.role: Accessible.PageTab
    Accessible.name: label
    Accessible.description: control.badge > 0 ? String(control.badge) : ""

    // Beside the rail rather than over it: above, it would cover the item
    // the pointer is moving to next.
    ToolTip {
        visible: control.collapsed && control.hovered && control.label.length > 0
        delay: 400
        text: control.label
        x: control.width + Theme.space.md
        y: Math.round((control.height - implicitHeight) / 2)
    }

    background: Rectangle {
        width: control.collapsed ? control.railSquare : control.width
        height: control.collapsed ? control.railSquare : control.height
        x: control.collapsed ? control.iconAxis - width / 2 : 0
        y: control.collapsed ? (control.height - height) / 2 : 0
        radius: control.cornerRadius
        color: control.down ? Theme.palette.pressedOverlay
             : control.hovered ? Theme.palette.hoverOverlay : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -Theme.space.xxs
            radius: parent.radius + Theme.space.xxs
            color: "transparent"
            border.width: 2
            border.color: Theme.palette.focusRing
            visible: control.visualFocus
        }
    }

    readonly property color ink: control.selected ? Theme.palette.accentText
                               : control.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary

    contentItem: Item {
        MeshIcon {
            x: control.iconAxis - width / 2 - control.leftPadding
            anchors.verticalCenter: parent.verticalCenter
            iconName: control.iconName
            size: Theme.icon.md
            color: control.ink
        }

        Text {
            x: control.labelX - control.leftPadding
            width: Math.max(0, parent.width - x - (pill.visible && !control.collapsed ? pill.width + Theme.space.md + Theme.space.sm : 0))
            anchors.verticalCenter: parent.verticalCenter
            visible: !control.collapsed
            text: control.label
            elide: Text.ElideRight
            color: control.ink
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
            font.weight: control.selected ? Font.DemiBold : Font.Medium
        }
    }

    // The count. Drawn over the background and the contentItem, so it sits
    // on the icon's corner without taking part in either's layout. Filled
    // like a Button: textOnAccent on the accent, and the canvas on the
    // success green (white would not read on the dark theme's light green).
    Rectangle {
        id: pill
        visible: control.badge > 0
        width: Math.max(height, pillText.implicitWidth + Theme.space.sm + Theme.focus.width * 2)
        height: Math.ceil(Theme.type.caption.lineHeightPx) + Theme.focus.width * 2
        radius: Theme.radius.pill
        border.width: Theme.focus.width
        // A selected item sits on the selection's own fill.
        border.color: control.selected ? Theme.palette.surfaceRaised : control.surround
        color: control.badgeTone === "success" ? Theme.palette.success : Theme.palette.accent
        // Rail: centred on the corner of the icon. Labelled: the row's end.
        x: control.collapsed ? control.iconAxis + Theme.icon.md / 2 - width / 2
                             : control.width - width - Theme.space.md
        y: control.collapsed ? (control.height - Theme.icon.md) / 2 - height / 2
                             : (control.height - height) / 2

        Text {
            id: pillText
            anchors.centerIn: parent
            text: control.badge > control.badgeCap ? control.badgeCap + "+" : String(control.badge)
            color: control.badgeTone === "success" ? Theme.palette.canvas : Theme.palette.textOnAccent
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
            font.weight: Font.DemiBold
        }
    }
}
