// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

T.ItemDelegate {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: Theme.space.sm
    horizontalPadding: Theme.space.md
    spacing: Theme.space.sm

    icon.width: Theme.icon.md
    icon.height: Theme.icon.md
    icon.color: !control.enabled ? Theme.palette.textDisabled
              : control.highlighted ? Theme.palette.accentText : Theme.palette.textPrimary

    // IconText, not IconLabel: the text is often a name from someone else and must stay plain.
    contentItem: IconText {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: control.display === T.AbstractButton.IconOnly || control.display === T.AbstractButton.TextUnderIcon
                   ? Qt.AlignCenter : Qt.AlignLeft

        iconName: control.icon.name
        iconSource: control.icon.source
        iconWidth: control.icon.width
        iconHeight: control.icon.height
        iconColor: control.icon.color
        iconCache: control.icon.cache
        text: control.text
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        font.weight: Theme.type.body.weight
        color: control.icon.color
    }

    background: Rectangle {
        implicitHeight: Theme.control.height
        radius: Theme.radius.sm
        color: control.highlighted ? Theme.palette.accentSubtle
             : control.down ? Theme.palette.pressedOverlay
             : control.hovered ? Theme.palette.hoverOverlay
             : "transparent"

        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        // Inside the row: rows sit edge to edge, and an outset ring would be
        // clipped by its neighbours.
        FocusRing {
            anchors.margins: Theme.space.xxs
            baseRadius: Math.max(0, Theme.radius.sm - Theme.space.xxs * 2)
            halo: true
            visible: control.visualFocus
        }
    }
}
