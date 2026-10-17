// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * Grid or list, as a launcher's library toolbar offers it: two small icon
 * buttons side by side, the current view's one lit (the Style ToolButton's
 * checked look), each named by its tooltip. One click switches; clicking
 * the lit one does nothing.
 */
Row {
    id: root

    // "grid" or "list"
    property string current: "grid"
    signal activated(string value)

    spacing: Theme.space.xxs

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("View")

    Repeater {
        model: [
            { value: "grid", icon: "grid", tip: qsTr("Grid view") },
            { value: "list", icon: "list", tip: qsTr("List view") }
        ]
        delegate: ToolButton {
            id: option
            required property var modelData
            readonly property bool active: option.modelData.value === root.current

            implicitWidth: Theme.control.height
            implicitHeight: Theme.control.height
            padding: 0
            display: AbstractButton.IconOnly
            icon.source: Icons.url(option.modelData.icon)
            icon.width: Theme.icon.md
            icon.height: Theme.icon.md
            // Mirrors `active` for the look and for accessibility; the
            // button must not toggle it itself, which would drop the binding.
            checked: option.active
            hoverEnabled: true

            Accessible.name: option.modelData.tip
            ToolTip.visible: option.hovered
            ToolTip.delay: 500
            ToolTip.text: option.modelData.tip

            onClicked: if (!option.active) root.activated(option.modelData.value)
        }
    }
}
