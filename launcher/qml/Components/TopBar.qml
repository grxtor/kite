// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The shell's top bar: where you are on the left -- the page's title, with
 * an optional count (and, on the Library, the total play time), or inside
 * an instance a trail back up ("Library › Fabric Optimized") -- and the
 * account on the right, the same spot on every page. It sits on the shell
 * surface beside the sidebar's brand row, the two forming one band, so it
 * draws no background of its own.
 *
 * Its optional second row, a search field and the actions passed in as
 * children (a PageToolbar), is for a TopBar used on its own; the shell keeps
 * that row off (searchVisible: false) and puts the Library's toolbar inside
 * the page area instead.
 */
Item {
    id: root

    property string title: ""
    property int count: -1
    // The library's total play time after the title and count (see
    // TotalPlayTime; it hides itself while that setting is off).
    property bool showPlayTime: false
    // The way back up, before the title: labels, outermost first. Each one
    // is clickable (trailActivated(index)); the title itself is not.
    property var trail: []
    signal trailActivated(int index)

    property alias searchText: toolbar.searchText
    property alias searchPlaceholder: toolbar.searchPlaceholder
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

    default property alias actions: toolbar.actions

    readonly property int titleRowHeight: Theme.control.height
    readonly property int toolbarRowHeight: Theme.control.heightLg
    implicitHeight: Theme.space.sm + titleRowHeight
                    + (root.searchVisible ? Theme.space.xs + toolbarRowHeight : 0)
                    + Theme.space.sm

    function focusSearch() { toolbar.focusSearch() }

    // For a dev-route snapshot ("profilemenu") that needs the dropdown open
    // without a real click.
    function openProfileMenu() { profileButton.openMenu() }

    // The same column every page's content uses: the page padding, or on a
    // wide window whatever centres a Theme.layout.maxContentWidth column --
    // so the title and the account stay aligned with the page below them.
    readonly property int sideMargin: Math.max(Theme.space.xl + Theme.space.xs,
                                               Math.round((root.width - Theme.layout.maxContentWidth) / 2))

    // One step of the trail: the heading face, quieter than the title, and
    // a link back to that level.
    component TrailStep: AbstractButton {
        id: step
        required property string label
        required property int index
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        implicitWidth: stepText.implicitWidth
        implicitHeight: stepText.implicitHeight
        Accessible.role: Accessible.Link
        Accessible.name: step.label
        onClicked: root.trailActivated(step.index)

        contentItem: Item {}
        Text {
            id: stepText
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, step.width)
            text: step.label
            elide: Text.ElideRight
            color: step.hovered || step.visualFocus ? Theme.palette.textPrimary : Theme.palette.textTertiary
            font.family: Theme.font.heading
            font.pixelSize: Theme.type.heading.pixelSize
            font.weight: Theme.type.heading.weight
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.space.sm
        anchors.bottomMargin: Theme.space.sm
        spacing: Theme.space.xs

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: root.sideMargin
            Layout.rightMargin: root.sideMargin
            Layout.preferredHeight: root.titleRowHeight
            spacing: Theme.space.sm

            Repeater {
                model: root.trail
                delegate: RowLayout {
                    id: crumb
                    required property var modelData
                    required property int index
                    Layout.minimumWidth: 0
                    spacing: Theme.space.sm

                    TrailStep {
                        Layout.alignment: Qt.AlignVCenter
                        // Long names give way before the title does.
                        Layout.maximumWidth: 220
                        label: crumb.modelData
                        index: crumb.index
                    }
                    MeshIcon {
                        Layout.alignment: Qt.AlignVCenter
                        iconName: "chevron-right"
                        size: Theme.icon.md
                        color: Theme.palette.textTertiary
                    }
                }
            }

            Text {
                Layout.alignment: Qt.AlignVCenter
                // Its own width, down to nothing under pressure: a long
                // instance name elides before the account moves.
                Layout.minimumWidth: 0
                text: root.title
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.heading
                font.pixelSize: Theme.type.heading.pixelSize
                font.weight: Font.Bold
                font.letterSpacing: -0.2
            }

            Tag {
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: Theme.space.xs
                visible: root.count >= 0
                text: root.count
            }

            TotalPlayTime {
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: Theme.space.xs
                wanted: root.showPlayTime
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

        PageToolbar {
            id: toolbar
            visible: root.searchVisible
            Layout.fillWidth: true
            Layout.preferredHeight: root.toolbarRowHeight
        }
    }
}
