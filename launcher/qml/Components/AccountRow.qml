// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One account in the Accounts page's "other accounts" list: face, name,
 * what kind it is and whether it still works, and the account actions --
 * "Use this account" to make it the one games launch with, the rest behind
 * ⋯.
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
    // Space under the card, counted in the row's own height: a ListView
    // spaces every delegate, even a collapsed one, so the page's list keeps
    // no spacing of its own and a hidden row leaves no gap behind.
    property int gap: 0
    // Bumped by the page whenever an account's skin may have changed, so the
    // face image's url changes and QML actually refetches it -- see
    // AccountsPage.qml's imageRevision.
    property int rev: 0

    signal makeDefaultRequested()
    signal refreshRequested()
    signal removeRequested()
    signal manageSkinRequested()

    readonly property string shownName: profileName.length > 0 ? profileName : name
    readonly property int faceSize: 48
    readonly property int rowHeight: 80
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
    implicitHeight: root.hidden ? 0 : root.rowHeight + root.gap
    visible: !root.hidden

    Rectangle {
        id: card
        width: parent.width
        height: root.rowHeight
        radius: Theme.radius.lg
        // The plain card look: surface plus border, one step lighter under
        // the pointer. Hover changes the fill alone -- no lift, no second
        // property.
        color: root.hovered ? Theme.palette.surfaceHover : Theme.palette.surface
        border.width: 1
        border.color: Theme.palette.border

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        // On the card, not the row, so the gap under it is not hover.
        HoverHandler { id: hoverHandler; enabled: !root.hidden }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.lg
            anchors.rightMargin: Theme.space.lg
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
                    // As a Home row's name (HomeJumpCard).
                    font.family: Theme.font.heading
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Theme.type.title.weight
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

            // "Use this account" stays a button; everything else opens
            // from behind ⋯, each with its own words rather than bare
            // refresh and bin glyphs.
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
                    MenuItem {
                        // "and", not "&": a MenuItem reads "&" as a
                        // keyboard mnemonic too.
                        text: qsTr("Skin and cape")
                        icon.source: Icons.url("image")
                        onTriggered: root.manageSkinRequested()
                    }
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
