// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * One "Continue playing" row of the Sade look: no box, a 64 px line on the page with a hairline under it. A 38 px
 * tile (HomeRowTile), name and what it runs, play time, a quiet Play that takes the accent while hovered, and ⋯.
 * The hovered row gets a soft fill that bleeds `bleed` past the column and loses its hairline.
 */
HomeRowBase {
    id: root

    property bool showDivider: true
    readonly property int bleed: Theme.space.md
    readonly property int tileSize: 38
    readonly property int timeWidth: 96
    // Below this the time gives way to the name.
    readonly property int timeMinWidth: 520
    readonly property string playTimeText: SettingsStore.bool("ShowGameTime") && Number(root.row.totalTimePlayed) > 0
                                           ? Format.playTime(root.row.totalTimePlayed) : ""

    implicitHeight: Theme.control.heightLg + Theme.space.lg
    cornerRadius: Theme.radius.md

    StateFill {
        anchors.fill: parent
        anchors.leftMargin: -root.bleed
        anchors.rightMargin: -root.bleed
        radius: Theme.radius.md
        hovered: root.hovered
        active: root.selected
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: -root.bleed
        anchors.rightMargin: -root.bleed
        height: 1
        color: Theme.palette.divider
        visible: root.showDivider && !root.hovered && !root.selected
    }

    RowLayout {
        anchors.fill: parent
        spacing: Theme.space.md + 2

        HomeRowTile {
            Layout.preferredWidth: root.tileSize
            Layout.preferredHeight: root.tileSize
            row: root.row
            radius: Theme.radius.md - 2
            surround: Theme.palette.canvas
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space.xxs

            Copy {
                Layout.fillWidth: true
                text: root.row.name
                role: "bodyStrong"
                elide: Text.ElideRight
            }
            HomeRowMeta {
                Layout.fillWidth: true
                row: root.row
            }
        }

        Item {
            Layout.preferredWidth: root.timeWidth
            Layout.preferredHeight: timeRow.implicitHeight
            visible: root.playTimeText.length > 0 && root.width >= root.timeMinWidth

            Row {
                id: timeRow
                anchors.right: parent.right
                spacing: Theme.space.sm - 2

                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "hourglass"
                    size: Theme.icon.xs
                    color: Theme.palette.textTertiary
                }
                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.playTimeText
                    role: "caption"
                    tone: "tertiary"
                    font.features: ({ "tnum": 1 })
                }
            }
        }

        HomeRowPlay {
            Layout.preferredWidth: Theme.control.heightSm + Theme.space.xs
            running: root.running
            lit: root.hovered
            enabled: root.playable
            onClicked: root.togglePlay()
        }

        IconButton {
            id: moreButton
            Layout.preferredWidth: Theme.space.xl + Theme.space.xs
            Layout.preferredHeight: Theme.space.xl + Theme.space.xs
            size: Theme.space.xl + Theme.space.xs
            iconName: "more"
            tip: qsTr("More")
            focusPolicy: Qt.NoFocus
            onClicked: root.openMenu(moreButton)
        }
    }
}
