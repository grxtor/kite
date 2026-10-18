// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import Kite.Theme

// Four variants, Modrinth-style:
//   primary   -> accent fill and a barely visible top-to-bottom tint; hover
//                is the lighter accent fill (no glow anywhere in the app)
//   secondary -> surfaceRaised plus the hairline border (the default)
//   ghost     -> transparent, only a hover/press overlay
//   danger    -> the danger red, filled, for a destructive confirmation
// `variant` names one outright. Left empty, the stock Qt Quick Controls
// `highlighted`/`flat` pair picks it, so existing call sites keep their look:
// highlighted -> primary (danger with `danger` also set), flat -> ghost,
// neither -> secondary. `highlighted` wins over `flat`, since "primary" and
// "quiet" is a contradiction a design system should resolve rather than
// render.
//
// Contract: at most one primary button per screen (design-plan.md
// Principle 1) -- it is the one full accent fill a page is allowed. Every
// other actionable control on that screen is secondary or ghost. This is a
// call-site discipline, not something this file can enforce by itself.
T.Button {
    id: control

    // "primary", "secondary", "ghost" or "danger"; see above.
    property string variant: ""
    // "sm", "md" or "lg": Theme.control.heightSm / height / heightLg, with
    // the padding, label and icon sizes that go with each.
    property string buttonSize: "md"
    // A highlighted button whose action is destructive (delete, remove)
    // reads as danger-red instead of accent; it has no effect unless
    // `highlighted` is also set (or use variant: "danger").
    property bool danger: false

    readonly property string resolvedVariant: control.variant.length > 0 ? control.variant
        : control.highlighted ? (control.danger ? "danger" : "primary")
        : control.flat ? "ghost" : "secondary"
    readonly property bool filled: resolvedVariant === "primary" || resolvedVariant === "danger"

    readonly property int sizeHeight: buttonSize === "sm" ? Theme.control.heightSm
                                    : buttonSize === "lg" ? Theme.control.heightLg
                                    : Theme.control.height
    readonly property int sizePadding: buttonSize === "sm" ? Theme.space.md
                                     : buttonSize === "lg" ? Theme.space.lg + Theme.space.xs
                                     : Theme.space.lg
    readonly property int sizeFont: buttonSize === "sm" ? Theme.type.label.pixelSize
                                  : buttonSize === "lg" ? Theme.type.body.pixelSize + 1
                                  : Theme.type.body.pixelSize
    readonly property int sizeIcon: buttonSize === "sm" ? Theme.icon.sm - 2
                                  : buttonSize === "lg" ? Theme.icon.sm + 2
                                  : Theme.icon.sm

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    horizontalPadding: control.sizePadding
    // An icon-only button is square: the icon sits in a box as wide as the
    // control is tall, not in a wide text-button slot.
    leftPadding: display === T.AbstractButton.IconOnly ? Theme.space.sm : horizontalPadding
    rightPadding: display === T.AbstractButton.IconOnly ? Theme.space.sm : horizontalPadding
    spacing: Theme.space.sm

    icon.width: control.sizeIcon
    icon.height: control.sizeIcon
    icon.color: contentColor

    // A filled (primary/danger) button that is not available yet drops to
    // the secondary button's look -- raised surface, border, disabled label
    // -- rather than fading: a faded accent over a dialog surface reads as a
    // broken colour, not as "not yet". Secondary and ghost buttons keep
    // their shape and fade as one piece.
    readonly property bool primaryLook: control.filled && control.enabled
    opacity: enabled || control.filled ? 1.0 : Theme.opacity.disabled

    // The same colour drives the icon and the label so a filled button never
    // ends up with mismatched icon/text tones. The dark theme's danger red is
    // a light coral that white cannot read on, so it takes the canvas'
    // near-black instead, the way Modrinth's own red buttons do.
    readonly property color contentColor: control.primaryLook
        ? (control.resolvedVariant === "danger" && Theme.dark ? Theme.palette.canvas : Theme.palette.textOnAccent)
        : control.filled ? Theme.palette.textDisabled
        : control.resolvedVariant === "ghost" && !control.hovered && !control.down && !control.checked
            ? Theme.palette.textSecondary
            : Theme.palette.textPrimary

    // Filled variants get their own hover/press colours (accentHover/
    // accentPressed, a lighter/darker danger) instead of the generic overlay
    // tokens: laying a translucent hoverOverlay on top of an already
    // saturated fill would shift its hue and muddy it. Neutral surfaces
    // (secondary/ghost) have no such dedicated variant, so they use the
    // overlay tokens instead.
    readonly property color restColor: control.primaryLook
        ? (control.resolvedVariant === "danger"
            ? (control.down ? Qt.darker(Theme.palette.danger, 1.12) : control.hovered ? Qt.lighter(Theme.palette.danger, 1.08) : Theme.palette.danger)
            : (control.down ? Theme.palette.accentPressed : control.hovered ? Theme.palette.accentHover : Theme.palette.accent))
        : (control.resolvedVariant === "ghost" ? "transparent" : Theme.palette.surfaceRaised)
    readonly property color overlayColor: control.primaryLook || !control.enabled
        ? "transparent"
        : (control.down || control.checked ? Theme.palette.pressedOverlay
           : control.hovered ? Theme.palette.hoverOverlay : "transparent")

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display

        icon: control.icon
        text: control.text
        font.family: Theme.font.family
        font.pixelSize: control.sizeFont
        font.weight: control.filled ? Font.DemiBold : Font.Medium
        color: control.contentColor
    }

    background: Rectangle {
        id: background
        implicitWidth: control.sizeHeight
        implicitHeight: control.sizeHeight
        radius: Theme.radius.md
        color: control.restColor
        // Ghost buttons only ever show the hover/press overlay, so the border
        // stays off for them -- a border would make "ghost" look like a
        // fifth, unrequested variant. Filled ones need no edge of their own.
        border.width: !control.primaryLook && control.resolvedVariant !== "ghost" ? 1 : 0
        border.color: control.hovered && control.enabled ? Theme.neutral.borderHover : Theme.palette.border

        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        // The barely visible top-to-bottom tint on a filled button.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: control.primaryLook
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.10) }
                GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.0) }
            }
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
            visible: control.visualFocus
        }
    }
}
