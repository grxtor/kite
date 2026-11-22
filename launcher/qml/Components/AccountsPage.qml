// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The accounts games launch with: the default account on a card of its own (AccountHeroCard) and the others as
 * rows of one card (AccountRow), under a header that adds an account.
 */
Item {
    id: root

    // AccountsController.
    property var controller: null
    // MicrosoftLoginController of the sign-in in progress; held here because the page is never destroyed.
    property var login: null

    readonly property var accounts: controller ? controller.accounts : null

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
        id: probes
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
            required property string lastError
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
                                    : root.count > 0 ? 0 : -1
    property var heroItem: null
    readonly property int count: probes.count
    readonly property bool heroIsMSA: !!root.debugHero || (!!heroItem && heroItem.isMSA)
    readonly property bool heroIsDefault: !!heroItem && heroItem.isDefault
    readonly property string heroName: heroItem
        ? (heroItem.profileName.length > 0 ? heroItem.profileName : heroItem.name) : ""
    readonly property string heroAccountId: heroItem ? heroItem.accountId : ""
    readonly property string heroStatus: root.debugHero ? root.debugHero.status : heroItem ? heroItem.status : ""
    readonly property string heroStateKey: root.debugHero ? "online" : heroItem ? heroItem.stateKey : ""
    // Expired, errored or gone: the hero says why and offers "Sign in again" in the open.
    readonly property bool heroAttention: AccountStates.needsAttention(root.heroStateKey)
    readonly property string heroReason: heroItem && root.heroAttention ? heroItem.lastError : ""
    readonly property int heroRow: heroItem ? heroItem.index : -1
    property bool skin3DAvailable: false
    // accountSkinInfo() of the hero row; re-read with imageRevision.
    readonly property var heroSkin: {
        const info = root.controller && root.heroRow >= 0 && root.imageRevision >= 0
                   ? root.controller.accountSkinInfo(root.heroRow) : ({})
        return root.debugHero ? Object.assign({}, info, root.debugHero.skin) : info
    }
    readonly property var heroCapes: root.heroIsMSA && root.heroSkin.valid && root.heroSkin.capes
                                     ? root.heroSkin.capes : []

    // Review renders only (KITE_QML_ROUTE "accountcapes"): the sample accounts are offline and own no capes, so
    // the hero is shown as a signed-in Microsoft account with stand-in capes cut from bundled instance icons.
    property var debugHero: null
    function debugShowCapes() {
        const icons = "qrc:/icons/multimc/128x128/instances/"
        const capes = [{ id: "a", alias: "Migrator", url: icons + "kite_amethyst.png" },
                       { id: "b", alias: "Vanilla", url: icons + "kite_copper.png" },
                       { id: "c", alias: "Minecon 2016", url: icons + "kite_emerald_ore.png" },
                       { id: "d", alias: "Pan", url: "" }]
        root.debugHero = { status: qsTranslate("AccountList", "Online", "Account status"),
                           skin: { valid: true, capes: capes, currentCapeId: "b" } }
    }

    AccountCapeChange {
        id: capeChange
        controller: root.debugHero ? null : root.controller
        row: root.heroRow
        accountId: root.heroAccountId
        onWorn: hero.showBack()
    }
    function wearCape(capeId) {
        if (root.debugHero)
            root.debugHero = { status: root.debugHero.status,
                               skin: Object.assign({}, root.debugHero.skin, { currentCapeId: capeId }) }
        else
            capeChange.wear(capeId)
    }

    function askRemove(row, name) {
        removeDialog.row = row
        removeDialog.text = qsTr("Remove “%1” from Kite? You can sign in again at any time.").arg(name)
        removeDialog.open()
    }

    Flickable {
        id: scroll
        anchors.fill: parent
        anchors.bottomMargin: Theme.space.lg
        visible: root.count > 0
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: page.implicitHeight + Theme.space.xl
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: page
            readonly property int sideMargin: Theme.space.xl + Theme.space.xs
            x: Math.max(sideMargin, Math.round((scroll.width - width) / 2))
            y: Theme.space.xl
            width: Math.min(scroll.width - sideMargin * 2, Theme.layout.maxContentWidth)
            spacing: Theme.space.lg

            AccountsHeader {
                Layout.fillWidth: true
                hasDefault: root.heroIsDefault
                onMicrosoftRequested: root.startMicrosoftLogin()
                onOfflineRequested: offlineDialog.open()
            }

            AccountHeroCard {
                id: hero
                Layout.fillWidth: true
                visible: !!root.heroItem
                name: root.heroName
                accountId: root.heroAccountId
                isMSA: root.heroIsMSA
                isDefault: root.heroIsDefault
                stateKey: root.heroStateKey
                status: root.heroStatus
                reason: root.heroReason
                skin: root.heroSkin
                capes: root.heroCapes
                capeBusy: capeChange.busy
                capeError: capeChange.error
                rev: root.imageRevision
                skin3DAvailable: root.skin3DAvailable
                onMakeDefaultRequested: root.controller.setDefault(root.heroRow)
                onRefreshRequested: root.controller.refresh(root.heroRow)
                onRemoveRequested: root.askRemove(root.heroRow, root.heroName)
                onSkinRequested: root.openSkinEditor(root.heroRow, root.heroAccountId, root.heroName)
                onCapeRequested: (capeId) => root.wearCape(capeId)
            }

            SectionHeader {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space.sm
                visible: root.count > 1
                collapsible: false
                title: qsTr("Other accounts")
                count: root.count - 1
            }

            FriendsListCard {
                id: others
                Layout.fillWidth: true
                visible: root.count > 1

                ListEntrance { id: entrance; view: rows }
                Repeater {
                    id: rows
                    model: root.accounts
                    delegate: AccountRow {
                        id: row
                        width: others.rowWidth
                        hidden: index === root.heroIndex
                        // The hero's row is skipped, so the first row shown is the second when the hero is the first.
                        divider: index > (root.heroIndex === 0 ? 1 : 0)
                        rev: root.imageRevision
                        onMakeDefaultRequested: root.controller.setDefault(index)
                        onRefreshRequested: root.controller.refresh(index)
                        onManageSkinRequested: { if (isMSA) root.openSkinEditor(index, accountId, shownName) }
                        onRemoveRequested: root.askRemove(index, shownName)
                        RiseIn { target: row; index: row.index; gate: entrance }
                    }
                }
            }
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: scroll.bottom
        visible: scroll.visible
        flickable: scroll
    }

    // An offline account needs a Microsoft one that owns the game first (AccountsController.addOffline), so with
    // no account at all signing in is the one way forward.
    EmptyState {
        anchors.centerIn: parent
        visible: root.count === 0
        title: qsTr("No accounts yet")
        body: qsTr("Sign in with the Microsoft account that owns Minecraft to start playing online.")
        actionText: qsTr("Sign in with Microsoft")
        actionIcon: "user"
        iconName: "user"
        onActionTriggered: root.startMicrosoftLogin()
    }

    ConfirmDialog {
        id: removeDialog
        property int row: -1
        title: qsTr("Remove account")
        confirmText: qsTr("Remove")
        onConfirmed: if (root.controller) root.controller.remove(row)
    }

    OfflineAccountDialog {
        id: offlineDialog
        controller: root.controller
    }

    AccountSignInDialog {
        id: loginDialog
        login: root.login
        onRetryRequested: root.login = root.controller.loginMicrosoft()
        onDismissed: root.login = null
    }

    SkinCapeEditor {
        id: skinCapeEditor
        controller: root.controller
        rev: root.imageRevision
        skin3DAvailable: root.skin3DAvailable
    }
}
