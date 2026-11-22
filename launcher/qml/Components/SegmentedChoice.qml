// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One of a few named choices that switch what a list shows (Mods / Resource packs / Shader packs), drawn like
 * ViewModeToggle: one outlined group of text segments split by hairlines, the current one filled with the hover
 * overlay. It sits in a toolbar row, so a page's own tab strip stays the only row of tabs.
 */
Rectangle {
    id: root

    // [{ value, label }]
    property var options: []
    property var current
    signal activated(var value)

    implicitWidth: segments.implicitWidth
    implicitHeight: Theme.control.height
    radius: Theme.radius.sm
    color: "transparent"

    Accessible.role: Accessible.Grouping

    // Bumped as segments are built: itemAt() is not reactive.
    property int rev: 0
    readonly property Item activeItem: {
        root.rev
        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i)
            if (item && item.active)
                return item
        }
        return null
    }
    SegmentSlider {
        target: root.activeItem
        radius: root.radius
    }

    Row {
        id: segments

        Repeater {
            id: repeater
            model: root.options
            onItemAdded: root.rev++
            onItemRemoved: root.rev++
            delegate: AbstractButton {
                id: option
                required property var modelData
                required property int index
                readonly property bool active: option.modelData.value === root.current
                readonly property bool first: option.index === 0
                readonly property bool last: option.index === root.options.length - 1

                implicitHeight: Theme.control.height
                implicitWidth: Math.ceil(label.implicitWidth) + Theme.space.md * 2
                hoverEnabled: true
                // Mirrors `active` for accessibility; clicking must not toggle it, which would drop the binding.
                checked: option.active
                Accessible.role: Accessible.RadioButton
                Accessible.name: option.modelData.label
                onClicked: if (!option.active) root.activated(option.modelData.value)

                // A rounded fill reaching past the inner edge, clipped there, so only the group's outer corners
                // are round.
                background: Item {
                    Item {
                        anchors.fill: parent
                        clip: true

                        Rectangle {
                            x: option.first ? 0 : -root.radius
                            width: parent.width + (option.first || option.last ? root.radius : root.radius * 2)
                            height: parent.height
                            radius: root.radius
                            // The active segment's fill is the group's SegmentSlider.
                            color: option.down && !option.active ? Theme.palette.pressedOverlay
                                 : option.hovered && !option.active ? Theme.palette.hoverOverlay : "transparent"
                            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                        }
                        Rectangle {
                            visible: !option.first
                            width: 1
                            height: parent.height
                            color: Theme.neutral.outline
                        }
                    }
                    FocusRing { visible: option.visualFocus }
                }

                contentItem: Copy {
                    id: label
                    text: option.modelData.label
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    role: "label"
                    color: option.active || option.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
                    font.weight: option.active ? Font.DemiBold : Font.Medium
                    scale: option.down ? Theme.motion.pressScaleLight : 1
                    Behavior on scale { NumberAnimation { duration: Theme.motion.press; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot } }
                    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                }
            }
        }
    }

    // The edge goes over the segments' fills.
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: Theme.neutral.outline
    }
}
