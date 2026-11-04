// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Launcher settings; every change is saved at once. The section list is on
 * the left, with one Settings*Section.qml per entry of `sections`, in the same
 * order. A section file hides itself from the list with `shown: false`.
 * Main.qml's navigate("settings/<id>") opens one by id.
 */
Item {
    id: root

    property int systemMemoryMiB: 8192
    property var pluginSurfaces: null
    // TranslationsModel (languageKey, name, completeness) and the function
    // that switches to one of its keys, live.
    property var languages: null
    property var selectLanguage: null
    property string section: "general"
    // QmlShell.about (AboutInfo) for the About section, and the
    // PluginListModel for "Installed plugins"; null hides either section.
    property var about: null
    property var pluginList: null
    // QmlShell.launcherLogs (LauncherLogs) for "Launcher logs"; null hides
    // that section.
    property var launcherLogs: null
    // QmlShell.friends (FriendsController) for "Friends"; null hides that
    // section.
    property var friends: null
    // QmlShell.shareCodes, for the Friends section's share codes.
    property var shareCodes: null
    // The default account's name if it is a Microsoft one ("" if not): the
    // account whose Friends data Delete in "Friends" removes.
    property string friendsAccountName: ""

    signal openPathRequested(string path)

    // "layers" is a mod loader elsewhere in the app, so Java does not wear it.
    readonly property var sections: [
        { id: "general", icon: "settings", label: qsTr("General") },
        { id: "java", icon: "server", label: qsTr("Java & memory") },
        { id: "minecraft", icon: "cube", label: qsTr("Minecraft") },
        { id: "friends", icon: "users", label: qsTr("Friends") },
        { id: "console", icon: "list", label: qsTr("Console") },
        { id: "appearance", icon: "sun", label: qsTr("Appearance") },
        { id: "commands", icon: "terminal", label: qsTr("Custom commands") },
        { id: "plugins-list", icon: "grid", label: qsTr("Installed plugins") },
        { id: "plugins", icon: "package", label: qsTr("Plugins") },
        { id: "proxy", icon: "globe", label: qsTr("Proxy") },
        { id: "external-tools", icon: "external-link", label: qsTr("External tools") },
        { id: "log-upload", icon: "arrow-up", label: qsTr("Log upload") },
        { id: "launcher-logs", icon: "archive", label: qsTr("Launcher logs") },
        { id: "updates", icon: "refresh", label: qsTr("Updates") },
        { id: "about", icon: "info", label: qsTr("About") }
    ]
    readonly property int sectionIndex: {
        // A section that keeps itself out of the list is not opened either: fall back to the first,
        // so list and page agree.
        for (var i = 0; i < sections.length; ++i)
            if (sections[i].id === section)
                return sectionShown(i) ? i : 0
        return 0
    }

    function sectionShown(i) {
        var item = sectionStack.children[i]
        return !item || item.shown === undefined || item.shown
    }

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.space.xl
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        spacing: Theme.space.xl

        // The same selection grammar as the main sidebar, via NavSelectionIndicator. It scrolls, so
        // every section stays reachable in a short window.
        Item {
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true
            Layout.preferredWidth: 208
            Layout.topMargin: Theme.space.xs

            Flickable {
                id: sectionNavHost
                anchors.fill: parent
                contentWidth: width
                contentHeight: sectionColumn.height + Theme.space.xl
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                ScrollBar.vertical: ScrollBar {}

                readonly property Item selectedItem: {
                    for (var i = 0; i < sectionRepeater.count; ++i) {
                        var item = sectionRepeater.itemAt(i)
                        if (item && item.modelData.id === root.section)
                            return item
                    }
                    return null
                }

                // A section opened by a route can lie below the fold.
                onSelectedItemChanged: {
                    var item = sectionNavHost.selectedItem
                    if (!item)
                        return
                    if (item.y < contentY)
                        contentY = item.y
                    else if (item.y + item.height > contentY + height)
                        contentY = item.y + item.height - height
                }

                NavSelectionIndicator {
                    target: sectionNavHost.selectedItem
                }

                Column {
                    id: sectionColumn
                    width: sectionNavHost.width
                    spacing: Theme.space.xxs

                    Repeater {
                        id: sectionRepeater
                        model: root.sections
                        delegate: NavItem {
                            required property var modelData
                            required property int index
                            width: parent.width
                            visible: root.sectionShown(index)
                            iconName: modelData.icon
                            label: modelData.label
                            selected: root.section === modelData.id
                            onClicked: root.section = modelData.id
                        }
                    }
                }
            }

            ScrollFade {
                flickable: sectionNavHost
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: Theme.palette.divider
        }

        StackLayout {
            id: sectionStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.sectionIndex

            SettingsGeneralSection {
                onOpenPathRequested: (path) => root.openPathRequested(path)
            }
            SettingsJavaSection {
                systemMemoryMiB: root.systemMemoryMiB
            }
            SettingsMinecraftSection {}
            SettingsFriendsSection {
                controller: root.friends
                shareCodes: root.shareCodes
                accountName: root.friendsAccountName
            }
            SettingsConsoleSection {}
            SettingsAppearanceSection {
                languages: root.languages
                selectLanguage: root.selectLanguage
                onOpenPathRequested: (path) => root.openPathRequested(path)
            }
            SettingsCommandsSection {}
            SettingsPluginsListSection {
                pluginList: root.pluginList
            }
            SettingsPluginsSection {
                pluginSurfaces: root.pluginSurfaces
            }
            SettingsProxySection {}
            SettingsExternalToolsSection {}
            SettingsLogUploadSection {}
            SettingsLauncherLogsSection {
                launcherLogs: root.launcherLogs
                onOpenPathRequested: (path) => root.openPathRequested(path)
            }
            SettingsUpdatesSection {}
            SettingsAboutSection {
                about: root.about
            }
        }
    }
}
