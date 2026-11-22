// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

T.MenuItem {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    // Icons on one column and text after them on another, whether or not a
    // row has an icon of its own.
    // Compact, like a system menu: caption-sized text and small icons in 28 px rows.
    verticalPadding: Theme.space.xxs
    horizontalPadding: Theme.space.sm
    spacing: Theme.space.sm

    icon.width: Theme.icon.xxs
    icon.height: Theme.icon.xxs
    // An entry that removes or blocks something reads in the danger colour, as the dialogs' buttons do.
    property bool danger: false
    readonly property color ink: !control.enabled ? Theme.palette.textDisabled
                               : control.danger ? Theme.palette.danger : Theme.palette.textPrimary

    icon.color: control.ink

    // IconText, not IconLabel: the text is often a name from someone else and must stay plain.
    contentItem: IconText {
        readonly property real arrowPadding: control.subMenu && control.arrow ? control.arrow.width + control.spacing : 0
        readonly property real indicatorPadding: control.checkable && control.indicator ? control.indicator.width + control.spacing : 0
        // A checkable row with an icon of its own keeps the icon column and carries its tick at the end.
        readonly property real leadPadding: control.tickAtEnd ? 0 : indicatorPadding
        readonly property real trailPadding: Math.max(arrowPadding, control.tickAtEnd ? indicatorPadding : 0)
        leftPadding: !control.mirrored ? leadPadding : trailPadding
        rightPadding: control.mirrored ? leadPadding : trailPadding

        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: Qt.AlignLeft

        // The menu's own size and colour, not the control's: a submenu's entry
        // gets its Menu's icon wholesale, with no size or colour, and drew it
        // at the SVG's own size.
        iconSource: control.icon.source
        iconName: control.icon.name
        iconWidth: Theme.icon.xxs
        iconHeight: Theme.icon.xxs
        iconColor: control.subMenu ? (control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled)
                                   : control.ink
        iconCache: control.icon.cache
        text: control.text
        font.family: Theme.font.family
        font.pixelSize: Theme.type.caption.pixelSize
        font.weight: Theme.type.label.weight
        color: control.ink
    }

    readonly property bool tickAtEnd: control.checkable && (control.icon.source.toString().length > 0
                                                            || control.icon.name.length > 0)

    indicator: CheckMark {
        x: (control.mirrored !== control.tickAtEnd) ? control.width - width - control.rightPadding : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: Theme.icon.xxs
        height: Theme.icon.xxs
        color: Theme.palette.accentText
        visible: control.checkable && control.checked
    }

    arrow: Chevron {
        x: control.mirrored ? control.leftPadding : control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: Theme.icon.xxs
        height: Theme.icon.xxs
        color: Theme.palette.textTertiary
        direction: 1
        visible: control.subMenu
    }

    // No width of its own; Menu.qml sets the floor.
    background: Rectangle {
        implicitHeight: Theme.control.menuItemHeight
        radius: Theme.radius.indicator
        color: !control.enabled ? "transparent"
             : control.down ? Theme.palette.pressedOverlay
             : (control.highlighted || control.hovered) ? Theme.palette.hoverOverlay
             : "transparent"

        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }
    }
}
