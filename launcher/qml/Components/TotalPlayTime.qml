// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * "179 h 41 min played" behind the hourglass (the play-time glyph
 * everywhere): the QML counterpart of the classic window's status-bar total,
 * as a quiet fact in the Library's title row (TopBar's showPlayTime). Shown
 * only while Settings > Minecraft > "Show total play time" is on and
 * something has been played, so a page can drop it in without checking
 * either.
 */
Row {
    id: root

    // Whether the page wants it at all (TopBar shows it on the Library).
    property bool wanted: true
    readonly property string total: LauncherStore.playTime
                                    ? Format.playTime(LauncherStore.playTime.totalSeconds) : ""

    visible: root.wanted && SettingsStore.bool("ShowGlobalGameTime") && total.length > 0
    spacing: Theme.space.xs

    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("Total play time: %1").arg(root.total)

    MeshIcon {
        anchors.verticalCenter: parent.verticalCenter
        iconName: "hourglass"
        size: Theme.icon.sm
        color: Theme.palette.textTertiary
    }
    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("%1 played").arg(root.total)
        color: Theme.palette.textTertiary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
    }
}
