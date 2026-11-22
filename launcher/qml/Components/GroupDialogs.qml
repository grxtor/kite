// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The two questions a library group's menu asks before LibraryController acts: its new name, and whether to delete
 * it. Not visible itself; the dialogs open over the window.
 */
Item {
    id: root

    visible: false

    // LibraryController.
    property var library: null

    signal renamed(string from, string to)

    function askRename(group) {
        renameDialog.group = group
        renameDialog.value = group
        renameDialog.open()
    }
    function askDelete(group) {
        deleteDialog.group = group
        deleteDialog.text = qsTr("Delete the group “%1”? Its instances stay in your library, without a group.").arg(group)
        deleteDialog.open()
    }

    PromptDialog {
        id: renameDialog
        property string group
        title: qsTr("Rename group")
        confirmText: qsTr("Rename")
        onSubmitted: (text) => {
            if (root.library)
                root.library.renameGroup(group, text)
            root.renamed(group, text)
            close()
        }
    }

    ConfirmDialog {
        id: deleteDialog
        property string group
        title: qsTr("Delete group")
        confirmText: qsTr("Delete group")
        // Nothing is lost: the instances stay, ungrouped.
        danger: false
        onConfirmed: if (root.library) root.library.deleteGroup(group)
    }
}
