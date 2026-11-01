// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One instance in the library grid, a single surface: its icon (or a pack's
 * logo) straight on it, then name and loader/version. A running or launching
 * instance shows a RunningDot and its state at the start of the meta line; Play
 * (Stop while it runs) and ⋯ sit at the end of that line. Used directly as a
 * delegate; the required properties are filled from InstanceList's named roles.
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
    signal openRequested()
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
    readonly property int runningDotSize: 7
    // Play is shown under the pointer and while the game runs, never during a launch.
    readonly property bool playShown: !root.launching && (root.hovered || root.isRunning)

    implicitWidth: 180
    implicitHeight: inset + squareSize + Theme.space.sm + Theme.type.title.lineHeightPx
                    + Theme.type.label.lineHeightPx + textInset

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
        id: card
        anchors.fill: parent
        hovered: root.hovered
        selected: root.selected
        focused: root.activeFocus
        onClicked: { root.forceActiveFocus(); root.clicked() }
        onDoubleClicked: root.openRequested()
        onTripleClicked: if (root.canLaunch && !root.isRunning) root.playRequested()
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

            // An instance counts as running from the moment its launch begins; amber until the game is up.
            RunningDot {
                id: statusDot
                visible: root.isRunning || root.launching
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                size: root.runningDotSize
                launching: root.launching
            }

            Copy {
                id: versionText
                x: statusDot.visible ? statusDot.width + Theme.space.xs : 0
                anchors.verticalCenter: parent.verticalCenter
                // Short of the buttons at the line's end; ⋯ is counted shown or not, so nothing reflows under it.
                width: Math.max(0, Math.min(implicitWidth, (root.playShown ? playButton.x : moreButton.x) - meta.x - x - Theme.space.xs))
                text: root.launching ? root.launchStatus
                    : root.isRunning ? qsTr("Running") : Format.versionLine(root.loader, root.gameVersion)
                elide: Text.ElideRight
                role: "label"
                tone: "secondary"
            }

            Copy {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: text.length > 0 && versionText.x + versionText.implicitWidth + Theme.space.sm + implicitWidth <= parent.width
                text: root.launching && root.launchProgress >= 0 ? qsTr("%1%").arg(Math.round(root.launchProgress * 100)) : ""
                role: "label"
                tone: "secondary"
                font.features: ({ "tnum": 1 })
            }
        }
    }

    // On the card, under the meta line, so nothing lies over the picture.
    LaunchProgressBar {
        visible: root.launching
        x: root.textInset
        width: root.width - root.textInset * 2
        y: root.height - Theme.space.sm
        progress: root.launchProgress
    }

    IconButton {
        id: playButton
        x: moreButton.x - width
        y: moreButton.y
        size: Theme.control.heightSm
        iconName: root.isRunning ? "stop" : "play"
        tip: root.isRunning ? qsTr("Stop") : qsTr("Play")
        icon.color: root.isRunning ? Theme.palette.danger : contentColor
        enabled: root.isRunning || root.canLaunch
        opacity: root.playShown ? 1 : 0
        visible: opacity > 0
        focusPolicy: Qt.NoFocus
        Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
        onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
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
