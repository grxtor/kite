// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * Page tabs: labels on a hairline, the current one underlined in the
 * accent. For switching views of one thing (an instance's mods, worlds,
 * log...), where the sidebar's pill style would read as navigation away.
 */
Item {
    id: root

    // [{ id, label, count? }]
    property var tabs: []
    property string current
    signal activated(string id)

    implicitHeight: Theme.control.heightLg
    implicitWidth: row.implicitWidth

    Accessible.role: Accessible.PageTabList

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.palette.divider
    }

    Row {
        id: row
        height: parent.height
        spacing: Theme.space.xl

        Repeater {
            model: root.tabs
            delegate: AbstractButton {
                id: tab
                required property var modelData
                readonly property bool selected: modelData.id === root.current

                height: row.height
                hoverEnabled: true
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.label
                onClicked: root.activated(modelData.id)

                contentItem: Row {
                    spacing: Theme.space.xs + 2
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: tab.modelData.label
                        color: tab.selected ? Theme.palette.textPrimary
                             : tab.hovered ? Theme.palette.textSecondary : Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.body.pixelSize
                        font.weight: tab.selected ? Font.DemiBold : Font.Medium
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: tab.modelData.count !== undefined && tab.modelData.count >= 0
                        text: tab.modelData.count !== undefined ? tab.modelData.count : ""
                        color: Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                        font.weight: Font.Medium
                    }
                }

                background: Item {
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 2
                        radius: 1
                        color: Theme.palette.accent
                        visible: tab.selected
                    }
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -2
                        radius: Theme.radius.sm
                        color: "transparent"
                        border.width: 2
                        border.color: Theme.palette.focusRing
                        visible: tab.visualFocus
                    }
                }
            }
        }
    }
}
