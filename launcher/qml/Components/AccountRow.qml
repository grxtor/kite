// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One account in the Accounts page's "Other accounts" card: head, name, the kind of account in words with its
 * state beside it, and the actions -- "Make default" and the rest behind ⋯. An account that cannot play online
 * (AccountStates) also says why under its name and has "Sign in again" on the row. Rows lie in one card with a
 * hairline between them (`divider`, on every row but the first shown); the page hides the row its hero shows
 * (`hidden`), which a Column then skips.
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
    required property string lastError

    property bool hidden: false
    property bool divider: false
    // Bumped by the page when a skin may have changed, so the face url changes (AccountsPage imageRevision).
    property int rev: 0

    signal makeDefaultRequested()
    signal refreshRequested()
    signal removeRequested()
    signal manageSkinRequested()

    readonly property string shownName: profileName.length > 0 ? profileName : name
    readonly property int faceSize: 40
    readonly property bool attention: AccountStates.needsAttention(root.stateKey)
    readonly property bool working: root.stateKey === "working"
    readonly property string reason: !root.attention ? ""
                                   : root.lastError.length > 0 ? root.lastError : AccountStates.summary(root.stateKey)
    readonly property int rowHeight: root.reason.length > 0 ? 88 : 68

    implicitHeight: root.rowHeight
    visible: !root.hidden

    // In the gap the card's column leaves above the row.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.space.md
        anchors.rightMargin: Theme.space.md
        y: -1
        height: 1
        color: Theme.palette.divider
        visible: root.divider
    }

    StateFill {
        anchors.fill: parent
        radius: Theme.radius.md
        hovered: hoverHandler.hovered
    }
    HoverHandler { id: hoverHandler }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.md
        anchors.rightMargin: Theme.space.sm
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
                role: "bodyStrong"
                elide: Text.ElideRight
            }

            Row {
                spacing: Theme.space.sm
                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.isMSA ? qsTr("Microsoft account") : qsTr("Offline account")
                    role: "label"
                    tone: "secondary"
                }
                AccountStateBadge {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: (root.isMSA || root.attention || root.working) && root.status.length > 0
                    stateKey: root.stateKey
                    text: root.status
                }
                BusyIndicator {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.working
                    running: visible
                    width: Theme.icon.sm
                    height: Theme.icon.sm
                }
            }

            Copy {
                width: parent.width
                visible: root.reason.length > 0
                text: root.reason
                role: "caption"
                tone: "tertiary"
                elide: Text.ElideRight
            }
        }

        Button {
            visible: AccountStates.canSignInAgain(root.stateKey)
            buttonSize: "sm"
            text: qsTr("Sign in again")
            icon.source: Icons.url("refresh")
            onClicked: root.refreshRequested()
        }
        Button {
            visible: !root.isDefault
            buttonSize: "sm"
            text: qsTr("Make default")
            onClicked: root.makeDefaultRequested()
        }
        IconButton {
            id: moreButton
            iconName: "more"
            tip: qsTr("More")
            onClicked: rowMenu.popup(moreButton, 0, moreButton.height + Theme.space.xs)

            Menu {
                id: rowMenu
                MenuEntry {
                    // Skins and capes belong to a Microsoft account.
                    visible: root.isMSA
                    enabled: root.isMSA
                    text: qsTr("Change skin")
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
                    danger: true
                    text: qsTr("Remove account…")
                    icon.source: Icons.url("trash")
                    onTriggered: root.removeRequested()
                }
            }
        }
    }
}
