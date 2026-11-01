// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A small neutral chip for facts about something ("Fabric", "1.21.4"). StatusBadge
 * is the coloured, stateful sibling and shares its height. Corners are
 * radius.sm: soft, but short of a pill, which is for toggles.
 */
Rectangle {
    id: root

    property string text
    property string iconName
    // For tinted or image backgrounds, where the default fill would vanish: a darker, see-through chip.
    property bool onMedia: false

    // Whole pixels, so a row of tags keeps its gaps exact and 1px borders stay sharp.
    implicitWidth: Math.ceil(row.implicitWidth) + Theme.space.sm * 2
    implicitHeight: Math.ceil(Theme.type.caption.lineHeightPx) + Theme.space.xs * 2
    radius: Theme.radius.sm
    color: root.onMedia ? Theme.media.chip : Theme.palette.surfaceOverlay
    border.width: 1
    border.color: root.onMedia ? Theme.media.chipBorder : Theme.palette.border

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.space.xs

        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.iconName.length > 0
            iconName: root.iconName
            size: Theme.icon.xs
            color: label.color
        }

        Copy {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            role: "caption"
            tone: root.onMedia ? "media" : "secondary"
            font.weight: Font.Medium
        }
    }
}
