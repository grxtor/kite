// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The pages visited, for Back and Forward. `route` is the window's current place as a navigate() path ("home",
 * "settings/java", "instance/<id>/<tab>"); every change of it is a visit. A new page drops what lay forward
 * and is added, at most `limit` entries; another tab or section of the same page replaces its entry, so Back
 * returns to the tab that was left. A modpack opened in Discover ("discover/<provider>/<id>") is a page of its own. back()/forward() move through the list with `go` (navigate) and add
 * nothing; an entry that no longer opens (a deleted instance) is dropped and the next one tried.
 */
QtObject {
    id: root

    property string route: ""
    // function (route): bool.
    property var go: null
    property int limit: 50

    property var entries: []
    property int index: -1
    readonly property bool canGoBack: root.index > 0
    readonly property bool canGoForward: root.index >= 0 && root.index < root.entries.length - 1

    // Later, so the steps of one navigation (page, then its tab) count once.
    onRouteChanged: Qt.callLater(root.visit, root.route)
    Component.onCompleted: root.visit(root.route)

    // The page of a route, without its tab or section.
    function pageOf(route) {
        var parts = route.split("/")
        if (parts[0] === "discover")
            return route
        return parts[0] === "instance" ? parts.slice(0, 2).join("/") : parts[0]
    }

    function visit(route) {
        if (route.length === 0 || (root.index >= 0 && root.entries[root.index] === route))
            return
        var list = root.entries.slice()
        if (root.index >= 0 && root.pageOf(list[root.index]) === root.pageOf(route)) {
            list[root.index] = route
            root.entries = list
            return
        }
        list = list.slice(Math.max(0, root.index + 2 - root.limit), root.index + 1)
        list.push(route)
        root.entries = list
        root.index = list.length - 1
    }

    function back() { return root.step(-1) }
    function forward() { return root.step(1) }

    function step(direction) {
        var list = root.entries.slice()
        var here = root.index
        var at = here + direction
        while (here >= 0 && at >= 0 && at < list.length) {
            // The same page twice in a row, left by an entry dropped between them, is one page.
            var same = root.pageOf(list[at]) === root.pageOf(list[here])
            if (!same) {
                root.entries = list
                root.index = at
                if (root.go(list[at]))
                    return true
            }
            list.splice(at, 1)
            if (direction < 0) {
                here -= 1
                at -= 1
            }
        }
        root.entries = list
        root.index = here
        return false
    }
}
