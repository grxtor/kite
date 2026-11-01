// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * "Are you sure?" for anything that cannot be undone from inside the launcher; the confirming button names the
 * action ("Delete world"), never a bare OK.
 */
AppDialog {
    id: root

    property string text
    property string confirmText: qsTr("Delete")
    signal confirmed()

    size: "sm"
    // Every confirm dialog guards a destructive action; set false for the plain accent button.
    danger: true
    closeOnOutsidePress: true
    icon: root.danger ? "alert-triangle" : ""
    primaryText: root.confirmText
    // Closed first, so a handler may open this dialog again.
    closeOnPrimary: false
    onPrimaryClicked: {
        root.close()
        root.confirmed()
    }

    contentItem: Copy {
        text: root.text
        tone: "secondary"
        wrapMode: Text.Wrap
        lineHeight: 1.3
    }
}
