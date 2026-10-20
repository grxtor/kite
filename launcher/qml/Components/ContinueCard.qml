// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One instance as a banner, with Play as the obvious next step: its newest
 * screenshot (or pack art, or a plain surface -- see CoverArt.qml) behind
 * its icon tile, name, facts and actions. No caps label over the name: the
 * name and its tags say what it is.
 *
 * Only the design gallery (Gallery.qml) shows it now: the library leads
 * with the play bar instead, the instance page has its own header, and
 * Home puts the played-last instance in a header of its own.
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
    // banner's own full-bleed backdrop.
    required property string coverImage
    // A platform pack's own logo (InstanceList's packArt role), or "": the
    // backdrop shows the pack's art when there is no screenshot (CoverArt).
    // Not required, so a static review page need not pass it.
    property string packArt: ""
    required property string launchStatus
    required property real launchProgress
    readonly property bool launching: launchStatus.length > 0
    // A picture under the text (screenshot or pack art): media colours.
    readonly property bool hasPhoto: coverImage.length > 0 || art.hasPackArt

    readonly property int minHeight: 240
    readonly property int tileSize: 124
    readonly property int tileIconSize: 88
    // The launch line's longest reading width.
    readonly property int launchWidth: 360

    signal playRequested()
    signal stopRequested()
    signal editRequested()
    signal folderRequested()
    signal menuRequested()

    implicitHeight: Math.max(root.minHeight, content.implicitHeight + Theme.space.xxl * 2)
    radius: Theme.radius.xl
    // Hidden behind the full-bleed CoverArt below; `outline` draws the edge.
    color: Theme.palette.surface
    // CoverArt's hover zoom would otherwise poke past the rounded corner.
    clip: true

    CoverArt {
        id: art
        anchors.fill: parent
        radius: root.radius
        // The banner sits straight on the page.
        matte: Theme.palette.canvas
        source: root.coverImage
        packArt: root.packArt
        // The tile already carries the icon.
        showLogo: false
        // The plain surface carries the page's own text colours, so it
        // needs no dark fade.
        scrim: root.hasPhoto ? "horizontal" : "none"
    }

    Row {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.space.xxl
        anchors.rightMargin: Theme.space.xxl
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space.xl

        // The instance's identity on the same neutral tile its library
        // card uses.
        PlaceholderSurface {
            id: tile
            anchors.verticalCenter: parent.verticalCenter
            width: root.tileSize
            height: width
            radius: Theme.radius.lg

            Image {
                anchors.centerIn: parent
                width: root.tileIconSize
                height: width
                source: Icons.instanceIcon(root.iconKey)
                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - tile.width - parent.spacing
            spacing: Theme.space.sm

            Text {
                width: parent.width
                text: root.name
                elide: Text.ElideRight
                // Over the photo's dark fade the text is light in both
                // themes; see Theme.media.
                color: root.hasPhoto ? Theme.media.text : Theme.palette.textPrimary
                font.family: Theme.font.heading
                font.pixelSize: Theme.type.display.pixelSize
                font.weight: Theme.type.display.weight
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
                // Clock = when it was last played, hourglass = for how
                // long in all; the ▶ triangle only ever means Play.
                Tag {
                    text: Format.lastPlayed(root.lastLaunch)
                    iconName: "clock"
                    onMedia: root.hasPhoto
                }
                Tag {
                    // Hidden while "Show play time per instance" is off.
                    readonly property string played: SettingsStore.bool("ShowGameTime")
                                                     ? Format.playTime(root.totalTimePlayed) : ""
                    visible: played.length > 0
                    text: qsTr("%1 played").arg(played)
                    iconName: "hourglass"
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
                    buttonSize: "lg"
                    text: qsTr("Edit")
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
                    Accessible.role: Accessible.ButtonMenu
                    onClicked: root.menuRequested()
                }
            }

            Column {
                visible: root.launching
                width: Math.min(parent.width, root.launchWidth)
                spacing: Theme.space.xs

                Text {
                    width: parent.width
                    text: root.launchProgress >= 0
                          ? qsTr("%1, %2%").arg(root.launchStatus).arg(Math.round(root.launchProgress * 100))
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
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: root.hasPhoto ? Theme.media.chipBorder : Theme.palette.border
    }
}
