// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import Kite.Theme

// The field look every input in this style shares (TextField, TextArea,
// ComboBox, SpinBox): the surface fill, the hairline border, radius md; under
// the pointer the border steps up, and focused it turns accent with a soft
// 2 px halo.
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

    // A field that is not being edited shows the start of its text: a long
    // value would otherwise leave the cursor, and the scroll, at its end.
    onActiveFocusChanged: if (!activeFocus) cursorPosition = 0
    onTextChanged: if (!activeFocus) cursorPosition = 0

    PlaceholderText {
        id: placeholder
        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)

        text: control.placeholderText
        textFormat: Text.PlainText
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
        color: Theme.palette.surface
        border.width: 1
        border.color: control.activeFocus ? Theme.palette.accent
                    : control.hovered && control.enabled ? Theme.neutral.borderHover : Theme.palette.border
        opacity: control.enabled ? 1.0 : Theme.opacity.disabled

        Behavior on border.color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        // The halo shows however the field got focus: typing lands here either
        // way.
        FocusRing {
            halo: true
            visible: control.activeFocus && !control.readOnly
        }
    }
}
