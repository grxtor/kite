// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * "Which log": one drop-down whose first entry is the live game log and
 * whose others are the instance's log files on disk (logs/*.log*, crash
 * reports). It replaces the Live | Other logs switch, so a log is one
 * choice instead of a mode plus a file. It only says what was chosen; the
 * Log tab switches its view (LogTab.qml).
 */
ComboBox {
    id: root

    // OtherLogsModel (roles: name; currentFile), or null.
    property var logs: null
    // Whether the live log is the one on show.
    property bool live: true
    readonly property string liveLabel: qsTr("Live game log")
    // The file names, in the model's order.
    property var files: []

    signal liveChosen()
    signal fileChosen(string name)

    function collect() {
        const names = []
        for (let i = 0; i < collector.count; ++i) {
            const o = collector.objectAt(i)
            if (o)
                names.push(o.name)
        }
        root.files = names
    }

    Instantiator {
        id: collector
        model: root.logs
        delegate: QtObject {
            required property string name
        }
        onObjectAdded: Qt.callLater(root.collect)
        onObjectRemoved: Qt.callLater(root.collect)
    }

    model: [root.liveLabel].concat(root.files)
    // Follows the Log tab's state rather than owning the choice.
    currentIndex: root.live ? 0 : (root.logs ? root.files.indexOf(root.logs.currentFile) + 1 : -1) || -1
    // Before any file is picked in the file view the box invites a choice.
    displayText: currentIndex < 0 ? qsTr("Choose a log file") : currentText
    Accessible.name: qsTr("Log")

    onActivated: (index) => {
        if (index === 0)
            root.liveChosen()
        else
            root.fileChosen(root.files[index - 1])
    }
}
