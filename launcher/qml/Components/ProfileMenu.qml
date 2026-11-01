// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The account dropdown opened from ProfileButton: who is playing, a quick switch
 * to another signed-in account and sign-out. Anything longer (signing in,
 * offline accounts, skins) goes to the Accounts page (openAccountsRequested()).
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

    // Every other account's row and shown name, plus the default row (for "Sign out"), gathered
    // like AccountsPage finds its hero row: a zero-size probe per row.
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

    // One size for every head so names share a column; a multiple of 8, the face's pixel grid (see
    // PlayerHead).
    readonly property int headSize: 32
    // Changes when the default skins arrive, so a skinless account's head is fetched again.
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
            Layout.leftMargin: Theme.space.md
            Layout.rightMargin: Theme.space.md
            spacing: Theme.space.sm

            PlayerHead {
                size: root.headSize
                source: root.avatarSource
            }

            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 1

                Copy {
                    width: parent.width
                    text: root.name
                    role: "bodyStrong"
                    elide: Text.ElideRight
                }
                Copy {
                    width: parent.width
                    text: root.kind === "Microsoft" ? qsTr("Microsoft account")
                        : root.kind === "Offline" ? qsTr("Offline account") : root.kind
                    role: "caption"
                    tone: "tertiary"
                    elide: Text.ElideRight
                }
            }
        }

        MenuSeparator { Layout.fillWidth: true; visible: root.otherAccounts.length > 0 }

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

                // PlayerHead, not ItemDelegate's alpha-recoloured icon slot: a skin texture must
                // never be tinted like a line icon.
                contentItem: RowLayout {
                    spacing: Theme.space.sm

                    PlayerHead {
                        size: root.headSize
                        source: switchRow.modelData.accountId.length > 0
                                ? "image://accountface/" + switchRow.modelData.accountId
                                  + "?rev=" + root.faceRevision
                                : ""
                    }
                    Copy {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: switchRow.modelData.name
                        elide: Text.ElideRight
                    }
                    // Defensive: otherAccounts excludes the default row, but if it is ever listed
                    // it should read as the active one.
                    MeshIcon {
                        visible: switchRow.modelData.isDefault === true
                        iconName: "check"
                        size: Theme.icon.sm
                        color: Theme.palette.accentText
                    }
                }
            }
        }

        MenuSeparator { Layout.fillWidth: true }

        ItemDelegate {
            Layout.fillWidth: true
            // Offline accounts have no skin control on the Accounts page.
            visible: root.kind !== "Offline"
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
