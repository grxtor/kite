// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
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

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: control.display === IconLabel.IconOnly || control.display === IconLabel.TextUnderIcon ? Qt.AlignCenter : Qt.AlignLeft

        icon: control.icon
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

        // Inside the row rather than around it: rows sit edge to edge in a
        // list, and an outset ring would be clipped by its neighbours.
        FocusRing {
            anchors.margins: Theme.space.xxs
            baseRadius: Math.max(0, Theme.radius.sm - Theme.space.xxs * 2)
            halo: true
            visible: control.visualFocus
        }
    }
}
