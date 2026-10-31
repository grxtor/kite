// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The account at the foot of the sidebar: the default account's head
 * (PlayerHead), opening ProfileMenu beside it. On the icon rail (`compact`) it is
 * the head alone, the name a tooltip. With no account a bordered "Sign in" goes
 * straight to the Accounts page.
 */
AbstractButton {
    id: control

    property string name: ""
    // "Microsoft", "Offline" or "" when there is no account.
    property string kind: ""
    property string avatarSource: ""
    // AccountsController: accounts, setDefault, remove.
    property var controller: null

    property bool compact: false
    property int railSquare: Theme.control.heightLg
    property int iconAxis: control.railSquare / 2
    property int labelX: control.iconAxis + control.headSize / 2 + Theme.space.sm
    property int cornerRadius: Theme.radius.md

    readonly property bool signedIn: control.name.length > 0
    // Four texels of the 8x8 face per pixel step: crisp at 1x and 2x.
    readonly property int headSize: 32

    signal openAccountsRequested()

    // For a dev-route snapshot ("profilemenu") that needs the dropdown open
    // without a real click.
    function openMenu() { if (control.signedIn) menu.open() }

    padding: 0
    implicitWidth: control.compact ? control.railSquare : 200
    implicitHeight: control.railSquare
    hoverEnabled: true

    Accessible.name: signedIn ? qsTr("Account menu: %1").arg(control.name) : qsTr("Sign in")

    onClicked: control.signedIn ? menu.toggle() : control.openAccountsRequested()

    ToolTip {
        visible: control.compact && control.hovered && !menu.visible
        delay: 400
        text: control.signedIn ? control.name : qsTr("Sign in")
        x: control.width + Theme.space.md
        y: Math.round((control.height - implicitHeight) / 2)
    }

    background: Rectangle {
        width: control.compact ? control.railSquare : control.width
        height: control.railSquare
        x: control.compact ? control.iconAxis - width / 2 : 0
        y: (control.height - height) / 2
        radius: control.cornerRadius
        color: control.down ? Theme.palette.pressedOverlay
             : control.hovered || menu.visible ? Theme.palette.hoverOverlay : "transparent"
        border.width: control.signedIn ? 0 : 1
        border.color: Theme.palette.border

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -Theme.space.xxs
            radius: parent.radius + Theme.space.xxs
            color: "transparent"
            border.width: 2
            border.color: Theme.palette.focusRing
            visible: control.visualFocus
        }
    }

    contentItem: Item {
        PlayerHead {
            x: control.iconAxis - width / 2
            anchors.verticalCenter: parent.verticalCenter
            visible: control.signedIn
            size: control.headSize
            source: control.signedIn ? control.avatarSource : ""
        }

        MeshIcon {
            x: control.iconAxis - width / 2
            anchors.verticalCenter: parent.verticalCenter
            visible: !control.signedIn
            iconName: "user"
            size: Theme.icon.md
            color: control.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: control.labelX
            anchors.right: chevron.visible ? chevron.left : parent.right
            anchors.rightMargin: Theme.space.sm
            anchors.verticalCenter: parent.verticalCenter
            visible: !control.compact
            text: control.signedIn ? control.name : qsTr("Sign in")
            elide: Text.ElideRight
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
            font.weight: Font.DemiBold
        }

        MeshIcon {
            id: chevron
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.md
            anchors.verticalCenter: parent.verticalCenter
            visible: !control.compact && control.signedIn
            iconName: "chevron-right"
            size: Theme.icon.sm
            color: Theme.palette.textTertiary
        }
    }

    // The button sits at the bottom of the window, so the menu opens beside it, foot to foot.
    ProfileMenu {
        id: menu
        x: control.width + Theme.space.sm
        y: control.height - height
        name: control.name
        kind: control.kind
        avatarSource: control.avatarSource
        controller: control.controller
        function toggle() { visible ? close() : open() }
        onOpenAccountsRequested: control.openAccountsRequested()
    }
}
