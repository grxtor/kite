// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * One "Continue playing" card of the Sahne look: 156 px tall, on the raised surface or over the instance's
 * cover under a dark fade (a wash of its icon's colour without one). "Crashed last time" (only then) and play time at the top, as plain text with no
 * chip; name and "Fabric 26.3 • 4 days ago" at the bottom; a 40 px Play appears under the pointer.
 */
HomeRowBase {
    id: root

    readonly property int cardHeight: 156
    readonly property int padding: Theme.space.lg - 2
    readonly property int sidePadding: Theme.space.lg
    readonly property int playSize: Theme.control.height
    readonly property bool hasPhoto: root.row.coverImage.length > 0
    readonly property color tint: Icons.tint(root.row.iconKey, root.row.instanceId)
    readonly property bool hasTint: root.tint.valid && root.tint.a > 0
    readonly property color ink: root.hasPhoto ? Theme.media.text : Theme.palette.textPrimary
    readonly property color inkSecondary: root.hasPhoto ? Theme.media.textSecondary : Theme.palette.textSecondary
    readonly property string playTimeText: SettingsStore.bool("ShowGameTime") && Number(root.row.totalTimePlayed) > 0
                                           ? Format.playTime(root.row.totalTimePlayed) : ""
    readonly property string subText: [Format.versionLine(root.row.loader, root.row.gameVersion),
                                       Format.lastPlayed(root.row.lastLaunch)]
                                      .filter((part) => part.length > 0).join("  •  ")

    implicitHeight: root.cardHeight

    component Note: Item {
        property string text
        property color ink: Theme.media.textSecondary
        property string iconName

        implicitWidth: noteRow.implicitWidth
        implicitHeight: noteRow.implicitHeight + Theme.space.xs * 2 - 1

        Row {
            id: noteRow
            anchors.centerIn: parent
            spacing: Theme.space.xs + 1

            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: parent.parent.iconName.length > 0
                iconName: parent.parent.iconName
                size: Theme.icon.xs
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

    StateFill {
        id: plate
        anchors.fill: parent
        card: true
        hovered: root.hovered
        active: root.selected
        radius: Theme.radius.lg
    }

    CoverArt {
        anchors.fill: parent
        visible: root.hasPhoto || root.hasTint
        source: root.row.coverImage
        kind: root.row.coverKind
        tint: root.tint
        radius: Theme.radius.lg
        scrim: root.hasPhoto ? "bottom" : "none"
        showLogo: false
        hovered: root.hovered
        matte: Theme.palette.canvas
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.lg
        color: "transparent"
        border.width: root.hovered || root.selected ? 1 : 0
        border.color: root.selected ? Theme.palette.accent : Theme.media.glassEdge
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

            Note {
                id: note
                visible: root.row.hasCrashed
                text: qsTr("Crashed last time")
                iconName: "alert-triangle"
                ink: Theme.palette.warning
            }
            Item {
                width: parent.width - (note.visible ? note.width + parent.spacing : 0)
                height: note.height
                visible: root.playTimeText.length > 0

                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.xs

                    MeshIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: "hourglass"
                        size: Theme.icon.xs
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

            Copy {
                width: parent.width
                text: root.row.name
                role: "cardName"
                elide: Text.ElideRight
                color: root.ink
            }
            Copy {
                width: parent.width
                text: root.subText
                role: "caption"
                elide: Text.ElideRight
                color: root.inkSecondary
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
