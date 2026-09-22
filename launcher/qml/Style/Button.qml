// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import MeshMC.Theme

// Three variants come out of T.Button's existing `flat`/`highlighted` pair
// rather than a custom `variant` enum, so call sites keep using the stock
// Qt Quick Controls API:
//   highlighted           -> accent-filled primary action
//   flat                  -> transparent, only a hover/press overlay
//   neither (the default) -> a raised neutral surface
// `highlighted` wins if both are set, since "primary" and "quiet" is a
// contradiction a design system should resolve rather than render.
T.Button {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    horizontalPadding: Theme.space.md
    spacing: Theme.space.xs

    icon.width: Theme.icon.md
    icon.height: Theme.icon.md
    icon.color: contentColor

    // Qt.PlainText/RichText aside, the same colour drives the icon and the
    // label so a disabled or accent button never ends up with mismatched
    // icon/text tones.
    readonly property color contentColor: !control.enabled
        ? Theme.palette.textDisabled
        : control.highlighted ? Theme.palette.textOnAccent : Theme.palette.textPrimary

    // The accent-filled variant gets its own hover/press colours
    // (accentHover/accentPressed) instead of the generic overlay tokens:
    // laying a translucent hoverOverlay on top of an already-saturated
    // accent fill would shift its hue/muddy it, where the dedicated tokens
    // are tuned to stay on-brand. Neutral surfaces (default/flat) have no
    // such dedicated variant, so they use the overlay tokens instead.
    readonly property color restColor: control.highlighted
        ? (control.down ? Theme.palette.accentPressed : control.hovered ? Theme.palette.accentHover : Theme.palette.accent)
        : (control.flat ? "transparent" : Theme.palette.surfaceRaised)
    readonly property color overlayColor: control.highlighted
        ? "transparent"
        : (control.down ? Theme.palette.pressedOverlay : control.hovered ? Theme.palette.hoverOverlay : "transparent")

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
        id: background
        implicitHeight: Theme.control.height
        radius: Theme.radius.md
        color: control.restColor
        // Flat buttons only ever show the hover/press overlay, so the border
        // stays off for them too -- a border would make "flat" look like a
        // fourth, unrequested variant.
        border.width: !control.highlighted && !control.flat ? 1 : 0
        border.color: Theme.palette.border
        opacity: control.enabled ? 1.0 : 0.45 // no disabled-opacity token in the contract; see report

        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.overlayColor
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
