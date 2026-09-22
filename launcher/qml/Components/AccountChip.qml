// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * The account entry pinned to the bottom of SidebarNav. Falls back to
 * initials when there is no avatar image, so an account with no skin/avatar
 * fetched yet is never a blank circle.
 */
Item {
    id: root

    property string name: ""
    property string status: ""
    property string avatarSource: ""
    // Mirrors SidebarNav's own collapsed state, so the chip shrinks to just
    // the avatar at the same width threshold as the nav items above it.
    property bool collapsed: false

    signal clicked()

    readonly property string initials: {
        var parts = root.name.trim().split(/\s+/).filter(function (part) { return part.length > 0 })
        if (parts.length === 0)
            return "?"
        if (parts.length === 1)
            return parts[0].charAt(0).toUpperCase()
        return (parts[0].charAt(0) + parts[parts.length - 1].charAt(0)).toUpperCase()
    }

    implicitHeight: Theme.control.height
    implicitWidth: collapsed ? implicitHeight : 200

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.md
        color: mouseArea.containsMouse ? Theme.palette.hoverOverlay : "transparent"

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: Theme.space.sm
        spacing: Theme.space.sm

        Rectangle {
            id: avatar
            width: Theme.control.heightSm
            height: Theme.control.heightSm
            radius: width / 2
            color: Theme.palette.accentSubtle
            clip: true

            Image {
                anchors.fill: parent
                visible: root.avatarSource.length > 0
                source: root.avatarSource
                sourceSize: Qt.size(avatar.width, avatar.height)
                fillMode: Image.PreserveAspectCrop
            }

            Text {
                anchors.centerIn: parent
                visible: root.avatarSource.length === 0
                text: root.initials
                color: Theme.palette.accentText
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.weight: Theme.type.bodyStrong.weight
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            visible: !root.collapsed
            spacing: 0

            Text {
                text: root.name
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.weight: Theme.type.label.weight
                elide: Text.ElideRight
            }

            Text {
                text: root.status
                visible: text.length > 0
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
                elide: Text.ElideRight
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
