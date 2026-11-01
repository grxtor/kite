// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The most recently played worlds across every instance as HomeWorldTile rows, at most `maxWorlds`: Home's side panel
 * and the Sahne stage's world pane show the same list. `inset` lets the rows bleed past the column's edges so their
 * hover fill lines up with the panel's content.
 */
Column {
    id: root

    // RecentWorldsModel, or null.
    property var worldsModel: null
    property bool glass: false
    property int inset: 0

    signal worldRequested(string instanceId)

    readonly property int maxWorlds: 4
    readonly property bool hasWorlds: worlds.count > 0

    Repeater {
        id: worlds
        model: root.worldsModel
        delegate: HomeWorldTile {
            required property int index

            visible: index < root.maxWorlds
            x: -root.inset
            width: root.width + root.inset * 2
            glass: root.glass
            onClicked: root.worldRequested(instanceId)
        }
    }
}
