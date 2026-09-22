// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One instance, opened from the library: the same banner as the library's
 * hero -- Play, folder, what it runs -- over tabs for what is inside it.
 * Everything not here yet (versions, servers, backups...) is in the
 * classic editor, one click away on the banner.
 */
Item {
    id: root

    // Single-row model of this instance (the same roles as the library).
    property var headerModel: null
    // InstanceDetails for it.
    property var details: null
    property int systemMemoryMiB: 8192
    property string tab: "overview"

    signal backRequested()
    signal launchRequested(string id)
    signal stopRequested(string id)
    signal classicEditorRequested(string id)
    signal openPathRequested(string path)

    readonly property var modsModel: details ? details.mods : null
    readonly property var worldsModel: details ? details.worlds : null
    readonly property var shotsModel: details ? details.screenshots : null

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        anchors.topMargin: Theme.space.md
        anchors.bottomMargin: Theme.space.lg
        spacing: Theme.space.md

        Button {
            flat: true
            text: qsTr("Library")
            icon.source: Icons.url("chevron-left")
            leftPadding: Theme.space.sm
            onClicked: root.backRequested()
        }

        Repeater {
            model: root.headerModel
            delegate: ContinueCard {
                Layout.fillWidth: true
                overline: qsTr("Instance")
                editText: qsTr("Classic editor")
                onPlayRequested: root.launchRequested(instanceId)
                onStopRequested: root.stopRequested(instanceId)
                onEditRequested: root.classicEditorRequested(instanceId)
                onFolderRequested: root.openPathRequested(root.details ? root.details.instanceRoot : "")
                onMenuRequested: root.classicEditorRequested(instanceId)

                // The overview needs these too, and this delegate is the
                // only place the header model's roles are at hand.
                Component.onCompleted: overview.syncFrom(this)
                onLastLaunchChanged: overview.syncFrom(this)
                onTotalTimePlayedChanged: overview.syncFrom(this)
                onGameVersionChanged: overview.syncFrom(this)
                onLoaderChanged: overview.syncFrom(this)
                onIsRunningChanged: overview.running = isRunning
            }
        }

        TabStrip {
            Layout.fillWidth: true
            current: root.tab
            tabs: [
                { id: "overview", label: qsTr("Overview") },
                { id: "mods", label: qsTr("Mods"), count: root.modsModel ? modsTab.count : -1 },
                { id: "worlds", label: qsTr("Worlds"), count: root.worldsModel ? worldsTab.count : -1 },
                { id: "screenshots", label: qsTr("Screenshots"), count: root.shotsModel ? root.shotsModel.count : -1 },
                { id: "log", label: qsTr("Log") },
                { id: "settings", label: qsTr("Settings") }
            ]
            onActivated: (id) => root.tab = id
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: Theme.space.sm
            currentIndex: ["overview", "mods", "worlds", "screenshots", "log", "settings"].indexOf(root.tab)

            InstanceOverviewTab {
                id: overview
                property bool running: false
                function syncFrom(card) {
                    loader = card.loader
                    gameVersion = card.gameVersion
                    lastLaunch = card.lastLaunch
                    totalTimePlayed = card.totalTimePlayed
                }
                modCount: root.modsModel ? modsTab.count : -1
                worldCount: root.worldsModel ? worldsTab.count : -1
                notes: root.details ? root.details.notes : ""
                onNotesEdited: (text) => { if (root.details) root.details.notes = text }
            }

            ModsTab {
                id: modsTab
                details: root.details
                onOpenFolderRequested: (path) => root.openPathRequested(path)
            }

            WorldsTab {
                id: worldsTab
                details: root.details
                onOpenFolderRequested: (path) => root.openPathRequested(path)
            }

            ScreenshotsTab {
                model: root.shotsModel
                directory: root.details ? root.details.screenshotsDir : ""
                onOpenFolderRequested: (path) => root.openPathRequested(path)
            }

            LogTab {
                log: root.details ? root.details.log : null
            }

            InstanceSettingsTab {
                adapter: root.details ? root.details.settings : null
                systemMemoryMiB: root.systemMemoryMiB
                running: overview.running
            }
        }
    }
}
