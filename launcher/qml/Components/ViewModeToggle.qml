// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Grid or list, as a library toolbar offers it: one outlined group of two icon
 * segments split by a hairline, the current view's segment filled with the
 * hover overlay, each named by its tooltip.
 */
Rectangle {
    id: root

    // "grid" or "list"
    property string current: "grid"
    signal activated(string value)

    readonly property var options: [
        { value: "grid", icon: "grid", tip: qsTr("Grid view") },
        { value: "list", icon: "list", tip: qsTr("List view") }
    ]

    implicitWidth: segments.implicitWidth
    implicitHeight: Theme.control.height
    radius: Theme.radius.sm
    color: "transparent"

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("View")

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
            delegate: ToolButton {
                id: option
                required property var modelData
                required property int index
                readonly property bool active: option.modelData.value === root.current
                readonly property bool first: option.index === 0
                readonly property bool last: option.index === root.options.length - 1

                implicitWidth: Theme.control.height
                implicitHeight: Theme.control.height
                padding: 0
                display: AbstractButton.IconOnly
                icon.source: Icons.url(option.modelData.icon)
                icon.width: Theme.icon.mid
                icon.height: Theme.icon.mid
                icon.color: option.active || option.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
                // Mirrors `active` for accessibility; the button must not
                // toggle it itself, which would drop the binding.
                checked: option.active
                hoverEnabled: true

                Accessible.name: option.modelData.tip
                ToolTip.visible: option.hovered
                ToolTip.delay: 500
                ToolTip.text: option.modelData.tip

                onClicked: if (!option.active) root.activated(option.modelData.value)

                // A rounded fill reaching past the inner edge, clipped
                // there, so only the group's outer corners are round.
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
