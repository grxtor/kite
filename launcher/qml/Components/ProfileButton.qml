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
 * straight to the Accounts page. An account that cannot play online (expired,
 * errored, gone: AccountStates) gets a dot on the head's corner and a tooltip
 * that says what is wrong; its menu then starts with the way out.
 */
AbstractButton {
    id: control

    property string name: ""
    // "Microsoft", "Offline" or "" when there is no account.
    property string kind: ""
    property string avatarSource: ""
    // AccountsController: accounts, setDefault, remove.
    property var controller: null
    // The sidebar's colour, which rings the warning dot off the head.
    property color surround: Theme.palette.canvas

    // The default account's state and the reason it gave, from the controller.
    readonly property string stateKey: control.controller && control.controller.defaultStateKey !== undefined
                                       ? control.controller.defaultStateKey : ""
    readonly property string stateError: control.controller && control.controller.defaultError !== undefined
                                         ? control.controller.defaultError : ""
    readonly property bool attention: control.signedIn && AccountStates.needsAttention(control.stateKey)

    property bool compact: false
    property int railSquare: Theme.control.heightLg
    property int iconAxis: control.railSquare / 2
    property int labelX: control.iconAxis + control.headSize / 2 + Theme.space.sm
    property int cornerRadius: Theme.radius.md

    readonly property bool signedIn: control.name.length > 0
    // Four texels of the 8x8 face per pixel step: crisp at 1x and 2x.
    readonly property int headSize: 32
    readonly property int dotSize: 8

    signal openAccountsRequested()
    signal shortcutsRequested()

    // For a dev-route snapshot ("profilemenu") that needs the dropdown open
    // without a real click.
    function openMenu() { if (control.signedIn) menu.open() }

    padding: 0
    implicitWidth: control.compact ? control.railSquare : 200
    implicitHeight: control.railSquare
    hoverEnabled: true

    Accessible.name: signedIn ? qsTr("Account menu: %1").arg(control.name) : qsTr("Sign in")
    Accessible.description: control.attention ? AccountStates.summary(control.stateKey) : ""

    onClicked: control.signedIn ? menu.toggle() : control.openAccountsRequested()

    ToolTip {
        visible: (control.compact || control.attention) && control.hovered && !menu.visible
        delay: 400
        text: control.attention ? AccountStates.summary(control.stateKey)
            : control.signedIn ? control.name : qsTr("Sign in")
        x: control.width + Theme.space.md
        y: Math.round((control.height - implicitHeight) / 2)
    }

    background: StateFill {
        width: control.compact ? control.railSquare : control.width
        height: control.railSquare
        x: control.compact ? control.iconAxis - width / 2 : 0
        y: (control.height - height) / 2
        radius: control.cornerRadius
        hovered: control.hovered || menu.visible
        down: control.down
        border.width: control.signedIn ? 0 : 1
        border.color: Theme.palette.border

        FocusFrame { around: Theme.space.xxs; visible: control.visualFocus }
    }

    contentItem: Item {
        PlayerHead {
            id: head
            x: control.iconAxis - width / 2
            anchors.verticalCenter: parent.verticalCenter
            visible: control.signedIn
            size: control.headSize
            source: control.signedIn ? control.avatarSource : ""
        }

        Rectangle {
            readonly property int ring: 2
            visible: control.attention
            width: control.dotSize + ring * 2
            height: width
            radius: width / 2
            x: head.x + head.width - width / 2 - ring
            y: head.y + head.height - height / 2 - ring
            color: Theme.tone(AccountStates.tone(control.stateKey)).ink
            border.width: ring
            border.color: control.surround
        }

        MeshIcon {
            x: control.iconAxis - width / 2
            anchors.verticalCenter: parent.verticalCenter
            visible: !control.signedIn
            iconName: "user"
            size: Theme.icon.md
            color: control.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
        }

        Copy {
            anchors.left: parent.left
            anchors.leftMargin: control.labelX
            anchors.right: chevron.visible ? chevron.left : parent.right
            anchors.rightMargin: Theme.space.sm
            anchors.verticalCenter: parent.verticalCenter
            visible: !control.compact
            text: control.signedIn ? control.name : qsTr("Sign in")
            elide: Text.ElideRight
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
        stateKey: control.attention ? control.stateKey : ""
        stateError: control.stateError
        function toggle() { visible ? close() : open() }
        onOpenAccountsRequested: control.openAccountsRequested()
        onShortcutsRequested: control.shortcutsRequested()
    }
}
