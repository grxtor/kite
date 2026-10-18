// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// Off is a calm grey box, on is the accent with a white tick -- the toggle
// grammar Switch and RadioButton share (phase-02 brief). No outline: the
// grey fill is the box, the way Modrinth's own checkboxes read.
T.CheckBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: Theme.space.xs
    spacing: Theme.space.sm

    indicator: Rectangle {
        id: box
        implicitWidth: Theme.icon.md - Theme.space.xxs
        implicitHeight: Theme.icon.md - Theme.space.xxs

        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding) : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        // Checked/partially-checked share the accent fill; only Unchecked is
        // the plain outlined surface, so this one flag decides both the fill
        // colour below and which of the two overlay/tint strategies applies.
        readonly property bool boxChecked: control.checkState !== Qt.Unchecked

        radius: Theme.radius.indicator
        color: box.boxChecked
            ? (control.down ? Theme.palette.accentPressed : control.hovered ? Theme.palette.accentHover : Theme.palette.accent)
            : (control.down ? Theme.neutral.trackHover : control.hovered ? Theme.neutral.trackHover : Theme.neutral.track)
        opacity: control.enabled ? 1.0 : Theme.opacity.disabled

        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        // Pressing an unchecked box darkens it one more step, the way the
        // checked one drops to accentPressed.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: !box.boxChecked && control.down
            color: Theme.palette.pressedOverlay
        }

        CheckMark {
            anchors.fill: parent
            anchors.margins: Theme.space.xxs
            color: Theme.palette.textOnAccent
            partial: control.checkState === Qt.PartiallyChecked
            visible: box.boxChecked
        }

        FocusRing {
            visible: control.visualFocus
        }
    }

    contentItem: Text {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0

        text: control.text
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        font.weight: Theme.type.body.weight
        color: control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}
