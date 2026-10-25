// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The ⋯ at the end of a person's row on the Friends page: what can be done
 * to them. It opens the page's FriendPersonMenu (`menu`) for this person; it
 * is always there, so Remove and Block can be found without hunting for a
 * row to hover.
 */
IconButton {
    id: root

    // The page's FriendPersonMenu.
    property var menu: null
    property string personUuid
    // Whose row it is, for a screen reader: every row has a "More".
    property string personName
    // Someone who is not a friend (a request) can only be blocked.
    property bool canRemove: true

    size: Theme.control.heightSm
    iconName: "more"
    tip: qsTr("More")
    Accessible.name: root.personName.length > 0 ? qsTr("More options for %1").arg(root.personName) : root.tip
    onClicked: if (root.menu) root.menu.openFor(root, root.personUuid, root.personName, root.canRemove)
}
