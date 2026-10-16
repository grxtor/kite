// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Launcher settings. Every change is saved the moment it is made -- there
 * is no OK/Cancel, as in any current desktop app's preferences.
 *
 * This file is only the frame: the section list on the left, and a stack
 * with one Settings*Section.qml file per entry of `sections` below, in the
 * same order. Each section's rows live in its own file. A section file can
 * keep itself out of the list by declaring `shown: false` -- the plugin
 * section while no plugin has added a setting, and the sections that are
 * still empty (About, Updates, Launcher logs, Installed plugins) until they
 * are filled in. Main.qml's navigate("settings/<id>") opens one by id.
 */
Item {
    id: root

    property int systemMemoryMiB: 8192
    // PluginSurfaceModel for the global settings anchor, or null.
    property var pluginSurfaces: null
    // TranslationsModel (languageKey, name, completeness) and the function
    // that switches to one of its keys, live.
    property var languages: null
    property var selectLanguage: null
    property string section: "general"
    // Whether launcher/qml/Cat was built at all (Main.qml's own
    // catAvailable, in turn MESHMC_HAS_CAT) -- passed on to the Appearance
    // section, which hides its Cat group without it.
    property bool catAvailable: false
    // QmlShell.about (AboutInfo) for the About section, and the
    // PluginListModel for "Installed plugins"; null hides either section.
    property var about: null
    property var pluginList: null
    // QmlShell.launcherLogs (LauncherLogs) for "Launcher logs"; null hides
    // that section.
    property var launcherLogs: null

    // Show a folder in the file manager (the General section's Folders).
    signal openPathRequested(string path)

    // Each icon says what its section is about ("layers" is a mod loader
    // elsewhere in the app, so Java does not wear it): the gear for the
    // general preferences, a server rack for the runtime and its memory, a
    // list of lines for the log window, a sun for the launcher's look, the
    // ">_" prompt for the shell commands, a grid of installed things for the
    // plugin list, an arrow out of a box for programs outside the launcher,
    // an up arrow for sending a log away, a filing box for the launcher's
    // own kept logs, the circling arrows of "check again" for updates, and
    // "i" for about.
    readonly property var sections: [
        { id: "general", icon: "settings", label: qsTr("General") },
        { id: "java", icon: "server", label: qsTr("Java & memory") },
        { id: "minecraft", icon: "cube", label: qsTr("Minecraft") },
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
        // A section that keeps itself out of the list (no updater in this
        // build, nothing to show yet) is not opened either: the list and the
        // page must agree, so this falls back to the first section instead
        // of showing a page the list does not highlight.
        for (var i = 0; i < sections.length; ++i)
            if (sections[i].id === section)
                return sectionShown(i) ? i : 0
        return 0
    }

    // Whether section @p i is offered in the list: its file decides, with
    // `shown` (see this file's header comment); without one it always is.
    function sectionShown(i) {
        var item = sectionStack.children[i]
        return !item || item.shown === undefined || item.shown
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        spacing: Theme.space.xl

        // Same rail selection grammar as the main sidebar (design-plan.md
        // Principle 6/§4): a sliding pill + left accent bar behind the row,
        // not just NavItem's own bare accent-icon/bold-text fallback. Shares
        // SidebarNav's own mechanism via NavSelectionIndicator rather than
        // re-deriving the geometry here.
        Item {
            id: sectionNavHost
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 208
            Layout.topMargin: Theme.space.xs
            implicitHeight: sectionColumn.implicitHeight

            readonly property Item selectedItem: {
                for (var i = 0; i < sectionRepeater.count; ++i) {
                    var item = sectionRepeater.itemAt(i)
                    if (item && item.modelData.id === root.section)
                        return item
                }
                return null
            }

            NavSelectionIndicator {
                target: sectionNavHost.selectedItem
            }

            Column {
                id: sectionColumn
                width: parent.width
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

        StackLayout {
            id: sectionStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.sectionIndex

            // One per entry of root.sections, in the same order.
            SettingsGeneralSection {
                onOpenPathRequested: (path) => root.openPathRequested(path)
            }
            SettingsJavaSection {
                systemMemoryMiB: root.systemMemoryMiB
            }
            SettingsMinecraftSection {}
            SettingsConsoleSection {}
            SettingsAppearanceSection {
                languages: root.languages
                selectLanguage: root.selectLanguage
                catAvailable: root.catAvailable
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
