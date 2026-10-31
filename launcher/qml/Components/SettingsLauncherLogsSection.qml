// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The "Launcher logs" section of SettingsPage.qml: Kite's own log files (a game's
 * are on each instance's Log tab): pick a run's log, search, copy, upload for a
 * bug report, delete old ones. Kite-0 is the current run.
 */
SettingsScroll {
    id: root

    // QmlShell.launcherLogs (LauncherLogs: logs, uploader, folder), or null.
    property var launcherLogs: null
    signal openPathRequested(string path)
    readonly property bool shown: !!root.launcherLogs

    // Not a settings row: a fixed height, tall enough to read a stretch of log without the page
    // scrolling.
    readonly property int viewerHeight: 520

    title: qsTr("Launcher logs")
    description: qsTr("Kite's own log files. The newest is the current run. Upload one when you report a problem.")

    OtherLogsView {
        width: parent.width
        height: root.viewerHeight
        logs: root.launcherLogs ? root.launcherLogs.logs : null
        uploader: root.launcherLogs ? root.launcherLogs.uploader : null
        selectFirst: true
        onOpenFolderRequested: (path) => root.openPathRequested(path)
    }
}
