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
 * Its icon large on the calm tile (or the pack's own logo across it), the
 * name in the heading face at display size, what it runs, when it was last
 * played and for how long; at the card's far edge Play (PlayButton, Stop
 * while it runs, "Starting…" and Cancel while a launch runs, its stage and a
 * bar under the name) and ▾ for the other ways to play (LaunchOptionsMenu,
 * as on the play bar). A real
 * screenshot of the instance is the card's backdrop when it has one;
 * otherwise the card is the neutral surface -- never a stand-in picture.
 * Clicking the card anywhere else opens the instance page.
 */
Item {
    id: root

    // A model whose first row is the instance to show.
    property var rowModel: null
    // Main.qml's launchOptions(): what ▾ offers.
    property var launchOptionsFor: null

    signal openRequested(string id)
    signal launchRequested(string id)
    // Stop, and Cancel while a launch runs.
    signal stopRequested(string id)
    signal launchModeRequested(string id, string mode)
    signal launchAsRequested(string id, string accountId)
    signal profilerChosen(string id, string profilerKey)
    signal navigateRequested(string target)

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
    readonly property bool showsPackArt: root.packArt.length > 0 && packLogo.status === Image.Ready
    readonly property string playTimeText: SettingsStore.bool("ShowGameTime") && Number(root.totalTimePlayed) > 0
                                           ? Format.playTime(root.totalTimePlayed) : ""
    readonly property string playedText: Number(root.lastLaunch) > 0
                                         ? qsTr("Played %1").arg(Format.lastPlayed(root.lastLaunch))
                                         : qsTr("Never played")

    // Lunar's scale: a big tile, a wide Play.
    readonly property int tileSize: Theme.control.heightLg * 2 + Theme.space.xxl
    readonly property real iconShare: 0.84
    readonly property int playWidth: 176
    readonly property int padding: Theme.space.xxl
    // On a photo the words take the media colours.
    readonly property color ink: root.hasBackdrop ? Theme.media.text : Theme.palette.textPrimary
    readonly property color inkSecondary: root.hasBackdrop ? Theme.media.textSecondary : Theme.palette.textSecondary

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
            spacing: Theme.space.xl

            Item {
                Layout.preferredWidth: root.tileSize
                Layout.preferredHeight: root.tileSize
                Layout.alignment: Qt.AlignVCenter

                PlaceholderSurface {
                    anchors.fill: parent
                    radius: Theme.radius.xl
                }
                Image {
                    anchors.centerIn: parent
                    visible: !root.showsPackArt
                    width: Math.round(root.tileSize * root.iconShare)
                    height: width
                    source: root.hasInstance ? "image://instanceicon/" + (root.iconKey.length > 0 ? root.iconKey : "default") : ""
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }
                Image {
                    id: packLogo
                    anchors.fill: parent
                    visible: root.showsPackArt
                    source: root.packArt
                    sourceSize: Qt.size(width * 2, height * 2)
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true
                    asynchronous: true
                }
                CornerMatte {
                    visible: root.showsPackArt
                    cornerRadius: Theme.radius.xl
                    matte: card.color
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.space.xs

                Text {
                    Layout.fillWidth: true
                    text: root.name
                    elide: Text.ElideRight
                    color: root.ink
                    font.family: Theme.font.heading
                    font.pixelSize: Theme.type.display.pixelSize
                    font.weight: Theme.type.display.weight
                }

                // "Fabric 1.21.4 • Played 2 h ago • ⌛ 3 h 15 min", or the
                // crash in place of when.
                Row {
                    Layout.fillWidth: true
                    spacing: Theme.space.sm

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: [Format.versionLine(root.loader, root.gameVersion),
                               root.hasCrashed ? "" : root.playedText].filter((part) => part.length > 0).join("  •  ")
                        color: root.inkSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.body.pixelSize
                    }
                    MeshIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.hasCrashed
                        iconName: "alert-triangle"
                        size: Theme.icon.sm
                        color: Theme.palette.warning
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.hasCrashed
                        text: qsTr("Crashed last time")
                        color: Theme.palette.warning
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.body.pixelSize
                    }
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.playTimeText.length > 0
                        leftPadding: Theme.space.sm
                        spacing: Theme.space.xs
                        MeshIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            iconName: "hourglass"
                            size: Theme.icon.sm
                            color: root.inkSecondary
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.playTimeText
                            color: root.inkSecondary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            font.features: ({ "tnum": 1 })
                        }
                    }
                }

                // While a launch runs: what it is doing, and how far.
                Column {
                    Layout.fillWidth: true
                    Layout.maximumWidth: root.playWidth * 2
                    Layout.topMargin: Theme.space.sm
                    visible: root.launching
                    spacing: Theme.space.xs

                    Text {
                        width: parent.width
                        text: root.launchProgress >= 0
                              ? qsTr("%1  •  %2%").arg(root.launchStatus).arg(Math.round(root.launchProgress * 100))
                              : root.launchStatus
                        elide: Text.ElideRight
                        color: root.inkSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                    LaunchProgressBar {
                        width: parent.width
                        onMedia: root.hasBackdrop
                        progress: root.launchProgress
                    }
                }
            }

            // The page's one primary button, at the card's far edge: Play,
            // Stop while the game runs, "Starting…" (and Cancel) while a
            // launch prepares it; ▾ for the other ways to play.
            Row {
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
