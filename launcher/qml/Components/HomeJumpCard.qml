// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One "Continue playing" row on the Home page, under the hero: a compact
 * 64px row with the instance's icon square (the library card's, smaller),
 * its name -- and
 * a "New" chip until it has been played -- one line of what it runs and
 * when it was last played, how long it has been played, Play and ⋯. Play is
 * a secondary one (QuietPlayButton): the play bar's is the page's one
 * primary button. Used through a wrapper against the shell's recentModel,
 * with the same named roles InstanceCard reads.
 *
 * Clicking the row opens the instance page; Play plays it (Stop while it
 * runs); ⋯ or a right-click asks the page for the instance menu. After a
 * crash the sub-line says "Crashed last time" in place of when -- a calm
 * reminder, not an alarm.
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
    // Newest screenshot url (InstanceList's coverImage role), or "". Not
    // shown in the row (HomePage puts the played-last one in its header);
    // kept so the row takes the same roles as the library's card.
    required property string coverImage
    // A platform pack's own logo (InstanceList's packArt role), or "": shown
    // across the icon square, as on the library card.
    property string packArt: ""
    // Set by LaunchTask around the game process exit.
    required property bool hasCrashed

    signal clicked()
    signal playRequested()
    signal stopRequested()
    // @p anchor: the ⋯ button when that asked, so the menu opens under it;
    // null for a right-click, which opens it at the pointer.
    signal menuRequested(Item anchor)

    readonly property bool hovered: hoverHandler.hovered
    readonly property bool showsPackArt: root.packArt.length > 0 && packLogo.status === Image.Ready
    // Never launched: the row says so with a "New" chip.
    readonly property bool isNew: !(Number(root.lastLaunch) > 0)
    // Compact, a step below the hero: a 64px row, a 40px icon square the
    // icon all but fills, as the library card's does.
    readonly property int rowHeight: Theme.control.heightLg + Theme.space.lg
    readonly property int squareSize: Theme.control.height
    readonly property real iconShare: 0.84
    readonly property int iconSize: Math.round(root.squareSize * root.iconShare)
    readonly property bool iconFills: Icons.fillsTile(root.iconKey)
    // Empty (so not shown) when "Show play time per instance" is off.
    readonly property string playTimeText: SettingsStore.bool("ShowGameTime") && !root.isNew
                                           ? Format.playTime(root.totalTimePlayed) : ""
    readonly property string versionText: Format.versionLine(root.loader, root.gameVersion)
    // Play time is the part that gives way on a narrow row.
    readonly property int playTimeMinWidth: 520

    implicitWidth: 640
    implicitHeight: root.rowHeight

    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.isRunning ? root.stopRequested() : root.playRequested()
    Keys.onMenuPressed: root.menuRequested(null)

    Accessible.role: Accessible.ListItem
    Accessible.name: root.name
    Accessible.description: root.versionText

    HoverHandler { id: hoverHandler }

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Theme.radius.lg
        color: root.hovered ? Theme.palette.surfaceHover : Theme.palette.surface

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        Shadow {
            radius: card.radius
            level: Theme.elevation.sm
            opacity: root.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        }

        // Declared before the content, so Play (a real control further
        // down) still gets first refusal on a press inside its own bounds.
        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked() }
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.menuRequested(null) }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.lg
            anchors.rightMargin: Theme.space.md
            spacing: Theme.space.md

            InstanceTile {
                Layout.preferredWidth: root.squareSize
                Layout.preferredHeight: root.squareSize
                radius: Theme.radius.md
                // Rounds the logo's corners in the row's own colour.
                matte: root.showsPackArt || root.iconFills ? card.color : "transparent"
                lifted: root.hovered

                Image {
                    anchors.centerIn: parent
                    visible: !root.showsPackArt
                    width: root.iconFills ? parent.width : root.iconSize
                    height: width
                    source: Icons.instanceIcon(root.iconKey)
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
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.xxs

                // The name, and "New" beside it until the first launch.
                // Placed by hand: a layout sizing the name from its own
                // width loops.
                Item {
                    Layout.fillWidth: true
                    implicitHeight: Theme.type.title.lineHeightPx

                    Text {
                        id: nameText
                        width: Math.min(implicitWidth, parent.width - (newChip.visible ? newChip.width + Theme.space.sm : 0))
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.name
                        elide: Text.ElideRight
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.heading
                        font.pixelSize: Theme.type.title.pixelSize
                        font.weight: Theme.type.title.weight
                    }
                    // The accent as an outline and its text colour: no
                    // tinted fill.
                    Rectangle {
                        id: newChip
                        visible: root.isNew
                        x: nameText.width + Theme.space.sm
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.ceil(newLabel.implicitWidth) + Theme.space.sm * 2
                        height: Math.ceil(Theme.type.caption.lineHeightPx) + Theme.space.xxs * 2
                        radius: Theme.radius.indicator
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.palette.accent

                        Text {
                            id: newLabel
                            anchors.centerIn: parent
                            text: qsTr("New")
                            color: Theme.palette.accentText
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                            font.weight: Font.DemiBold
                        }
                    }
                }

                // "Fabric 1.21.4 • 3 days ago", or the crash in place of
                // when.
                Row {
                    Layout.fillWidth: true
                    spacing: Theme.space.xs

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, parent.width)
                        text: root.hasCrashed ? root.versionText
                                              : [root.versionText, Format.lastPlayed(root.lastLaunch)]
                                                .filter((part) => part.length > 0).join("  •  ")
                        elide: Text.ElideRight
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                    MeshIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.hasCrashed
                        iconName: "alert-triangle"
                        size: Theme.icon.sm - 2
                        color: Theme.palette.warning
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.hasCrashed
                        text: qsTr("Crashed last time")
                        color: Theme.palette.warning
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                }
            }

            // How long it has been played: a quiet fact before Play.
            Row {
                visible: root.playTimeText.length > 0 && root.width >= root.playTimeMinWidth
                spacing: Theme.space.xs

                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "hourglass"
                    size: Theme.icon.sm
                    color: Theme.palette.textTertiary
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.playTimeText
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.features: ({ "tnum": 1 })
                }
            }

            // Secondary: on Home the play bar's Play is the one primary.
            QuietPlayButton {
                Layout.leftMargin: Theme.space.sm
                running: root.isRunning
                enabled: root.isRunning || root.canLaunch
                focusPolicy: Qt.NoFocus
                onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
            }

            IconButton {
                id: moreButton
                Layout.leftMargin: -Theme.space.xs
                size: Theme.control.heightSm
                iconName: "more"
                tip: qsTr("More")
                focusPolicy: Qt.NoFocus
                onClicked: root.menuRequested(moreButton)
            }
        }
    }

    // The hairline, over everything so the logo's corner matte never
    // paints across it.
    Rectangle {
        anchors.fill: card
        radius: card.radius
        color: "transparent"
        border.width: 1
        border.color: root.hovered ? Theme.neutral.borderHover : Theme.palette.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    Rectangle {
        // Keyboard focus ring, outset so it never competes with the
        // row's own border -- the same idiom InstanceCard uses.
        anchors.fill: card
        anchors.margins: -Theme.space.xs
        radius: card.radius + Theme.space.xs
        color: "transparent"
        border.width: Theme.focus.width
        border.color: Theme.palette.focusRing
        visible: root.activeFocus
    }
}
