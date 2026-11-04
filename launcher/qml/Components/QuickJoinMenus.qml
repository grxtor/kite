// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The quick-join bar's two menus, kept out of QuickJoinBar: a server's (right-click: join, join with another instance,
 * invite, copy the address, remove it from the bar) and the bar's ⋯ (the servers that did not fit, then "Add a
 * server…" and "Edit the bar…"). Each acts through `bar`'s signals and functions.
 */
Item {
    id: root

    // The QuickJoinBar these belong to.
    property var bar: null

    readonly property bool serverMenuOpen: serverMenu.visible
    readonly property string serverMenuId: serverMenu.visible && serverMenu.server.id ? serverMenu.server.id : ""

    // Join with… / Copy address / Remove for @p server, under @p anchor, or at the pointer without one.
    function openServer(server, anchor) {
        serverMenu.server = server
        serverMenu.instanceName = root.bar.joinFlow ? root.bar.joinFlow.instanceName(server) : ""
        if (anchor)
            serverMenu.popup(anchor, 0, anchor.height)
        else
            serverMenu.popup()
    }
    // Opens leftward under @p button, its right edge under the button's, so it stays inside the page's column.
    function openMore(button) {
        overflowMenu.popup(button, button.width - overflowMenu.width, button.height)
    }

    Menu {
        id: serverMenu
        property var server: ({})
        property string instanceName: ""

        MenuItem {
            text: serverMenu.instanceName.length > 0 ? qsTr("Join with %1").arg(serverMenu.instanceName) : qsTr("Join…")
            icon.source: Icons.url("play")
            onTriggered: root.bar.joinRequested(serverMenu.server)
        }
        MenuItem {
            text: qsTr("Join with another instance…")
            icon.source: Icons.url("layers")
            onTriggered: root.bar.joinWithRequested(serverMenu.server)
        }
        MenuEntry {
            visible: root.bar && root.bar.canInvite
            text: qsTr("Invite a friend…")
            icon.source: Icons.url("user-plus")
            onTriggered: root.bar.inviteRequested(serverMenu.server)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Copy address")
            icon.source: Icons.url("copy")
            onTriggered: clipboard.copyText(serverMenu.server.address)
        }
        MenuEntry {
            objectName: "quickJoinRemove"
            visible: !!root.bar && !!root.bar.layout
            text: qsTr("Remove from bar")
            icon.source: Icons.url("x")
            onTriggered: root.bar.layout.remove(serverMenu.server.id)
        }
    }

    Menu {
        id: overflowMenu
        objectName: "quickJoinMoreMenu"

        // What the list is, so a single row reads as a menu.
        MenuEntry {
            visible: root.bar && root.bar.hidden.length > 0
            enabled: false
            background: null
            contentItem: Copy {
                text: qsTr("More servers")
                role: "caption"
                tone: "tertiary"
            }
        }

        // The same line as an entry; a click joins, a right-click offers the server's own menu.
        Instantiator {
            model: root.bar ? root.bar.hidden : []
            delegate: MenuItem {
                id: hiddenRow
                required property var modelData

                text: hiddenRow.modelData.name
                contentItem: Item {
                    implicitWidth: line.implicitWidth
                    implicitHeight: line.implicitHeight

                    QuickJoinSummary {
                        id: line
                        anchors.verticalCenter: parent.verticalCenter
                        server: hiddenRow.modelData
                        status: root.bar.statusOf(hiddenRow.modelData.address,
                                                  root.bar.pinger ? root.bar.pinger.revision : 0)
                        friends: root.bar.friendsOn(hiddenRow.modelData.address)
                    }
                }
                onTriggered: root.bar.joinRequested(hiddenRow.modelData)

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        overflowMenu.close()
                        root.openServer(hiddenRow.modelData, null)
                    }
                }
            }
            // After the heading.
            onObjectAdded: (index, object) => overflowMenu.insertItem(index + 1, object)
            onObjectRemoved: (index, object) => overflowMenu.removeItem(object)
        }

        MenuDivider {
            visible: root.bar && root.bar.hidden.length > 0
        }
        MenuEntry {
            objectName: "quickJoinAddServer"
            visible: !!root.bar && !!root.bar.layout
            text: qsTr("Add a server…")
            icon.source: Icons.url("plus")
            onTriggered: root.bar.openAdd()
        }
        MenuEntry {
            objectName: "quickJoinEditBar"
            visible: !!root.bar && !!root.bar.layout
            text: qsTr("Edit the bar…")
            icon.source: Icons.url("sliders")
            onTriggered: root.bar.openEdit()
        }
    }

    ClipboardText { id: clipboard }
}
