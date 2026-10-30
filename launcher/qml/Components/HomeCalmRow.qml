// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * One "Continue playing" row of the Sade look: no box, no fill -- a 64 px
 * line on the page with a hairline under it. A 38 px tile (HomeRowTile),
 * the name and what it runs, how long
 * it has been played, a quiet Play that takes the accent while the row is
 * hovered, and ⋯. The hovered row gets a soft fill that bleeds `bleed`
 * past the column on both sides, and loses its hairline. Clicking opens the
 * instance page; a right-click or ⋯ its menu.
 */
HomeRowBase {
    id: root

    // The last row has no hairline under it.
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

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: -root.bleed
        anchors.rightMargin: -root.bleed
        radius: Theme.radius.md
        color: root.hovered ? Theme.palette.hoverOverlay : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: -root.bleed
        anchors.rightMargin: -root.bleed
        height: 1
        color: Theme.palette.divider
        visible: root.showDivider && !root.hovered
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

            Text {
                Layout.fillWidth: true
                text: root.row.name
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.bodyStrong.pixelSize
                font.weight: Theme.type.bodyStrong.weight
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
                    size: Theme.icon.sm - 2
                    color: Theme.palette.textTertiary
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.playTimeText
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
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
