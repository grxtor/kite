// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import MeshMC.Theme

// Inputs sit in a sunken surface rather than behind a heavy border, per the
// brief's "surfaces separated by lightness rather than by lines" -- the
// field reads as a recess in the panel, not a boxed-off widget.
T.TextField {
    id: control

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    leftPadding: Theme.space.md
    rightPadding: Theme.space.md
    topPadding: Theme.space.xs
    bottomPadding: Theme.space.xs

    color: control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled
    font.family: Theme.font.family
    font.pixelSize: Theme.type.body.pixelSize
    font.weight: Theme.type.body.weight
    selectionColor: Theme.palette.selection
    selectedTextColor: Theme.palette.selectionText
    placeholderTextColor: Theme.palette.textTertiary
    verticalAlignment: TextInput.AlignVCenter

    PlaceholderText {
        id: placeholder
        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)

        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        visible: !control.length && !control.preeditText && (!control.activeFocus || control.horizontalAlignment !== Qt.AlignHCenter)
        elide: Text.ElideRight
        renderType: control.renderType
    }

    background: Rectangle {
        implicitWidth: Theme.control.heightLg * 4
        implicitHeight: Theme.control.height
        radius: Theme.radius.md
        color: Theme.palette.surfaceRaised
        border.width: 1
        // An accent border on activeFocus tells a mouse user they landed in
        // the field; the outset ring below is the separate, keyboard-only
        // affordance the brief asks for.
        border.color: control.activeFocus ? Theme.palette.accent
                    : control.hovered ? Theme.palette.borderStrong : Theme.palette.border
        opacity: control.enabled ? 1.0 : Theme.opacity.disabled

        Behavior on border.color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        FocusRing {
            anchors.fill: parent
            anchors.margins: -Theme.space.xxs
            radius: parent.radius + Theme.space.xxs
            // T.TextField has no visualFocus (it isn't Control-derived, so it
            // never gained the property) -- this is the same "keyboard, not
            // mouse" test Control computes internally.
            visible: control.activeFocus && control.focusReason !== Qt.MouseFocusReason
        }
    }
}
