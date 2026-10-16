// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The "Plugins" section of SettingsPage.qml: settings the installed plugins
 * have added to the global settings anchor. Kept out of the section list
 * (`shown`) while no plugin has added anything.
 */
SettingsScroll {
    id: root

    // PluginSurfaceModel for the global settings anchor, or null.
    property var pluginSurfaces: null
    // Read by SettingsPage.qml's section list.
    readonly property bool shown: pluginsView.count > 0

    title: qsTr("Plugins")
    description: qsTr("Settings the installed plugins have added.")
    PluginSurfaces {
        id: pluginsView
        width: parent.width
        model: root.pluginSurfaces
    }
}
