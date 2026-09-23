// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The hero at the top of the library: one instance, big, with Play as the
 * obvious next step. Which instance that is -- the selected one, the one
 * played last, or simply the first -- is the page's decision; `overline`
 * says which, so the user is never left guessing why this one is shown.
 */
Rectangle {
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
    // Newest screenshot url (InstanceList's coverImage role), or "" -- the
    // hero's own full-bleed backdrop, same source as InstanceCard's cover.
    required property string coverImage
    required property string launchStatus
    required property real launchProgress
    readonly property bool launching: launchStatus.length > 0
    readonly property bool hasPhoto: coverImage.length > 0

    property string overline: qsTr("Continue playing")
    property string editText: qsTr("Edit")
    // A shorter banner for pages where the content below matters more.
    property bool compact: false

    signal playRequested()
    signal stopRequested()
    signal editRequested()
    signal folderRequested()
    signal menuRequested()

    implicitHeight: Math.max(compact ? 184 : 240, content.implicitHeight + (compact ? Theme.space.xl : Theme.space.xxl) * 2)
    radius: Theme.radius.xl
    border.width: 1
    border.color: Theme.palette.border
    // Hidden behind the full-bleed CoverArt below, which also covers this
    // rectangle's own border -- see `outline` for the one that shows.
    color: Theme.palette.surface
    // CoverArt's hover zoom would otherwise poke past the rounded corner.
    clip: true

    CoverArt {
        id: art
        anchors.fill: parent
        radius: root.radius
        // The hero sits straight on the page.
        matte: Theme.palette.canvas
        source: root.coverImage
        tint: root.iconTint
        // The tile to the right already carries the icon; showing it again,
        // huge, in the backdrop would just be clutter.
        iconKey: ""
        scrim: "horizontal"
    }

    Row {
        id: content
        anchors.left: parent.left
        anchors.leftMargin: Theme.space.xxl
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.xxl
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space.xl

        Item {
            id: tile
            anchors.verticalCenter: parent.verticalCenter
            width: root.compact ? 96 : 124
            height: root.compact ? 96 : 124

            // A soft shadow: a larger, blurred-by-opacity copy of the tile
            // offset below it -- the layered-rectangle trick this codebase
            // uses in place of a drop-shadow effect. Needed now that the
            // tile can sit over a photo instead of always the flat surface
            // colour, so it still reads as raised.
            Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                anchors.topMargin: 1
                radius: Theme.radius.xl + 3
                color: Qt.rgba(0, 0, 0, Theme.dark ? 0.35 : 0.22)
            }

            Rectangle {
                anchors.fill: parent
                radius: Theme.radius.xl
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.08)
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Format.shade(root.iconTint, Theme.dark ? 0.34 : 0.84, 0.55) }
                    GradientStop { position: 1.0; color: Format.shade(root.iconTint, Theme.dark ? 0.18 : 0.72, 0.55) }
                }

                Image {
                    anchors.centerIn: parent
                    width: root.compact ? 68 : 88
                    height: width
                    source: root.iconKey.length > 0 ? "image://instanceicon/" + root.iconKey : ""
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                }
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - tile.width - parent.spacing
            spacing: Theme.space.sm

            Text {
                // Compact no longer blanks this outright: the page decides
                // what, if anything, `overline` says (see the file
                // comment), and an empty string already renders as nothing.
                visible: root.overline.length > 0
                text: root.overline.toUpperCase()
                color: Theme.palette.accent
                font.family: Theme.font.family
                font.pixelSize: Theme.type.overline.pixelSize
                font.weight: Font.Bold
                font.letterSpacing: Theme.type.overline.letterSpacing * 1.5
            }

            Text {
                width: parent.width
                text: root.name
                elide: Text.ElideRight
                // Over the photo's dark fade the text is light in both
                // themes; see Theme.media.
                color: root.hasPhoto ? Theme.media.text : Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: root.compact ? Theme.type.display.pixelSize - 4 : Theme.type.display.pixelSize + 2
                font.weight: Font.Bold
                font.letterSpacing: -0.5
            }

            Row {
                spacing: Theme.space.sm

                Tag {
                    text: root.loader.length > 0 ? root.loader : qsTr("Vanilla")
                    iconName: "layers"
                    onMedia: root.hasPhoto
                }
                Tag {
                    visible: root.gameVersion.length > 0
                    text: root.gameVersion
                    iconName: "cube"
                    onMedia: root.hasPhoto
                }
                Tag {
                    text: Format.lastPlayed(root.lastLaunch)
                    iconName: "clock"
                    onMedia: root.hasPhoto
                }
                Tag {
                    readonly property string played: Format.playTime(root.totalTimePlayed)
                    visible: played.length > 0
                    text: qsTr("%1 played").arg(played)
                    onMedia: root.hasPhoto
                }
            }

            Row {
                topPadding: Theme.space.sm
                spacing: Theme.space.sm

                PlayButton {
                    round: false
                    size: Theme.control.heightLg
                    running: root.isRunning
                    busy: root.launching
                    enabled: !root.launching && (root.isRunning || root.canLaunch)
                    onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
                }

                Button {
                    height: Theme.control.heightLg
                    text: root.editText
                    icon.source: Icons.url("settings")
                    onClicked: root.editRequested()
                }

                IconButton {
                    size: Theme.control.heightLg
                    flat: false
                    iconName: "folder"
                    tip: qsTr("Open folder")
                    onClicked: root.folderRequested()
                }

                IconButton {
                    size: Theme.control.heightLg
                    flat: false
                    iconName: "more"
                    tip: qsTr("More")
                    onClicked: root.menuRequested()
                }
            }

            Column {
                visible: root.launching
                width: Math.min(parent.width, 360)
                spacing: Theme.space.xs

                Text {
                    width: parent.width
                    text: root.launchProgress >= 0
                          ? qsTr("%1 · %2%").arg(root.launchStatus).arg(Math.round(root.launchProgress * 100))
                          : root.launchStatus
                    elide: Text.ElideRight
                    color: root.hasPhoto ? Theme.media.textSecondary : Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }

                LaunchProgressBar {
                    width: parent.width
                    progress: root.launchProgress
                }
            }
        }
    }

    // Drawn last: children paint over the root's own border, and the art
    // fills the whole card.
    Rectangle {
        id: outline
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: root.hasPhoto ? Theme.media.chipBorder : Theme.palette.border
    }
}
