// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The bar ContentBrowserView shows over its results while rows are ticked: how many, and "Install (N)". While the
 * batch runs it says how far it is and offers to stop the rest. Opens and closes like ContentSelectionBar, by
 * growing to its height, so the list under it moves instead of jumping; `gap` is the host layout's spacing.
 */
BoxSurface {
    id: root

    // The ContentBatchInstall.
    required property var batch
    // Why nothing can be installed right now; "" when it can.
    property string blockedReason: ""
    property real gap: 0

    readonly property bool open: !!root.batch && (root.batch.selecting || root.batch.running)
    // 0 closed, 1 open.
    property real reveal: root.open ? 1 : 0
    Behavior on reveal { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

    visible: root.reveal > 0
    opacity: root.reveal
    clip: true
    Layout.preferredHeight: root.implicitHeight * root.reveal
    Layout.bottomMargin: -root.gap * (1 - root.reveal)

    implicitHeight: Theme.control.height + Theme.space.sm * 2
    color: Theme.palette.surfaceRaised

    RowLayout {
        // Its own full height whatever the bar is at, so the buttons are uncovered, not squeezed.
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.implicitHeight
        anchors.leftMargin: Theme.space.lg
        anchors.rightMargin: Theme.space.sm
        spacing: Theme.space.md

        BusyIndicator {
            Layout.preferredWidth: Theme.icon.md
            Layout.preferredHeight: Theme.icon.md
            visible: root.batch.running
            running: visible
        }
        Copy {
            Layout.fillWidth: true
            text: root.batch.running
                  ? (root.batch.current
                     ? qsTr("Installing %1 of %2: %3").arg(Math.min(root.batch.total, root.batch.done + 1))
                                                      .arg(root.batch.total).arg(root.batch.current.title)
                     : qsTr("Installing…"))
                  : qsTr("%1 selected").arg(root.batch.selected.length)
            elide: Text.ElideRight
            role: "bodyStrong"
        }

        Button {
            visible: root.batch.running && root.batch.queue.length > 0
            text: qsTr("Stop the rest")
            onClicked: root.batch.stop()
        }
        HoverReason {
            visible: !root.batch.running
            reason: root.blockedReason
            Button {
                highlighted: true
                enabled: root.blockedReason.length === 0
                text: qsTr("Install (%1)").arg(root.batch.selected.length)
                icon.source: Icons.url("download")
                onClicked: root.batch.start()
            }
        }
        IconButton {
            visible: !root.batch.running
            iconName: "x"
            tip: qsTr("Clear selection")
            onClicked: root.batch.clear()
        }
    }
}
