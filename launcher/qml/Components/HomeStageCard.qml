// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * One "Continue playing" card of the Sahne look: 156 px tall, rounded, on
 * the raised surface -- or, when the instance has a screenshot of its own,
 * over that screenshot under a dark fade at the foot. Top: what it runs as
 * a chip (or "Crashed last time" as a warning one) and how long it has been
 * played; bottom: the name and "Fabric 26.3 • 4 days ago". Under the
 * pointer a 40 px Play in the kite's cut appears at the bottom right (Stop
 * while it runs). Clicking opens the instance page; a right-click its menu.
 */
HomeRowBase {
    id: root

    readonly property int cardHeight: 156
    readonly property int padding: Theme.space.lg - 2
    readonly property int sidePadding: Theme.space.lg
    readonly property int playSize: Theme.control.height
    readonly property int nameSize: 16
    readonly property bool hasPhoto: root.row.coverImage.length > 0
    // Text over a screenshot takes the media colours.
    readonly property color ink: root.hasPhoto ? Theme.media.text : Theme.palette.textPrimary
    readonly property color inkSecondary: root.hasPhoto ? Theme.media.textSecondary : Theme.palette.textSecondary
    readonly property string loaderText: root.row.loader.length > 0 ? root.row.loader : qsTr("Vanilla")
    readonly property string playTimeText: SettingsStore.bool("ShowGameTime") && Number(root.row.totalTimePlayed) > 0
                                           ? Format.playTime(root.row.totalTimePlayed) : ""
    readonly property string subText: [Format.versionLine(root.row.loader, root.row.gameVersion),
                                       Format.lastPlayed(root.row.lastLaunch)]
                                      .filter((part) => part.length > 0).join("  •  ")

    implicitHeight: root.cardHeight

    // A chip: a small rounded label, with a warning icon when it is one.
    component Chip: Rectangle {
        property string text
        property color ink: Theme.media.textSecondary
        property string iconName

        implicitWidth: chipRow.implicitWidth + Theme.space.sm * 2
        implicitHeight: chipRow.implicitHeight + Theme.space.xs * 2 - 1
        radius: Theme.radius.sm - 1

        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: Theme.space.xs + 1

            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: parent.parent.iconName.length > 0
                iconName: parent.parent.iconName
                size: Theme.icon.sm - 2
                color: parent.parent.ink
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: parent.parent.text
                color: parent.parent.ink
                font.family: Theme.font.family
                font.pixelSize: Theme.type.overline.pixelSize
                font.weight: Font.DemiBold
            }
        }
    }

    Rectangle {
        id: plate
        anchors.fill: parent
        radius: Theme.radius.lg
        color: root.hovered ? Theme.palette.surfaceHover : Theme.palette.surface
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    // Its newest screenshot, with the fade the words sit on.
    CoverArt {
        anchors.fill: parent
        visible: root.hasPhoto
        source: root.row.coverImage
        radius: Theme.radius.lg
        scrim: "bottom"
        showLogo: false
        hovered: root.hovered
        matte: Theme.palette.canvas
    }

    // The hairline a hovered card gets, over the picture.
    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.lg
        color: "transparent"
        border.width: root.hovered ? 1 : 0
        border.color: Qt.rgba(1, 1, 1, 0.08)
    }

    Item {
        anchors.fill: parent
        anchors.topMargin: root.padding
        anchors.bottomMargin: root.padding
        anchors.leftMargin: root.sidePadding
        anchors.rightMargin: root.sidePadding

        Row {
            id: top
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Theme.space.sm

            Chip {
                id: chip
                text: root.row.hasCrashed ? qsTr("Crashed last time") : root.loaderText
                iconName: root.row.hasCrashed ? "alert-triangle" : ""
                ink: root.row.hasCrashed ? Theme.palette.warning : Theme.media.textSecondary
                color: root.row.hasCrashed ? Theme.palette.warningSubtle
                     : root.hasPhoto ? Theme.media.chip : Qt.rgba(1, 1, 1, 0.08)
            }
            Item {
                width: parent.width - chip.width - parent.spacing
                height: chip.height
                visible: root.playTimeText.length > 0

                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.xs

                    MeshIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: "hourglass"
                        size: Theme.icon.sm - 2
                        color: root.inkSecondary
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.playTimeText
                        color: root.inkSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.overline.pixelSize
                        font.features: ({ "tnum": 1 })
                    }
                }
            }
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: root.playSize + Theme.space.sm
            anchors.bottom: parent.bottom
            spacing: Theme.space.xxs + 1

            Text {
                width: parent.width
                text: root.row.name
                elide: Text.ElideRight
                color: root.ink
                font.family: Theme.font.family
                font.pixelSize: root.nameSize
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                text: root.subText
                elide: Text.ElideRight
                color: root.inkSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }

        HomeRowPlay {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            size: root.playSize
            iconSize: Theme.icon.sm
            running: root.running
            lit: true
            enabled: root.playable
            opacity: root.hovered || root.running ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            onClicked: root.togglePlay()
        }
    }
}
