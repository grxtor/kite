// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * "Total play time: 38 h 20 min": one quiet line for a page header, the
 * QML counterpart of the classic window's status-bar total. Shown only
 * while Settings > Minecraft > "Show total play time" is on and something
 * has been played, so a page can drop it in without checking either.
 */
Text {
    id: root

    readonly property string total: LauncherStore.playTime
                                    ? Format.playTime(LauncherStore.playTime.totalSeconds) : ""

    visible: SettingsStore.bool("ShowGlobalGameTime") && total.length > 0
    text: qsTr("Total play time: %1").arg(total)
    color: Theme.palette.textTertiary
    font.family: Theme.font.family
    font.pixelSize: Theme.type.caption.pixelSize
}
