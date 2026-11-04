// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// A toolbar/icon button: transparent with only the hover/press overlay, and a
// soft accent-tinted fill when checked, so a toggled tool reads as "on"
// without the primary Button's full accent fill, which is for committing
// actions.
T.ToolButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Theme.space.sm
    spacing: Theme.space.xs

    icon.width: Theme.icon.md
    icon.height: Theme.icon.md
    icon.color: contentColor

    readonly property color contentColor: !control.enabled
        ? Theme.palette.textDisabled
        : (control.checked || control.highlighted) ? Theme.palette.accentText : Theme.palette.textPrimary

    // IconText, not IconLabel: the text is often a name from someone else and must stay plain.
    contentItem: IconText {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display

        iconName: control.icon.name
        iconSource: control.icon.source
        iconWidth: control.icon.width
        iconHeight: control.icon.height
        iconColor: control.icon.color
        iconCache: control.icon.cache
        text: control.text
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        font.weight: Theme.type.body.weight
        color: control.contentColor
    }

    background: Rectangle {
        implicitWidth: Theme.control.height
        implicitHeight: Theme.control.height
        radius: Theme.radius.md
        color: (control.checked || control.highlighted) ? Theme.palette.accentSubtle : "transparent"
        opacity: control.enabled ? 1.0 : Theme.opacity.disabled

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.down ? Theme.palette.pressedOverlay : control.hovered ? Theme.palette.hoverOverlay : "transparent"
            Behavior on color {
                ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
            }
        }

        FocusRing {
            visible: control.visualFocus
        }
    }
}
