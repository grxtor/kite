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
    readonly property bool hovered: !root.hidden && hoverHandler.hovered

    function stateTone(key) {
        switch (key) {
        case "online": return "success"
        case "working": return "info"
        case "errored": case "gone": return "danger"
        case "expired": return "warning"
        default: return "neutral"
        }
    }

    // A hidden row collapses out of the ListView entirely rather than just
    // turning invisible, so it leaves no gap where it used to be.
    implicitHeight: root.hidden ? 0 : 72
    visible: !root.hidden

    HoverHandler { id: hoverHandler; enabled: !root.hidden }

    Rectangle {
        id: card
        width: parent.width
        height: parent.height
        radius: Theme.radius.lg
        color: root.isDefault ? Theme.palette.surfaceRaised
             : root.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
        border.width: root.isDefault ? 2 : 1
        border.color: root.isDefault ? Theme.palette.accent
                    : root.hovered ? Theme.palette.borderStrong : Theme.palette.border
        // A small lift on hover, same idiom as the library's InstanceCard.
        y: root.hovered ? -2 : 0

        Behavior on y { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.md
            spacing: Theme.space.md

            Rectangle {
                id: face
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: width / 2
                color: Theme.palette.accentSubtle
                border.width: 1
                border.color: Theme.palette.border

                Text {
                    anchors.centerIn: parent
                    text: root.shownName.charAt(0).toUpperCase()
                    color: Theme.palette.accent
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }
                // Drawn over the initial; an account without a skin comes
                // back transparent and the initial shows through.
                Image {
                    anchors.fill: parent
                    anchors.margins: 1
                    source: !root.hidden && root.accountId.length > 0
                            ? "image://accountface/" + root.accountId + "?rev=" + root.rev : ""
                    sourceSize: Qt.size(80, 80)
                    smooth: false
                }
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

            Button {
                visible: !root.isDefault
                text: qsTr("Use this account")
                onClicked: root.makeDefaultRequested()
            }
            IconButton {
                iconName: "image"
                tip: root.isMSA ? qsTr("Manage skin & cape") : qsTr("Skins need a Microsoft account")
                onClicked: root.manageSkinRequested()
            }
            IconButton {
                visible: root.isMSA
                iconName: "refresh"
                tip: qsTr("Sign in again")
                onClicked: root.refreshRequested()
            }
            IconButton {
                iconName: "trash"
                tip: qsTr("Remove account")
                onClicked: root.removeRequested()
            }
        }
    }
}
