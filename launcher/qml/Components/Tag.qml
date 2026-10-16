// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * A small neutral chip for facts about something ("Fabric", "1.21.4",
 * "9"). StatusBadge is the coloured, stateful sibling; this one never
 * signals state, so it has no tone. Corners use radius.sm, the tile radius,
 * rather than a pill.
 */
Rectangle {
    id: root

    property string text
    property string iconName
    // On a tinted or image background the default fill would vanish; this
    // lets such a caller ask for a darker, see-through chip instead.
    property bool onMedia: false

    // Whole pixels, so a row of tags keeps its gaps exact and 1px borders
    // stay sharp.
    implicitWidth: Math.ceil(row.implicitWidth) + Theme.space.sm * 2
    implicitHeight: Theme.control.heightSm - Theme.space.xs
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
            size: Theme.icon.sm - 2
            color: label.color
        }

        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.onMedia ? Theme.media.text : Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
            font.weight: Font.Medium
        }
    }
}
