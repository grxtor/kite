// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import Kite.Theme

// A Modrinth-style tab: the active one is a soft pill (accentSubtle behind
// an accentText label), the rest are plain text that gain a neutral hover
// overlay -- the tab-strip grammar Theme.qml documents. The accent does one
// quiet job here instead of underlining or filling a box.
T.TabButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Theme.space.xs
    horizontalPadding: Theme.space.md + Theme.space.xxs
    spacing: Theme.space.sm

    icon.width: Theme.icon.sm
    icon.height: Theme.icon.sm
    icon.color: contentColor

    readonly property color contentColor: !control.enabled ? Theme.palette.textDisabled
        : control.checked ? Theme.palette.accentText
        : control.hovered || control.down ? Theme.palette.textPrimary
        : Theme.palette.textSecondary

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display

        icon: control.icon
        text: control.text
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        font.weight: control.checked ? Theme.type.bodyStrong.weight : Theme.type.label.weight
        color: control.contentColor
    }

    background: Rectangle {
        implicitHeight: Theme.control.heightSm + Theme.space.xxs
        radius: Theme.radius.pill
        color: control.checked ? Theme.palette.accentSubtle
             : control.down ? Theme.palette.pressedOverlay
             : control.hovered ? Theme.palette.hoverOverlay
             : "transparent"

        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        FocusRing {
            visible: control.visualFocus
        }
    }
}
