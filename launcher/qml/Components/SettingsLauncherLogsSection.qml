// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * The "Launcher logs" section of SettingsPage.qml: MeshMC's own log files
 * (not a game's -- those are on each instance's Log tab), the widget
 * MeshMCLogsDialog without the dialog. Pick a run's log, search it, copy it,
 * upload it for a bug report, delete old ones. MeshMC-0 is the current run.
 *
 * The files come from `launcherLogs` (QmlShell.launcherLogs, handed down by
 * SettingsPage); until there is one the section stays out of the list.
 */
SettingsScroll {
    id: root

    // QmlShell.launcherLogs (LauncherLogs: logs, uploader, folder), or null.
    property var launcherLogs: null
    // Asks the app to show a folder in the file manager.
    signal openPathRequested(string path)
    // Read by SettingsPage.qml's section list.
    readonly property bool shown: !!root.launcherLogs

    // The viewer is not a settings row: it gets a fixed height, tall enough
    // to read a stretch of log without the page itself scrolling.
    readonly property int viewerHeight: 520

    title: qsTr("Launcher logs")
    description: qsTr("MeshMC's own log files. The newest is the current run. Upload one when you report a problem.")

    OtherLogsView {
        width: parent.width
        height: root.viewerHeight
        logs: root.launcherLogs ? root.launcherLogs.logs : null
        uploader: root.launcherLogs ? root.launcherLogs.uploader : null
        selectFirst: true
        onOpenFolderRequested: (path) => root.openPathRequested(path)
    }
}
