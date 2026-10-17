// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import MeshMC.Theme

T.MenuItem {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: Theme.space.sm
    spacing: Theme.space.sm

    icon.width: Theme.icon.md
    icon.height: Theme.icon.md
    icon.color: control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled

    contentItem: IconLabel {
        readonly property real arrowPadding: control.subMenu && control.arrow ? control.arrow.width + control.spacing : 0
        readonly property real indicatorPadding: control.checkable && control.indicator ? control.indicator.width + control.spacing : 0
        leftPadding: !control.mirrored ? indicatorPadding : arrowPadding
        rightPadding: control.mirrored ? indicatorPadding : arrowPadding

        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: Qt.AlignLeft

        // Field by field rather than `icon: control.icon`: a submenu's entry
        // gets its Menu's icon wholesale, with no size or colour, and drew
        // it at the SVG's own size in black-or-whatever -- "Play as" stood
        // bigger than "Play offline" and its label started further right.
        icon.source: control.icon.source
        icon.name: control.icon.name
        icon.width: Theme.icon.md
        icon.height: Theme.icon.md
        icon.color: control.subMenu ? (control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled)
                                    : control.icon.color
        icon.cache: control.icon.cache
        text: control.text
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        font.weight: Theme.type.body.weight
        color: control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled
    }

    indicator: CheckMark {
        x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: Theme.icon.sm
        height: Theme.icon.sm
        color: Theme.palette.accent
        visible: control.checkable && control.checked
    }

    arrow: Chevron {
        x: control.mirrored ? control.leftPadding : control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: Theme.icon.sm
        height: Theme.icon.sm
        color: Theme.palette.textSecondary
        direction: 1
        visible: control.subMenu
    }

    // No width of its own: an entry is as wide as what it says, and the
    // Menu sets the floor (see Menu.qml's minimumWidth).
    background: Rectangle {
        implicitHeight: Theme.control.height
        x: Theme.space.xxs
        y: Theme.space.xxs / 2
        width: control.width - Theme.space.xxs * 2
        height: control.height - Theme.space.xxs
        radius: Theme.radius.sm
        color: control.down ? Theme.palette.pressedOverlay
             : (control.highlighted || control.hovered) ? Theme.palette.hoverOverlay
             : "transparent"
    }
}
