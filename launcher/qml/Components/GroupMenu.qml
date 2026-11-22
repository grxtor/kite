// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * What a library group offers, from its section header's ⋯ and from its filter chip: rename and delete. Only asks;
 * LibrarySections owns the dialogs.
 */
Menu {
    id: root

    property string group

    signal renameRequested(string group)
    signal deleteRequested(string group)

    MenuItem {
        text: qsTr("Rename group…")
        icon.source: Icons.url("edit")
        onTriggered: root.renameRequested(root.group)
    }
    MenuItem {
        text: qsTr("Delete group…")
        icon.source: Icons.url("trash")
        onTriggered: root.deleteRequested(root.group)
    }
}
