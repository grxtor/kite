// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Small tinted chip for a status word ("Beta", "Modded"), the same height and
 * corners as Tag. "neutral" has no <tone>/<tone>Subtle pair in the token contract,
 * so it falls back to surfaceOverlay/textSecondary.
 */
Rectangle {
    id: root

    property string text: ""
    property string tone: "neutral"

    function toneTextColor() {
        switch (root.tone) {
        case "success": return Theme.palette.success
        case "warning": return Theme.palette.warning
        case "danger": return Theme.palette.danger
        case "info": return Theme.palette.info
        default: return Theme.palette.textSecondary
        }
    }

    function toneBackgroundColor() {
        switch (root.tone) {
        case "success": return Theme.palette.successSubtle
        case "warning": return Theme.palette.warningSubtle
        case "danger": return Theme.palette.dangerSubtle
        case "info": return Theme.palette.infoSubtle
        default: return Theme.palette.surfaceOverlay
        }
    }

    radius: Theme.radius.sm
    color: root.toneBackgroundColor()
    implicitWidth: Math.ceil(label.implicitWidth) + Theme.space.sm * 2
    implicitHeight: Math.ceil(Theme.type.caption.lineHeightPx) + Theme.space.xs * 2

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.toneTextColor()
        font.family: Theme.font.family
        font.pixelSize: Theme.type.caption.pixelSize
        font.weight: Font.Medium
    }
}
