// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * "Recent worlds" on the Sahne stage: the most recently played worlds across every instance (HomeWorldTile rows, at
 * most four) on a dark glass pane over the screenshot. Its height is its content's.
 */
HomeGlass {
    id: root

    // RecentWorldsModel, or null.
    property var worldsModel: null

    signal worldRequested(string instanceId)

    readonly property bool hasWorlds: worldList.hasWorlds
    readonly property int padding: Theme.space.lg + Theme.space.xs
    readonly property int headingGap: Theme.space.md

    dark: true
    radius: Theme.radius.xl
    implicitHeight: column.height + root.padding + Theme.space.sm

    Column {
        id: column
        x: root.padding
        y: root.padding
        width: root.width - root.padding * 2
        spacing: root.headingGap

        HomeLabel {
            width: parent.width
            text: qsTr("Recent worlds")
            tracking: 0.1
            color: Theme.media.textSecondary
        }

        HomeWorldList {
            id: worldList
            width: parent.width
            worldsModel: root.worldsModel
            glass: true
            onWorldRequested: (id) => root.worldRequested(id)
        }
    }
}
