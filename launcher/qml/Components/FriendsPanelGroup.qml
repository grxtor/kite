// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One group of the friends panel ("ONLINE — 3"): a caption that folds the group away over compact FriendStatusRow
 * rows. A joinable friend has a small Join; the ⋯ (`menu`) fades in with the pointer or keyboard focus, its room
 * always kept so names do not shorten. Hides itself while empty; the panel confirms and acts.
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

                FriendStatusRow {
                    id: row
                    x: -Theme.space.sm
                    width: slot.width + Theme.space.sm * 2
                    friend: slot.modelData
                    stale: root.stale
                    compact: true
                    headSlot: row.headSize
                    // The narrow column cannot fit host, version and Join: the host tells friends apart, so the
                    // version is left to the Friends page.
                    detailSuffix: ""

                    Button {
                        id: join
                        visible: Format.friendJoinable(slot.modelData)
                        enabled: root.actionsEnabled
                        buttonSize: "sm"
                        text: qsTr("Join")
                        Accessible.name: qsTr("Join %1 on %2").arg(slot.modelData.name).arg(slot.modelData.host)
                        // Worked out as the pointer arrives, so it is never an old answer.
                        ToolTip.visible: join.hovered
                        ToolTip.delay: 500
                        ToolTip.text: join.hovered && root.joinFlow ? root.joinFlow.tip(slot.modelData) : ""
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
