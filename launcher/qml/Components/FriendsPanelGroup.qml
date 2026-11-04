// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One group of the friends panel ("IN KITE — 3"): a caption that folds the group away over compact FriendStatusRow
 * rows, each saying where the friend plays (server favicon and name, game version). A joinable friend has a small
 * Join; the ⋯ (`menu`) fades in with the pointer or keyboard focus, its room always kept so names do not shorten.
 * Hides itself while empty; the panel confirms and acts.
 */
Column {
    id: root

    property string title
    property var friends: []
    property bool expanded: true
    property var menu: null
    property var joinFlow: null
    property bool stale: false
    property bool actionsEnabled: true
    // Main.qml's FriendPlaces, for the server names and favicons.
    property FriendPlaces places: FriendPlaces {}

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

        background: Item {
            FocusFrame { around: 0; cornerRadius: Theme.radius.sm; visible: header.visualFocus }
        }
        contentItem: RowLayout {
            spacing: Theme.space.xs

            MeshIcon {
                Layout.alignment: Qt.AlignVCenter
                iconName: root.expanded ? "chevron-down" : "chevron-right"
                size: Theme.icon.xxs
                color: header.ink
            }
            Copy {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: root.title
                elide: Text.ElideRight
                role: "overline"
                color: header.ink
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

                FriendStatusRow {
                    id: row
                    x: -Theme.space.sm
                    width: slot.width + Theme.space.sm * 2
                    friend: slot.modelData
                    stale: root.stale
                    compact: true
                    headSlot: row.headSize
                    place: root.places.of(slot.modelData)

                    FriendJoinButton {
                        enabled: root.actionsEnabled
                        friend: slot.modelData
                        joinFlow: root.joinFlow
                        onClicked: root.joinRequested(slot.modelData)
                    }
                    FriendMoreButton {
                        id: more
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
