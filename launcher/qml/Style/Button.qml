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

    // A highlighted button whose action is destructive (delete, remove)
    // reads as danger-red instead of accent-cyan; it has no effect unless
    // `highlighted` is also set, same as `flat` only matters unhighlighted.
    property bool danger: false

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    horizontalPadding: Theme.space.lg
    // An icon-only button is square: the icon sits in a box as wide as the
    // control is tall, not in a wide text-button slot.
    leftPadding: display === T.AbstractButton.IconOnly ? Theme.space.sm : horizontalPadding
    rightPadding: display === T.AbstractButton.IconOnly ? Theme.space.sm : horizontalPadding
    spacing: Theme.space.sm

    icon.width: Theme.icon.sm
    icon.height: Theme.icon.sm
    icon.color: contentColor

    // Fades as one piece, so a disabled accent button still reads as the
    // same button rather than a grey one.
    opacity: enabled ? 1.0 : Theme.opacity.disabled

    // The same colour drives the icon and the label so an accent button never
    // ends up with mismatched icon/text tones.
    readonly property color contentColor: control.highlighted
        ? Theme.palette.textOnAccent
        : control.flat && !control.hovered ? Theme.palette.textSecondary
                                           : Theme.palette.textPrimary

    // The accent-filled variant gets its own hover/press colours
    // (accentHover/accentPressed) instead of the generic overlay tokens:
    // laying a translucent hoverOverlay on top of an already-saturated
    // accent fill would shift its hue/muddy it, where the dedicated tokens
    // are tuned to stay on-brand. Neutral surfaces (default/flat) have no
    // such dedicated variant, so they use the overlay tokens instead.
    readonly property color restColor: control.highlighted
        ? (control.danger
            ? (control.down ? Qt.darker(Theme.palette.danger, 1.2) : control.hovered ? Qt.lighter(Theme.palette.danger, 1.12) : Theme.palette.danger)
            : (control.down ? Theme.palette.accentPressed : control.hovered ? Theme.palette.accentHover : Theme.palette.accent))
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
        font.pixelSize: Theme.type.label.pixelSize + 1
        font.weight: control.highlighted ? Font.DemiBold : Font.Medium
        color: control.contentColor
    }

    background: Rectangle {
        id: background
        implicitWidth: Theme.control.height
        implicitHeight: Theme.control.height
        radius: Theme.radius.md
        color: control.restColor
        // Flat buttons only ever show the hover/press overlay, so the border
        // stays off for them too -- a border would make "flat" look like a
        // fourth, unrequested variant.
        border.width: !control.highlighted && !control.flat ? 1 : 0
        border.color: control.hovered ? Theme.palette.borderStrong : Theme.palette.border

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

        // A faint light from above on the accent fill: gives the primary
        // action some depth without a drop shadow.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: control.highlighted && !control.down
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.22) }
                GradientStop { position: 0.55; color: Qt.rgba(1, 1, 1, 0.0) }
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
