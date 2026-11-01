// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One account in the Accounts page's "other accounts" list: face, name, kind and state, and the actions --
 * "Use this account" and the rest behind ⋯. The page hides the row shown in its hero (`hidden`).
 */
Item {
    id: root

    required property int index
    required property string profileName
    required property string name
    required property bool isDefault
    required property bool isMSA
    required property string stateKey
    required property string status
    required property string accountId

    property bool hidden: false
    // Space under the card, counted in the row's own height: a ListView spaces even a collapsed delegate, so
    // the list has no spacing of its own and a hidden row leaves no gap.
    property int gap: 0
    // Bumped by the page when a skin may have changed, so the face url changes (AccountsPage imageRevision).
    property int rev: 0

    signal makeDefaultRequested()
    signal refreshRequested()
    signal removeRequested()
    signal manageSkinRequested()

    readonly property string shownName: profileName.length > 0 ? profileName : name
    readonly property int faceSize: 48
    readonly property int rowHeight: 80
    readonly property bool hovered: !root.hidden && hoverHandler.hovered

    implicitHeight: root.hidden ? 0 : root.rowHeight + root.gap
    visible: !root.hidden

    StateFill {
        width: parent.width
        height: root.rowHeight
        radius: Theme.radius.lg
        card: true
        hovered: root.hovered
        border.width: 1
        border.color: Theme.palette.border

        HoverHandler { id: hoverHandler; enabled: !root.hidden }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.lg
            anchors.rightMargin: Theme.space.lg
            spacing: Theme.space.md

            PlayerHead {
                size: root.faceSize
                source: !root.hidden && root.accountId.length > 0
                        ? "image://accountface/" + root.accountId + "?rev=" + root.rev : ""
            }

            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.xxs

                Copy {
                    width: parent.width
                    text: root.shownName
                    role: "title"
                    elide: Text.ElideRight
                }

                Row {
                    spacing: Theme.space.sm
                    Tag {
                        iconName: root.isMSA ? "user" : "users"
                        text: root.isMSA ? qsTr("Microsoft") : qsTr("Offline")
                    }
                    AccountStateBadge {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.isMSA && root.status.length > 0
                        stateKey: root.stateKey
                        text: root.status
                    }
                }
            }

            Button {
                visible: !root.isDefault
                variant: "secondary"
                text: qsTr("Use this account")
                onClicked: root.makeDefaultRequested()
            }
            IconButton {
                id: moreButton
                flat: false
                iconName: "more"
                tip: qsTr("More")
                onClicked: rowMenu.popup(moreButton, 0, moreButton.height + Theme.space.xs)

                Menu {
                    id: rowMenu
                    MenuEntry {
                        // Skins and capes belong to a Microsoft account.
                        visible: root.isMSA
                        enabled: root.isMSA
                        // "and", not "&": a MenuItem reads "&" as a mnemonic too.
                        text: qsTr("Skin and cape")
                        icon.source: Icons.url("image")
                        onTriggered: root.manageSkinRequested()
                    }
                    MenuEntry {
                        visible: root.isMSA
                        enabled: root.isMSA
                        text: qsTr("Sign in again")
                        icon.source: Icons.url("refresh")
                        onTriggered: root.refreshRequested()
                    }
                    MenuItem {
                        text: qsTr("Remove account\u2026")
                        icon.source: Icons.url("trash")
                        onTriggered: root.removeRequested()
                    }
                }
            }
        }
    }
}
