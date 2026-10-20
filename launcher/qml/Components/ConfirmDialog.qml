// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * "Are you sure?" for anything that cannot be undone from inside the
 * launcher. The confirming button names the action ("Delete world"), never
 * a bare OK, and Escape or a click outside cancels.
 */
Dialog {
    id: root

    property string text
    property string confirmText: qsTr("Delete")
    // Every confirm dialog in this codebase guards a destructive action
    // (delete, remove); a caller that ever needs the plain accent-coloured
    // button back can set this to false.
    property bool danger: true
    signal confirmed()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(440, parent ? parent.width - Theme.space.xxl * 2 : 440)
    modal: true
    // The title row sets the inset; the gap under it is the dialog's own spacing.
    topPadding: 0
    // The actions row sets the bottom inset; this and the spacing over it
    // leave one inset (xl) between the body and the actions.
    bottomPadding: Theme.space.sm
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    header: DialogHeader {
        title: root.title
        icon: root.danger ? "alert-triangle" : ""
        iconColor: Theme.palette.danger
    }

    contentItem: Text {
        text: root.text
        wrapMode: Text.Wrap
        color: Theme.palette.textSecondary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        lineHeight: 1.3
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.xl
        topPadding: 0

        Button {
            text: root.confirmText
            highlighted: true
            danger: root.danger
            onClicked: {
                root.close()
                root.confirmed()
            }
        }
        Button {
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    }
}
