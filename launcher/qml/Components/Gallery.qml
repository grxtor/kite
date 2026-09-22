// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Every component, in every state it can be in, on one scrollable page.
 * This is the artifact design review happens against, so each section is
 * labelled and each state called out explicitly rather than left to be
 * inferred from a single "normal" example. All sample data is self
 * contained -- nothing outside this file needs to feed it a model.
 */
Item {
    id: root

    readonly property var iconTints: ({
        grass: "#6aa84f",
        gold: "#e0b000",
        diamond: "#3ec6d0",
        enderman: "#30203a",
        fabric: "#d9773f",
        creeper: "#4b8f3e",
        iron: "#b8b8b8",
        stone: "#8a8a8a",
        tnt: "#c0392b"
    })

    readonly property var iconNames: [
        "home", "library", "compass", "package", "settings", "search", "plus", "play",
        "stop", "folder", "more", "chevron-down", "chevron-right", "user", "users",
        "log-out", "refresh", "sort", "clock", "cube", "x", "check", "sun", "moon",
        "external-link", "trash", "copy", "edit", "download", "grid", "list", "bell",
        "info", "alert-triangle", "layers", "terminal", "image", "globe"
    ]

    readonly property var navItems: [
        { id: "library", icon: "library", label: qsTr("Library") },
        { id: "modpacks", icon: "compass", label: qsTr("Discover") }
    ]
    readonly property var navFooterItems: [
        { id: "settings", icon: "settings", label: qsTr("Settings") }
    ]

    Rectangle {
        anchors.fill: parent
        color: Theme.palette.canvas
    }

    ListModel {
        id: recentModel
        ListElement { instanceId: "recent-1"; name: "Vanilla 1.21.4"; iconKey: "grass"; isRunning: false }
        ListElement { instanceId: "recent-2"; name: "All the Mods 10"; iconKey: "iron"; isRunning: true }
        ListElement { instanceId: "recent-3"; name: "Skyblock Extreme"; iconKey: "diamond"; isRunning: false }
    }

    ListModel {
        id: vanillaModel
        ListElement { instanceId: "vanilla-1"; name: "Vanilla 1.21.4"; iconKey: "grass"; isRunning: false; canLaunch: true; lastLaunch: 1726000000000; gameVersion: "1.21.4"; loader: ""; iconTint: "#6aa84f"; launchStatus: ""; launchProgress: -1 }
        ListElement { instanceId: "vanilla-2"; name: "Superflat Creative"; iconKey: "stone"; isRunning: false; canLaunch: true; lastLaunch: 0; gameVersion: "1.20.1"; loader: ""; iconTint: "#8a8a8a"; launchStatus: ""; launchProgress: -1 }
        ListElement { instanceId: "vanilla-3"; name: "Hardcore Survival"; iconKey: "tnt"; isRunning: false; canLaunch: true; lastLaunch: 1706000000000; gameVersion: "1.19.4"; loader: ""; iconTint: "#c0392b"; launchStatus: ""; launchProgress: -1 }
    }

    ListModel {
        id: moddedModel
        ListElement { instanceId: "modded-1"; name: "All the Mods 10"; iconKey: "iron"; isRunning: true; canLaunch: true; lastLaunch: 1726000000000; gameVersion: "1.20.1"; loader: "Fabric"; iconTint: "#b8b8b8"; launchStatus: ""; launchProgress: -1 }
        ListElement { instanceId: "modded-2"; name: "Create: Above and Beyond"; iconKey: "fabric"; isRunning: false; canLaunch: true; lastLaunch: 1706000000000; gameVersion: "1.18.2"; loader: "Fabric"; iconTint: "#d9773f"; launchStatus: ""; launchProgress: -1 }
        ListElement { instanceId: "modded-3"; name: "Enderman Challenge"; iconKey: "enderman"; isRunning: false; canLaunch: true; lastLaunch: 1706000000000; gameVersion: "1.21.1"; loader: "Forge"; iconTint: "#30203a"; launchStatus: ""; launchProgress: -1 }
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
                count: 12
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

                SidebarNav {
                    Layout.preferredWidth: 244
                    Layout.preferredHeight: 420
                    items: root.navItems
                    footerItems: root.navFooterItems
                    currentId: "library"
                    recentModel: recentModel
                    accountName: "Steve"
                    accountKind: "Microsoft"
                }
            }

            // Nav Item states: unselected, selected.
            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.sm

                NavItem { iconName: "library"; label: qsTr("Unselected") }
                NavItem { iconName: "library"; label: qsTr("Selected"); selected: true }
            }

            // Account Chip states: signed in (Microsoft), signed in (Offline), signed out.
            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.lg

                AccountChip { name: "Steve"; kind: "Microsoft" }
                AccountChip { name: "Steve"; kind: "Offline" }
                AccountChip { name: ""; kind: "" }
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
                title: qsTr("Buttons")
                collapsible: false
            }

            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.md

                PlayButton { }
                PlayButton { running: true }
                PlayButton { size: Theme.control.height }
                PlayButton { round: false }
                PlayButton { round: false; running: true }
                IconButton { iconName: "folder"; tip: qsTr("Open folder") }
                IconButton { iconName: "more"; tip: qsTr("More"); flat: false }
                IconButton { iconName: "refresh"; tip: qsTr("Refresh"); size: Theme.control.heightLg }
            }

            SectionHeader {
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Tag & Search Field")
                collapsible: false
            }

            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.sm

                Tag { text: "Fabric"; iconName: "layers" }
                Tag { text: "1.21.4"; iconName: "cube" }
                Tag { text: qsTr("2 h ago"); iconName: "clock" }

                Rectangle {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: Theme.control.heightLg
                    radius: Theme.radius.md
                    color: Theme.palette.textPrimary

                    Tag {
                        anchors.centerIn: parent
                        text: qsTr("On media")
                        iconName: "check"
                        onMedia: true
                    }
                }

                SearchField {
                    Layout.preferredWidth: 220
                    placeholderText: qsTr("Search")
                }
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

                // Every state InstanceCard can be in: rest, forceHovered,
                // selected, running, never played, launching.
                Repeater {
                    model: [
                        { instanceId: "card-rest", name: qsTr("Vanilla 1.21.4"), iconKey: "grass", version: "1.21.4", loader: "", last: Date.now() - 2 * 86400000, running: false, hovered: false, selected: false },
                        { instanceId: "card-hovered", name: qsTr("Skyblock Extreme"), iconKey: "diamond", version: "1.20.1", loader: "Fabric", last: Date.now(), running: false, hovered: true, selected: false },
                        { instanceId: "card-selected", name: qsTr("A very long modpack name that needs to wrap"), iconKey: "gold", version: "1.20.1", loader: "Forge", last: Date.now() - 3 * 86400000, running: false, hovered: false, selected: true },
                        { instanceId: "card-running", name: qsTr("Create: Above and Beyond"), iconKey: "enderman", version: "1.18.2", loader: "Fabric", last: Date.now(), running: true, hovered: false, selected: false },
                        { instanceId: "card-never-played", name: qsTr("Superflat Creative"), iconKey: "stone", version: "1.20.1", loader: "", last: 0, running: false, hovered: false, selected: false },
                        { instanceId: "card-launching", name: qsTr("Better Minecraft"), iconKey: "tnt", version: "1.20.1", loader: "Forge", last: 0, running: false, hovered: false, selected: false, status: qsTr("Downloading assets"), progress: 0.45 }
                    ]

                    delegate: InstanceCard {
                        required property var modelData
                        Layout.preferredWidth: 208
                        instanceId: modelData.instanceId
                        name: modelData.name
                        iconKey: modelData.iconKey
                        isRunning: modelData.running
                        canLaunch: true
                        lastLaunch: modelData.last
                        gameVersion: modelData.version
                        loader: modelData.loader
                        iconTint: root.iconTints[modelData.iconKey]
                        forceHovered: modelData.hovered
                        selected: modelData.selected
                        launchStatus: modelData.status || ""
                        launchProgress: modelData.progress !== undefined ? modelData.progress : -1
                    }
                }
            }

            SectionHeader {
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Continue Card")
                collapsible: false
            }

            ContinueCard {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space.lg
                Layout.rightMargin: Theme.space.lg
                instanceId: "continue-1"
                name: qsTr("All the Mods 10")
                iconKey: "iron"
                isRunning: false
                canLaunch: true
                lastLaunch: Date.now() - 3600000
                totalTimePlayed: 5 * 3600 + 20 * 60
                gameVersion: "1.20.1"
                loader: "Fabric"
                iconTint: root.iconTints.iron
                launchStatus: ""
                launchProgress: -1
            }

            SectionHeader {
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Instance Section")
                collapsible: false
            }

            InstanceSection {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space.lg
                Layout.rightMargin: Theme.space.lg
                title: qsTr("Vanilla")
                model: vanillaModel
                columns: 3
                cardWidth: 208
            }

            InstanceSection {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space.lg
                Layout.rightMargin: Theme.space.lg
                title: qsTr("Modded")
                model: moddedModel
                columns: 3
                cardWidth: 208
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

                MeshIcon {
                    iconName: "package"
                    size: Theme.icon.lg * 2
                    color: Theme.palette.textTertiary
                }
            }

            SectionHeader {
                Layout.leftMargin: Theme.space.lg
                title: qsTr("Icons")
                collapsible: false
            }

            Grid {
                Layout.leftMargin: Theme.space.lg
                Layout.rightMargin: Theme.space.lg
                columns: 8
                columnSpacing: Theme.space.lg
                rowSpacing: Theme.space.lg

                Repeater {
                    model: root.iconNames

                    delegate: Column {
                        required property string modelData
                        width: 64
                        spacing: Theme.space.xs

                        MeshIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            iconName: parent.modelData
                            size: Theme.icon.lg
                        }

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: parent.modelData
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: Theme.space.xxl }
        }
    }
}
