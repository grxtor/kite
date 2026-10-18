// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// A small raised card rather than a dark chip: tooltipBackground (the
// raised surface in Amethyst), the hairline border and the md shadow, like
// a menu without the glass edge.
T.ToolTip {
    id: control

    x: parent ? (parent.width - implicitWidth) / 2 : 0
    y: -implicitHeight - Theme.space.xs

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: Theme.space.xs
    topPadding: Theme.space.xs + Theme.space.xxs
    bottomPadding: Theme.space.xs + Theme.space.xxs
    leftPadding: Theme.space.sm + Theme.space.xxs
    rightPadding: Theme.space.sm + Theme.space.xxs

    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }

    contentItem: Text {
        text: control.text
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
        font.weight: Theme.type.body.weight
        wrapMode: Text.Wrap
        color: Theme.palette.tooltipText
    }

    background: Rectangle {
        // md rather than the menus' lg: on a one-line tooltip lg already
        // reads as a pill.
        radius: Theme.radius.md
        color: Theme.palette.tooltipBackground
        border.width: 1
        border.color: Theme.palette.border

        PopupShadow {
            radius: parent.radius
            level: Theme.elevation.md
        }
    }
}
