// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The accounts games are launched with. Microsoft sign-in happens in the
 * browser; this page says so while it waits, and can reopen the page if
 * the browser tab was lost. Offline accounts need a Microsoft account that
 * owns the game first -- the same rule the classic page enforced.
 */
Item {
    id: root

    // AccountsController: accounts, hasDefault, setDefault, remove,
    // refresh, addOffline, loginMicrosoft.
    property var controller: null
    // MicrosoftLoginController of the sign-in in progress, if any. Held
    // here, on a page that is never destroyed, for as long as it runs.
    property var login: null

    readonly property var accounts: controller ? controller.accounts : null
    function startMicrosoftLogin() {
        if (!root.controller)
            return
        root.login = root.controller.loginMicrosoft()
        loginDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        anchors.bottomMargin: Theme.space.lg
        spacing: Theme.space.lg

        RowLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 860
            spacing: Theme.space.sm

            Text {
                Layout.fillWidth: true
                text: qsTr("Games launch with the default account. Sign in with the Microsoft account that owns Minecraft.")
                wrapMode: Text.Wrap
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            Button {
                text: qsTr("Add offline account")
                onClicked: offlineDialog.open()
            }
            Button {
                highlighted: true
                text: qsTr("Sign in with Microsoft")
                icon.source: Icons.url("plus")
                onClicked: root.startMicrosoftLogin()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.maximumWidth: 860
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space.sm
            boundsBehavior: Flickable.StopAtBounds
            model: root.accounts
            ScrollBar.vertical: ScrollBar {}

            delegate: AccountRow {
                width: list.width - Theme.space.md
                onMakeDefaultRequested: root.controller.setDefault(index)
                onRefreshRequested: root.controller.refresh(index)
                onRemoveRequested: {
                    removeDialog.row = index
                    removeDialog.text = qsTr("Remove “%1” from MeshMC? You can sign in again at any time.").arg(shownName)
                    removeDialog.open()
                }
            }
        }
    }

    EmptyState {
        anchors.centerIn: parent
        visible: list.count === 0
        title: qsTr("No accounts yet")
        body: qsTr("Sign in with the Microsoft account that owns Minecraft to start playing online.")
        actionText: qsTr("Sign in with Microsoft")
        actionIcon: "user"
        onActionTriggered: root.startMicrosoftLogin()
        MeshIcon { iconName: "user"; size: 40; color: Theme.palette.textTertiary }
    }

    ConfirmDialog {
        id: removeDialog
        property int row: -1
        title: qsTr("Remove account")
        confirmText: qsTr("Remove")
        onConfirmed: if (root.controller) root.controller.remove(row)
    }

    Dialog {
        id: offlineDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        modal: true
        title: qsTr("Add offline account")
        onOpened: {
            usernameField.text = ""
            offlineError.text = ""
            usernameField.forceActiveFocus()
        }

        contentItem: Column {
            spacing: Theme.space.sm
            Text {
                width: parent.width
                text: qsTr("The name shown in game. Offline accounts cannot join online-mode servers.")
                wrapMode: Text.Wrap
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            TextField {
                id: usernameField
                width: parent.width
                placeholderText: qsTr("Username")
                selectByMouse: true
                onAccepted: addButton.clicked()
            }
            Text {
                id: offlineError
                width: parent.width
                visible: text.length > 0
                wrapMode: Text.Wrap
                color: Theme.palette.danger
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }

        footer: Row {
            layoutDirection: Qt.RightToLeft
            spacing: Theme.space.sm
            padding: Theme.space.lg
            topPadding: 0
            Button {
                id: addButton
                text: qsTr("Add account")
                highlighted: true
                enabled: usernameField.text.trim().length > 0
                onClicked: {
                    if (root.controller && root.controller.addOffline(usernameField.text))
                        offlineDialog.close()
                    else
                        offlineError.text = qsTr("Offline accounts need a Microsoft account that owns the game first, and a name no other offline account uses.")
                }
            }
            Button {
                text: qsTr("Cancel")
                flat: true
                onClicked: offlineDialog.close()
            }
        }
    }

    Dialog {
        id: loginDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 460
        modal: true
        closePolicy: Popup.NoAutoClose
        title: qsTr("Sign in with Microsoft")

        readonly property bool done: !!root.login && root.login.succeeded
        readonly property bool failed: !!root.login && root.login.failed

        contentItem: Column {
            spacing: Theme.space.md

            Row {
                spacing: Theme.space.md
                BusyIndicator {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !loginDialog.done && !loginDialog.failed
                    running: visible
                    width: 32; height: 32
                }
                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: loginDialog.done || loginDialog.failed
                    iconName: loginDialog.done ? "check" : "alert-triangle"
                    size: Theme.icon.lg
                    color: loginDialog.done ? Theme.palette.success : Theme.palette.danger
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 360
                    text: loginDialog.done ? qsTr("Signed in. You're ready to play.")
                        : loginDialog.failed ? (root.login.error || qsTr("Sign-in failed."))
                        : (root.login && root.login.status.length > 0 ? root.login.status
                                                                       : qsTr("Continue in your browser…"))
                    wrapMode: Text.Wrap
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                }
            }

            Text {
                width: parent.width
                visible: !loginDialog.done && !loginDialog.failed
                text: qsTr("A browser window opened on Microsoft's sign-in page. Sign in there; this window updates by itself.")
                wrapMode: Text.Wrap
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
        }

        footer: Row {
            layoutDirection: Qt.RightToLeft
            spacing: Theme.space.sm
            padding: Theme.space.lg
            topPadding: 0
            Button {
                visible: loginDialog.done
                highlighted: true
                text: qsTr("Done")
                onClicked: { loginDialog.close(); root.login = null }
            }
            Button {
                visible: loginDialog.failed
                highlighted: true
                text: qsTr("Try again")
                onClicked: { root.login = root.controller.loginMicrosoft() }
            }
            Button {
                visible: !loginDialog.done && !loginDialog.failed && !!root.login
                         && root.login.browserUrl.toString().length > 0
                text: qsTr("Open browser again")
                icon.source: Icons.url("external-link")
                onClicked: root.login.openBrowser()
            }
            Button {
                visible: !loginDialog.done
                flat: true
                text: qsTr("Cancel")
                onClicked: {
                    if (root.login && root.login.running)
                        root.login.cancel()
                    loginDialog.close()
                    root.login = null
                }
            }
        }
    }
}
