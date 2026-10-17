// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One account in the Accounts page's "other accounts" list: face, name,
 * what kind it is and whether it still works, and the account actions. The
 * default account -- the one games launch with -- gets the accent border;
 * any other can be made default in one click.
 *
 * The page hides whichever row is shown big in its hero instead (see
 * `hidden`), so this file never needs to know what a hero is.
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

    // Set by the page for whichever row it is already showing in the hero
    // card, so the same account is never listed twice.
    property bool hidden: false
    // Bumped by the page whenever an account's skin may have changed, so the
    // face image's url changes and QML actually refetches it -- see
    // AccountsPage.qml's imageRevision.
    property int rev: 0

    signal makeDefaultRequested()
    signal refreshRequested()
    signal removeRequested()
    signal manageSkinRequested()

    readonly property string shownName: profileName.length > 0 ? profileName : name
    readonly property int faceSize: 40
    readonly property int rowHeight: 72
    readonly property bool hovered: !root.hidden && hoverHandler.hovered

    function stateTone(key) {
        switch (key) {
        case "online": return "success"
        // A sign-in in progress is not news: no blue chip (as on the
        // Accounts page's hero).
        case "working": return "neutral"
        case "errored": case "gone": return "danger"
        case "expired": return "warning"
        default: return "neutral"
        }
    }

    // A hidden row collapses out of the ListView entirely rather than just
    // turning invisible, so it leaves no gap where it used to be.
    implicitHeight: root.hidden ? 0 : root.rowHeight
    visible: !root.hidden

    HoverHandler { id: hoverHandler; enabled: !root.hidden }

    Rectangle {
        id: card
        width: parent.width
        height: parent.height
        radius: Theme.radius.lg
        color: root.isDefault ? Theme.palette.surfaceRaised : Theme.palette.surface
        border.width: root.isDefault ? 2 : 1
        // Hover changes the border alone -- no lift, no second property.
        border.color: root.isDefault ? Theme.palette.accent
                    : root.hovered ? Theme.palette.borderStrong : Theme.palette.border

        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.md
            spacing: Theme.space.md

            // The player's head: its skin, or the game's default for an
            // account without one (see PlayerHead).
            PlayerHead {
                size: root.faceSize
                source: !root.hidden && root.accountId.length > 0
                        ? "image://accountface/" + root.accountId + "?rev=" + root.rev : ""
            }

            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.xxs

                Text {
                    width: parent.width
                    text: root.shownName
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.bodyStrong.pixelSize
                    font.weight: Theme.type.bodyStrong.weight
                }

                Row {
                    spacing: Theme.space.sm
                    Tag {
                        iconName: root.isMSA ? "user" : "users"
                        text: root.isMSA ? qsTr("Microsoft") : qsTr("Offline")
                    }
                    StatusBadge {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.isMSA && root.status.length > 0
                        tone: root.stateTone(root.stateKey)
                        text: root.status
                    }
                }
            }

            // The same actions, words and order as the hero card's
            // (AccountsPage.qml): labelled buttons, and the rest behind ⋯
            // rather than bare refresh and bin glyphs.
            Button {
                visible: !root.isDefault
                text: qsTr("Use this account")
                onClicked: root.makeDefaultRequested()
            }
            Button {
                // "and", not "&": a Button reads "&" as a keyboard
                // mnemonic and underlines the next letter.
                text: qsTr("Skin and cape")
                icon.source: Icons.url("image")
                onClicked: root.manageSkinRequested()
            }
            IconButton {
                id: moreButton
                flat: false
                iconName: "more"
                tip: qsTr("More")
                onClicked: rowMenu.popup(moreButton, 0, moreButton.height + Theme.space.xs)

                Menu {
                    id: rowMenu
                    MenuItem {
                        // A hidden item still takes its row in a Menu;
                        // zero it as well.
                        visible: root.isMSA
                        height: visible ? implicitHeight : 0
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
