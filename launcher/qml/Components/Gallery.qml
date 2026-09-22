// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Every component, in every state it can be in, on one scrollable page.
 * This is the artifact design review happens against, so each section is
 * labelled and each state called out explicitly rather than left to be
 * inferred from a single "normal" example.
 */
Item {
    id: root

    // Real instance data comes from the core via InstanceList; this page is
    // reviewed long before that wiring exists, so the caller hands it
    // whatever stands in for a model (a ListModel, a plain JS array of role
    // objects, ...) and InstanceGrid is none the wiser.
    required property var sampleModel

    readonly property var navItems: [
        { id: "instances", icon: "▦", label: qsTr("Instances") },
        { id: "modpacks", icon: "⚙", label: qsTr("Modpacks") },
        { id: "settings", icon: "⚙", label: qsTr("Settings") }
    ]

    Rectangle {
        anchors.fill: parent
        color: Theme.palette.canvas
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: root.width
            spacing: Theme.space.xxl

            SectionHeader {
                Layout.topMargin: Theme.space.lg
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Top Bar")
                collapsible: false
            }

            TopBar {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space.lg
                Layout.rightMargin: Theme.space.lg
                title: qsTr("Instances")
                searchText: "vanilla"

                Button {
                    text: qsTr("New instance")
                    highlighted: true
                }
            }

            SectionHeader {
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Sidebar Nav")
                collapsible: false
            }

            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.lg

                SidebarNav {
                    Layout.preferredWidth: 220
                    Layout.preferredHeight: 360
                    items: root.navItems
                    currentId: "instances"
                    accountName: "Steve"
                    accountStatus: qsTr("Online")
                }

                // Same rail, collapsed -- shows the icons-only threshold
                // behaviour without needing an interactive resize.
                SidebarNav {
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 360
                    items: root.navItems
                    currentId: "modpacks"
                    accountName: "Steve"
                    accountStatus: qsTr("Online")
                }
            }

            SectionHeader {
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Nav Item")
                collapsible: false
            }

            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.sm

                NavItem { icon: "▦"; label: qsTr("Unselected") }
                NavItem { icon: "▦"; label: qsTr("Selected"); selected: true }
                NavItem { icon: "▦"; label: qsTr("Icon only"); iconOnly: true }
            }

            SectionHeader {
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Account Chip")
                collapsible: false
            }

            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.lg

                AccountChip { name: "Steve"; status: qsTr("Online") }
                AccountChip { name: "Guest"; status: "" }
                AccountChip { name: "Steve"; status: qsTr("Online"); collapsed: true }
            }

            SectionHeader {
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Status Badge")
                collapsible: false
            }

            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.sm

                StatusBadge { text: qsTr("Local"); tone: "neutral" }
                StatusBadge { text: qsTr("Up to date"); tone: "success" }
                StatusBadge { text: qsTr("Update available"); tone: "warning" }
                StatusBadge { text: qsTr("Broken"); tone: "danger" }
                StatusBadge { text: qsTr("Modded"); tone: "info" }
            }

            SectionHeader {
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Instance Card")
                collapsible: false
            }

            RowLayout {
                Layout.leftMargin: Theme.space.lg
                Layout.rightMargin: Theme.space.lg
                spacing: Theme.space.md

                InstanceCard {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 220
                    instanceId: "normal"
                    name: qsTr("Vanilla 1.20.4")
                    iconKey: "default"
                    group: qsTr("Vanilla")
                    isRunning: false
                    canLaunch: true
                    lastLaunch: 0
                }

                InstanceCard {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 220
                    instanceId: "hovered"
                    name: qsTr("All the Mods 9")
                    iconKey: "default"
                    group: qsTr("Modded")
                    isRunning: false
                    canLaunch: true
                    lastLaunch: Date.now()
                    forceHovered: true
                }

                InstanceCard {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 220
                    instanceId: "selected"
                    name: qsTr("A very long modpack name that needs to wrap")
                    iconKey: "default"
                    group: qsTr("Modded")
                    isRunning: false
                    canLaunch: true
                    lastLaunch: Date.now() - 3 * 86400000
                    selected: true
                }

                InstanceCard {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 220
                    instanceId: "running"
                    name: qsTr("Create: Above and Beyond")
                    iconKey: "default"
                    group: qsTr("Modded")
                    isRunning: true
                    canLaunch: true
                    lastLaunch: Date.now()
                }
            }

            SectionHeader {
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Instance Grid")
                collapsible: false
            }

            InstanceGrid {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space.lg
                Layout.rightMargin: Theme.space.lg
                Layout.preferredHeight: 480
                model: root.sampleModel
            }

            SectionHeader {
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Empty State")
                collapsible: false
            }

            EmptyState {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                title: qsTr("No instances yet")
                body: qsTr("Create an instance to see it here.")
                actionText: qsTr("Create instance")

                Text {
                    text: "▦"
                    color: Theme.palette.textTertiary
                    font.pixelSize: Theme.icon.lg * 2
                }
            }

            Item { Layout.preferredHeight: Theme.space.xxl }
        }
    }
}
