// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * A row of cat pictures to choose from (Settings > Appearance): every pack
 * CatPackList finds, shown as a small picture with its name under it, the
 * chosen one outlined. The choice is the BackgroundCat setting.
 */
Flow {
    id: root

    property var source: SettingsStore
    property string key: "BackgroundCat"
    readonly property var packs: LauncherStore.catPacks ? LauncherStore.catPacks.packs : []
    // The pack in use; with none chosen yet (or a pack that is gone) the
    // first one, which is what the cat shows too.
    readonly property string current: {
        var chosen = root.source.string(root.key)
        for (var i = 0; i < root.packs.length; ++i)
            if (root.packs[i].id === chosen)
                return chosen
        return root.packs.length > 0 ? root.packs[0].id : ""
    }

    spacing: Theme.space.md

    Repeater {
        model: root.packs

        delegate: Item {
            id: tile
            required property var modelData
            readonly property bool selected: root.current === modelData.id

            width: 96
            height: swatch.height + caption.height + Theme.space.xs

            Rectangle {
                id: swatch
                width: parent.width
                height: 80
                radius: Theme.radius.md
                color: tile.selected ? Theme.palette.accentSubtle : Theme.palette.surface
                border.width: tile.selected ? 2 : 1
                border.color: tile.selected ? Theme.palette.accent : Theme.palette.border

                Image {
                    anchors.fill: parent
                    anchors.margins: Theme.space.xs
                    source: tile.modelData.source
                    sourceSize.height: 160
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    smooth: true
                    mipmap: true
                }
            }

            Text {
                id: caption
                anchors.top: swatch.bottom
                anchors.topMargin: Theme.space.xs
                width: parent.width
                text: tile.modelData.name
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                color: tile.selected ? Theme.palette.textPrimary : Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }

            TapHandler {
                onTapped: root.source.setValue(root.key, tile.modelData.id)
            }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            Accessible.role: Accessible.RadioButton
            Accessible.name: tile.modelData.name
            Accessible.checked: tile.selected
        }
    }
}
