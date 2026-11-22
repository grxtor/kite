// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * "Popular modpacks" under the empty library's two buttons: the first packs of Discover's default listing (the same
 * Modrinth model, asked once while this is on screen and nothing was searched for yet) as ModpackTiles. A tile opens
 * the pack in Discover (openRequested); Install is Discover's row install. Skeleton tiles while the listing loads;
 * when it fails, or Discover's search is on something else, this takes no room and says nothing.
 */
Column {
    id: root

    // QmlShell.modpackModel and (projectId, versionId, name, group) -> TaskWatcher, as DiscoverPage's.
    property var model: null
    property var installer: null
    // The empty library is what the user is looking at.
    property bool active: false

    // { projectId, title, author, description, logoUrl, downloads, updated, categories, galleryUrl, accentColor }.
    signal openRequested(var pack)

    readonly property int maxTiles: 6
    readonly property int minTileWidth: 208
    readonly property int gutter: Theme.space.md
    readonly property int columns: Math.max(1, Math.min(root.maxTiles,
                                   Math.floor((root.width + root.gutter) / (root.minTileWidth + root.gutter))))
    // Whole rows only.
    readonly property int shown: Math.floor(root.maxTiles / root.columns) * root.columns
    readonly property real tileWidth: Math.floor((root.width - root.gutter * (root.columns - 1)) / root.columns)

    // The listing is the default one: no search words, no loader, the first sort.
    readonly property bool pristine: !!root.model && root.model.query === "" && root.model.loader === ""
        && (!root.model.sortOptions || root.model.sortOptions.length === 0
            || root.model.sort === root.model.sortOptions[0].id)
    readonly property bool loading: root.pristine && root.model.searching && root.model.count === 0
    readonly property bool ready: root.pristine && root.model.count > 0

    // Once: a failed request is not asked again behind the user's back.
    property bool asked: false
    function ask() {
        if (root.asked || !root.active || !root.pristine || root.model.count > 0 || root.model.searching)
            return
        root.asked = true
        root.model.search()
    }
    onActiveChanged: root.ask()
    onModelChanged: root.ask()
    Component.onCompleted: root.ask()

    visible: root.active && (root.loading || root.ready)
    spacing: Theme.space.md

    DiscoverQuickInstall {
        id: quickInstall
        model: root.model
        installer: root.installer
        provider: "modrinth"
        onFailed: (pack) => root.openRequested(pack)
    }

    HomeSectionTitle {
        width: parent.width
        text: qsTr("Popular modpacks")
    }

    Grid {
        columns: root.columns
        columnSpacing: root.gutter
        rowSpacing: root.gutter

        Repeater {
            model: root.ready ? root.model : null
            delegate: ModpackTile {
                id: tile
                required property var model
                readonly property string projectId: model.projectId
                readonly property var pack: ({ projectId: projectId, title: model.title, author: model.author,
                                               description: model.description, logoUrl: model.logoUrl,
                                               downloads: model.downloads, updated: model.updated,
                                               categories: model.categories, galleryUrl: model.galleryUrl || "",
                                               accentColor: model.accentColor })
                readonly property var install: quickInstall.watcherFor(projectId)

                visible: model.index < root.shown
                width: root.tileWidth
                title: model.title
                author: model.author
                logoUrl: visible ? model.logoUrl : ""
                downloads: model.downloads
                installState: quickInstall.pendingId === projectId ? "pending"
                            : !install ? ""
                            : install.running ? "installing"
                            : install.succeeded ? "installed"
                            : install.failed ? "failed" : ""
                installError: install && install.failed ? (install.error || "") : ""
                installEnabled: quickInstall.pendingId.length === 0
                onInstallRequested: quickInstall.install(pack)
                onClicked: root.openRequested(pack)
            }
        }

        Repeater {
            model: root.loading ? root.shown : 0
            delegate: ModpackTile {
                width: root.tileWidth
                skeleton: true
            }
        }
    }
}
