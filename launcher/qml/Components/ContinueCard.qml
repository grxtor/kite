// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The hero at the top of the library: one instance, big, with Play as the
 * obvious next step. Which instance that is -- the selected one, the one
 * played last, or simply the first -- is the page's decision. No caps
 * label over the name: the name and its tags say what it is.
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
    // A platform pack's own logo (InstanceList's packArt role), or "": the
    // backdrop then shows the pack's art (its colours as a wash) ahead of
    // any screenshot, as Library and Home do (CoverArt). Not required, so a
    // static review page need not pass it.
    property string packArt: ""
    required property string launchStatus
    required property real launchProgress
    readonly property bool launching: launchStatus.length > 0
    // A picture under the text (screenshot or pack art): media colours.
    readonly property bool hasPhoto: coverImage.length > 0 || art.hasPackArt

    property string editText: qsTr("Edit")
    // A shorter banner for pages where the content below matters more.
    property bool compact: false
    // False on the instance page: the persistent play bar plays and stops
    // the opened instance now, so its own header no longer needs a second
    // Play/Stop control.
    property bool showPlay: true
    // False on the instance page: its own tabs (Servers, Backups, Worlds,
    // Managed pack, ...) now cover everything the widget "Classic editor"
    // opened, so that button is gone there -- see InstancePage.qml.
    property bool showEdit: true
    // The folder button; off where the folder already sits in `menu`.
    property bool showFolder: true
    // The ⋯ button. With `menu` set it opens that menu right under itself
    // (the instance page's own actions); otherwise it emits menuRequested().
    property bool showMenu: true
    property Menu menu: null
    // The ⋯ button, which lives in a loaded actions row.
    property Item menuAnchor: null
    // Right-aligned under the ⋯ button, like any ⋯ menu, and held there:
    // entries that show or hide as the menu opens change its width after
    // it is placed, which used to push its right edge past the button.
    function openMenu() {
        const anchor = root.menuAnchor
        if (!root.menu || !anchor)
            return
        root.menu.popup(anchor, anchor.width - root.menu.width, anchor.height + Theme.space.xs)
        root.menu.x = Qt.binding(() => anchor.width - root.menu.width)
    }
    // A one-line strip: title and tags beside the icon, actions on the
    // right -- for pages whose content needs the height (the instance
    // page's header).
    property bool slim: false
    // Without Play there is no primary action to lead the row under the
    // title, so the remaining buttons sit on the right instead of trailing
    // the tags on a line of their own.
    readonly property bool actionsBeside: root.slim || !root.showPlay

    // Sizes of the banner and its icon tile in each of the three modes.
    readonly property int minHeight: compact ? 184 : 240
    readonly property int tileSize: slim ? 56 : compact ? 96 : 124
    readonly property int tileIconSize: slim ? 40 : compact ? 68 : 88
    // How far the tile's soft shadow spreads past the tile, and how far it
    // drops below it.
    readonly property int shadowSpread: 3
    readonly property int shadowDrop: 1

    signal playRequested()
    signal stopRequested()
    signal editRequested()
    signal folderRequested()
    signal menuRequested()

    implicitHeight: slim ? content.implicitHeight + Theme.space.lg * 2
                         : Math.max(root.minHeight, content.implicitHeight + (compact ? Theme.space.xl : Theme.space.xxl) * 2)
    Behavior on implicitHeight { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
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
        packArt: root.packArt
        tint: root.iconTint
        // The tile to the right already carries the icon; showing it again,
        // huge, in the backdrop would just be clutter.
        iconKey: ""
        showLogo: false
        // Same id the instance's card and play-bar strip hash, so a
        // screenshot-less instance shows the same fallback scene everywhere.
        seed: root.instanceId
        scrim: "horizontal"
    }

    Row {
        id: content
        anchors.left: parent.left
        anchors.leftMargin: root.slim ? Theme.space.lg : Theme.space.xxl
        anchors.right: parent.right
        anchors.rightMargin: root.actionsBeside ? besideActions.width + Theme.space.lg * 2 : Theme.space.xxl
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.slim ? Theme.space.md : Theme.space.xl

        Item {
            id: tile
            anchors.verticalCenter: parent.verticalCenter
            width: root.tileSize
            height: width

            // A soft shadow: a larger, blurred-by-opacity copy of the tile
            // offset below it -- the layered-rectangle trick this codebase
            // uses in place of a drop-shadow effect. Needed now that the
            // tile can sit over a photo instead of always the flat surface
            // colour, so it still reads as raised.
            Rectangle {
                anchors.fill: parent
                anchors.margins: -root.shadowSpread
                anchors.topMargin: root.shadowDrop
                radius: Theme.radius.lg
                color: Qt.rgba(0, 0, 0, Theme.dark ? 0.35 : 0.22)
            }

            // The instance's identity, so a card corner (lg), never pill or
            // hero-sized.
            Rectangle {
                anchors.fill: parent
                radius: Theme.radius.lg
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.08)
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Format.shade(root.iconTint, Theme.dark ? 0.34 : 0.84, 0.55) }
                    GradientStop { position: 1.0; color: Format.shade(root.iconTint, Theme.dark ? 0.18 : 0.72, 0.55) }
                }

                Image {
                    anchors.centerIn: parent
                    width: root.tileIconSize
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
            spacing: root.slim ? Theme.space.xs : Theme.space.sm

            Text {
                width: parent.width
                text: root.name
                elide: Text.ElideRight
                // Over the photo's dark fade the text is light in both
                // themes; see Theme.media.
                color: root.hasPhoto ? Theme.media.text : Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: root.slim ? Theme.type.heading.pixelSize : Theme.type.display.pixelSize
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

            // Zero height, but the column's spacing around it doubles the
            // gap between the tags and the buttons.
            Item { width: 1; height: 0; visible: !root.actionsBeside }
            Loader {
                active: !root.actionsBeside
                visible: active
                sourceComponent: actionsComponent
            }

            Column {
                visible: root.launching
                width: Math.min(parent.width, 360)
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

    // The actions, below the title when Play leads them, on the right
    // otherwise (see actionsBeside).
    Component {
        id: actionsComponent
        Row {
            spacing: Theme.space.sm

            PlayButton {
                visible: root.showPlay
                round: false
                size: Theme.control.heightLg
                running: root.isRunning
                busy: root.launching
                enabled: !root.launching && (root.isRunning || root.canLaunch)
                onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
            }

            Button {
                visible: root.showEdit
                height: Theme.control.heightLg
                text: root.editText
                icon.source: Icons.url("settings")
                onClicked: root.editRequested()
            }

            IconButton {
                visible: root.showFolder
                size: Theme.control.heightLg
                flat: false
                iconName: "folder"
                tip: qsTr("Open folder")
                onClicked: root.folderRequested()
            }

            IconButton {
                id: menuButton
                Component.onCompleted: root.menuAnchor = menuButton
                Component.onDestruction: if (root.menuAnchor === menuButton) root.menuAnchor = null
                visible: root.showMenu
                size: root.slim ? Theme.control.height : Theme.control.heightLg
                flat: false
                iconName: "more"
                tip: qsTr("More")
                Accessible.role: Accessible.ButtonMenu
                onClicked: {
                    if (!root.menu) {
                        root.menuRequested()
                    } else if (root.menu.visible) {
                        root.menu.close()
                    } else {
                        root.openMenu()
                    }
                }
            }
        }
    }

    Loader {
        id: besideActions
        active: root.actionsBeside
        visible: active
        anchors.right: parent.right
        anchors.rightMargin: root.slim ? Theme.space.lg : Theme.space.xl
        anchors.verticalCenter: parent.verticalCenter
        sourceComponent: actionsComponent
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
