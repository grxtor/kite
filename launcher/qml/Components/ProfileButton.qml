// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The account, top-right on every page: the default account's Minecraft
 * head (PlayerHead), its name and a chevron, opening ProfileMenu below it.
 * As quiet as the toolbar's other flat buttons -- no box of its own, only
 * the hover plate -- so the head is what catches the eye. With no account
 * there is nothing to open -- a plain "Sign in" button goes straight to the
 * Accounts page instead (see openAccountsRequested()), the same page
 * ProfileMenu's own account actions land on.
 */
AbstractButton {
    id: control

    property string name: ""
    // "Microsoft", "Offline" or "" when there is no account.
    property string kind: ""
    property string avatarSource: ""
    // AccountsController: accounts, setDefault, remove -- see ProfileMenu.
    property var controller: null

    readonly property bool signedIn: control.name.length > 0
    readonly property int maxNameWidth: 130
    // Three texels of the 8x8 face per pixel step: crisp at 1x and 2x.
    readonly property int headSize: 24

    signal openAccountsRequested()

    // For a dev-route snapshot ("profilemenu") that needs the dropdown open
    // without a real click.
    function openMenu() { if (control.signedIn) menu.open() }

    leftPadding: control.signedIn ? Theme.space.sm : Theme.space.md
    rightPadding: control.signedIn ? Theme.space.sm : Theme.space.md
    implicitWidth: contentRow.implicitWidth + leftPadding + rightPadding
    implicitHeight: Theme.control.height
    hoverEnabled: true

    Accessible.name: signedIn ? qsTr("Account menu: %1").arg(control.name) : qsTr("Sign in")

    onClicked: control.signedIn ? menu.toggle() : control.openAccountsRequested()

    // A flat button's plate: nothing at rest, the shared overlay on hover
    // and while its menu is open. "Sign in" keeps a border, as the one
    // thing to do here.
    background: Rectangle {
        radius: Theme.radius.md
        color: control.down ? Theme.palette.pressedOverlay
             : control.hovered || menu.visible ? Theme.palette.hoverOverlay : "transparent"
        border.width: control.signedIn ? 0 : 1
        border.color: Theme.palette.border

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    // A plain Item, not the Row itself, is the contentItem: QQC2 stretches
    // contentItem to fill the button's whole content box, and a Row whose
    // own width is forced wider than its packed children just leaves the
    // surplus as dead space after the last one (the chevron). Centering the
    // Row inside this wrapper instead keeps the Row itself sized to its
    // content, so the plate hugs it.
    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: Theme.space.sm

            PlayerHead {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.signedIn
                size: control.headSize
                source: control.signedIn ? control.avatarSource : ""
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.signedIn
                width: Math.min(implicitWidth, control.maxNameWidth)
                text: control.name
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.weight: Font.DemiBold
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: !control.signedIn
                text: qsTr("Sign in")
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.weight: Font.DemiBold
            }

            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.signedIn
                iconName: "chevron-down"
                size: Theme.icon.sm
                color: Theme.palette.textSecondary
                rotation: menu.visible ? 180 : 0

                Behavior on rotation { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            }
        }
    }

    ProfileMenu {
        id: menu
        x: control.width - width
        y: control.height + Theme.space.xs
        name: control.name
        kind: control.kind
        avatarSource: control.avatarSource
        controller: control.controller
        function toggle() { visible ? close() : open() }
        onOpenAccountsRequested: control.openAccountsRequested()
    }
}
