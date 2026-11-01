// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/* The accounts games launch with: a hero for the default account and a row for each other one. */
Item {
    id: root

    // AccountsController.
    property var controller: null
    // MicrosoftLoginController of the sign-in in progress; held here because the page is never destroyed.
    property var login: null

    readonly property var accounts: controller ? controller.accounts : null

    readonly property int loginIndicatorSize: 32

    // Bumped on any account data change so the image://accountface urls change; QML caches by url.
    property int imageRevision: 0
    Connections {
        target: root.accounts
        function onDataChanged() { root.imageRevision++ }
    }
    Connections {
        target: root.controller
        function onDefaultSkinsRevisionChanged() { root.imageRevision++ }
    }

    function openSkinEditor(row, accountId, accountName) {
        if (!root.controller)
            return
        skinCapeEditor.row = row
        skinCapeEditor.accountId = accountId
        skinCapeEditor.accountName = accountName || ""
        skinCapeEditor.open()
    }

    // Preview tools have only offline accounts: KITE_QML_ROUTE "accounts-demo" opens the skin editor with demo data.
    Component.onCompleted: {
        if (root.controller && root.controller.skinDemoRequested)
            root.openSkinEditor(-1, "", qsTr("Demo"))
    }
    function startMicrosoftLogin() {
        if (!root.controller)
            return
        root.login = root.controller.loginMicrosoft()
        loginDialog.open()
    }

    // The default account's row: its accountId against the controller's defaultAccountId, probed per row by
    // zero-size items.
    readonly property string defaultAccountId: root.controller ? root.controller.defaultAccountId : ""
    property int defaultIndex: -1
    Repeater {
        model: root.accounts
        delegate: Item {
            id: probe
            required property int index
            readonly property bool isDefault: root.defaultAccountId.length > 0
                                              && probe.accountId === root.defaultAccountId
            // What the hero shows; a ListView's currentItem is not reliably created for a collapsed row.
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
            // Re-asserts on isDefault or index changes; either can leave a stale index.
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
    // Falls back to the first row until the first account is flagged default.
    readonly property int heroIndex: root.defaultIndex >= 0 ? root.defaultIndex
                                    : list.count > 0 ? 0 : -1
    property var heroItem: null
    readonly property bool heroIsMSA: !!heroItem && heroItem.isMSA
    readonly property bool heroIsDefault: !!heroItem && heroItem.isDefault
    readonly property string heroName: heroItem
        ? (heroItem.profileName.length > 0 ? heroItem.profileName : heroItem.name) : ""
    readonly property string heroAccountId: heroItem ? heroItem.accountId : ""
    readonly property string heroStatus: heroItem ? heroItem.status : ""
    readonly property string heroStateKey: heroItem ? heroItem.stateKey : ""
    readonly property int heroRow: heroItem ? heroItem.index : -1
    property bool skin3DAvailable: false
    // accountSkinInfo() of the hero row; re-read with imageRevision.
    readonly property var heroSkin: root.controller && root.heroRow >= 0 && root.imageRevision >= 0
                                    ? root.controller.accountSkinInfo(root.heroRow) : ({})
    readonly property var heroCapes: root.heroIsMSA && root.heroSkin.valid && root.heroSkin.capes
                                     ? root.heroSkin.capes : []
    readonly property int heroHeight: 248
    readonly property int heroPreviewWidth: 168
    readonly property int heroPreviewHeight: 200

    // The hero's player turns to show a cape just put on.
    signal capeWorn()

    property var capeWatcher: null
    readonly property bool capeBusy: !!root.capeWatcher && root.capeWatcher.running
    property string capeError: ""
    onHeroAccountIdChanged: { root.capeWatcher = null; root.capeError = "" }
    function wearCape(capeId) {
        if (!root.controller || root.capeBusy)
            return
        root.capeError = ""
        var w = root.controller.changeCape(root.heroRow, capeId)
        if (!w) {
            root.capeError = qsTr("The cape could not be changed.")
            return
        }
        root.capeWatcher = w
        w.finished.connect(function(ok) {
            if (root.capeWatcher !== w)
                return
            if (!ok)
                root.capeError = w.error.length > 0 ? w.error : qsTr("The cape could not be changed.")
            else if (capeId.length > 0)
                root.capeWorn()
        })
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.space.xl
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        anchors.bottomMargin: Theme.space.lg
        spacing: Theme.space.lg

        RowLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            // The empty state has its own actions.
            visible: list.count > 0
            spacing: Theme.space.sm

            Copy {
                Layout.fillWidth: true
                text: qsTr("Games launch with the default account.")
                role: "label"
                tone: "tertiary"
                wrapMode: Text.Wrap
            }
            Button {
                id: addAccountButton
                variant: "secondary"
                text: qsTr("Add account")
                icon.source: Icons.url("plus")
                onClicked: addAccountMenu.popup(addAccountButton, 0, addAccountButton.height + Theme.space.xs)

                Menu {
                    id: addAccountMenu
                    MenuItem {
                        text: qsTr("Microsoft account")
                        icon.source: Icons.url("user")
                        onTriggered: root.startMicrosoftLogin()
                    }
                    MenuItem {
                        text: qsTr("Offline account")
                        onTriggered: offlineDialog.open()
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true

            ListView {
                id: list
                anchors.fill: parent
                clip: true
                // Rows carry their own gap (AccountRow.gap).
                spacing: 0
                boundsBehavior: Flickable.StopAtBounds
                model: root.accounts
                ScrollBar.vertical: ScrollBar {}

                // The hero and the list heading scroll with the rows, so a short window never squeezes "Other accounts" away.
                header: Item {
                    width: list.width
                    height: headerColumn.implicitHeight + (list.count > 1 ? Theme.space.lg : 0)

                    ColumnLayout {
                        id: headerColumn
                        width: parent.width
                        spacing: Theme.space.lg

                        BoxSurface {
                            id: hero
                            Layout.fillWidth: true
                            visible: !!root.heroItem
                            implicitHeight: Math.max(root.heroHeight, heroContent.implicitHeight + Theme.space.xl * 2)

                            Connections {
                                target: root
                                function onCapeWorn() { heroPreview.showBack() }
                            }

                            RowLayout {
                                id: heroContent
                                anchors.fill: parent
                                anchors.margins: Theme.space.xl
                                spacing: Theme.space.xl

                                // An account without a skin wears the game's default; a neutral figure holds its place until it
                                // downloads.
                                SkinPreview {
                                    id: heroPreview
                                    Layout.preferredWidth: root.heroPreviewWidth
                                    Layout.preferredHeight: root.heroPreviewHeight
                                    Layout.alignment: Qt.AlignVCenter
                                    skin3DAvailable: root.skin3DAvailable
                                    skinTexture: root.heroSkin.hasSkin && root.heroAccountId.length > 0
                                                 ? "image://accountface/skin/" + root.heroAccountId + "?rev=" + root.imageRevision : ""
                                    flatSource: root.heroAccountId.length > 0
                                                ? "image://accountface/body/" + root.heroAccountId + "?rev=" + root.imageRevision : ""
                                    slim: !!root.heroSkin.slim
                                    capeTexture: root.heroSkin.currentCapeUrl || ""
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: Theme.space.sm

                                    Copy {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: root.heroName
                                        role: "display"
                                        elide: Text.ElideRight
                                    }

                                    // The facts are one line of text; the state is the only badge.
                                    Row {
                                        spacing: Theme.space.sm
                                        Copy {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("%1  ·  %2")
                                                .arg(root.heroIsDefault ? qsTr("Default account") : qsTr("Suggested default"))
                                                .arg(root.heroIsMSA ? qsTr("Microsoft") : qsTr("Offline"))
                                            role: "label"
                                            tone: "secondary"
                                        }
                                        AccountStateBadge {
                                            anchors.verticalCenter: parent.verticalCenter
                                            visible: root.heroIsMSA && root.heroStatus.length > 0
                                            stateKey: root.heroStateKey
                                            text: root.heroStatus
                                        }
                                    }

                                    Copy {
                                        Layout.fillWidth: true
                                        visible: !root.heroIsMSA
                                        text: qsTr("Offline only: online-mode servers and custom skins need a Microsoft account.")
                                        role: "label"
                                        tone: "tertiary"
                                        wrapMode: Text.Wrap
                                    }

                                    Row {
                                        topPadding: Theme.space.sm
                                        spacing: Theme.space.sm

                                        Button {
                                            visible: !root.heroIsDefault
                                            variant: "primary"
                                            text: qsTr("Use this account")
                                            onClicked: root.controller.setDefault(root.heroRow)
                                        }
                                        Button {
                                            // Skins and capes belong to a Microsoft account.
                                            visible: root.heroIsMSA
                                            variant: "ghost"
                                            // "and", not "&": a Button reads "&" as a mnemonic.
                                            text: qsTr("Skin and cape")
                                            icon.source: Icons.url("image")
                                            onClicked: root.openSkinEditor(root.heroRow, root.heroAccountId, root.heroName)
                                        }
                                        IconButton {
                                            id: heroMoreButton
                                            iconName: "more"
                                            tip: qsTr("More")
                                            onClicked: heroMenu.popup(heroMoreButton, 0, heroMoreButton.height + Theme.space.xs)

                                            Menu {
                                                id: heroMenu
                                                MenuEntry {
                                                    visible: root.heroIsMSA
                                                    enabled: root.heroIsMSA
                                                    text: qsTr("Sign in again")
                                                    icon.source: Icons.url("refresh")
                                                    onTriggered: root.controller.refresh(root.heroRow)
                                                }
                                                MenuItem {
                                                    text: qsTr("Remove account\u2026")
                                                    icon.source: Icons.url("trash")
                                                    onTriggered: {
                                                        removeDialog.row = root.heroRow
                                                        removeDialog.text = qsTr("Remove “%1” from Kite? You can sign in again at any time.").arg(root.heroName)
                                                        removeDialog.open()
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Column {
                                        visible: root.heroCapes.length > 0
                                        topPadding: Theme.space.md
                                        spacing: Theme.space.sm

                                        Row {
                                            spacing: Theme.space.sm
                                            Copy {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: qsTr("Cape")
                                                role: "label"
                                                tone: "secondary"
                                                font.weight: Font.DemiBold
                                            }
                                            BusyIndicator {
                                                anchors.verticalCenter: parent.verticalCenter
                                                visible: root.capeBusy
                                                running: visible
                                                width: Theme.icon.sm
                                                height: Theme.icon.sm
                                            }
                                        }

                                        Flow {
                                            width: heroContent.width - root.heroPreviewWidth - heroContent.spacing
                                            spacing: Theme.space.sm

                                            CapeTile {
                                                compact: true
                                                label: qsTr("No cape")
                                                isNoCapeOption: true
                                                selected: !root.heroSkin.currentCapeId
                                                enabled: !root.capeBusy
                                                onClicked: if (!selected) root.wearCape("")
                                            }
                                            Repeater {
                                                model: root.heroCapes
                                                delegate: CapeTile {
                                                    required property var modelData
                                                    compact: true
                                                    label: modelData.alias
                                                    capeUrl: modelData.url
                                                    selected: modelData.id === root.heroSkin.currentCapeId
                                                    enabled: !root.capeBusy
                                                    onClicked: if (!selected) root.wearCape(modelData.id)
                                                }
                                            }
                                        }

                                        Copy {
                                            width: heroContent.width - root.heroPreviewWidth - heroContent.spacing
                                            visible: root.capeError.length > 0
                                            text: root.capeError
                                            role: "caption"
                                            tone: "danger"
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }
                            }
                        }

                        SectionHeader {
                            Layout.fillWidth: true
                            visible: list.count > 1
                            collapsible: false
                            title: qsTr("Other accounts")
                            count: list.count - 1
                        }
                    }
                }

                delegate: AccountRow {
                    width: list.width
                    hidden: index === root.heroIndex
                    gap: Theme.space.md
                    rev: root.imageRevision
                    onMakeDefaultRequested: root.controller.setDefault(index)
                    onRefreshRequested: root.controller.refresh(index)
                    onManageSkinRequested: { if (isMSA) root.openSkinEditor(index, accountId, shownName) }
                    onRemoveRequested: {
                        removeDialog.row = index
                        removeDialog.text = qsTr("Remove “%1” from Kite? You can sign in again at any time.").arg(shownName)
                        removeDialog.open()
                    }
                }
            }

            ScrollFade {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                flickable: list
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

            MeshIcon {
                iconName: "user"
                size: Theme.icon.xl
                color: Theme.palette.textTertiary
            }
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            variant: "ghost"
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

    AppDialog {
        id: offlineDialog
        size: "sm"
        closeOnPrimary: false
        title: qsTr("Add offline account")
        primaryText: qsTr("Add account")
        primaryEnabled: usernameField.text.trim().length > 0
        onPrimaryClicked: offlineDialog.submit()

        function submit() {
            if (root.controller && root.controller.addOffline(usernameField.text))
                offlineDialog.close()
            else
                offlineError.text = qsTr("Offline accounts need a Microsoft account that owns the game first, and a name no other offline account uses.")
        }

        onOpened: {
            usernameField.text = ""
            offlineError.text = ""
            usernameField.forceActiveFocus()
        }

        contentItem: Column {
            spacing: Theme.space.sm
            Copy {
                width: parent.width
                text: qsTr("The name shown in game. Offline accounts cannot join online-mode servers.")
                role: "label"
                tone: "secondary"
                wrapMode: Text.Wrap
            }
            TextField {
                id: usernameField
                width: parent.width
                placeholderText: qsTr("Username")
                selectByMouse: true
                onAccepted: if (offlineDialog.primaryEnabled) offlineDialog.submit()
            }
            Copy {
                id: offlineError
                width: parent.width
                visible: text.length > 0
                role: "caption"
                tone: "danger"
                wrapMode: Text.Wrap
            }
        }
    }

    AppDialog {
        id: loginDialog
        size: "sm"
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
                    width: root.loginIndicatorSize; height: root.loginIndicatorSize
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
                    width: loginDialog.availableWidth - root.loginIndicatorSize - parent.spacing
                    text: loginDialog.done ? qsTr("Signed in. You're ready to play.")
                        : loginDialog.failed ? (root.login.error || qsTr("Sign-in failed."))
                        : (root.login && root.login.status.length > 0 ? root.login.status
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
                onClicked: { loginDialog.close(); root.login = null }
            }
            Button {
                visible: loginDialog.failed
                variant: "primary"
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
                variant: "secondary"
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

    SkinCapeEditor {
        id: skinCapeEditor
        controller: root.controller
        rev: root.imageRevision
        skin3DAvailable: root.skin3DAvailable
    }
}
