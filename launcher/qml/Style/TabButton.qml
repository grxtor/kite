// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// Plain text: the active tab textPrimary, the rest textTertiary (textSecondary
// under the pointer). The 2 px accent underline is TabBar's, one for the row.
// No pill: the tab-strip grammar Theme.qml documents.
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
        : control.checked ? Theme.palette.textPrimary
        : control.hovered || control.down ? Theme.palette.textSecondary
        : Theme.palette.textTertiary

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
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    background: Rectangle {
        implicitHeight: Theme.control.heightSm + Theme.space.xxs
        radius: Theme.radius.sm
        color: "transparent"

        FocusRing {
            visible: control.visualFocus
        }
    }
}
