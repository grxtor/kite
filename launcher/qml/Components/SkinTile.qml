// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One skin in the skin editor's library (SkinCapeEditor.qml): its flat
 * front-and-back sprite, its name under it (and as a tooltip, for a name
 * too long to show whole), and a mark on the one the account wears. Used
 * for the user's own skins and for the game's default ones alike.
 *
 * Click selects it. `menuEnabled` tiles also open actions: right click, or
 * the ⋯ that shows on hover (over the sprite, so only while pointed at).
 * A tile whose texture has not downloaded yet (`ready` false) shows a
 * loading block instead of the sprite and cannot be picked.
 */
AbstractButton {
    id: tile

    property string name: ""
    property string spriteUrl: ""
    property bool selected: false
    property bool worn: false
    property bool ready: true
    property bool menuEnabled: false
    property int spriteSize: 72
    signal menuRequested()

    implicitWidth: 96
    implicitHeight: 120
    enabled: tile.ready
    hoverEnabled: true
    Accessible.role: Accessible.RadioButton
    Accessible.name: tile.name
    Accessible.checked: tile.selected

    ToolTip.visible: tile.hovered && tile.name.length > 0
    ToolTip.delay: 500
    ToolTip.text: tile.name

    background: Rectangle {
        radius: Theme.radius.md
        color: tile.selected ? Theme.palette.accentSubtle
             : tile.hovered ? Theme.palette.hoverOverlay : "transparent"
        border.width: 1
        border.color: tile.selected ? Theme.palette.accent : Theme.palette.border
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }
    contentItem: Item {
        Image {
            id: sprite
            anchors.top: parent.top
            anchors.topMargin: Theme.space.sm
            anchors.horizontalCenter: parent.horizontalCenter
            width: tile.spriteSize
            height: tile.spriteSize
            visible: tile.ready
            source: tile.ready ? tile.spriteUrl : ""
            sourceSize: Qt.size(tile.spriteSize, tile.spriteSize)
            smooth: false
            fillMode: Image.PreserveAspectFit
        }
        Skeleton {
            anchors.fill: sprite
            visible: !tile.ready
        }
        Text {
            anchors.top: sprite.bottom
            anchors.topMargin: Theme.space.xs
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Theme.space.xs
            horizontalAlignment: Text.AlignHCenter
            text: tile.name
            elide: Text.ElideRight
            color: tile.ready ? Theme.palette.textPrimary : Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
        }
        MeshIcon {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: Theme.space.xs
            visible: tile.worn
            iconName: "check"
            size: Theme.icon.sm
            color: Theme.palette.accent
        }
        IconButton {
            id: moreButton
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: Theme.space.xxs
            size: Theme.control.heightSm
            visible: tile.menuEnabled && (tile.hovered || moreButton.hovered)
            iconName: "more"
            tip: qsTr("More")
            onClicked: tile.menuRequested()
        }
    }

    TapHandler {
        enabled: tile.menuEnabled
        acceptedButtons: Qt.RightButton
        onTapped: tile.menuRequested()
    }
}
