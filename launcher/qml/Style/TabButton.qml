// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import MeshMC.Theme

// An underline tab rather than Basic's filled-box tab: it reads lighter and
// leaves the accent doing one job (marking the active tab) instead of also
// colouring a whole box, which matches the brief's "accent used sparingly".
T.TabButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Theme.space.sm
    horizontalPadding: Theme.space.md
    spacing: Theme.space.xs

    icon.width: Theme.icon.sm
    icon.height: Theme.icon.sm
    icon.color: control.checked ? Theme.palette.accentText : Theme.palette.textSecondary

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display

        icon: control.icon
        text: control.text
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
        font.weight: control.checked ? Theme.type.bodyStrong.weight : Theme.type.label.weight
        color: control.checked ? Theme.palette.accentText : Theme.palette.textSecondary
    }

    background: Rectangle {
        implicitHeight: Theme.control.height
        color: control.down ? Theme.palette.pressedOverlay : control.hovered ? Theme.palette.hoverOverlay : "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2
            radius: 1
            color: Theme.palette.accent
            opacity: control.checked ? 1.0 : 0.0

            Behavior on opacity {
                NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
            }
        }

        FocusRing {
            anchors.fill: parent
            anchors.margins: Theme.space.xxs
            radius: Theme.radius.sm
            visible: control.visualFocus
        }
    }
}
