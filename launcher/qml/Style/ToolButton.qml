// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import MeshMC.Theme

// A toolbar/icon button: quiet by default (transparent, only the hover/press
// overlay shows), with a soft accent-tinted fill when checked so a toggled
// tool (e.g. a pinned view) reads as "on" without borrowing the primary
// Button's full accent fill, which is reserved for committing actions.
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

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display

        icon: control.icon
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
        opacity: control.enabled ? 1.0 : 0.45

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.down ? Theme.palette.pressedOverlay : control.hovered ? Theme.palette.hoverOverlay : "transparent"
            Behavior on color {
                ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
            }
        }

        FocusRing {
            anchors.fill: parent
            anchors.margins: -Theme.space.xxs
            radius: parent.radius + Theme.space.xxs
            visible: control.visualFocus
        }
    }
}
