// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One modpack in Discover's results: logo, name and author, the pitch, and
 * how popular it is. The whole row opens the pack; nothing on it installs
 * directly, since installing means picking a version first.
 */
AbstractButton {
    id: root

    required property string projectId
    required property string title
    required property string author
    required property string description
    required property string logoUrl
    required property var downloads

    implicitHeight: 112
    hoverEnabled: true

    Accessible.name: title
    Accessible.description: description

    background: Rectangle {
        radius: Theme.radius.lg
        color: root.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
        border.width: 1
        border.color: root.hovered ? Theme.palette.borderStrong : Theme.palette.border
        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.palette.focusRing
            visible: root.visualFocus
        }
    }

    contentItem: Item {
        Rectangle {
            id: logoFrame
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.md
            anchors.verticalCenter: parent.verticalCenter
            width: 80
            height: 80
            radius: Theme.radius.lg
            color: Theme.palette.surfaceSunken

            Image {
                id: logo
                anchors.fill: parent
                anchors.margins: 1
                source: root.logoUrl
                sourceSize: Qt.size(160, 160)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: status === Image.Ready
            }

            MeshIcon {
                anchors.centerIn: parent
                visible: !logo.visible
                iconName: "package"
                size: Theme.icon.lg
                color: Theme.palette.textTertiary
            }
        }

        Column {
            anchors.left: logoFrame.right
            anchors.leftMargin: Theme.space.lg
            anchors.right: stats.left
            anchors.rightMargin: Theme.space.lg
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.xs

            Row {
                width: parent.width
                spacing: Theme.space.sm

                Text {
                    id: titleText
                    width: Math.min(implicitWidth, parent.width - byline.width - parent.spacing)
                    text: root.title
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }

                Text {
                    id: byline
                    anchors.baseline: titleText.baseline
                    visible: root.author.length > 0
                    text: qsTr("by %1").arg(root.author)
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }

            Text {
                width: parent.width
                text: root.description
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                lineHeight: 1.25
            }
        }

        Column {
            id: stats
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.lg
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Row {
                anchors.right: parent.right
                spacing: Theme.space.xs
                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "download"
                    size: Theme.icon.sm
                    color: Theme.palette.textSecondary
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Format.compactNumber(root.downloads)
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    font.weight: Font.DemiBold
                }
            }
            Text {
                anchors.right: parent.right
                text: qsTr("downloads")
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }
    }
}
