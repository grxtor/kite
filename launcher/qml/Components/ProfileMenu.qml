// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The account dropdown opened from ProfileButton: who is playing, a quick
 * switch to any other signed-in account, and the account actions that used
 * to live at the foot of the sidebar. Everything that takes more than one
 * click -- signing in, adding an offline account, managing skins -- goes to
 * the Accounts page instead of repeating that flow here (see
 * openAccountsRequested()); this menu only ever does the one-click things
 * itself: switching the default account, and signing out of it.
 */
Popup {
    id: root

    // AccountsController: accounts, setDefault(row), remove(row).
    property var controller: null
    property string name: ""
    property string kind: ""
    property string avatarSource: ""

    signal openAccountsRequested()

    readonly property var accounts: root.controller ? root.controller.accounts : null

    // Every other account's row and shown name, plus which row is the
    // default (for "Sign out"), gathered the same way AccountsPage finds
    // its own hero row: a zero-size probe per row rather than paging
    // through the model by hand.
    property var otherAccounts: []
    property int defaultRow: -1
    function rebuildAccounts() {
        var rows = []
        var def = -1
        for (var i = 0; i < accountProbes.count; ++i) {
            var probe = accountProbes.itemAt(i)
            if (!probe)
                continue
            if (probe.isDefault)
                def = probe.index
            else
                rows.push({ row: probe.index,
                            name: probe.profileName.length > 0 ? probe.profileName : probe.name,
                            accountId: probe.accountId,
                            isDefault: probe.isDefault })
        }
        root.defaultRow = def
        root.otherAccounts = rows
    }

    Repeater {
        id: accountProbes
        model: root.accounts
        delegate: Item {
            id: probe
            required property int index
            required property bool isDefault
            required property string profileName
            required property string name
            required property string accountId
            visible: false
            width: 0
            height: 0
            onIsDefaultChanged: root.rebuildAccounts()
            onIndexChanged: root.rebuildAccounts()
            Component.onCompleted: root.rebuildAccounts()
            Component.onDestruction: Qt.callLater(root.rebuildAccounts)
        }
    }

    // One size for every head here, so the names share a column; a
    // multiple of 8, the face's own pixel grid (see PlayerHead).
    readonly property int headSize: 32
    // Changes when the game's default skins arrive, so a skinless account's
    // head is fetched again (AccountsController.defaultSkinsRevision).
    readonly property int faceRevision: root.controller && root.controller.defaultSkinsRevision !== undefined
                                        ? root.controller.defaultSkinsRevision : 0

    width: 300
    padding: Theme.space.xxs
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.space.sm
            // The same inset as the account rows below (ItemDelegate's
            // horizontal padding), so avatars and names share one column.
            Layout.leftMargin: Theme.space.md
            Layout.rightMargin: Theme.space.md
            spacing: Theme.space.sm

            // The player's head, as on the button that opened this menu.
            PlayerHead {
                size: root.headSize
                source: root.avatarSource
            }

            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 1

                Text {
                    width: parent.width
                    text: root.name
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.bodyStrong.pixelSize
                    font.weight: Theme.type.bodyStrong.weight
                }
                Text {
                    width: parent.width
                    text: root.kind === "Microsoft" ? qsTr("Microsoft account")
                        : root.kind === "Offline" ? qsTr("Offline account") : root.kind
                    elide: Text.ElideRight
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                }
            }
        }

        MenuSeparator { Layout.fillWidth: true; visible: root.otherAccounts.length > 0 }

        // The other accounts, one click to switch to -- each row is the
        // player's head and name, which says what it does without a caps
        // "SWITCH ACCOUNT" overline above it.
        Repeater {
            model: root.otherAccounts
            delegate: ItemDelegate {
                id: switchRow
                required property var modelData
                Layout.fillWidth: true
                hoverEnabled: true
                onClicked: {
                    if (root.controller)
                        root.controller.setDefault(switchRow.modelData.row)
                    root.close()
                }

                // The row's own head (PlayerHead), not ItemDelegate's
                // alpha-recoloured icon slot -- a skin texture must never be
                // tinted like a line icon.
                contentItem: RowLayout {
                    spacing: Theme.space.sm

                    PlayerHead {
                        size: root.headSize
                        source: switchRow.modelData.accountId.length > 0
                                ? "image://accountface/" + switchRow.modelData.accountId
                                  + "?rev=" + root.faceRevision
                                : ""
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: switchRow.modelData.name
                        elide: Text.ElideRight
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.body.pixelSize
                    }
                    // Defensive only: otherAccounts excludes the default row
                    // by construction (see rebuildAccounts), so this never
                    // shows today -- but if it is ever listed, it should
                    // read as the active one rather than an identical,
                    // ambiguous row.
                    MeshIcon {
                        visible: switchRow.modelData.isDefault === true
                        iconName: "check"
                        size: Theme.icon.sm
                        color: Theme.palette.accent
                    }
                }
            }
        }

        MenuSeparator { Layout.fillWidth: true }

        ItemDelegate {
            Layout.fillWidth: true
            hoverEnabled: true
            text: qsTr("Skin and cape")
            icon.source: Icons.url("image")
            onClicked: { root.close(); root.openAccountsRequested() }
        }
        ItemDelegate {
            Layout.fillWidth: true
            hoverEnabled: true
            text: qsTr("Manage accounts")
            icon.source: Icons.url("users")
            onClicked: { root.close(); root.openAccountsRequested() }
        }
        ItemDelegate {
            Layout.fillWidth: true
            hoverEnabled: true
            text: qsTr("Add account")
            icon.source: Icons.url("plus")
            onClicked: { root.close(); root.openAccountsRequested() }
        }

        MenuSeparator { Layout.fillWidth: true }

        ItemDelegate {
            Layout.fillWidth: true
            hoverEnabled: true
            text: qsTr("Sign out")
            icon.source: Icons.url("log-out")
            onClicked: {
                root.close()
                signOutDialog.open()
            }
        }
    }

    ConfirmDialog {
        id: signOutDialog
        title: qsTr("Sign out")
        text: qsTr("Sign out of “%1”? You can sign in again at any time.").arg(root.name)
        confirmText: qsTr("Sign out")
        onConfirmed: if (root.controller && root.defaultRow >= 0) root.controller.remove(root.defaultRow)
    }
}
