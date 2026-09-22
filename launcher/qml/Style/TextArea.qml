// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import MeshMC.Theme

// Unlike Basic's TextArea, this one draws its own background rather than
// leaving it to whatever Frame/ScrollView happens to wrap it -- most call
// sites here use a bare TextArea, and an unstyled one would show raw text
// floating on the surface behind it, breaking the "sunken field" language
// TextField establishes.
T.TextArea {
    id: control

    implicitWidth: Math.max(contentWidth + leftPadding + rightPadding,
                            implicitBackgroundWidth + leftInset + rightInset,
                            placeholder.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(contentHeight + topPadding + bottomPadding,
                             implicitBackgroundHeight + topInset + bottomInset,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    padding: Theme.space.sm

    color: control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled
    font.family: Theme.font.family
    font.pixelSize: Theme.type.body.pixelSize
    font.weight: Theme.type.body.weight
    placeholderTextColor: Theme.palette.textTertiary
    selectionColor: Theme.palette.selection
    selectedTextColor: Theme.palette.selectionText

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
        implicitHeight: Theme.control.heightLg * 2
        radius: Theme.radius.md
        color: Theme.palette.surfaceSunken
        border.width: 1
        border.color: control.activeFocus ? Theme.palette.borderStrong : Theme.palette.border
        opacity: control.enabled ? 1.0 : 0.45

        Behavior on border.color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        FocusRing {
            anchors.fill: parent
            anchors.margins: -Theme.space.xxs
            radius: parent.radius + Theme.space.xxs
            // T.TextArea has no visualFocus for the same reason as
            // TextField.qml: it isn't Control-derived. Same "keyboard, not
            // mouse" test as Control computes internally, done by hand.
            visible: control.activeFocus && control.focusReason !== Qt.MouseFocusReason
        }
    }
}
