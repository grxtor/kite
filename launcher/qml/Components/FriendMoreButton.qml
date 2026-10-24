// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The ⋯ at the end of a person's row in the Friends dialog: what can be done
 * to them. It only says what was asked for; the dialog confirms and acts.
 * Someone who is not a friend (a request) cannot be removed, only blocked.
 */
IconButton {
    id: root

    property bool canRemove: true
    signal removeRequested()
    signal blockRequested()

    size: Theme.control.heightSm
    iconName: "more"
    tip: qsTr("More")
    onClicked: menu.popup(root, root.width - menu.width, root.height + Theme.space.xs)

    Menu {
        id: menu

        MenuItem {
            visible: root.canRemove
            height: visible ? implicitHeight : 0
            text: qsTr("Remove friend…")
            icon.source: Icons.url("x")
            onTriggered: root.removeRequested()
        }
        MenuItem {
            text: qsTr("Block…")
            icon.source: Icons.url("ban")
            onTriggered: root.blockRequested()
        }
    }
}
