// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Java installs Kite found on this computer (JavaInstallList), one JavaInstallRow each in a box, with a spinner
 * while it looks and `emptyText` when it found none. Clicking a row emits picked(). `fitHeight` is the height of
 * its rows (two at least), for a host that sizes the box to them.
 */
BoxSurface {
    id: root

    property var model: null
    property bool detecting: false
    property string currentPath
    property string emptyText
    readonly property int count: view.count
    readonly property int rowHeight: Theme.control.heightLg + Theme.space.xs
    // Room for the busy indicator or the empty text; with rows, just the rows.
    readonly property int minHeight: view.count > 0 ? view.contentHeight + Theme.space.xs * 2 : root.rowHeight * 2
    readonly property int fitHeight: Math.max(root.minHeight, view.contentHeight + Theme.space.xs * 2)

    signal picked(string path, string version)

    ListView {
        id: view
        anchors.fill: parent
        anchors.margins: Theme.space.xs
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.model
        ScrollBar.vertical: ScrollBar {}

        delegate: JavaInstallRow {
            required property string path
            required property var version
            required property var architecture
            required property bool recommended

            width: view.width
            title: qsTr("Java %1").arg(String(version))
            location: path
            chosen: root.currentPath === path
            bit32: String(architecture) === "32"
            suggested: recommended
            onClicked: root.picked(path, String(version))
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: root.detecting
        visible: running
    }
    Copy {
        anchors.centerIn: parent
        width: parent.width - Theme.space.xxl * 2
        visible: view.count === 0 && !root.detecting
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: root.emptyText
        role: "label"
        tone: "tertiary"
        lineHeight: spec.lineHeight
    }
}
