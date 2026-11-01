// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Home's "Continue playing" rows under the hero: recently played instances first, then never-played ones while
 * there is room, minus the hero's (`excludeId`). The never-played ones come off the library model, which follows
 * the Library's search, so they hold still while a search is on (`frozen`).
 */
Column {
    id: root

    property var recentModel: null
    property var instanceModel: null
    property string excludeId: ""
    // The instance picked on Home; its row is marked.
    property string selectedId: ""
    property bool frozen: false

    signal selectRequested(string id)
    signal openRequested(string id)
    signal launchRequested(string id)
    signal stopRequested(string id)
    signal menuRequested(string id, bool running, string name, string iconKey, Item anchor)

    readonly property string look: Theme.look
    readonly property int maxRows: 4
    readonly property int gap: root.look === "stage" ? Theme.space.lg
                              : root.look === "calm" ? 0 : Theme.space.md
    readonly property int stageCardMinWidth: 190
    readonly property int nightRowMinWidth: 300
    readonly property int rowMinWidth: root.look === "stage" ? root.stageCardMinWidth
                                     : root.look === "night" ? root.nightRowMinWidth : 480
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
            coverImage: source.coverImage || "", coverKind: source.coverKind || "",
            hasCrashed: source.hasCrashed
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
        required property string coverKind
        required property bool hasCrashed
        readonly property string signature: [instanceId, name, iconKey, isRunning, canLaunch, lastLaunch,
                                             totalTimePlayed, gameVersion, loader, packArt, coverImage,
                                             coverKind, hasCrashed].join("\n")
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

    // One tap sequence for every row, not one per delegate: the rows are rebuilt whenever the data of any of them
    // changes, which would reset a sequence under way. The first tap picks, two open, three launch.
    property string tappedId: ""
    function tap(id) {
        if (id !== root.tappedId)
            taps.finish()
        root.tappedId = id
        taps.tap()
    }
    TapSequence {
        id: taps
        onSingle: root.selectRequested(root.tappedId)
        onTwice: root.openRequested(root.tappedId)
        onThrice: {
            for (var i = 0; i < root.rows.length; ++i) {
                var row = root.rows[i]
                if (row.instanceId === root.tappedId && row.canLaunch && !row.isRunning)
                    root.launchRequested(row.instanceId)
            }
        }
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
            selected: root.selectedId === row.instanceId
            onTapped: root.tap(row.instanceId)
            onSelectRequested: root.selectRequested(row.instanceId)
            onOpenRequested: root.openRequested(row.instanceId)
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
