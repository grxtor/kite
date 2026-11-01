// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One instance in the library's list view: InstanceCard as a row, with the same
 * roles and signals so InstanceSection can use either delegate. Trailing
 * columns drop out at narrow widths; the name keeps the room it needs.
 */
Item {
    id: root

    required property string instanceId
    required property string name
    required property string iconKey
    required property bool isRunning
    required property bool canLaunch
    required property var lastLaunch
    required property var totalTimePlayed
    required property string gameVersion
    required property string loader
    required property color iconTint
    required property string launchStatus
    required property real launchProgress
    // A platform pack's logo (InstanceList's packArt role) across the icon square; "" otherwise.
    property string packArt: ""

    property bool selected: false

    signal clicked()
    signal openRequested()
    signal playRequested()
    signal stopRequested()
    // @p anchor: the ⋯ button when that asked, so the menu opens under it;
    // null for a right-click or the Menu key, which open at the pointer.
    signal menuRequested(Item anchor)

    readonly property bool hovered: hoverHandler.hovered
    readonly property bool launching: launchStatus.length > 0
    readonly property int squareSize: Theme.control.heightLg
    readonly property int runningDotSize: 9
    readonly property int versionColumnWidth: 168
    readonly property int lastPlayedColumnWidth: 104
    readonly property int playTimeColumnWidth: 84
    readonly property bool showVersionColumn: width > 480
    readonly property bool showStatsColumns: width > 660

    implicitWidth: 400
    // The square's inset keeps the logo's corner matte inside the row.
    implicitHeight: squareSize + Theme.space.md * 2

    activeFocusOnTab: true
    Keys.onReturnPressed: root.openRequested()
    Keys.onSpacePressed: root.clicked()
    Keys.onMenuPressed: root.menuRequested(null)

    Accessible.role: Accessible.ListItem
    Accessible.selectable: true
    Accessible.selected: root.selected
    Accessible.name: root.name
    Accessible.description: [root.launching ? root.launchStatus : root.isRunning ? qsTr("Running") : "",
                             Format.versionLine(root.loader, root.gameVersion)]
                            .filter((part) => part.length > 0).join(", ")

    HoverHandler { id: hoverHandler }

    InstanceSurface {
        id: surface
        anchors.fill: parent
        hovered: root.hovered
        selected: root.selected
        focused: root.activeFocus
        onClicked: { root.forceActiveFocus(); root.clicked() }
        onDoubleClicked: root.openRequested()
        onTripleClicked: if (root.canLaunch && !root.isRunning) root.playRequested()
        onContextRequested: { root.forceActiveFocus(); root.clicked(); root.menuRequested(null) }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.md
            spacing: Theme.space.lg

            InstanceTile {
                Layout.preferredWidth: root.squareSize
                Layout.preferredHeight: root.squareSize
                radius: Theme.radius.md
                matte: art.showsPackArt || art.iconFills ? surface.color : "transparent"
                lifted: root.hovered

                InstanceArt {
                    id: art
                    instanceId: root.instanceId
                    iconKey: root.iconKey
                    packArt: root.packArt
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.sm

                // Pulses while running, amber while the launch is still starting the game.
                RunningDot {
                    visible: root.isRunning || root.launching
                    Layout.preferredWidth: size
                    Layout.preferredHeight: size
                    Layout.alignment: Qt.AlignVCenter
                    size: root.runningDotSize
                    launching: root.launching
                    pulsing: true
                }

                Copy {
                    id: nameText
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: root.name
                    elide: Text.ElideRight
                    role: "title"
                    font.family: Theme.font.family

                    HoverHandler { id: nameHover; enabled: nameText.truncated }
                    ToolTip.visible: nameText.truncated && nameHover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: root.name
                }
            }

            Copy {
                visible: root.showVersionColumn
                Layout.preferredWidth: root.versionColumnWidth
                Layout.minimumWidth: 0
                text: root.launching ? root.launchStatus : Format.versionLine(root.loader, root.gameVersion)
                elide: Text.ElideRight
                role: "label"
                tone: "secondary"
            }

            Copy {
                visible: root.showStatsColumns
                Layout.preferredWidth: root.lastPlayedColumnWidth
                horizontalAlignment: Text.AlignRight
                text: !root.launching ? Format.lastPlayed(root.lastLaunch)
                    : root.launchProgress >= 0 ? qsTr("%1%").arg(Math.round(root.launchProgress * 100)) : ""
                elide: Text.ElideRight
                role: "label"
                tone: root.launching ? "secondary" : "tertiary"
                font.features: ({ "tnum": 1 })
            }

            Copy {
                // "Show play time per instance" (Settings > Minecraft).
                visible: root.showStatsColumns && SettingsStore.bool("ShowGameTime")
                Layout.preferredWidth: root.playTimeColumnWidth
                horizontalAlignment: Text.AlignRight
                text: Format.playTime(root.totalTimePlayed)
                role: "label"
                tone: "tertiary"
                font.features: ({ "tnum": 1 })
            }

            Item {
                Layout.preferredWidth: Theme.control.height * 2 + Theme.space.xs
                Layout.preferredHeight: root.squareSize

                IconButton {
                    id: moreButton
                    anchors.right: playButton.left
                    anchors.rightMargin: Theme.space.xs
                    anchors.verticalCenter: parent.verticalCenter
                    size: Theme.control.height
                    iconName: "more"
                    tip: qsTr("More")
                    opacity: root.hovered && !root.launching ? 1 : 0
                    visible: opacity > 0
                    focusPolicy: Qt.NoFocus
                    Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                    onClicked: root.menuRequested(moreButton)
                }

                LaunchProgressBar {
                    visible: root.launching
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    progress: root.launchProgress
                }

                PlayButton {
                    id: playButton
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    size: Theme.control.height
                    running: root.isRunning
                    enabled: root.isRunning || root.canLaunch
                    opacity: !root.launching && (root.hovered || root.isRunning) ? 1 : 0
                    visible: opacity > 0
                    focusPolicy: Qt.NoFocus
                    Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                    onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
                }
            }
        }
    }
}
