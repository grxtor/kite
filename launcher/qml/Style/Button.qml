// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// Four variants, all outlined but one:
//   primary   -> filled with textPrimary (near-white on dark, near-black on
//                light) under a canvas-coloured label; never the accent,
//                which only Play fills (PlayButton)
//   secondary -> transparent with a 1 px neutral.outline edge (the default);
//                icon-only, the subtler palette.border edge
//   ghost     -> transparent, no edge, textSecondary; only a hover/press
//                overlay
//   danger    -> transparent with a danger edge and a danger label, also for
//                a final destructive confirmation: a filled red would be the
//                loudest thing on a neutral screen
// `variant` names one outright. Left empty, `highlighted`/`flat` pick it:
// highlighted -> primary (danger with `danger` also set), flat -> ghost,
// neither -> secondary. `highlighted` wins over `flat`.
//
// At most one primary button per screen. This is a call-site discipline this
// file cannot enforce.
T.Button {
    id: control

    // "primary", "secondary", "ghost" or "danger"; see above.
    property string variant: ""
    // "sm", "md" or "lg": Theme.control.heightSm / height / heightLg, with the
    // padding, label and icon sizes that go with each.
    property string buttonSize: "md"
    // With `highlighted`, a destructive action reads danger-red instead of
    // primary. No effect alone; use variant: "danger" otherwise.
    property bool danger: false

    readonly property string resolvedVariant: control.variant.length > 0 ? control.variant
        : control.highlighted ? (control.danger ? "danger" : "primary")
        : control.flat ? "ghost" : "secondary"

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
    // An icon-only button is square.
    leftPadding: display === T.AbstractButton.IconOnly ? Theme.space.sm : horizontalPadding
    rightPadding: display === T.AbstractButton.IconOnly ? Theme.space.sm : horizontalPadding
    spacing: Theme.space.sm

    icon.width: control.sizeIcon
    icon.height: control.sizeIcon
    icon.color: contentColor

    // Only primary is filled. One that is not available drops to the
    // secondary look rather than fading: a faded white slab reads as broken.
    // The others keep their shape and fade.
    readonly property bool filled: resolvedVariant === "primary"
    readonly property bool primaryLook: control.filled && control.enabled
    readonly property bool isDanger: resolvedVariant === "danger"
    readonly property bool iconOnly: display === T.AbstractButton.IconOnly
    opacity: enabled || control.filled ? 1.0 : Theme.opacity.disabled
    // Pressed, the whole button gives a little and comes back on release.
    scale: control.down ? Theme.motion.pressScale : 1
    Behavior on scale { NumberAnimation { duration: Theme.motion.press; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot } }

    // One colour for icon and label.
    readonly property color contentColor: control.primaryLook ? Theme.palette.canvas
        : control.filled ? Theme.palette.textDisabled
        : control.isDanger ? Theme.palette.danger
        : control.resolvedVariant === "ghost" && !control.hovered && !control.down && !control.checked
            ? Theme.palette.textSecondary
            : Theme.palette.textPrimary

    // The primary fill steps toward white under the pointer on dark (and
    // lifts off black on light), and sinks when pressed.
    readonly property color restColor: !control.primaryLook ? "transparent"
        : control.down ? Qt.tint(Theme.palette.textPrimary, Theme.dark ? Qt.rgba(0, 0, 0, 0.14) : Qt.rgba(0, 0, 0, 0.45))
        : control.hovered ? Qt.tint(Theme.palette.textPrimary, Theme.dark ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(1, 1, 1, 0.14))
        : Theme.palette.textPrimary
    readonly property color overlayColor: control.primaryLook || !control.enabled
        ? "transparent"
        : (control.down || control.checked ? Theme.palette.pressedOverlay
           : control.hovered ? Theme.palette.hoverOverlay : "transparent")
    readonly property bool lit: control.hovered && control.enabled
    readonly property color edgeColor: control.isDanger
        ? Qt.rgba(Theme.palette.danger.r, Theme.palette.danger.g, Theme.palette.danger.b, control.lit ? 0.7 : 0.45)
        : control.iconOnly ? (control.lit ? Theme.neutral.outline : Theme.palette.border)
        : control.lit ? Theme.neutral.outlineHover : Theme.neutral.outline

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
        font.pixelSize: control.sizeFont
        font.weight: control.filled ? Font.DemiBold : Font.Medium
        color: control.contentColor
    }

    background: Rectangle {
        id: background
        implicitWidth: control.sizeHeight
        implicitHeight: control.sizeHeight
        radius: Theme.radius.sm
        color: control.restColor
        // Ghost shows only the hover/press overlay; a border would make it
        // look like a fifth variant. The filled primary needs no edge.
        border.width: !control.primaryLook && control.resolvedVariant !== "ghost" ? 1 : 0
        border.color: control.edgeColor

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
            visible: control.visualFocus
        }
    }
}
