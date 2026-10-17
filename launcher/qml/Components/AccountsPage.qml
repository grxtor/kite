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
 * A hero at the top shows the account games actually launch with -- the
 * player in its skin (or the game's default Steve/Alex for an account
 * without one), in 3D where Qt Quick3D draws (SkinPreview), and a Microsoft
 * account's capes to wear in one click -- the same way the library's
 * ContinueCard/InstancePage hero leads with the instance that matters most.
 * Every other account is a compact row below it, as wide as the hero.
 *
 * "The default account" is one thing everywhere: the account switcher at
 * the top right (ProfileMenu), "Use this account" here and in the rows all
 * call AccountsController.setDefault(), games launch with
 * AccountList::defaultAccount(), and the hero is whichever row
 * AccountsController.defaultAccountId names.
 */
Item {
    id: root

    // AccountsController: accounts, hasDefault, defaultAccountId,
    // setDefault, remove, refresh, addOffline, loginMicrosoft,
    // accountSkinInfo, changeCape.
    property var controller: null
    // MicrosoftLoginController of the sign-in in progress, if any. Held
    // here, on a page that is never destroyed, for as long as it runs.
    property var login: null

    readonly property var accounts: controller ? controller.accounts : null

    // The dialogs' widths, before a narrow window takes its margin off.
    readonly property int offlineDialogWidth: 420
    readonly property int loginDialogWidth: 460
    readonly property int skinNoticeWidth: 380
    // The sign-in dialog's spinner.
    readonly property int loginIndicatorSize: 32
    // The empty state's avatar: a faint ring, a tinted disc, the icon.
    readonly property int avatarRingSize: 96
    readonly property int avatarDiscSize: 72
    readonly property int avatarIconSize: 32
    readonly property real avatarRingOpacity: 0.10

    // Bumped whenever any account's data changes (a refresh landing, a skin
    // upload finishing...), so the "image://accountface/..." urls below
    // change and QML actually refetches them -- it caches by url, and
    // nothing about the url itself would otherwise say a skin changed.
    property int imageRevision: 0
    Connections {
        target: root.accounts
        function onDataChanged() { root.imageRevision++ }
    }
    // The game's default skins arriving redraws every account without a
    // skin of its own (see DefaultSkins).
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

    // qml-preview-tools has only offline accounts to snapshot with, so no
    // real row's isMSA role ever lets the skin/cape editor open on its own;
    // MESHMC_QML_ROUTE containing "accounts-demo" opens it anyway, filled
    // with AccountsController::accountSkinInfo(-1)'s canned demo data. See
    // AccountsController::skinDemoRequested()'s own comment for why -1.
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

    /*
     * Which row is the default account: the one whose accountId is the
     * controller's defaultAccountId -- the account games launch with, read
     * straight from AccountList::defaultAccount() and notified the moment
     * it moves -- rather than any row's own isDefault role. AccountList has
     * no "here is the default row" query of its own, and a Repeater is the
     * cheapest way to look at every row without paging through count()/
     * data() by hand -- each probe is a zero-size Item, never drawn.
     */
    readonly property string defaultAccountId: root.controller ? root.controller.defaultAccountId : ""
    property int defaultIndex: -1
    Repeater {
        model: root.accounts
        delegate: Item {
            id: probe
            required property int index
            readonly property bool isDefault: root.defaultAccountId.length > 0
                                              && probe.accountId === root.defaultAccountId
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
            // Re-asserts on either change (isDefault flips, e.g. another
            // account was made default or this one was removed) or a plain
            // row-index shift (an unrelated row above this one was removed/
            // inserted) -- either can leave a stale index behind otherwise.
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
        // A sign-in in progress is not news: no blue chip (blue is in no
        // other place in the Grass scheme).
        case "working": return "neutral"
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
    // Whether launcher/qml/Cat (Qt Quick3D) exists -- see SkinPreview.
    property bool catAvailable: false
    // The hero account's skin facts (AccountsController::accountSkinInfo):
    // whether there is a skin to draw (its own, or the game's default for
    // it once downloaded), its arm width, and for a Microsoft account its
    // capes. Re-read whenever the images are (imageRevision).
    readonly property var heroSkin: root.controller && root.heroRow >= 0 && root.imageRevision >= 0
                                    ? root.controller.accountSkinInfo(root.heroRow) : ({})
    readonly property var heroCapes: root.heroIsMSA && root.heroSkin.valid && root.heroSkin.capes
                                     ? root.heroSkin.capes : []
    readonly property int heroHeight: 248
    readonly property int heroPreviewWidth: 168
    readonly property int heroPreviewHeight: 200

    // The cape change started from the hero, if any: its tiles wait while
    // it runs, and a failure is said under them.
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
                heroPreview.showBack()
        })
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        anchors.bottomMargin: Theme.space.lg
        spacing: Theme.space.lg

        // Every block below spans the same width as the header above the
        // page (its title on the left, the account button on the right),
        // up to Theme.layout.maxContentWidth on a wide window.
        RowLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            // Redundant with the empty state's own actions below once there
            // is nothing to manage yet.
            visible: list.count > 0
            spacing: Theme.space.sm

            // One short line, so it never wraps into a stray last word
            // beside the buttons; what a Microsoft account is needed for is
            // said where it matters (the offline hero's note, the empty
            // state, the offline dialog).
            Text {
                Layout.fillWidth: true
                text: qsTr("Games launch with the default account.")
                wrapMode: Text.Wrap
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            // Signing in with Microsoft is what this page is for, so it is
            // the one accent-filled action and comes first -- except while
            // the hero offers "Use this account" (no default chosen yet),
            // which is then the screen's one primary action instead.
            Button {
                highlighted: !root.heroItem || root.heroIsDefault
                text: qsTr("Sign in with Microsoft")
                icon.source: Icons.url("user")
                onClicked: root.startMicrosoftLogin()
            }
            Button {
                text: qsTr("Add offline account")
                onClicked: offlineDialog.open()
            }
        }

        // The account games actually launch with, big -- see heroIndex.
        Rectangle {
            id: hero
            Layout.fillWidth: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            visible: !!root.heroItem
            // Taller only when the cape row needs the room.
            implicitHeight: Math.max(root.heroHeight, heroContent.implicitHeight + Theme.space.xl * 2)
            radius: Theme.radius.xl
            // A flat, bordered surface: the player in the account's skin (or
            // the game's default one) is the hero's visual anchor, not a
            // decorative wash or gradient.
            color: Theme.palette.surfaceRaised
            border.width: 1
            border.color: Theme.palette.border

            RowLayout {
                id: heroContent
                anchors.fill: parent
                anchors.margins: Theme.space.xl
                spacing: Theme.space.xl

                // The player in the account's skin, with its cape, turned by
                // dragging where it is 3D (the flat front view otherwise).
                // An account without a skin of its own wears the game's
                // default for it, as in game; the neutral figure only holds
                // its place until that default has downloaded.
                SkinPreview {
                    id: heroPreview
                    Layout.preferredWidth: root.heroPreviewWidth
                    Layout.preferredHeight: root.heroPreviewHeight
                    Layout.alignment: Qt.AlignVCenter
                    catAvailable: root.catAvailable
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

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: root.heroName
                        elide: Text.ElideRight
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.display.pixelSize
                        font.weight: Theme.type.display.weight
                    }

                    // Which account games launch with is a fact about the
                    // account, so it sits with its other facts as a tag --
                    // not as a shouted label above the name.
                    Row {
                        spacing: Theme.space.sm
                        Tag {
                            iconName: root.heroIsDefault ? "check" : "user"
                            text: root.heroIsDefault ? qsTr("Default account") : qsTr("Suggested default")
                        }
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

                    Text {
                        Layout.fillWidth: true
                        visible: !root.heroIsMSA
                        // "Custom": the player already wears the game's
                        // default skin here, offline or not. Short enough
                        // to stay one line at the narrowest window.
                        text: qsTr("Offline only: online-mode servers and custom skins need a Microsoft account.")
                        wrapMode: Text.Wrap
                        color: Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }

                    // Theme.control.height like every other button on the
                    // page; the ⋯ is the same bordered square as the rows'
                    // icon buttons below (AccountRow.qml).
                    Row {
                        topPadding: Theme.space.sm
                        spacing: Theme.space.sm

                        Button {
                            visible: !root.heroIsDefault
                            highlighted: true
                            text: qsTr("Use this account")
                            onClicked: root.controller.setDefault(root.heroRow)
                        }
                        Button {
                            // "and", not "&": a Button reads "&" as a
                            // keyboard mnemonic and underlines the next letter.
                            text: qsTr("Skin and cape")
                            icon.source: Icons.url("image")
                            onClicked: root.heroIsMSA ? root.openSkinEditor(root.heroRow, root.heroAccountId, root.heroName)
                                                       : offlineSkinDialog.open()
                        }
                        // The rest behind ⋯, each with its own words, rather
                        // than bare refresh and bin glyphs beside "Skin and cape".
                        IconButton {
                            id: heroMoreButton
                            flat: false
                            iconName: "more"
                            tip: qsTr("More")
                            onClicked: heroMenu.popup(heroMoreButton, 0, heroMoreButton.height + Theme.space.xs)

                            Menu {
                                id: heroMenu
                                MenuItem {
                                    // A hidden item still takes its row in a
                                    // Menu; zero it as well.
                                    visible: root.heroIsMSA
                                    height: visible ? implicitHeight : 0
                                    text: qsTr("Sign in again")
                                    icon.source: Icons.url("refresh")
                                    onTriggered: root.controller.refresh(root.heroRow)
                                }
                                MenuItem {
                                    text: qsTr("Remove account\u2026")
                                    icon.source: Icons.url("trash")
                                    onTriggered: {
                                        removeDialog.row = root.heroRow
                                        removeDialog.text = qsTr("Remove “%1” from MeshMC? You can sign in again at any time.").arg(root.heroName)
                                        removeDialog.open()
                                    }
                                }
                            }
                        }
                    }

                    // A Microsoft account's capes, worn with one click --
                    // the same choice as in "Skin and cape", without opening
                    // it. Compact tiles; each one's name shows on hover.
                    Column {
                        visible: root.heroCapes.length > 0
                        topPadding: Theme.space.md
                        spacing: Theme.space.sm

                        Row {
                            spacing: Theme.space.sm
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("Cape")
                                color: Theme.palette.textSecondary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.label.pixelSize
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

                        Text {
                            width: heroContent.width - root.heroPreviewWidth - heroContent.spacing
                            visible: root.capeError.length > 0
                            text: root.capeError
                            wrapMode: Text.Wrap
                            color: Theme.palette.danger
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }
                }
            }
        }

        SectionHeader {
            Layout.fillWidth: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            visible: list.count > 1
            collapsible: false
            title: qsTr("Other accounts")
            count: list.count - 1
        }

        Item {
            Layout.fillWidth: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true

            // Rows run the full width, the hero card's own: the scroll bar
            // overlays them rather than keeping a gutter of its own.
            ListView {
                id: list
                anchors.fill: parent
                clip: true
                spacing: Theme.space.sm
                boundsBehavior: Flickable.StopAtBounds
                model: root.accounts
                currentIndex: root.heroIndex
                ScrollBar.vertical: ScrollBar {}

                delegate: AccountRow {
                    width: list.width
                    hidden: index === root.heroIndex
                    rev: root.imageRevision
                    onMakeDefaultRequested: root.controller.setDefault(index)
                    onRefreshRequested: root.controller.refresh(index)
                    onManageSkinRequested: isMSA ? root.openSkinEditor(index, accountId, shownName) : offlineSkinDialog.open()
                    onRemoveRequested: {
                        removeDialog.row = index
                        removeDialog.text = qsTr("Remove “%1” from MeshMC? You can sign in again at any time.").arg(shownName)
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

            Item {
                width: root.avatarRingSize; height: root.avatarRingSize
                Rectangle {
                    anchors.centerIn: parent
                    width: root.avatarRingSize; height: root.avatarRingSize
                    radius: width / 2
                    color: Theme.palette.accent
                    opacity: root.avatarRingOpacity
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: root.avatarDiscSize; height: root.avatarDiscSize
                    radius: width / 2
                    color: Theme.palette.accentSubtle
                }
                MeshIcon { anchors.centerIn: parent; iconName: "user"; size: root.avatarIconSize; color: Theme.palette.accent }
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
        width: Math.min(root.offlineDialogWidth, parent ? parent.width - Theme.space.xxl * 2 : root.offlineDialogWidth)
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
        width: Math.min(root.loginDialogWidth, parent ? parent.width - Theme.space.xxl * 2 : root.loginDialogWidth)
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
                    width: root.loginIndicatorSize; height: root.loginIndicatorSize
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
                    // The rest of the dialog's width, beside the spinner.
                    width: loginDialog.availableWidth - root.loginIndicatorSize - parent.spacing
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

    SkinCapeEditor {
        id: skinCapeEditor
        controller: root.controller
        rev: root.imageRevision
        catAvailable: root.catAvailable
    }

    Dialog {
        id: offlineSkinDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        // A fixed width, not clamped to the window like the others: it fits
        // the narrowest window anyway, and a width bound to the overlay
        // made this text-only dialog's height depend on itself.
        width: root.skinNoticeWidth
        modal: true
        title: qsTr("Skin and cape")

        contentItem: Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("Skins and capes belong to a Microsoft account. Sign in with the Microsoft account that owns Minecraft to change how this character looks.")
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
        }

        footer: Row {
            layoutDirection: Qt.RightToLeft
            spacing: Theme.space.sm
            padding: Theme.space.lg
            topPadding: 0
            Button {
                text: qsTr("Close")
                onClicked: offlineSkinDialog.close()
            }
        }
    }
}
