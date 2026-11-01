// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Pick one of the Java installs Kite found on this computer, plus the runtimes
 * Kite downloaded itself. Choosing a row emits picked() and closes; the caller
 * decides what to do with the path.
 */
AppDialog {
    id: root

    property string currentPath
    readonly property var java: JavaStore.controller
    // The list is as tall as its rows, up to this or what the window leaves room for.
    readonly property int listMaxHeight: 520

    signal picked(string path, string version)

    title: qsTr("Choose Java")
    showCancel: false
    primaryText: qsTr("Close")
    closeOnOutsidePress: true
    footerLeading: [
        Button {
            flat: true
            text: qsTr("Detect again")
            icon.source: Icons.url("refresh")
            enabled: !!root.java && !root.java.detecting
            onClicked: root.java.detect()
        }
    ]

    onOpened: if (root.java && root.java.installs && list.count === 0 && !root.java.detecting) root.java.detect()

    contentItem: JavaInstallList {
        id: list
        sunken: true
        implicitHeight: Math.max(list.minHeight, Math.min(list.fitHeight, root.listMaxHeight,
                                 root.parent ? root.parent.height - Theme.space.xxl * 6 : root.listMaxHeight))
        model: root.java ? root.java.installs : null
        detecting: !!root.java && root.java.detecting
        currentPath: root.currentPath
        emptyText: qsTr("No Java found on this computer. Download one, or browse for a Java you installed yourself.")
        onPicked: (path, version) => {
            root.picked(path, version)
            root.close()
        }
    }
}
