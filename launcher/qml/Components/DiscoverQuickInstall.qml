// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * "Install" on a Discover row: the pack page's own install without opening the page. It loads the pack's versions
 * into the provider's `detail` (as opening the page does), takes the one the page would preselect (the featured
 * version, otherwise the newest) and hands it to the same `installer`. Every install of this session, from a row or
 * from the page, is kept by pack so the rows can say "Installing…" and "Installed".
 */
QtObject {
    id: root

    // The provider's modpack model: loadDetail() and `detail`.
    property var model: null
    // (projectId, versionId, name, group) -> TaskWatcher, as ModpackDetail's.
    property var installer: null
    property string provider: ""

    // The row's pack whose versions are being fetched; one at a time, the provider has one `detail`.
    property var pending: null
    readonly property string pendingId: root.pending ? root.pending.projectId : ""
    // No versions, or the install could not start: the page says why.
    signal failed(var pack)

    // TaskWatchers by provider and pack. A plain JS object does not notify, so `revision` is bumped on every change.
    property var watchers: ({})
    property int revision: 0
    function watcherFor(projectId) {
        root.revision
        return root.watchers[root.provider + "/" + projectId] || null
    }
    function remember(projectId, watcher) {
        if (!watcher)
            return
        root.watchers[root.provider + "/" + projectId] = watcher
        root.revision++
    }

    // loadDetail() empties the old pack's detail before it says it is loading.
    property bool starting: false
    function install(pack) {
        if (!root.model || !root.installer || root.pending)
            return
        root.pending = pack
        root.starting = true
        root.model.loadDetail(pack.projectId)
        root.starting = false
        root.settle()
    }

    function settle() {
        if (!root.pending || root.starting || !root.model)
            return
        var pack = root.pending
        var detail = root.model.detail
        // Another pack's page was opened meanwhile; its load replaced this one.
        if (!detail || detail.projectId !== pack.projectId) {
            root.pending = null
            return
        }
        var versions = detail.versions || []
        if (versions.length === 0) {
            if (!detail.loading) {
                root.pending = null
                root.failed(pack)
            }
            return
        }
        root.pending = null
        var featured = versions.findIndex((v) => v.featured)
        var watcher = root.installer(pack.projectId, versions[featured >= 0 ? featured : 0].id, pack.title, "")
        if (watcher)
            root.remember(pack.projectId, watcher)
        else
            root.failed(pack)
    }

    onProviderChanged: root.pending = null

    property Connections detailWatch: Connections {
        target: root.model ? root.model.detail : null
        ignoreUnknownSignals: true
        function onVersionsChanged() { root.settle() }
        function onLoadingChanged() { root.settle() }
        function onProjectIdChanged() { root.settle() }
    }
}
