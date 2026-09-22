// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import MeshMC.Theme

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
        implicitWidth: Theme.icon.lg
        implicitHeight: Theme.icon.lg

        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding) : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        // Checked/partially-checked share the accent fill; only Unchecked is
        // the plain outlined surface, so this one flag decides both the fill
        // colour below and which of the two overlay/tint strategies applies.
        readonly property bool boxChecked: control.checkState !== Qt.Unchecked

        radius: Theme.radius.sm
        color: box.boxChecked
            ? (control.down ? Theme.palette.accentPressed : control.hovered ? Theme.palette.accentHover : Theme.palette.accent)
            : Theme.palette.surfaceRaised
        border.width: box.boxChecked ? 0 : 1
        border.color: Theme.palette.border
        opacity: control.enabled ? 1.0 : 0.45

        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        Rectangle {
            // The unchecked box has no dedicated hover/press colour, so it
            // uses the generic overlay tokens instead; the checked (accent)
            // box already darkens via accentHover/accentPressed above.
            anchors.fill: parent
            radius: parent.radius
            visible: !box.boxChecked
            color: control.down ? Theme.palette.pressedOverlay : control.hovered ? Theme.palette.hoverOverlay : "transparent"
        }

        CheckMark {
            anchors.fill: parent
            anchors.margins: Theme.space.xxs
            color: Theme.palette.textOnAccent
            partial: control.checkState === Qt.PartiallyChecked
            visible: box.boxChecked
        }

        FocusRing {
            anchors.fill: parent
            anchors.margins: -Theme.space.xxs
            radius: parent.radius + Theme.space.xxs
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
