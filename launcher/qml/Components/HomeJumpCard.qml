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
 *
 * Two bands: a header at the top-left -- the instance's icon on a small
 * tile, its name, and its version as a caption line -- and a footer row of
 * facts (when last played, how long) level with Play, which owns the
 * bottom-right corner. The facts stay on one row short of Play, dropping
 * the least useful one on a narrow card. There is always a picture under
 * all of it -- a screenshot, or else one of CoverArt's rendered scenes --
 * behind CoverArt's horizontal scrim, so the text, tags and icon tile
 * always use the media colours, in either theme.
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
    // source as InstanceCard's cover; CoverArt falls back to a rendered scene.
    required property string coverImage
    // Set by LaunchTask around the game process exit -- a calm reminder,
    // not an alarm.
    required property bool hasCrashed

    signal clicked()
    signal playRequested()
    signal stopRequested()

    readonly property bool hovered: hoverHandler.hovered
    readonly property string playTimeText: Format.playTime(root.totalTimePlayed)
    // The header's icon tile: Theme.icon.lg plus a thin frame around it.
    readonly property int iconTileSize: 32

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
            // Inside the card's 1px border, so the border stays visible.
            anchors.margins: card.border.width
            radius: card.radius
            source: root.coverImage
            tint: root.iconTint
            // No icon from CoverArt: its badge sits under the scrim. The
            // header below draws the icon over it instead.
            iconKey: ""
            seed: root.instanceId
            scrim: "horizontal"
            hovered: root.hovered
            // What is really behind the card's rounded corners: the page.
            // The card's own colour there showed as square nubs.
            matte: Theme.palette.canvas
        }

        // -- Header: icon tile, name, version ------------------------------
        Rectangle {
            id: iconTile
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Theme.space.lg
            width: root.iconTileSize
            height: root.iconTileSize
            radius: Theme.radius.sm
            color: Theme.media.chip
            border.width: 1
            border.color: Theme.media.chipBorder

            Image {
                anchors.centerIn: parent
                width: Theme.icon.lg
                height: Theme.icon.lg
                source: root.iconKey.length > 0 ? "image://instanceicon/" + root.iconKey : ""
                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
        }

        Column {
            anchors.left: iconTile.right
            anchors.leftMargin: Theme.space.sm
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.lg
            anchors.verticalCenter: iconTile.verticalCenter

            Text {
                width: parent.width
                text: root.name
                elide: Text.ElideRight
                color: Theme.media.text
                font.family: Theme.font.family
                font.pixelSize: Theme.type.bodyStrong.pixelSize + 1
                font.weight: Font.Bold
            }
            Text {
                width: parent.width
                visible: text.length > 0
                text: Format.versionLine(root.loader, root.gameVersion)
                elide: Text.ElideRight
                color: Theme.media.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }

        // -- Footer: facts, level with Play --------------------------------
        // Always one row, stopping short of Play. The card's width is set by
        // HomePage, so on a narrow card a fact that would not fit is dropped
        // rather than wrapped: last played always shows, then the crash
        // reminder if it fits, then play time (also on the instance page)
        // if there is still room. Fitting is judged on implicitWidth, which does not
        // depend on visibility, so the checks cannot feed back on themselves.
        Row {
            id: facts
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.lg
            anchors.verticalCenter: playButton.verticalCenter
            spacing: Theme.space.xs

            readonly property real room: playButton.x - Theme.space.sm - facts.x
            readonly property real crashWidth: facts.spacing + crashBadge.implicitWidth
            readonly property bool showCrash: root.hasCrashed
                && lastPlayedTag.implicitWidth + facts.crashWidth <= facts.room
            readonly property bool showPlayTime: root.playTimeText.length > 0
                && lastPlayedTag.implicitWidth + (facts.showCrash ? facts.crashWidth : 0)
                   + facts.spacing + playTimeTag.implicitWidth <= facts.room

            Tag {
                id: lastPlayedTag
                text: Format.lastPlayed(root.lastLaunch)
                onMedia: true
            }
            // "2 h ago" / "Yesterday" says "when" on its own; the clock goes
            // on the play time, which would otherwise read as a bare number.
            Tag {
                id: playTimeTag
                visible: facts.showPlayTime
                text: root.playTimeText
                iconName: "clock"
                onMedia: true
            }
            // The tags' height (its label centres itself), so it sits level
            // with them.
            StatusBadge {
                id: crashBadge
                visible: facts.showCrash
                height: lastPlayedTag.height
                tone: "danger"
                text: qsTr("Crashed last time")
            }
        }

        Button {
            id: playButton
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            // The same lg inset as the header, so all four corners match.
            anchors.margins: Theme.space.lg
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
