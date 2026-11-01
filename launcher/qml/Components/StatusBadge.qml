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

    radius: Theme.radius.sm
    color: Theme.tone(root.tone).soft
    implicitWidth: Math.ceil(label.implicitWidth) + Theme.space.sm * 2
    implicitHeight: Math.ceil(Theme.type.caption.lineHeightPx) + Theme.space.xs * 2

    Copy {
        id: label
        anchors.centerIn: parent
        text: root.text
        role: "caption"
        color: Theme.tone(root.tone).ink
        font.weight: Font.Medium
    }
}
