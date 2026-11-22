// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * A Microsoft sign-in under way: what the MicrosoftLoginController (`login`) says, the way back to the browser, and
 * Done, Try again or Cancel. The page owns the login; `retryRequested` asks it for a new attempt, `dismissed` to
 * let go of the one shown.
 */
AppDialog {
    id: loginDialog

    // MicrosoftLoginController of the attempt shown, or null.
    property var login: null
    readonly property int indicatorSize: 32

    signal retryRequested()
    signal dismissed()

    size: "sm"
    closePolicy: Popup.NoAutoClose
    title: qsTr("Sign in with Microsoft")

    readonly property bool done: !!loginDialog.login && loginDialog.login.succeeded
    readonly property bool failed: !!loginDialog.login && loginDialog.login.failed

    contentItem: Column {
        spacing: Theme.space.md

        Row {
            spacing: Theme.space.md
            BusyIndicator {
                anchors.verticalCenter: parent.verticalCenter
                visible: !loginDialog.done && !loginDialog.failed
                running: visible
                width: loginDialog.indicatorSize; height: loginDialog.indicatorSize
            }
            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: loginDialog.done || loginDialog.failed
                iconName: loginDialog.done ? "check" : "alert-triangle"
                size: Theme.icon.lg
                color: loginDialog.done ? Theme.palette.success : Theme.palette.danger
            }
            Copy {
                anchors.verticalCenter: parent.verticalCenter
                width: loginDialog.availableWidth - loginDialog.indicatorSize - parent.spacing
                text: loginDialog.done ? qsTr("Signed in. You're ready to play.")
                    : loginDialog.failed ? (loginDialog.login.error || qsTr("Sign-in failed."))
                    : (loginDialog.login && loginDialog.login.status.length > 0 ? loginDialog.login.status
                                                                   : qsTr("Continue in your browser…"))
                wrapMode: Text.Wrap
            }
        }

        Copy {
            width: parent.width
            visible: !loginDialog.done && !loginDialog.failed
            text: qsTr("A browser window opened on Microsoft's sign-in page. Sign in there; this window updates by itself.")
            role: "label"
            tone: "tertiary"
            wrapMode: Text.Wrap
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.xl
        topPadding: 0
        Button {
            visible: loginDialog.done
            variant: "primary"
            text: qsTr("Done")
            onClicked: { loginDialog.close(); loginDialog.dismissed() }
        }
        Button {
            visible: loginDialog.failed
            variant: "primary"
            text: qsTr("Try again")
            onClicked: { loginDialog.retryRequested() }
        }
        Button {
            visible: !loginDialog.done && !loginDialog.failed && !!loginDialog.login
                     && loginDialog.login.browserUrl.toString().length > 0
            text: qsTr("Open browser again")
            icon.source: Icons.url("external-link")
            onClicked: loginDialog.login.openBrowser()
        }
        Button {
            visible: !loginDialog.done
            variant: "secondary"
            text: qsTr("Cancel")
            onClicked: {
                if (loginDialog.login && loginDialog.login.running)
                    loginDialog.login.cancel()
                loginDialog.close()
                loginDialog.dismissed()
            }
        }
    }
}
