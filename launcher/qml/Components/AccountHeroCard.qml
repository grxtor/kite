// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The default account on the Accounts page, in two columns: the player in its skin on a sunken plate (turned by
 * dragging where the 3D view runs), and beside it the name, what kind of account it is and its state, the
 * actions as buttons, and the capes it owns. An account that cannot play online (AccountStates) says why and
 * offers the way out. Narrow, the plate goes on top.
 */
BoxSurface {
    id: root

    property string name: ""
    property string accountId: ""
    property bool isMSA: false
    property bool isDefault: false
    property string stateKey: ""
    property string status: ""
    // The account's own reason for a state that needs attention; AccountStates words it when empty.
    property string reason: ""
    // AccountsController.accountSkinInfo() of the account.
    property var skin: ({})
    property var capes: []
    property bool capeBusy: false
    property string capeError: ""
    // Bumped by the page when a skin may have changed, so the image urls change.
    property int rev: 0
    property bool skin3DAvailable: false

    signal makeDefaultRequested()
    signal refreshRequested()
    signal removeRequested()
    signal skinRequested()
    signal capeRequested(string capeId)

    readonly property bool attention: AccountStates.needsAttention(root.stateKey)
    readonly property bool working: root.stateKey === "working"
    readonly property bool stacked: root.width < 720
    readonly property int plateMinWidth: 260
    readonly property int plateMaxWidth: 400
    readonly property real plateShare: 0.36
    readonly property int plateHeight: root.stacked ? 240 : 288
    readonly property int pad: Theme.space.lg

    // Turns the player round to show a cape just put on.
    function showBack() { preview.showBack() }

    implicitHeight: grid.implicitHeight + root.pad * 2

    GridLayout {
        id: grid
        anchors.fill: parent
        anchors.margins: root.pad
        columns: root.stacked ? 1 : 2
        columnSpacing: Theme.space.xl
        rowSpacing: Theme.space.lg

        Rectangle {
            Layout.fillWidth: root.stacked
            Layout.fillHeight: !root.stacked
            Layout.preferredWidth: Math.max(root.plateMinWidth,
                                            Math.min(root.plateMaxWidth, Math.round(root.width * root.plateShare)))
            Layout.preferredHeight: root.plateHeight
            Layout.minimumHeight: root.plateHeight
            radius: Theme.radius.md
            color: Theme.palette.surfaceSunken

            // An account without a skin wears the game's default; a neutral figure holds its place until it
            // downloads.
            SkinPreview {
                id: preview
                anchors.fill: parent
                anchors.margins: Theme.space.lg
                skin3DAvailable: root.skin3DAvailable
                skinTexture: root.skin.hasSkin && root.accountId.length > 0
                             ? "image://accountface/skin/" + root.accountId + "?rev=" + root.rev : ""
                flatSource: root.accountId.length > 0
                            ? "image://accountface/body/" + root.accountId + "?rev=" + root.rev : ""
                slim: !!root.skin.slim
                capeTexture: root.skin.currentCapeUrl || ""
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space.sm

            Copy {
                Layout.fillWidth: true
                text: root.name
                role: "display"
                elide: Text.ElideRight
            }

            // The kind in words; the state is the only badge.
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
                Layout.fillWidth: true
                visible: root.attention || !root.isMSA
                text: !root.attention ? qsTr("Plays offline. Online-mode servers and your own skin need a Microsoft account.")
                    : root.reason.length > 0 ? root.reason : AccountStates.summary(root.stateKey)
                role: "label"
                tone: "tertiary"
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }

            Flow {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space.sm
                spacing: Theme.space.sm

                Button {
                    visible: !root.isDefault
                    text: qsTr("Make default")
                    onClicked: root.makeDefaultRequested()
                }
                Button {
                    visible: AccountStates.canSignInAgain(root.stateKey)
                    text: qsTr("Sign in again")
                    icon.source: Icons.url("refresh")
                    onClicked: root.refreshRequested()
                }
                Button {
                    // Skins and capes belong to a Microsoft account.
                    visible: root.isMSA
                    text: qsTr("Change skin")
                    icon.source: Icons.url("image")
                    onClicked: root.skinRequested()
                }
                // An offline account has nothing else to offer, so its one action is in the open.
                Button {
                    visible: !root.isMSA
                    text: qsTr("Remove account…")
                    icon.source: Icons.url("trash")
                    onClicked: root.removeRequested()
                }
                IconButton {
                    id: moreButton
                    visible: root.isMSA
                    iconName: "more"
                    tip: qsTr("More")
                    onClicked: menu.popup(moreButton, 0, moreButton.height + Theme.space.xs)

                    Menu {
                        id: menu
                        MenuItem {
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

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space.sm
                Layout.bottomMargin: Theme.space.xs
                visible: capePicker.visible
                height: 1
                color: Theme.palette.divider
            }

            AccountCapePicker {
                id: capePicker
                Layout.fillWidth: true
                visible: root.capes.length > 0
                capes: root.capes
                currentCapeId: root.skin.currentCapeId || ""
                busy: root.capeBusy
                error: root.capeError
                onCapeRequested: (capeId) => root.capeRequested(capeId)
            }
        }
    }
}
