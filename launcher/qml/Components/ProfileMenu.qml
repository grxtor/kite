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

    width: 300
    padding: Theme.space.xxs
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.space.sm
            spacing: Theme.space.sm

            Rectangle {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: width / 2
                color: Theme.palette.accentSubtle
                border.width: 1
                border.color: Theme.palette.border

                Text {
                    anchors.centerIn: parent
                    visible: root.name.length > 0
                    text: root.name.charAt(0).toUpperCase()
                    color: Theme.palette.accent
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }
                // Drawn over the initial, not the other way around -- see
                // ProfileButton's own face for why.
                Image {
                    id: headerFace
                    anchors.fill: parent
                    anchors.margins: 1
                    source: root.avatarSource
                    visible: root.avatarSource.length > 0
                    smooth: false
                    sourceSize: Qt.size(80, 80)
                }
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

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space.md
            Layout.topMargin: Theme.space.xs
            visible: root.otherAccounts.length > 0
            text: qsTr("SWITCH ACCOUNT")
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.overline.pixelSize
            font.weight: Theme.type.overline.weight
            font.letterSpacing: Theme.type.overline.letterSpacing
        }

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

                // The row's own face, not ItemDelegate's alpha-recoloured
                // icon slot -- a skin texture must never be tinted like a
                // line icon. Same size and fallback-initial treatment as the
                // header row's face above, so an account without a loaded
                // texture reads the same way everywhere in this menu instead
                // of leaving a blank gap.
                contentItem: RowLayout {
                    spacing: Theme.space.sm

                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: width / 2
                        color: Theme.palette.accentSubtle
                        border.width: 1
                        border.color: Theme.palette.border

                        Text {
                            anchors.centerIn: parent
                            visible: switchRow.modelData.name.length > 0
                            text: switchRow.modelData.name.charAt(0).toUpperCase()
                            color: Theme.palette.accent
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.label.pixelSize
                            font.weight: Font.Bold
                        }
                        Image {
                            anchors.fill: parent
                            anchors.margins: 1
                            source: switchRow.modelData.accountId.length > 0
                                    ? "image://accountface/" + switchRow.modelData.accountId : ""
                            visible: switchRow.modelData.accountId.length > 0
                            smooth: false
                            sourceSize: Qt.size(80, 80)
                        }
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
