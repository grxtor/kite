// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The top of the Home page: one instance (`rowModel`'s first row), big, with the page's one primary Play.
 * Theme.look picks the view (HomeHeroCard, HomeStageHero, HomeCalmHero, HomeNightHero), which reads the instance
 * off this item, sends its signals through it and reports `implicitHeight`, `gapBelow` and `playCenterY`. The hero
 * spans the page; the content column is `columnX`/`columnWidth`.
 */
Item {
    id: root

    property var rowModel: null
    property var launchOptionsFor: null
    property var worldsModel: null

    signal openRequested(string id)
    signal launchRequested(string id)
    signal stopRequested(string id)
    signal launchModeRequested(string id, string mode)
    signal launchAsRequested(string id, string accountId)
    signal profilerChosen(string id, string profilerKey)
    signal performanceChosen(string id, bool enabled)
    signal navigateRequested(string target)
    signal worldsRequested(string id)

    // A role model has no properties to bind to; a throwaway delegate reads row 0's roles (PlayDock's idiom).
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
    property string coverKind: ""
    property string packArt: ""
    property bool hasCrashed: false
    property string crashCause: ""
    property string launchStatus: ""
    property real launchProgress: -1

    // The instance picked on Home and the one Home shows without a pick (the last played); they differ while a pick
    // is on.
    property string selectedId: ""
    property string lastPlayedId: ""

    property real columnX: 0
    property real columnWidth: width

    // The icon as drawn, averaged: the gradient behind a hero without a cover.
    readonly property color iconColor: Icons.tint(root.iconKey, root.instanceId)
    readonly property bool hasInstance: root.instanceId.length > 0
    readonly property bool launching: root.launchStatus.length > 0
    readonly property bool hasBackdrop: root.coverImage.length > 0
    readonly property bool hasTint: root.iconColor.valid && root.iconColor.a > 0
    readonly property bool isSelection: root.selectedId.length > 0 && root.instanceId !== root.lastPlayedId
    readonly property string playTimeText: SettingsStore.bool("ShowGameTime") && Number(root.totalTimePlayed) > 0
                                           ? Format.playTime(root.totalTimePlayed) : ""
    readonly property real gapBelow: view.item ? view.item.gapBelow : 0
    readonly property real playCenterY: view.item ? view.item.playCenterY : 0

    // The views feed their taps here. The hero already shows this instance, so one tap does nothing: two open its page,
    // three launch it.
    function tap() { taps.tap() }
    TapSequence {
        id: taps
        onTwice: root.openRequested(root.instanceId)
        onThrice: if (root.canLaunch && !root.isRunning) root.launchRequested(root.instanceId)
    }

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
            required property string coverKind
            required property var packArt
            required property bool hasCrashed
            required property string crashCause
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
                root.coverKind = probe.coverKind
                root.packArt = probe.packArt || ""
                root.hasCrashed = probe.hasCrashed
                root.crashCause = probe.crashCause
                root.launchStatus = probe.launchStatus
                root.launchProgress = probe.launchProgress
            }
            readonly property string signature: [index, instanceId, name, iconKey, isRunning, canLaunch,
                                                 lastLaunch, totalTimePlayed, gameVersion, loader, coverImage,
                                                 coverKind, packArt, hasCrashed, crashCause, launchStatus,
                                                 launchProgress].join("\n")
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
