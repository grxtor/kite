// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One group of the friends panel ("ONLINE — 3"): a caption that folds the
 * group away over compact rows (FriendStatusRow at the head's own width).
 * A friend who can be joined has a small Join in plain sight; the ⋯ that
 * removes or blocks them (the panel's FriendPersonMenu, `menu`) fades in
 * with the pointer, keyboard focus, or while its menu is open. The room for
 * the ⋯ is always kept, so a name does not shorten as the pointer arrives.
 * It hides itself while the group is empty, and only says what was asked
 * for: the panel confirms and acts.
 */
Column {
    id: root

    // The caption, in capitals, with its count.
    property string title
    // Friend maps (FriendsController.friends).
    property var friends: []
    // Whether the rows show; the panel keeps this for the session.
    property bool expanded: true
    // The panel's FriendPersonMenu.
    property var menu: null
    // Main.qml's FriendJoinFlow, which says what Join would start.
    property var joinFlow: null
    // The list is the last one read, not a live one.
    property bool stale: false
    // The server answers: Join and ⋯ need it, reading the list does not.
    property bool actionsEnabled: true

    signal toggled()
    signal joinRequested(var friend)

    visible: root.friends.length > 0
    spacing: Theme.space.xxs

    AbstractButton {
        id: header
        width: parent.width
        height: Theme.control.heightSm
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        onClicked: root.toggled()
        Accessible.name: root.title

        readonly property color ink: header.hovered || header.visualFocus ? Theme.palette.textSecondary
                                                                         : Theme.palette.textTertiary

        background: Rectangle {
            radius: Theme.radius.sm
            color: "transparent"
            border.width: header.visualFocus ? Theme.focus.width : 0
            border.color: Theme.palette.focusRing
        }
        contentItem: RowLayout {
            spacing: Theme.space.xs

            MeshIcon {
                Layout.alignment: Qt.AlignVCenter
                iconName: root.expanded ? "chevron-down" : "chevron-right"
                // A caption's glyph, smaller than a line of text's.
                size: Theme.icon.sm - 4
                color: header.ink
            }
            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: root.title
                elide: Text.ElideRight
                color: header.ink
                font.family: Theme.font.family
                font.pixelSize: Theme.type.overline.pixelSize
                font.weight: Theme.type.overline.weight
                font.letterSpacing: Theme.type.overline.letterSpacing
                Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            }
        }
    }

    Column {
        id: rows
        width: parent.width
        visible: root.expanded

        Repeater {
            model: root.friends
            delegate: Item {
                id: slot
                required property var modelData

                width: rows.width
                height: row.height

                // The head stands on the column's edge, with the captions;
                // the row's hover bleeds out by its own padding.
                FriendStatusRow {
                    id: row
                    x: -Theme.space.sm
                    width: slot.width + Theme.space.sm * 2
                    friend: slot.modelData
                    stale: root.stale
                    compact: true
                    headSlot: row.headSize

                    Button {
                        id: join
                        visible: Format.friendJoinable(slot.modelData)
                        enabled: root.actionsEnabled
                        buttonSize: "sm"
                        text: qsTr("Join")
                        Accessible.name: qsTr("Join %1 on %2").arg(slot.modelData.name).arg(slot.modelData.host)
                        // What it does: it starts the game, with the instance
                        // named. Worked out as the pointer arrives, so it is
                        // never an old answer.
                        ToolTip.visible: join.hovered
                        ToolTip.delay: 500
                        ToolTip.text: join.hovered && root.joinFlow ? root.joinFlow.tip(slot.modelData) : ""
                        onClicked: root.joinRequested(slot.modelData)
                    }
                    FriendMoreButton {
                        id: more
                        // This person's menu is the one open.
                        readonly property bool menuOpen: !!root.menu && root.menu.visible
                                                         && root.menu.personUuid === slot.modelData.uuid

                        menu: root.menu
                        personUuid: slot.modelData.uuid
                        personName: slot.modelData.name
                        enabled: root.actionsEnabled
                        opacity: row.hovered || more.visualFocus || more.menuOpen
                                 ? (more.enabled ? 1 : Theme.opacity.disabled) : 0
                        Behavior on opacity { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                    }
                }
            }
        }
    }
}
