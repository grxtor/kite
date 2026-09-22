// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * "Are you sure?" for anything that cannot be undone from inside the
 * launcher. The confirming button names the action ("Delete world"), never
 * a bare OK, and Escape or a click outside cancels.
 */
Dialog {
    id: root

    property string text
    property string confirmText: qsTr("Delete")
    signal confirmed()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(440, parent ? parent.width - Theme.space.xxl * 2 : 440)
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

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
        padding: Theme.space.lg
        topPadding: 0

        Button {
            text: root.confirmText
            highlighted: true
            onClicked: {
                root.close()
                root.confirmed()
            }
        }
        Button {
            text: qsTr("Cancel")
            flat: true
            onClicked: root.close()
        }
    }
}
