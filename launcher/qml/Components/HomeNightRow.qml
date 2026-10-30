// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * One "Continue playing" row of the Gece look: a 76 px glass pane on the
 * night sky (HomeGlass), a 44 px tile (HomeRowTile), the name, one line of
 * what it runs, when and how long, and Play: a bare glyph that fills with
 * the accent while the row is hovered, never a box of its own at rest. The
 * row lights a step under the pointer. Clicking opens the instance page; a
 * right-click (or the menu key) opens the instance menu.
 */
HomeRowBase {
    id: root

    readonly property int tileSize: 44
    readonly property int playSize: 38
    readonly property int padding: Theme.space.lg

    implicitHeight: 76

    HomeGlass {
        id: pane
        anchors.fill: parent
        lit: root.hovered
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.padding
        anchors.rightMargin: root.padding
        spacing: Theme.space.md + 2

        HomeRowTile {
            Layout.preferredWidth: root.tileSize
            Layout.preferredHeight: root.tileSize
            row: root.row
            radius: Theme.radius.md
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space.xxs + 1

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
                inlineTime: true
                crashText: qsTr("Crashed")
            }
        }

        HomeRowPlay {
            Layout.preferredWidth: root.playSize
            Layout.preferredHeight: root.playSize
            size: root.playSize
            running: root.running
            lit: root.hovered
            enabled: root.playable
            restInk: Theme.palette.textSecondary
            onClicked: root.togglePlay()
        }
    }
}
