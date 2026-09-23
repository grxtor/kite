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
 *
 * A hero at the top shows the account games actually launch with -- full
 * body, standing on a small "stage" -- the same way the library's
 * ContinueCard/InstancePage hero leads with the instance that matters most.
 * Every other account is a compact row below it.
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

    /*
     * Which row is the default account, found by watching every row's own
     * isDefault role rather than asking AccountList for one directly: it
     * exposes the flag per row (for the list delegate below) but not as a
     * "here is the default index" query of its own. A Repeater is the
     * cheapest way to look at every row without paging through count()/
     * data() by hand -- each probe is a zero-size Item, never drawn.
     */
    property int defaultIndex: -1
    Repeater {
        model: root.accounts
        delegate: Item {
            id: probe
            required property int index
            required property bool isDefault
            // Everything the hero card shows, so it can read this probe
            // instead of the list's currentItem -- which ListView does not
            // reliably create for a row it lays out collapsed.
            required property string profileName
            required property string name
            required property bool isMSA
            required property string stateKey
            required property string status
            required property string accountId
            readonly property bool isHero: index === root.heroIndex
            onIsHeroChanged: claimHero()
            function claimHero() {
                if (isHero)
                    root.heroItem = probe
                else if (root.heroItem === probe)
                    root.heroItem = null
            }
            Component.onDestruction: if (root.heroItem === probe) root.heroItem = null
            visible: false
            width: 0
            height: 0
            // Re-asserts on either role change (isDefault flips, e.g. the
            // previous default was just removed) or a plain row-index shift
            // (an unrelated row above this one was removed/inserted) --
            // either can leave a stale index behind otherwise.
            function report() {
                if (isDefault)
                    root.defaultIndex = index
                else if (root.defaultIndex === index)
                    root.defaultIndex = -1
            }
            onIsDefaultChanged: report()
            onIndexChanged: report()
            Component.onCompleted: { report(); claimHero() }
        }
    }
    // Falls back to the first row so the hero still has someone to show
    // right after the very first account is added, before it is flagged
    // default.
    readonly property int heroIndex: root.defaultIndex >= 0 ? root.defaultIndex
                                    : list.count > 0 ? 0 : -1
    // The probe above for the hero's row (see its claimHero()).
    property var heroItem: null
    function stateTone(key) {
        switch (key) {
        case "online": return "success"
        case "working": return "info"
        case "errored": case "gone": return "danger"
        case "expired": return "warning"
        default: return "neutral"
        }
    }
    readonly property bool heroIsMSA: !!heroItem && heroItem.isMSA
    readonly property bool heroIsDefault: !!heroItem && heroItem.isDefault
    readonly property string heroName: heroItem
        ? (heroItem.profileName.length > 0 ? heroItem.profileName : heroItem.name) : ""
    readonly property string heroAccountId: heroItem ? heroItem.accountId : ""
    readonly property string heroStatus: heroItem ? heroItem.status : ""
    readonly property string heroStateKey: heroItem ? heroItem.stateKey : ""
    readonly property int heroRow: heroItem ? heroItem.index : -1

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        anchors.bottomMargin: Theme.space.lg
        spacing: Theme.space.lg

        RowLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 860
            // Redundant with the empty state's own actions below once there
            // is nothing to manage yet.
            visible: list.count > 0
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

        // The account games actually launch with, big -- see heroIndex.
        Rectangle {
            id: hero
            Layout.fillWidth: true
            Layout.maximumWidth: 860
            visible: !!root.heroItem
            implicitHeight: 248
            radius: Theme.radius.xl
            border.width: 1
            border.color: Theme.palette.border
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Format.shade(Theme.palette.accent, Theme.dark ? 0.20 : 0.90, 0.7) }
                GradientStop { position: 0.55; color: Theme.palette.surface }
                GradientStop { position: 1.0; color: Theme.palette.surface }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.space.xl
                spacing: Theme.space.xl

                // The stage: an accent glow, a floor ellipse, and either the
                // account's real skin or a neutral silhouette standing on it.
                Item {
                    id: stage
                    Layout.preferredWidth: 168
                    Layout.preferredHeight: 200
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: -6
                        width: 168; height: 168
                        radius: width / 2
                        color: Theme.palette.accent
                        opacity: 0.10
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 18
                        width: 120; height: 120
                        radius: width / 2
                        color: Theme.palette.accent
                        opacity: 0.16
                    }

                    // Soft floor shadow the figure appears to stand on. A
                    // flattened pill rather than a true ellipse -- Rectangle
                    // has no radial shape, but a wide, short rounded rect
                    // reads the same way at this size.
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        width: 116; height: 20
                        radius: height / 2
                        color: Theme.palette.accent
                        opacity: 0.22
                    }

                    // Neutral placeholder, shown underneath the body render:
                    // an offline account (or one whose texture has not
                    // loaded yet) gets a transparent image back from the
                    // provider, and this shows through -- same idiom as the
                    // face avatars' initial letter elsewhere on this page.
                    Item {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 12
                        width: 64
                        height: 170
                        opacity: 0.55

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 0
                            width: 30; height: 30
                            radius: width / 2
                            color: Theme.palette.textTertiary
                        }
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 34
                            width: 42; height: 58
                            radius: Theme.radius.lg
                            color: Theme.palette.textTertiary
                        }
                        Rectangle {
                            x: parent.width / 2 - 19
                            y: 94
                            width: 16; height: 68
                            radius: Theme.radius.sm
                            color: Theme.palette.textTertiary
                        }
                        Rectangle {
                            x: parent.width / 2 + 3
                            y: 94
                            width: 16; height: 68
                            radius: Theme.radius.sm
                            color: Theme.palette.textTertiary
                        }
                    }

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 12
                        width: 88
                        height: 176
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        source: root.heroAccountId.length > 0
                                ? "image://accountface/body/" + root.heroAccountId : ""
                        sourceSize: Qt.size(176, 352)
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: Theme.space.sm

                    Text {
                        text: (root.heroIsDefault ? qsTr("ACTIVE ACCOUNT") : qsTr("SUGGESTED DEFAULT")).toUpperCase()
                        color: Theme.palette.accent
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.overline.pixelSize
                        font.weight: Font.Bold
                        font.letterSpacing: Theme.type.overline.letterSpacing * 1.5
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: root.heroName
                        elide: Text.ElideRight
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.display.pixelSize
                        font.weight: Font.Bold
                        font.letterSpacing: -0.5
                    }

                    Row {
                        spacing: Theme.space.sm
                        Tag {
                            iconName: root.heroIsMSA ? "user" : "users"
                            text: root.heroIsMSA ? qsTr("Microsoft") : qsTr("Offline")
                        }
                        StatusBadge {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: root.heroIsMSA && root.heroStatus.length > 0
                            tone: root.stateTone(root.heroStateKey)
                            text: root.heroStatus
                        }
                    }

                    Row {
                        topPadding: Theme.space.sm
                        spacing: Theme.space.sm

                        Button {
                            height: Theme.control.heightLg
                            visible: !root.heroIsDefault
                            highlighted: true
                            text: qsTr("Use this account")
                            onClicked: root.controller.setDefault(root.heroRow)
                        }
                        IconButton {
                            size: Theme.control.heightLg
                            flat: false
                            visible: root.heroIsMSA
                            iconName: "refresh"
                            tip: qsTr("Sign in again")
                            onClicked: root.controller.refresh(root.heroRow)
                        }
                        IconButton {
                            size: Theme.control.heightLg
                            flat: false
                            iconName: "trash"
                            tip: qsTr("Remove account")
                            onClicked: {
                                removeDialog.row = root.heroRow
                                removeDialog.text = qsTr("Remove “%1” from MeshMC? You can sign in again at any time.").arg(root.heroName)
                                removeDialog.open()
                            }
                        }
                    }
                }
            }
        }

        SectionHeader {
            Layout.fillWidth: true
            Layout.maximumWidth: 860
            visible: list.count > 1
            collapsible: false
            title: qsTr("Other accounts")
            count: list.count - 1
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
            currentIndex: root.heroIndex
            ScrollBar.vertical: ScrollBar {}

            delegate: AccountRow {
                width: list.width - Theme.space.md
                hidden: index === root.heroIndex
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

    Column {
        anchors.centerIn: parent
        visible: list.count === 0
        spacing: Theme.space.sm

        EmptyState {
            anchors.horizontalCenter: parent.horizontalCenter
            title: qsTr("No accounts yet")
            body: qsTr("Sign in with the Microsoft account that owns Minecraft to start playing online.")
            actionText: qsTr("Sign in with Microsoft")
            actionIcon: "user"
            onActionTriggered: root.startMicrosoftLogin()

            Item {
                width: 96; height: 96
                Rectangle {
                    anchors.centerIn: parent
                    width: 96; height: 96
                    radius: width / 2
                    color: Theme.palette.accent
                    opacity: 0.10
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 72; height: 72
                    radius: width / 2
                    color: Theme.palette.accentSubtle
                }
                MeshIcon { anchors.centerIn: parent; iconName: "user"; size: 32; color: Theme.palette.accent }
            }
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            flat: true
            text: qsTr("Add offline account")
            onClicked: offlineDialog.open()
        }
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
