// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "Edit the bar": every server of Home's quick-join bar, the big ones and the player's own, in the bar's order. Drag a
 * row by its handle to move it, untick it to hide it, remove one the player added. "Reset to default" puts back the
 * big servers as they come. Nothing changes until Save (QuickJoinLayout.apply()); Cancel keeps the bar as it was.
 */
AppDialog {
    id: root

    // QuickJoinLayout, or null.
    property var layout: null
    // ServerPinger, or null: the favicons.
    property var pinger: null

    // Where the row being dragged started.
    property int dragFrom: -1
    readonly property int maxRows: 7

    // Takes a fresh copy of the bar each time.
    function edit() {
        root.fill(root.layout ? root.layout.entries : [])
        root.open()
    }
    function fill(entries) {
        rows.clear()
        for (var i = 0; i < entries.length; ++i) {
            var entry = entries[i]
            rows.append({ entryId: entry.id, name: entry.name, address: entry.address, featured: !!entry.featured,
                          own: !!entry.own, hidden: !!entry.hidden })
        }
    }
    function entries() {
        var out = []
        for (var i = 0; i < rows.count; ++i) {
            var row = rows.get(i)
            out.push({ id: row.entryId, name: row.name, address: row.address, featured: row.featured, own: row.own,
                       hidden: row.hidden })
        }
        return out
    }
    function faviconOf(address, revision) {
        if (!root.pinger)
            return ""
        var status = root.pinger.status(address)
        return status && status.state === "online" && status.favicon ? status.favicon : ""
    }

    size: "sm"
    title: qsTr("Edit the bar")
    primaryText: qsTr("Save")
    onPrimaryClicked: if (root.layout) root.layout.apply(root.entries())

    footerLeading: [
        Button {
            objectName: "quickJoinEditReset"
            flat: true
            text: qsTr("Reset to default")
            onClicked: root.fill(root.layout ? root.layout.defaults() : [])
        }
    ]

    ListModel { id: rows }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Copy {
            Layout.fillWidth: true
            text: qsTr("Drag to change the order. Untick a server to hide it from the bar.")
            role: "label"
            tone: "secondary"
            wrapMode: Text.Wrap
        }

        BoxSurface {
            Layout.fillWidth: true
            Layout.preferredHeight: list.contentHeight > 0
                                    ? Math.min(list.contentHeight, list.rowHeight * root.maxRows) + Theme.space.xs * 2
                                    : list.rowHeight
            sunken: true
            radius: Theme.radius.md

            ListView {
                id: list
                objectName: "quickJoinEditList"
                anchors.fill: parent
                anchors.margins: Theme.space.xs
                clip: true
                model: rows
                interactive: contentHeight > height && root.dragFrom < 0
                boundsBehavior: Flickable.StopAtBounds
                readonly property real rowHeight: Theme.control.heightLg + Theme.space.xs
                ScrollBar.vertical: ScrollBar {}

                moveDisplaced: Transition {
                    NumberAnimation { properties: "y"; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
                }

                // The roles through `model`: the row's own properties share their names.
                delegate: QuickJoinEditRow {
                    required property int index
                    required property var model

                    objectName: "quickJoinEditRow"
                    width: ListView.view.width
                    step: list.rowHeight
                    name: model.name
                    address: model.address
                    featured: model.featured
                    own: model.own
                    shown: !model.hidden
                    favicon: root.faviconOf(model.address, root.pinger ? root.pinger.revision : 0)
                    onShownToggled: (shown) => rows.setProperty(index, "hidden", !shown)
                    onRemoveRequested: rows.remove(index)
                    onDragged: (steps) => {
                        if (root.dragFrom < 0)
                            root.dragFrom = index
                        var to = Math.max(0, Math.min(rows.count - 1, root.dragFrom + steps))
                        if (to !== index)
                            rows.move(index, to, 1)
                    }
                    onDragFinished: root.dragFrom = -1
                    Component.onCompleted: if (root.pinger) root.pinger.ping(model.address)
                }
            }
        }
    }
}
