// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The "Installed plugins" section of SettingsPage.qml: every plugin MeshMC
 * found, with its status. Not the same as the "Plugins" section
 * (SettingsPluginsSection.qml), which shows the settings plugins add.
 * Empty for now, so it stays out of the section list (`shown: false`)
 * until its content is added here; flip `shown` then.
 */
SettingsScroll {
    // Read by SettingsPage.qml's section list.
    readonly property bool shown: false

    title: qsTr("Installed plugins")
}
