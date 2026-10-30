// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The line under a "Continue playing" row's name in the Sade and Gece
 * looks: "Fabric 26.3 • 4 days ago", or -- after a crash -- what it runs and
 * the crash in the warning colour in place of when. Gece also puts how
 * long it has been played on the line (`inlineTime`); Sade keeps that in a
 * column of its own. The first part gives way (elides) before the rest.
 */
Item {
    id: root

    // One of HomeContinuePlaying's rows.
    required property var row
    property int size: Theme.type.caption.pixelSize
    property bool inlineTime: false
    // "Crashed last time", or a short "Crashed" where there is less room.
    property string crashText: qsTr("Crashed last time")

    readonly property string versionText: Format.versionLine(root.row.loader, root.row.gameVersion)
    readonly property string playTimeText: root.inlineTime && SettingsStore.bool("ShowGameTime")
                                           && Number(root.row.totalTimePlayed) > 0
                                           ? Format.playTime(root.row.totalTimePlayed) : ""
    // What the first (elidable) part says: the version, and when unless it
    // crashed; Gece's time follows a crash, so only a clean row's time is
    // joined here.
    readonly property string leading: root.row.hasCrashed ? root.versionText
        : [root.versionText, Format.lastPlayed(root.row.lastLaunch), root.playTimeText]
          .filter((part) => part.length > 0).join("  •  ")
    readonly property string trailing: root.row.hasCrashed && root.playTimeText.length > 0
                                       ? "•  " + root.playTimeText : ""

    implicitHeight: Math.ceil(first.implicitHeight)
    clip: true

    Row {
        spacing: Theme.space.xs

        Text {
            id: first
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, root.width - (rest.visible ? rest.width + parent.spacing : 0))
            text: root.leading
            elide: Text.ElideRight
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: root.size
        }
        Row {
            id: rest
            anchors.verticalCenter: parent.verticalCenter
            visible: root.row.hasCrashed
            spacing: Theme.space.xs

            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "alert-triangle"
                size: root.size
                color: Theme.palette.warning
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.crashText
                color: Theme.palette.warning
                font.family: Theme.font.family
                font.pixelSize: root.size
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.trailing.length > 0
                text: root.trailing
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: root.size
            }
        }
    }
}
