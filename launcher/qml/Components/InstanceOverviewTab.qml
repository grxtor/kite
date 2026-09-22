// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The instance at a glance -- what it runs, how much it has been played,
 * how much is in it -- and the user's own notes about it.
 */
SettingsScroll {
    id: root

    property string loader
    property string gameVersion
    property var lastLaunch: 0
    property var totalTimePlayed: 0
    property int modCount: -1
    property int worldCount: -1
    property string notes
    signal notesEdited(string text)

    title: qsTr("Overview")

    Flow {
        width: parent.width
        spacing: Theme.space.md

        readonly property real tileWidth: Math.floor((width - spacing * 2) / 3)

        StatTile {
            width: parent.tileWidth
            iconName: "cube"
            label: qsTr("Minecraft")
            value: root.gameVersion
        }
        StatTile {
            width: parent.tileWidth
            iconName: "layers"
            label: qsTr("Mod loader")
            value: root.loader.length > 0 ? root.loader : qsTr("Vanilla")
        }
        StatTile {
            width: parent.tileWidth
            iconName: "clock"
            label: qsTr("Last played")
            value: Format.lastPlayed(root.lastLaunch)
        }
        StatTile {
            width: parent.tileWidth
            iconName: "play"
            label: qsTr("Time played")
            value: Format.playTime(root.totalTimePlayed) || qsTr("Not yet")
        }
        StatTile {
            width: parent.tileWidth
            iconName: "package"
            label: qsTr("Mods")
            value: root.modCount >= 0 ? String(root.modCount) : ""
        }
        StatTile {
            width: parent.tileWidth
            iconName: "globe"
            label: qsTr("Worlds")
            value: root.worldCount >= 0 ? String(root.worldCount) : ""
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Notes")
        description: qsTr("Anything worth remembering about this instance. Saved when you click away.")

        Item {
            property bool showDivider: false
            width: parent.width
            height: 180

            ScrollView {
                anchors.fill: parent
                anchors.margins: Theme.space.sm

                TextArea {
                    id: notesArea
                    placeholderText: qsTr("Seed, server address, which mods to update…")
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    background: null
                    Component.onCompleted: text = root.notes
                    onActiveFocusChanged: if (!activeFocus && text !== root.notes) root.notesEdited(text)
                }
            }
        }
    }
}
