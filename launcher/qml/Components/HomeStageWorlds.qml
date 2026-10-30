// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * "Recent worlds" on the Sahne stage: the worlds played most recently
 * across every instance (the side panel's own HomeWorldTile rows, at most
 * four) on a dark glass pane over the screenshot. Its height is its
 * content's.
 */
HomeGlass {
    id: root

    // QmlShell.recentWorlds (RecentWorldsModel), or null.
    property var worldsModel: null

    signal worldRequested(string instanceId)

    readonly property int maxWorlds: 4
    readonly property bool hasWorlds: worldsRepeater.count > 0
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
            size: Theme.type.overline.pixelSize
            tracking: 0.1
            color: Theme.media.textSecondary
        }

        Column {
            width: parent.width

            Repeater {
                id: worldsRepeater
                model: root.worldsModel
                // A plain wrapper, so `model.index` resolves despite
                // HomeWorldTile's required properties.
                delegate: Item {
                    visible: model.index < root.maxWorlds
                    width: visible ? column.width : 0
                    height: visible ? tile.height : 0

                    HomeWorldTile {
                        id: tile
                        width: parent.width
                        glass: true
                        worldName: model.worldName
                        folderName: model.folderName
                        iconUrl: model.iconUrl
                        lastPlayed: model.lastPlayed
                        instanceId: model.instanceId
                        instanceName: model.instanceName
                        onClicked: root.worldRequested(model.instanceId)
                    }
                }
            }
        }
    }
}
