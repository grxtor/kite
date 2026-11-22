// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The Accounts page's header row: what the card under it is and what a default account means, and "Add account",
 * the page's one filled button, which asks which kind.
 */
RowLayout {
    id: root

    // False while no account is flagged default and the page only suggests one.
    property bool hasDefault: true

    signal microsoftRequested()
    signal offlineRequested()

    spacing: Theme.space.md

    Column {
        Layout.fillWidth: true
        spacing: Theme.space.xxs

        Copy {
            width: parent.width
            text: qsTr("Default account")
            role: "heading"
            elide: Text.ElideRight
        }
        Copy {
            width: parent.width
            text: root.hasDefault ? qsTr("Games launch with the default account.")
                                  : qsTr("No default account chosen yet.")
            role: "label"
            tone: "tertiary"
            wrapMode: Text.Wrap
        }
    }

    Button {
        id: addButton
        Layout.alignment: Qt.AlignVCenter
        highlighted: true
        text: qsTr("Add account")
        icon.source: Icons.url("plus")
        onClicked: addMenu.popup(addButton, 0, addButton.height + Theme.space.xs)

        Menu {
            id: addMenu
            MenuItem {
                text: qsTr("Microsoft account")
                icon.source: Icons.url("user")
                onTriggered: root.microsoftRequested()
            }
            MenuItem {
                text: qsTr("Offline account")
                onTriggered: root.offlineRequested()
            }
        }
    }
}
