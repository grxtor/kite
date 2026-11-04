// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// The active tab is a soft pill (accentSubtle behind an accentText label), the
// rest plain text with a neutral hover overlay: the tab-strip grammar Theme.qml
// documents.
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
