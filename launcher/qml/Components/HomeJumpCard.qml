// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One "Jump back in" card on the Home page: a recently played instance,
 * wide enough for its own cover art plus a line of facts. Used directly as
 * a Repeater delegate against the shell's recentModel, so the required
 * properties are filled from InstanceList's named roles the same way
 * InstanceCard's are.
 *
 * Play here is a plain neutral button, not the accent-filled PlayButton
 * used elsewhere: the Home page's one accent-filled control is the
 * persistent play bar's own Play (design-plan.md Principle 1). Clicking the
 * card body itself opens the instance page instead of playing it.
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
    // Newest screenshot url (InstanceList's coverImage role), or "" -- same
    // source as InstanceCard's cover; CoverArt falls back to a tinted plate.
    required property string coverImage
    // Set by LaunchTask around the game process exit -- a calm reminder,
    // not an alarm.
    required property bool hasCrashed

    signal clicked()
    signal playRequested()
    signal stopRequested()

    readonly property bool hasPhoto: root.coverImage.length > 0
    readonly property bool hovered: hoverHandler.hovered

    implicitWidth: 336
    implicitHeight: 148

    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()

    Accessible.role: Accessible.ListItem
    Accessible.name: root.name
    Accessible.description: Format.versionLine(root.loader, root.gameVersion)

    HoverHandler { id: hoverHandler }

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Theme.radius.lg
        color: root.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
        border.width: 1
        border.color: root.hovered ? Theme.palette.borderStrong : Theme.palette.border
        clip: true

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        // Declared before the cover/content below, same order InstanceCard
        // uses, so the neutral Play button (a real child control further
        // down) still gets first refusal on a press inside its own bounds.
        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked() }
        }

        CoverArt {
            id: art
            anchors.fill: parent
            radius: card.radius
            source: root.coverImage
            tint: root.iconTint
            iconKey: root.iconKey
            iconSize: 48
            scrim: "horizontal"
            hovered: root.hovered
            matte: card.color
        }

        Column {
            id: content
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.lg
            anchors.right: playButton.left
            anchors.rightMargin: Theme.space.md
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.xs

            Text {
                width: parent.width
                text: root.name
                elide: Text.ElideRight
                color: root.hasPhoto ? Theme.media.text : Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.bodyStrong.pixelSize + 1
                font.weight: Font.Bold
            }

            Row {
                spacing: Theme.space.xs

                Tag {
                    text: Format.versionLine(root.loader, root.gameVersion)
                    iconName: "layers"
                    onMedia: root.hasPhoto
                }
                Tag {
                    readonly property string played: Format.playTime(root.totalTimePlayed)
                    text: played.length > 0 ? qsTr("%1 · %2").arg(Format.lastPlayed(root.lastLaunch)).arg(played)
                                             : Format.lastPlayed(root.lastLaunch)
                    iconName: "clock"
                    onMedia: root.hasPhoto
                }
            }

            StatusBadge {
                visible: root.hasCrashed
                tone: "danger"
                text: qsTr("Crashed last time")
            }
        }

        Button {
            id: playButton
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.space.md
            text: root.isRunning ? qsTr("Stop") : qsTr("Play")
            icon.source: Icons.url(root.isRunning ? "stop" : "play")
            enabled: root.isRunning || root.canLaunch
            focusPolicy: Qt.NoFocus
            onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
        }
    }

    Rectangle {
        // Keyboard focus ring, outset so it never competes with the card's
        // own border -- same idiom InstanceCard uses.
        x: card.x - 3
        y: card.y - 3
        width: card.width + 6
        height: card.height + 6
        radius: card.radius + 3
        color: "transparent"
        border.width: 2
        border.color: Theme.palette.focusRing
        visible: root.activeFocus
    }
}
