// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import Kite.Theme

// Unlike Basic's TextArea, this draws its own background: most call sites use
// a bare TextArea, which would otherwise show raw text floating on the surface
// behind it.
T.TextArea {
    id: control

    implicitWidth: Math.max(contentWidth + leftPadding + rightPadding,
                            implicitBackgroundWidth + leftInset + rightInset,
                            placeholder.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(contentHeight + topPadding + bottomPadding,
                             implicitBackgroundHeight + topInset + bottomInset,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    padding: Theme.space.sm
    leftPadding: Theme.space.md
    rightPadding: Theme.space.md

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
        implicitHeight: Theme.control.heightLg * 2
        radius: Theme.radius.md
        color: Theme.palette.surface
        border.width: 1
        border.color: control.activeFocus && !control.readOnly ? Theme.palette.accent
                    : control.hovered && control.enabled ? Theme.neutral.borderHover : Theme.palette.border
        opacity: control.enabled ? 1.0 : Theme.opacity.disabled

        Behavior on border.color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        // As TextField.qml: any focus, unless read-only (a log is selected,
        // not typed into).
        FocusRing {
            halo: true
            visible: control.activeFocus && !control.readOnly
        }
    }
}
