// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Home's "Continue playing", under the hero: the other instances to get
 * back to, as compact rows (HomeJumpCard) -- the most recently played first
 * (recentModel), then, while there is room, the ones never played yet, each
 * with a "New" chip. The hero's own instance (`excludeId`) is never listed
 * again. Two columns on a window wide enough for both to stay readable;
 * hidden with nothing to list.
 *
 * How the rows look is the page's look (Theme.look): the classic rows
 * (HomeJumpCard), a row of cards with the instance's screenshot for Sahne
 * (HomeStageCard), hairline lines on the page for Sade (HomeCalmRow) and a
 * grid of glass panes for Gece (HomeNightRow). What they list and when it is
 * read again is the same in all of them.
 *
 * Both lists are read into plain rows (and read again whenever anything a
 * row shows changes). The never-played ones come off the library's model,
 * which follows the Library's search, so while one is on (`frozen`) they
 * hold still: typing in the search under this never moves the rows above
 * it, or the search field itself.
 */
Column {
    id: root

    // Most recently played instances first; never-played ones left out
    // (QmlShell.recentModel).
    property var recentModel: null
    // The library (QmlShell's instance proxy), for the never-played rows.
    property var instanceModel: null
    // The hero's instance: not listed here.
    property string excludeId: ""
    property bool frozen: false

    signal openRequested(string id)
    signal launchRequested(string id)
    signal stopRequested(string id)
    // A row's ⋯ (@p anchor) or right-click (no anchor), with what the row
    // shows, for the instance menu.
    signal menuRequested(string id, bool running, string name, string iconKey, Item anchor)

    readonly property string look: Theme.look
    readonly property int maxRows: 4
    // Between rows, and from the title to the first: Sahne's cards stand
    // further apart, Sade's rows touch (their hairlines are the gap).
    readonly property int gap: root.look === "stage" ? Theme.space.lg
                              : root.look === "calm" ? 0 : Theme.space.md
    // Below this a row's name and meta crowd its Play; a Sahne card or a
    // Gece pane is narrower than a classic row.
    readonly property int rowMinWidth: root.look === "stage" ? 190
                                     : root.look === "night" ? 300 : 480
    readonly property int columns: root.look === "stage"
        ? (root.width >= root.rowMinWidth * root.maxRows + root.gap * (root.maxRows - 1) ? root.maxRows : 2)
        : root.look === "calm" ? 1
        : root.width >= root.rowMinWidth * 2 + root.gap ? 2 : 1
    readonly property int rowWidth: Math.floor((root.width - root.gap * (root.columns - 1)) / root.columns)
    readonly property int titleGap: root.look === "stage" ? Theme.space.md + 2
                                  : root.look === "calm" ? Theme.space.sm + 2
                                  : root.look === "night" ? Theme.space.md + 2 : Theme.space.lg

    property var playedRows: []
    property var freshRows: []
    readonly property var rows: {
        var shown = []
        var lists = [root.playedRows, root.freshRows]
        for (var l = 0; l < lists.length; ++l) {
            for (var i = 0; i < lists[l].length && shown.length < root.maxRows; ++i) {
                if (lists[l][i].instanceId !== root.excludeId)
                    shown.push(lists[l][i])
            }
        }
        return shown
    }

    function rowOf(source) {
        return {
            instanceId: source.instanceId, name: source.name, iconKey: source.iconKey,
            isRunning: source.isRunning, canLaunch: source.canLaunch, lastLaunch: source.lastLaunch,
            totalTimePlayed: source.totalTimePlayed, gameVersion: source.gameVersion,
            loader: source.loader, iconTint: source.iconTint, packArt: source.packArt || "",
            coverImage: source.coverImage || "", hasCrashed: source.hasCrashed
        }
    }
    // One more than shown, so the hero's own row can drop out.
    function read(reader, played) {
        var rows = []
        for (var i = 0; i < reader.count && rows.length <= root.maxRows; ++i) {
            var row = reader.objectAt(i)
            if (row && (Number(row.lastLaunch) > 0) === played)
                rows.push(root.rowOf(row))
        }
        return rows
    }
    function readPlayed() { root.playedRows = root.read(playedReader, true) }
    function readFresh() {
        if (!root.frozen)
            root.freshRows = root.read(freshReader, false)
    }
    onFrozenChanged: if (!root.frozen) Qt.callLater(root.readFresh)

    // What a row shows, off one model row. Any of it moving reads the rows
    // again.
    component RowReader: QtObject {
        required property string instanceId
        required property string name
        required property string iconKey
        required property bool isRunning
        required property bool canLaunch
        required property var lastLaunch
        required property var totalTimePlayed
        required property string gameVersion
        required property string loader
        required property var iconTint
        required property var packArt
        required property string coverImage
        required property bool hasCrashed
        readonly property string signature: [instanceId, name, iconKey, isRunning, canLaunch, lastLaunch,
                                             totalTimePlayed, gameVersion, loader, packArt, coverImage,
                                             hasCrashed].join("\n")
    }
    Instantiator {
        id: playedReader
        model: root.recentModel
        delegate: RowReader {
            onSignatureChanged: Qt.callLater(root.readPlayed)
        }
        onObjectAdded: Qt.callLater(root.readPlayed)
        onObjectRemoved: Qt.callLater(root.readPlayed)
    }
    Instantiator {
        id: freshReader
        model: root.instanceModel
        delegate: RowReader {
            onSignatureChanged: Qt.callLater(root.readFresh)
        }
        onObjectAdded: Qt.callLater(root.readFresh)
        onObjectRemoved: Qt.callLater(root.readFresh)
    }

    visible: root.rows.length > 0
    spacing: root.titleGap

    HomeSectionTitle {
        width: parent.width
        text: qsTr("Continue playing")
    }

    Grid {
        columns: root.columns
        columnSpacing: root.gap
        rowSpacing: root.gap

        Repeater {
            model: root.rows
            delegate: root.look === "stage" ? stageCard
                    : root.look === "calm" ? calmRow
                    : root.look === "night" ? nightRow : classicRow
        }
    }

    Component {
        id: classicRow
        HomeJumpCard {
            id: row
            required property var modelData
            width: root.rowWidth
            instanceId: modelData.instanceId
            name: modelData.name
            iconKey: modelData.iconKey
            isRunning: modelData.isRunning
            canLaunch: modelData.canLaunch
            lastLaunch: modelData.lastLaunch
            totalTimePlayed: modelData.totalTimePlayed
            gameVersion: modelData.gameVersion
            loader: modelData.loader
            iconTint: modelData.iconTint
            coverImage: ""
            packArt: modelData.packArt
            hasCrashed: modelData.hasCrashed
            onClicked: root.openRequested(row.instanceId)
            onPlayRequested: root.launchRequested(row.instanceId)
            onStopRequested: root.stopRequested(row.instanceId)
            onMenuRequested: (anchor) => root.menuRequested(row.instanceId, row.isRunning, row.name, row.iconKey, anchor)
        }
    }
    Component {
        id: stageCard
        HomeStageCard {
            required property var modelData
            row: modelData
            host: root
            width: root.rowWidth
        }
    }
    Component {
        id: calmRow
        HomeCalmRow {
            required property var modelData
            required property int index
            row: modelData
            host: root
            width: root.rowWidth
            showDivider: index < root.rows.length - 1
        }
    }
    Component {
        id: nightRow
        HomeNightRow {
            required property var modelData
            row: modelData
            host: root
            width: root.rowWidth
        }
    }
}
