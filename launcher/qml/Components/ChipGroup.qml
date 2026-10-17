// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * A row of filter chips, one of them on: what kind of thing a list shows
 * (Mods / Resource packs / Shader packs), the way Modrinth App filters an
 * instance's content. Each chip stands on its own -- no shared track, so it
 * never reads as a segmented switch -- and the row looks the same at every
 * window width. Filter chips are true toggles, so they are pills.
 */
Row {
    id: root

    // [{ value, label }]
    property var options: []
    property var current
    signal activated(var value)

    spacing: Theme.space.sm

    Accessible.role: Accessible.Grouping

    Repeater {
        model: root.options
        delegate: AbstractButton {
            id: chip
            required property var modelData
            readonly property bool selected: modelData.value === root.current

            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            implicitHeight: Theme.control.heightSm
            // Whole pixels, so the chips after it stay on the pixel grid.
            implicitWidth: Math.ceil(label.implicitWidth) + Theme.space.md * 2
            hoverEnabled: true
            // Mirrors `selected` for accessibility only; clicking must not
            // toggle it, which would drop the binding.
            checked: selected
            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData.label
            onClicked: if (!selected) root.activated(modelData.value)

            background: Rectangle {
                radius: Theme.radius.pill
                color: chip.selected ? Theme.palette.accentSubtle
                     : chip.hovered ? Theme.palette.surfaceRaised : "transparent"
                border.width: 1
                border.color: chip.selected ? Theme.palette.accent : Theme.palette.border
                Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -Theme.space.xxs
                    radius: Theme.radius.pill
                    color: "transparent"
                    border.width: 2
                    border.color: Theme.palette.focusRing
                    visible: chip.visualFocus
                }
            }

            contentItem: Text {
                id: label
                text: chip.modelData.label
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: chip.selected ? Theme.palette.accentText : Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.weight: chip.selected ? Font.DemiBold : Font.Medium
            }
        }
    }
}
