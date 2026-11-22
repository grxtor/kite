// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * Several results of ContentBrowserView installed in one go. `selected` holds the ticked projects; start() takes
 * them one after another through the view's own install (startInstall(): the version a row's Install would pick,
 * its dependency review when one is needed), each beginning once the one before has settled, so a dependency two of
 * them share is downloaded once. A project that cannot be installed is counted and skipped. stop() leaves what is
 * running and drops the rest.
 */
QtObject {
    id: root

    // The ContentBrowserView: projectKey(), startInstall(), installSettled().
    required property var owner
    property var browser: null

    // [{ id, title, author }]
    property var selected: []
    readonly property bool selecting: root.selected.length > 0

    property var queue: []
    property var current: null
    property int total: 0
    property int done: 0
    property int failed: 0
    // From start() until the last one has settled.
    property bool running: false

    // (installed, failed) once a batch is over.
    signal finished(int installed, int failed)

    function isSelected(id) {
        return root.selected.some((one) => one.id === id)
    }
    function toggle(id, title, author) {
        if (root.isSelected(id))
            root.selected = root.selected.filter((one) => one.id !== id)
        else
            root.selected = root.selected.concat([{ id: id, title: title, author: author }])
    }
    function clear() {
        root.selected = []
    }

    function start() {
        if (root.running || !root.selecting || !root.browser)
            return
        root.queue = root.selected.slice()
        root.total = root.queue.length
        root.done = 0
        root.failed = 0
        root.selected = []
        root.running = true
        root.next()
    }
    function stop() {
        root.total -= root.queue.length
        root.queue = []
    }
    // A new search replaces the rows: nothing ticked is on screen any more, the rest of a running batch is dropped,
    // and one still waiting for its versions will never hear of them.
    function interrupt() {
        root.selected = []
        root.stop()
        if (root.current && !root.current.started)
            root.settle(false)
    }
    // The same for a row whose versions never arrive (the list was replaced by a filter change meanwhile).
    readonly property int patience: 20000
    property var watchdog: Timer {
        interval: root.patience
        running: root.current !== null && !root.current.started
        onTriggered: root.settle(false)
    }

    function next() {
        if (root.queue.length === 0) {
            root.current = null
            const wasRunning = root.running
            root.running = false
            if (wasRunning)
                root.finished(root.done - root.failed, root.failed)
            root.total = 0
            return
        }
        const item = root.queue[0]
        root.queue = root.queue.slice(1)
        root.current = item
        const row = root.browser.rowOfProject(item.id)
        if (row < 0)
            root.settle(false)
        else
            root.browser.prepareRow(row)
    }
    function settle(ok) {
        root.done += 1
        if (!ok)
            root.failed += 1
        root.current = null
        Qt.callLater(root.next)
    }

    property var prepared: Connections {
        target: root.browser
        function onRowPrepared(row) {
            const item = root.current
            if (!item || item.started || root.browser.rowOfProject(item.id) !== row)
                return
            // A new object, so what reads `current.started` (the watchdog) hears of it.
            const key = root.owner.projectKey(item.title, item.author)
            root.current = { id: item.id, title: item.title, author: item.author, started: true, key: key }
            if (!root.owner.startInstall(key, row, root.browser.bestVersionId(row)))
                root.settle(false)
        }
    }
    property var settled: Connections {
        target: root.owner
        function onInstallSettled(key, ok) {
            if (root.current && root.current.key === key)
                root.settle(ok)
        }
    }
}
