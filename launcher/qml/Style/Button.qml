// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// Four variants:
//   primary   -> accent fill and a faint top-to-bottom tint; hover is the
//                lighter accent fill
//   secondary -> surfaceRaised plus the hairline border (the default)
//   ghost     -> transparent, only a hover/press overlay
//   danger    -> the danger red, filled, for a destructive confirmation
// `variant` names one outright. Left empty, `highlighted`/`flat` pick it:
// highlighted -> primary (danger with `danger` also set), flat -> ghost,
// neither -> secondary. `highlighted` wins over `flat`.
//
// At most one primary button per screen: the one full accent fill a page is
// allowed. This is a call-site discipline this file cannot enforce.
T.Button {
    id: control

    // "primary", "secondary", "ghost" or "danger"; see above.
    property string variant: ""
    // "sm", "md" or "lg": Theme.control.heightSm / height / heightLg, with the
    // padding, label and icon sizes that go with each.
    property string buttonSize: "md"
    // With `highlighted`, a destructive action reads danger-red instead of
    // accent. No effect alone; use variant: "danger" otherwise.
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
    // An icon-only button is square.
    leftPadding: display === T.AbstractButton.IconOnly ? Theme.space.sm : horizontalPadding
    rightPadding: display === T.AbstractButton.IconOnly ? Theme.space.sm : horizontalPadding
    spacing: Theme.space.sm

    icon.width: control.sizeIcon
    icon.height: control.sizeIcon
    icon.color: contentColor

    // A filled button that is not available drops to the secondary look
    // rather than fading: a faded accent over a dialog surface reads as a
    // broken colour. Secondary and ghost buttons keep their shape and fade.
    readonly property bool primaryLook: control.filled && control.enabled
    opacity: enabled || control.filled ? 1.0 : Theme.opacity.disabled

    // One colour for icon and label. The dark theme's danger red is a light
    // coral that white cannot read on, so it takes the canvas' near-black.
    readonly property color contentColor: control.primaryLook
        ? (control.resolvedVariant === "danger" && Theme.dark ? Theme.palette.canvas : Theme.palette.textOnAccent)
        : control.filled ? Theme.palette.textDisabled
        : control.resolvedVariant === "ghost" && !control.hovered && !control.down && !control.checked
            ? Theme.palette.textSecondary
            : Theme.palette.textPrimary

    // Filled variants have their own hover/press colours: a translucent
    // hoverOverlay on a saturated fill would shift its hue.
    readonly property color restColor: control.primaryLook
        ? (control.resolvedVariant === "danger"
            ? (control.down ? Qt.darker(Theme.palette.danger, 1.12) : control.hovered ? Qt.lighter(Theme.palette.danger, 1.08) : Theme.palette.danger)
            : (control.down ? Theme.palette.accentPressed : control.hovered ? Theme.palette.accentHover : Theme.palette.accent))
        : (control.resolvedVariant === "ghost" ? "transparent" : Theme.palette.surfaceRaised)
    readonly property color overlayColor: control.primaryLook || !control.enabled
        ? "transparent"
        : (control.down || control.checked ? Theme.palette.pressedOverlay
           : control.hovered ? Theme.palette.hoverOverlay : "transparent")

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
        radius: Theme.radius.md
        color: control.restColor
        // Ghost shows only the hover/press overlay; a border would make it
        // look like a fifth variant. Filled ones need no edge.
        border.width: !control.primaryLook && control.resolvedVariant !== "ghost" ? 1 : 0
        border.color: control.hovered && control.enabled ? Theme.neutral.borderHover : Theme.palette.border

        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        // The faint tint on a filled button.
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
