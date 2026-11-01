// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One instance in the library grid: its icon (or a pack's logo) on a neutral
 * tile, then name and loader/version. Used directly as a delegate; the
 * required properties are filled from InstanceList's named roles.
 */
Item {
    id: root

    required property string instanceId
    required property string name
    required property string iconKey
    required property bool isRunning
    required property bool canLaunch
    required property var lastLaunch
    required property string gameVersion
    required property string loader
    required property color iconTint
    // InstanceList's coverImage role; unused here, required so the card takes the same roles as
    // other delegates.
    required property string coverImage
    // A platform pack's logo (InstanceList's packArt role) across the square; not required.
    property string packArt: ""
    // Empty while nothing launches; progress is 0..1, negative when the launch cannot say how far
    // along it is.
    required property string launchStatus
    required property real launchProgress

    property bool selected: false

    signal clicked()
    signal playRequested()
    signal stopRequested()
    // @p anchor: the ⋯ button when that asked, so the menu opens under it;
    // null for a right-click or the Menu key, which open at the pointer.
    signal menuRequested(Item anchor)

    readonly property bool hovered: hoverHandler.hovered
    readonly property bool launching: launchStatus.length > 0
    readonly property int inset: Theme.space.sm
    readonly property int textInset: Theme.space.md
    readonly property int squareSize: Math.max(0, width - inset * 2)
    readonly property int squareRadius: Theme.radius.md
    readonly property int playSize: Theme.control.height
    readonly property int cornerInset: Theme.space.sm
    readonly property int runningDotSize: 7

    implicitWidth: 180
    implicitHeight: inset + squareSize + Theme.space.sm + Theme.type.title.lineHeightPx
                    + Theme.type.label.lineHeightPx + textInset

    activeFocusOnTab: true
    Keys.onReturnPressed: root.playRequested()
    Keys.onSpacePressed: root.clicked()
    Keys.onMenuPressed: root.menuRequested(null)

    Accessible.role: Accessible.ListItem
    Accessible.name: root.name
    Accessible.description: Format.versionLine(root.loader, root.gameVersion)

    HoverHandler { id: hoverHandler }

    InstanceSurface {
        id: card
        anchors.fill: parent
        hovered: root.hovered
        selected: root.selected
        focused: root.activeFocus
        onClicked: { root.forceActiveFocus(); root.clicked() }
        onDoubleClicked: root.playRequested()
        onContextRequested: { root.forceActiveFocus(); root.clicked(); root.menuRequested(null) }

        InstanceTile {
            id: square
            x: root.inset
            y: root.inset
            width: root.squareSize
            height: root.squareSize
            radius: root.squareRadius
            matte: art.showsPackArt || art.iconFills ? card.color : "transparent"
            lifted: root.hovered

            InstanceArt {
                id: art
                instanceId: root.instanceId
                iconKey: root.iconKey
                packArt: root.packArt
                iconShare: 0.82
            }

            KiteShape {
                anchors.fill: parent
                radius: root.squareRadius
                color: Theme.media.chip
                opacity: root.launching ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }
            }

            LaunchProgressBar {
                visible: root.launching
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: root.cornerInset
                onMedia: true
                progress: root.launchProgress
            }

            // Running chip; "Starting…" until the game is up (an instance counts as running from
            // the moment its launch begins).
            Rectangle {
                visible: root.isRunning || root.launching
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: root.cornerInset
                radius: Theme.radius.indicator
                height: runningRow.implicitHeight + Theme.space.xs * 2
                width: runningRow.implicitWidth + Theme.space.sm * 2
                color: Theme.palette.surfaceOverlay
                border.width: 1
                border.color: Theme.palette.border

                Row {
                    id: runningRow
                    anchors.centerIn: parent
                    spacing: Theme.space.xs
                    RunningDot {
                        anchors.verticalCenter: parent.verticalCenter
                        size: root.runningDotSize
                        launching: root.launching
                    }
                    Copy {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.launching ? qsTr("Starting…") : qsTr("Running")
                        role: "caption"
                        font.weight: Font.DemiBold
                    }
                }
            }

            PlayButton {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: root.cornerInset
                size: root.playSize
                running: root.isRunning
                enabled: root.isRunning || root.canLaunch
                opacity: !root.launching && (root.hovered || root.isRunning) ? 1 : 0
                visible: opacity > 0
                focusPolicy: Qt.NoFocus
                Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
            }
        }

        Copy {
            id: title
            anchors.top: square.bottom
            anchors.topMargin: Theme.space.sm
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: root.textInset
            anchors.rightMargin: root.textInset
            height: Theme.type.title.lineHeightPx
            verticalAlignment: Text.AlignVCenter
            text: root.name
            elide: Text.ElideRight
            role: "title"
            font.family: Theme.font.family

            HoverHandler { id: titleHover; enabled: title.truncated }
            ToolTip.visible: title.truncated && titleHover.hovered
            ToolTip.delay: 500
            ToolTip.text: root.name
        }

        Item {
            id: meta
            anchors.top: title.bottom
            anchors.left: title.left
            anchors.right: title.right
            height: Theme.type.label.lineHeightPx

            Copy {
                id: versionText
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                // Short of ⋯ (moreButton), shown or not, so nothing reflows under the pointer.
                width: Math.min(implicitWidth, parent.width - moreButton.width)
                text: root.launching ? root.launchStatus : Format.versionLine(root.loader, root.gameVersion)
                elide: Text.ElideRight
                role: "label"
                tone: "secondary"
            }

            Copy {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: text.length > 0 && versionText.implicitWidth + Theme.space.sm + implicitWidth <= parent.width
                text: root.launching && root.launchProgress >= 0 ? qsTr("%1%").arg(Math.round(root.launchProgress * 100)) : ""
                role: "label"
                tone: "secondary"
                font.features: ({ "tnum": 1 })
            }
        }
    }

    IconButton {
        id: moreButton
        x: card.width - width - (root.textInset - Theme.space.xs)
        y: meta.y + (meta.height - height) / 2
        size: Theme.control.heightSm
        iconName: "more"
        tip: qsTr("More")
        // Not while a launch runs: its percentage has the line's end then.
        opacity: root.hovered && !root.launching ? 1 : 0
        visible: opacity > 0
        focusPolicy: Qt.NoFocus
        Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
        onClicked: root.menuRequested(moreButton)
    }
}
