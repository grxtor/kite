// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The hero at the top of the library: one instance, big, with Play as the
 * obvious next step. Which instance that is -- the selected one, the one
 * played last, or simply the first -- is the page's decision; `overline`
 * says which, so the user is never left guessing why this one is shown.
 */
Rectangle {
    id: root

    required property string instanceId
    required property string name
    required property string iconKey
    required property bool isRunning
    required property bool canLaunch
    required property var lastLaunch
    required property var totalTimePlayed
    required property string gameVersion
    required property string loader
    required property color iconTint
    required property string launchStatus
    required property real launchProgress
    readonly property bool launching: launchStatus.length > 0

    property string overline: qsTr("Continue playing")
    property string editText: qsTr("Edit")

    signal playRequested()
    signal stopRequested()
    signal editRequested()
    signal folderRequested()
    signal menuRequested()

    implicitHeight: Math.max(212, content.implicitHeight + Theme.space.xxl * 2)
    radius: Theme.radius.xl
    border.width: 1
    border.color: Theme.palette.border
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: Format.shade(root.iconTint, Theme.dark ? 0.22 : 0.88, 0.9) }
        GradientStop { position: 0.62; color: Theme.palette.surface }
        GradientStop { position: 1.0; color: Theme.palette.surface }
    }

    Row {
        id: content
        anchors.left: parent.left
        anchors.leftMargin: Theme.space.xxl
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.xxl
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space.xl

        Rectangle {
            id: tile
            anchors.verticalCenter: parent.verticalCenter
            width: 124
            height: 124
            radius: Theme.radius.xl
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.08)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Format.shade(root.iconTint, Theme.dark ? 0.34 : 0.84, 0.9) }
                GradientStop { position: 1.0; color: Format.shade(root.iconTint, Theme.dark ? 0.18 : 0.72, 0.8) }
            }

            Image {
                anchors.centerIn: parent
                width: 88
                height: 88
                source: root.iconKey.length > 0 ? "image://instanceicon/" + root.iconKey : ""
                sourceSize: Qt.size(88, 88)
                fillMode: Image.PreserveAspectFit
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - tile.width - parent.spacing
            spacing: Theme.space.sm

            Text {
                text: root.overline.toUpperCase()
                color: Theme.palette.accent
                font.family: Theme.font.family
                font.pixelSize: Theme.type.overline.pixelSize
                font.weight: Font.Bold
                font.letterSpacing: Theme.type.overline.letterSpacing * 1.5
            }

            Text {
                width: parent.width
                text: root.name
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.display.pixelSize + 2
                font.weight: Font.Bold
                font.letterSpacing: -0.5
            }

            Row {
                spacing: Theme.space.sm

                Tag {
                    text: root.loader.length > 0 ? root.loader : qsTr("Vanilla")
                    iconName: "layers"
                }
                Tag {
                    visible: root.gameVersion.length > 0
                    text: root.gameVersion
                    iconName: "cube"
                }
                Tag {
                    text: Format.lastPlayed(root.lastLaunch)
                    iconName: "clock"
                }
                Tag {
                    readonly property string played: Format.playTime(root.totalTimePlayed)
                    visible: played.length > 0
                    text: qsTr("%1 played").arg(played)
                }
            }

            Row {
                topPadding: Theme.space.sm
                spacing: Theme.space.sm

                PlayButton {
                    round: false
                    size: Theme.control.heightLg
                    running: root.isRunning
                    busy: root.launching
                    enabled: !root.launching && (root.isRunning || root.canLaunch)
                    onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
                }

                Button {
                    height: Theme.control.heightLg
                    text: root.editText
                    icon.source: Icons.url("settings")
                    onClicked: root.editRequested()
                }

                IconButton {
                    size: Theme.control.heightLg
                    flat: false
                    iconName: "folder"
                    tip: qsTr("Open folder")
                    onClicked: root.folderRequested()
                }

                IconButton {
                    size: Theme.control.heightLg
                    flat: false
                    iconName: "more"
                    tip: qsTr("More")
                    onClicked: root.menuRequested()
                }
            }

            Column {
                visible: root.launching
                width: Math.min(parent.width, 360)
                spacing: Theme.space.xs

                Text {
                    width: parent.width
                    text: root.launchProgress >= 0
                          ? qsTr("%1 · %2%").arg(root.launchStatus).arg(Math.round(root.launchProgress * 100))
                          : root.launchStatus
                    elide: Text.ElideRight
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }

                LaunchProgressBar {
                    width: parent.width
                    progress: root.launchProgress
                }
            }
        }
    }
}
