// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Page header: title with an optional count and the account on one row,
 * a search field and whatever actions the page passes in as children on the
 * one below. It sits on the page itself rather than on its own band, so the
 * page reads as one surface.
 */
Item {
    id: root

    property string title: ""
    property int count: -1
    property string searchText: ""
    property string searchPlaceholder: qsTr("Search")
    property bool searchVisible: true

    // The account, on the title row's right, vertically centred on the
    // title -- the same spot on every page (see ProfileButton).
    // AccountsController: accounts, setDefault, remove.
    property string accountName: ""
    property string accountKind: ""
    property string accountAvatarSource: ""
    property var accountsController: null
    // "Skin and cape", "Manage accounts", "Add account" and the signed-out
    // "Sign in" button all land on the same place -- the Accounts page,
    // which already has the add/sign-in flow this never reimplements.
    signal openAccountsRequested()

    default property alias actions: actionsRow.data

    readonly property int titleRowHeight: Theme.control.height
    readonly property int toolbarRowHeight: Theme.control.heightLg
    // The toolbar row collapses to nothing on a page with no search/actions
    // (Settings, Discover, Accounts) rather than leaving an empty gap under
    // the title -- same total height those pages had before the account
    // moved out of their toolbar row and in with the title.
    implicitHeight: Theme.space.sm + titleRowHeight
                    + (root.searchVisible ? Theme.space.xs + toolbarRowHeight : 0)
                    + Theme.space.sm

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    // For a dev-route snapshot ("profilemenu") that needs the dropdown open
    // without a real click.
    function openProfileMenu() { profileButton.openMenu() }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        anchors.topMargin: Theme.space.sm
        anchors.bottomMargin: Theme.space.sm
        spacing: Theme.space.xs

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.titleRowHeight
            spacing: Theme.space.md

            Text {
                Layout.alignment: Qt.AlignVCenter
                text: root.title
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.heading.pixelSize + 4
                font.weight: Font.Bold
                font.letterSpacing: -0.3
            }

            Tag {
                Layout.alignment: Qt.AlignVCenter
                visible: root.count >= 0
                text: root.count
            }

            Item { Layout.fillWidth: true }

            ProfileButton {
                id: profileButton
                Layout.alignment: Qt.AlignVCenter
                name: root.accountName
                kind: root.accountKind
                avatarSource: root.accountAvatarSource
                controller: root.accountsController
                onOpenAccountsRequested: root.openAccountsRequested()
            }
        }

        // The search field gives up its width first under pressure --
        // sort/view/New instance keep their own size, since shrinking a
        // combo box or a labelled button reads as broken where a narrower
        // search field still reads as a search field.
        RowLayout {
            visible: root.searchVisible
            Layout.fillWidth: true
            Layout.preferredHeight: root.toolbarRowHeight
            spacing: Theme.space.md

            SearchBox {
                id: searchField
                Layout.fillWidth: true
                Layout.minimumWidth: 120
                Layout.maximumWidth: 300
                placeholderText: root.searchPlaceholder

                // TextField.text is a plain notifying property, not a
                // bindable one -- typing into it would otherwise permanently
                // sever a plain "text: root.searchText" binding. Both
                // directions are wired explicitly instead so external resets
                // of searchText still take.
                onTextChanged: if (text !== root.searchText) root.searchText = text
                Component.onCompleted: text = root.searchText

                Connections {
                    target: root
                    function onSearchTextChanged() {
                        if (searchField.text !== root.searchText)
                            searchField.text = root.searchText
                    }
                }
            }

            Row {
                id: actionsRow
                spacing: Theme.space.sm
            }
        }
    }
}
