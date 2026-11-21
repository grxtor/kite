// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * A row of filter chips, one of them on (a category). Each chip is its own outline, so it never reads as a
 * segmented switch; the selected one's outline and label turn textPrimary. `tabs: true` draws the same choice as
 * text tabs over one hairline with the selected tab underlined in the accent, for a kind picker (Mods / Resource
 * packs / Shader packs) that switches what a whole list shows.
 */
Row {
    id: root

    property var options: []
    property var current
    property bool tabs: false
    signal activated(var value)

    spacing: root.tabs ? 0 : Theme.space.sm

    Accessible.role: Accessible.Grouping

    // Bumped as delegates come and go: itemAt() is not reactive, so a rebuilt row would leave the underline
    // without its tab.
    property int rev: 0
    // The selected tab, for the underline.
    readonly property Item currentItem: {
        root.rev
        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i)
            if (item && item.selected)
                return item
        }
        return null
    }
    // Takes no room in the row (no width, and tabs have no spacing); only there for tabs.
    Item {
        visible: root.tabs
        z: 1
        height: Theme.control.height
        TabUnderline { target: root.tabs ? root.currentItem : null }
    }

    Repeater {
        id: repeater
        model: root.options
        onItemAdded: root.rev++
        onItemRemoved: root.rev++
        delegate: AbstractButton {
            id: chip
            required property var modelData
            readonly property bool selected: modelData.value === root.current

            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            implicitHeight: root.tabs ? Theme.control.height : Theme.control.heightSm
            implicitWidth: Math.ceil(label.implicitWidth) + Theme.space.md * 2
            hoverEnabled: true
            // Mirrors `selected` for accessibility only; clicking must not
            // toggle it, which would drop the binding.
            checked: selected
            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData.label
            onClicked: if (!selected) root.activated(modelData.value)

            background: Rectangle {
                radius: root.tabs ? Theme.radius.sm : Theme.radius.indicator
                color: !root.tabs && chip.hovered && !chip.selected ? Theme.palette.hoverOverlay : "transparent"
                border.width: root.tabs ? 0 : 1
                border.color: chip.selected ? Theme.palette.textPrimary
                            : chip.hovered ? Theme.neutral.outlineHover : Theme.neutral.outline
                Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

                // Tabs: the hairline under the whole row (each tab carries
                // its stretch); the row's one underline slides over it.
                Rectangle {
                    visible: root.tabs
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Theme.palette.border
                }

                FocusFrame {
                    around: Theme.space.xxs
                    visible: chip.visualFocus
                }
            }

            contentItem: Copy {
                id: label
                text: chip.modelData.label
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                role: "label"
                color: chip.selected ? Theme.palette.textPrimary
                     : root.tabs ? (chip.hovered ? Theme.palette.textSecondary : Theme.palette.textTertiary)
                     : chip.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
                font.weight: chip.selected ? Font.DemiBold : Font.Medium
                Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            }
        }
    }
}
