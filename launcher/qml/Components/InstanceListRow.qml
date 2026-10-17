// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One instance in the library's list view: a dense row instead of
 * InstanceCard's cover art, for a library that would rather scan names than
 * browse thumbnails. Same roles and signals as InstanceCard (it is bound to
 * the same per-group model), so the two delegates are interchangeable from
 * InstanceSection's point of view -- only the layout differs.
 *
 * The trailing columns drop out at narrow widths rather than truncating into
 * nonsense; the name always keeps the room it needs to stay readable.
 *
 * While a launch runs it takes Play's place, as on the card and the play
 * bar: a bar where Play sits, the stage ("Downloading assets") in the
 * version column and how far it is in the last-played one, and the
 * running dot in the warning colour until the game is actually up.
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

    property bool selected: false
    property bool forceHovered: false

    signal clicked()
    signal doubleClicked()
    signal playRequested()
    signal stopRequested()
    // @p anchor: the ⋯ button when that asked, so the menu opens under it;
    // null for a right-click or the Menu key, which open at the pointer.
    signal menuRequested(Item anchor)

    readonly property bool hovered: forceHovered || hoverHandler.hovered
    readonly property bool launching: launchStatus.length > 0
    // The same icon chip InstanceCard puts on its cover: 32px of icon on a
    // 4px frame.
    readonly property int badgeSize: 40
    readonly property int badgeInset: Theme.space.xs
    readonly property int runningDotSize: 9
    readonly property string iconSource: "image://instanceicon/" + (root.iconKey.length > 0 ? root.iconKey : "default")
    readonly property int versionColumnWidth: 150
    readonly property int lastPlayedColumnWidth: 92
    readonly property int playTimeColumnWidth: 72
    readonly property bool showVersionColumn: width > 460
    readonly property bool showStatsColumns: width > 620

    implicitWidth: 400
    implicitHeight: badgeSize + Theme.space.sm * 2

    activeFocusOnTab: true
    Keys.onReturnPressed: root.playRequested()
    Keys.onSpacePressed: root.clicked()
    Keys.onMenuPressed: root.menuRequested(null)

    Accessible.role: Accessible.ListItem
    Accessible.name: root.name
    Accessible.description: Format.versionLine(root.loader, root.gameVersion)

    HoverHandler { id: hoverHandler }

    // A card like InstanceCard, only flat: the same surface, border and
    // selected/hover grammar, so switching Grid/List changes the layout and
    // nothing else.
    Rectangle {
        id: surface
        anchors.fill: parent
        radius: Theme.radius.lg
        color: Theme.palette.surface
        // Hover changes this one property, as on the grid card.
        border.width: root.selected ? 2 : 1
        border.color: root.selected ? Theme.palette.accent
                    : root.hovered ? Theme.palette.borderStrong : Theme.palette.border

        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked() }
            onDoubleTapped: root.playRequested()
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked(); root.menuRequested(null) }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.sm
            anchors.rightMargin: Theme.space.sm
            spacing: Theme.space.md

            Rectangle {
                Layout.preferredWidth: root.badgeSize
                Layout.preferredHeight: root.badgeSize
                radius: Theme.radius.sm
                color: Theme.palette.surfaceRaised
                border.width: 1
                border.color: Theme.palette.border

                Image {
                    id: icon
                    anchors.fill: parent
                    anchors.margins: root.badgeInset
                    source: root.iconSource
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                    // Rendered artwork: filtered and mipmapped, so any
                    // scaling the scene graph does stays clean.
                    smooth: true
                    mipmap: true
                }

                // Pulsing while running, same treatment as the sidebar's
                // Recent list -- one running-instance language everywhere.
                // Amber while the launch is still starting the game.
                Rectangle {
                    visible: root.isRunning || root.launching
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: -Theme.space.xxs
                    width: root.runningDotSize
                    height: width
                    radius: width / 2
                    color: root.launching ? Theme.palette.warning : Theme.palette.success
                    border.width: 2
                    border.color: surface.color

                    SequentialAnimation on opacity {
                        running: root.isRunning || root.launching
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.45; duration: 900; easing.type: Easing.InOutSine }
                        NumberAnimation { from: 0.45; to: 1.0; duration: 900; easing.type: Easing.InOutSine }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.name
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.bodyStrong.pixelSize
                font.weight: Theme.type.bodyStrong.weight
            }

            Text {
                visible: root.showVersionColumn
                Layout.preferredWidth: root.versionColumnWidth
                Layout.minimumWidth: 0
                text: root.launching ? root.launchStatus : Format.versionLine(root.loader, root.gameVersion)
                elide: Text.ElideRight
                color: root.launching ? Theme.palette.accentText : Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
                font.weight: Font.Medium
            }

            Text {
                visible: root.showStatsColumns
                Layout.preferredWidth: root.lastPlayedColumnWidth
                horizontalAlignment: Text.AlignRight
                text: !root.launching ? Format.lastPlayed(root.lastLaunch)
                    : root.launchProgress >= 0 ? qsTr("%1%").arg(Math.round(root.launchProgress * 100)) : ""
                elide: Text.ElideRight
                color: root.launching ? Theme.palette.accentText : Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
                // Digits of one width, so the number does not jitter.
                font.features: ({ "tnum": 1 })
            }

            Text {
                // "Show play time per instance" (Settings > Minecraft).
                visible: root.showStatsColumns && SettingsStore.bool("ShowGameTime")
                Layout.preferredWidth: root.playTimeColumnWidth
                horizontalAlignment: Text.AlignRight
                text: Format.playTime(root.totalTimePlayed)
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }

            Item {
                Layout.preferredWidth: Theme.control.height * 2 + Theme.space.xs
                Layout.preferredHeight: root.badgeSize

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

                // Where Play sits, across the whole slot.
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
                    // A plain fade, no overshoot scale-in (design-plan.md §4/§2.4).
                    opacity: !root.launching && (root.hovered || root.isRunning) ? 1 : 0
                    visible: opacity > 0
                    focusPolicy: Qt.NoFocus
                    Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                    onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
                }
            }
        }
    }

    Rectangle {
        // Keyboard focus ring, outset like InstanceCard's: 4px out from a
        // 12px corner is a 16px corner.
        anchors.fill: surface
        anchors.margins: -Theme.space.xs
        radius: Theme.radius.xl
        color: "transparent"
        border.width: 2
        border.color: Theme.palette.focusRing
        visible: root.activeFocus && !root.selected
    }
}
