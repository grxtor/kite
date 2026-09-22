// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One group of the library: a collapsible header and a grid of cards.
 * Column count and card width come from the page so every section lines
 * up with every other one.
 */
Column {
    id: root

    property string title
    property bool showHeader: true
    property var model: null
    property int columns: 4
    property real cardWidth: 208
    property int gutter: Theme.space.lg
    property string selectedId
    property bool collapsed: false

    signal selectRequested(string id)
    signal launchRequested(string id)
    signal stopRequested(string id)
    signal menuRequested(string id, bool running)
    signal toggleRequested()

    spacing: Theme.space.md

    Item {
        visible: root.showHeader
        width: parent.width
        height: Theme.control.height

        AbstractButton {
            id: header
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height
            width: headerRow.implicitWidth + Theme.space.sm
            hoverEnabled: true
            Accessible.name: root.title
            onClicked: root.toggleRequested()

            contentItem: Row {
                id: headerRow
                spacing: Theme.space.sm

                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "chevron-down"
                    size: Theme.icon.sm
                    rotation: root.collapsed ? -90 : 0
                    color: header.hovered ? Theme.palette.textPrimary : Theme.palette.textTertiary
                    Behavior on rotation { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.title
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.model ? root.model.count : ""
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.weight: Font.Medium
                }
            }
            background: Item {}
        }

        Rectangle {
            anchors.left: header.right
            anchors.leftMargin: Theme.space.md
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 1
            color: Theme.palette.divider
        }
    }

    Grid {
        visible: !root.collapsed
        columns: root.columns
        columnSpacing: root.gutter
        rowSpacing: root.gutter

        Repeater {
            model: root.model
            delegate: InstanceCard {
                width: root.cardWidth
                selected: root.selectedId === instanceId
                onClicked: root.selectRequested(instanceId)
                onPlayRequested: root.launchRequested(instanceId)
                onStopRequested: root.stopRequested(instanceId)
                onMenuRequested: {
                    root.selectRequested(instanceId)
                    root.menuRequested(instanceId, isRunning)
                }
            }
        }
    }
}
