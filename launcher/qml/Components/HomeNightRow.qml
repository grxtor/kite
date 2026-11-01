// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * One "Continue playing" row of the Gece look: a 76 px glass pane on the night sky (HomeGlass), a 44 px tile
 * (HomeRowTile), the name, one line of what it runs, when and how long, and a bare Play glyph that fills with the
 * accent while hovered. Clicks work as on every Home row (HomeRowBase); a right-click or the menu key opens the
 * instance menu.
 */
HomeRowBase {
    id: root

    readonly property int tileSize: 44
    readonly property int playSize: 38
    readonly property int padding: Theme.space.lg
    readonly property int rowHeight: 76

    implicitHeight: root.rowHeight

    HomeGlass {
        id: pane
        anchors.fill: parent
        lit: root.hovered || root.selected
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

            Copy {
                Layout.fillWidth: true
                text: root.row.name
                role: "bodyStrong"
                elide: Text.ElideRight
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
