// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The top of the Home page, the way Lunar Client opens: one instance, big,
 * and the page's one primary Play. The instance is `rowModel`'s first row
 * -- the shell's heroModel (the selection, else the instance played last,
 * else the first one) or, until that is wired, the instance played last.
 *
 * Its picture on the calm tile (HomeHeroTile); its name and what it runs
 * (HomeHeroTitle); label over value, when it was last played, for how long
 * and the world it was last in (HomeHeroFacts); at the card's far edge Play
 * (PlayButton: Stop while it runs, "Starting…" and Cancel while a launch
 * runs) and ▾ for the other ways to play (LaunchOptionsMenu, as on the play
 * bar). The facts sit between the name and Play on a card wide enough to
 * leave room on both sides, so a wide card has no empty middle; else under
 * the name; on a narrow card they become words on the line under it.
 *
 * A real screenshot of the instance is the card's backdrop when it has one
 * (the facts then stay under the name, on its darker side); otherwise the
 * card is the neutral surface -- never a stand-in picture. Clicking the card
 * anywhere else opens the instance page.
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

    readonly property bool hasInstance: root.instanceId.length > 0
    readonly property bool launching: root.launchStatus.length > 0
    readonly property bool hasBackdrop: root.coverImage.length > 0
    readonly property string playTimeText: SettingsStore.bool("ShowGameTime") && Number(root.totalTimePlayed) > 0
                                           ? Format.playTime(root.totalTimePlayed) : ""

    // Lunar's scale: a big tile, a wide Play.
    readonly property int tileSize: Theme.control.heightLg * 2 + Theme.space.xxl
    readonly property int playWidth: 176
    readonly property int padding: Theme.space.xxl
    // On a photo the words take the media colours.
    readonly property color ink: root.hasBackdrop ? Theme.media.text : Theme.palette.textPrimary
    readonly property color inkSecondary: root.hasBackdrop ? Theme.media.textSecondary : Theme.palette.textSecondary
    readonly property color inkLabel: root.hasBackdrop ? Theme.media.textSecondary : Theme.palette.textTertiary

    // Where the facts go: beside the name with at least factsGap on either
    // side, else under it, else nowhere (the line under the name says when
    // and how long instead). Measured off natural widths only, never off
    // the layout they change.
    readonly property int gap: Theme.space.xl
    readonly property int factsGap: Theme.space.xxl
    readonly property real innerWidth: root.width - root.padding * 2
    readonly property real besideName: root.innerWidth - root.tileSize - actions.implicitWidth - root.gap * 2
    readonly property bool factsBeside: !root.hasBackdrop
                                        && root.besideName - title.naturalWidth >= factsBelow.naturalWidth + root.factsGap * 2
    readonly property bool factsUnder: !root.factsBeside && root.besideName >= factsBelow.naturalWidth
    readonly property bool showsFacts: root.factsBeside || root.factsUnder

    visible: root.hasInstance
    implicitHeight: root.tileSize + root.padding * 2

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

    function openLaunchMenu() {
        launchMenu.refresh()
        launchMenu.popup(moreButton, moreButton.width - launchMenu.implicitWidth, moreButton.height + Theme.space.xs)
    }

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Theme.radius.xl
        color: Theme.palette.surface
        border.width: root.hasBackdrop ? 0 : 1
        border.color: Theme.palette.border

        // Anywhere but its buttons: the instance page.
        TapHandler {
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: root.openRequested(root.instanceId)
        }

        // The instance's own newest screenshot, darker on the words' side.
        CoverArt {
            anchors.fill: parent
            visible: root.hasBackdrop
            source: root.coverImage
            glyph: ""
            showLogo: false
            radius: card.radius
            scrim: "horizontal"
            matte: Theme.palette.canvas
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: root.padding
            spacing: root.gap

            HomeHeroTile {
                Layout.preferredWidth: root.tileSize
                Layout.preferredHeight: root.tileSize
                Layout.alignment: Qt.AlignVCenter
                iconKey: root.hasInstance ? (root.iconKey.length > 0 ? root.iconKey : "default") : ""
                packArt: root.packArt
                matte: card.color
            }

            ColumnLayout {
                // Beside the facts it is only as wide as the name.
                Layout.fillWidth: !root.factsBeside
                Layout.preferredWidth: root.factsBeside ? title.naturalWidth : -1
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                spacing: 0

                HomeHeroTitle {
                    id: title
                    Layout.fillWidth: true
                    name: root.name
                    versionText: Format.versionLine(root.loader, root.gameVersion)
                    hasCrashed: root.hasCrashed
                    lastLaunch: root.lastLaunch
                    playTimeText: root.playTimeText
                    showsWhen: !root.showsFacts
                    launchStatus: root.launchStatus
                    launchProgress: root.launchProgress
                    onMedia: root.hasBackdrop
                    ink: root.ink
                    inkSecondary: root.inkSecondary
                    progressMaxWidth: root.playWidth * 2
                }

                HomeHeroFacts {
                    id: factsBelow
                    Layout.topMargin: Theme.space.md
                    visible: root.factsUnder && !root.launching
                    instanceId: root.instanceId
                    lastLaunch: root.lastLaunch
                    playTimeText: root.playTimeText
                    worldsModel: root.worldsModel
                    ink: root.ink
                    inkLabel: root.inkLabel
                    onWorldRequested: (id) => root.worldsRequested(id)
                }
            }

            // The facts beside the name, in the middle of the room between
            // it and Play.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.factsBeside

                HomeHeroFacts {
                    anchors.centerIn: parent
                    instanceId: root.instanceId
                    lastLaunch: root.lastLaunch
                    playTimeText: root.playTimeText
                    worldsModel: root.worldsModel
                    ink: root.ink
                    inkLabel: root.inkLabel
                    onWorldRequested: (id) => root.worldsRequested(id)
                }
            }

            // The page's one primary button, at the card's far edge: Play,
            // Stop while the game runs, "Starting…" (and Cancel) while a
            // launch prepares it; ▾ for the other ways to play.
            Row {
                id: actions
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.space.sm

                Button {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.launching
                    buttonSize: "lg"
                    text: qsTr("Cancel")
                    onClicked: root.stopRequested(root.instanceId)
                }
                PlayButton {
                    anchors.verticalCenter: parent.verticalCenter
                    round: false
                    size: Theme.control.heightLg
                    implicitWidth: root.playWidth
                    running: root.isRunning && !root.launching
                    busy: root.launching
                    enabled: !root.launching && (root.isRunning || root.canLaunch)
                    onClicked: root.isRunning ? root.stopRequested(root.instanceId)
                                              : root.launchRequested(root.instanceId)
                }
                IconButton {
                    id: moreButton
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !root.isRunning && !root.launching
                    size: Theme.control.heightLg
                    flat: false
                    iconName: "chevron-down"
                    icon.width: Theme.icon.md
                    icon.height: Theme.icon.md
                    tip: qsTr("More ways to play")
                    enabled: root.canLaunch && !!root.launchOptionsFor
                    onClicked: launchMenu.visible ? launchMenu.close() : root.openLaunchMenu()
                }
            }
        }
    }

    LaunchOptionsMenu {
        id: launchMenu
        instanceId: root.instanceId
        optionsFor: root.launchOptionsFor
        onLaunchModeRequested: (id, mode) => root.launchModeRequested(id, mode)
        onLaunchAsRequested: (id, accountId) => root.launchAsRequested(id, accountId)
        onProfilerChosen: (id, key) => root.profilerChosen(id, key)
        onNavigateRequested: (target) => root.navigateRequested(target)
    }
}
