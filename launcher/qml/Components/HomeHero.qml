// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The top of the Home page: one instance, big, and the page's one primary
 * Play. The instance is `rowModel`'s first row -- the shell's heroModel (the
 * selection, else the instance played last, else the first one) or, until
 * that is wired, the instance played last.
 *
 * This is the hero's model and its signals; how it looks is the page's
 * look (Theme.look), each its own view of it: the card of the classic look
 * (HomeHeroCard), the full-bleed stage of Sahne (HomeStageHero), the open
 * line of Sade (HomeCalmHero) and the words on the sky of Gece
 * (HomeNightHero). A view reads the instance off this item and sends its
 * buttons' signals through it, and tells the page how tall it is
 * (`implicitHeight`), how far under it the next section starts (`gapBelow`)
 * and where its Play is (`playCenterY`).
 *
 * The hero spans the page's full width, so a look can bleed to its edges;
 * the column the page keeps its content in is `columnX` and `columnWidth`.
 * A real screenshot of the instance, softened to its colours, is the
 * backdrop when it has one; otherwise the neutral surface -- never a
 * stand-in picture.
 */
Item {
    id: root

    // A model whose first row is the instance to show.
    property var rowModel: null
    // Main.qml's launchOptions(): what ▾ offers.
    property var launchOptionsFor: null
    // QmlShell.recentWorlds: the "Last world" fact.
    property var worldsModel: null

    signal openRequested(string id)
    signal launchRequested(string id)
    // Stop, and Cancel while a launch runs.
    signal stopRequested(string id)
    signal launchModeRequested(string id, string mode)
    signal launchAsRequested(string id, string accountId)
    signal profilerChosen(string id, string profilerKey)
    signal performanceChosen(string id, bool enabled)
    signal navigateRequested(string target)
    // The "Last world" fact: the instance's Worlds tab.
    signal worldsRequested(string id)

    // -- The row, read into plain properties -------------------------------
    // A role model has no properties of its own to bind to; a throwaway
    // delegate reads row 0's roles (PlayDock's idiom).
    property string instanceId: ""
    property string name: ""
    property string iconKey: ""
    property bool isRunning: false
    property bool canLaunch: false
    property var lastLaunch: 0
    property var totalTimePlayed: 0
    property string gameVersion: ""
    property string loader: ""
    property string coverImage: ""
    property string packArt: ""
    property bool hasCrashed: false
    property string launchStatus: ""
    property real launchProgress: -1

    // The page's column, for views that keep inside it.
    property real columnX: 0
    property real columnWidth: width

    readonly property bool hasInstance: root.instanceId.length > 0
    readonly property bool launching: root.launchStatus.length > 0
    readonly property bool hasBackdrop: root.coverImage.length > 0
    readonly property string playTimeText: SettingsStore.bool("ShowGameTime") && Number(root.totalTimePlayed) > 0
                                           ? Format.playTime(root.totalTimePlayed) : ""
    // The screenshot is decoded this small and stretched back up (CoverArt's
    // photoSoftness): its colours stay, while what was written on it -- a
    // server's "You are AFK", the crosshair, the HUD -- blurs away instead
    // of fighting the name.
    readonly property real backdropSoftness: 0.02

    // What the look's view says about itself.
    readonly property real gapBelow: view.item ? view.item.gapBelow : 0
    readonly property real playCenterY: view.item ? view.item.playCenterY : 0

    visible: root.hasInstance
    implicitHeight: view.item ? view.item.implicitHeight : 0

    Repeater {
        model: root.rowModel
        delegate: Item {
            id: probe
            required property int index
            required property string instanceId
            required property string name
            required property string iconKey
            required property bool isRunning
            required property bool canLaunch
            required property var lastLaunch
            required property var totalTimePlayed
            required property string gameVersion
            required property string loader
            required property string coverImage
            required property var packArt
            required property bool hasCrashed
            required property string launchStatus
            required property real launchProgress
            visible: false

            function sync() {
                if (probe.index !== 0)
                    return
                root.instanceId = probe.instanceId
                root.name = probe.name
                root.iconKey = probe.iconKey
                root.isRunning = probe.isRunning
                root.canLaunch = probe.canLaunch
                root.lastLaunch = probe.lastLaunch
                root.totalTimePlayed = probe.totalTimePlayed
                root.gameVersion = probe.gameVersion
                root.loader = probe.loader
                root.coverImage = probe.coverImage
                root.packArt = probe.packArt || ""
                root.hasCrashed = probe.hasCrashed
                root.launchStatus = probe.launchStatus
                root.launchProgress = probe.launchProgress
            }
            readonly property string signature: [index, instanceId, name, iconKey, isRunning, canLaunch,
                                                 lastLaunch, totalTimePlayed, gameVersion, loader, coverImage,
                                                 packArt, hasCrashed, launchStatus, launchProgress].join("\n")
            onSignatureChanged: sync()
            Component.onCompleted: sync()
            Component.onDestruction: if (probe.index === 0 && root.instanceId === probe.instanceId) root.instanceId = ""
        }
    }


    Loader {
        id: view
        width: root.width
        sourceComponent: Theme.look === "stage" ? stageView
                       : Theme.look === "calm" ? calmView
                       : Theme.look === "night" ? nightView : cardView
    }
    Component { id: cardView; HomeHeroCard { hero: root } }
    Component { id: stageView; HomeStageHero { hero: root } }
    Component { id: calmView; HomeCalmHero { hero: root } }
    Component { id: nightView; HomeNightHero { hero: root } }
}
