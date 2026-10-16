// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * A few mutually exclusive choices shown side by side, the current one
 * raised -- for short option lists where a combo box would hide the
 * alternatives behind a click.
 */
Rectangle {
    id: root

    // [{ value, label }]
    property var options: []
    property var current
    signal activated(var value)

    // The selected segment sits this far inside the track: its 1px border
    // plus a space.xxs gap.
    readonly property int inset: Theme.space.xxs + 1

    implicitWidth: row.implicitWidth + inset * 2
    implicitHeight: Theme.control.height
    radius: Theme.radius.md
    color: Theme.palette.surfaceSunken
    border.width: 1
    border.color: Theme.palette.border

    Accessible.role: Accessible.PageTabList

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.space.xxs

        Repeater {
            model: root.options
            delegate: AbstractButton {
                id: segment
                required property var modelData
                readonly property bool selected: modelData.value === root.current

                height: root.height - root.inset * 2
                // Whole pixels: a fractional segment width put every control
                // after it on a half pixel, blurring 1px borders.
                width: Math.ceil(label.implicitWidth) + Theme.space.lg * 2
                hoverEnabled: true
                // `checked` only mirrors `selected` (for accessibility); the
                // button must not toggle it itself, which would drop the binding.
                checked: selected
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.label
                onClicked: if (!selected) root.activated(modelData.value)

                background: Rectangle {
                    radius: Theme.radius.sm
                    color: segment.selected ? Theme.palette.surfaceRaised
                         : segment.hovered ? Theme.palette.hoverOverlay : "transparent"
                    border.width: segment.selected ? 1 : 0
                    border.color: Theme.palette.border
                    Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
                }

                contentItem: Text {
                    id: label
                    text: segment.modelData.label
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: segment.selected ? Theme.palette.textPrimary : Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.weight: segment.selected ? Font.DemiBold : Font.Medium
                }
            }
        }
    }
}
