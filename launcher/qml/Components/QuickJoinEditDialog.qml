// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "Edit the bar": every server of Home's quick-join bar, the big ones and the player's own, in the bar's order. Drag a
 * row by its handle to move it, untick it to hide it, remove one the player added, add one under the list ("Add a
 * server…", the menu's flow). "Reset to default" puts back the big servers as they come. Nothing changes until Save
 * (QuickJoinLayout.apply()); Cancel keeps the bar as it was.
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
    // A server from "Add a server…": the row of the same server shown again and kept as theirs, else a new one last.
    function addRow(name, address) {
        var id = root.layout ? root.layout.ownIdOf(address) : ""
        if (id.length === 0)
            return
        for (var i = 0; i < rows.count; ++i) {
            if (root.layout.ownIdOf(rows.get(i).address) === id) {
                rows.setProperty(i, "hidden", false)
                rows.setProperty(i, "own", true)
                list.positionViewAtIndex(i, ListView.Contain)
                return
            }
        }
        rows.append({ entryId: id, name: name.length > 0 ? name : address, address: address, featured: false,
                      own: true, hidden: false })
        list.positionViewAtEnd()
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

    QuickJoinAddDialog {
        id: addDialog
        layout: root.layout
        pinger: root.pinger
        pins: false
        onChosen: (name, address) => root.addRow(name, address)
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Copy {
            Layout.fillWidth: true
            text: qsTr("Drag to change the order. Untick a server to hide it from the bar.")
            role: "label"
            tone: "secondary"
            wrapMode: Text.Wrap
        }

        // The rows scroll past maxRows; "Add a server…" stays under them.
        BoxSurface {
            Layout.fillWidth: true
            Layout.preferredHeight: list.height + addRow.height + Theme.space.xs * 2
            sunken: true
            radius: Theme.radius.md

            ListView {
                id: list
                objectName: "quickJoinEditList"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space.xs
                height: Math.min(contentHeight, list.rowHeight * root.maxRows)
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

            ItemDelegate {
                id: addRow
                objectName: "quickJoinEditAdd"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: list.bottom
                anchors.leftMargin: Theme.space.xs
                anchors.rightMargin: Theme.space.xs
                height: list.rowHeight
                // The plus where the rows have their handles.
                leftPadding: Theme.space.md
                text: qsTr("Add a server…")
                icon.source: Icons.url("plus")
                icon.width: Theme.icon.xs
                icon.height: Theme.icon.xs
                onClicked: addDialog.start()
            }
        }
    }
}
