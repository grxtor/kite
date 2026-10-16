// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The "Launcher logs" section of SettingsPage.qml: MeshMC's own log files
 * (not a game's -- those are on each instance's Log tab). Empty for now,
 * so it stays out of the section list (`shown: false`) until its content
 * is added here; flip `shown` then.
 */
SettingsScroll {
    // Read by SettingsPage.qml's section list.
    readonly property bool shown: false

    title: qsTr("Launcher logs")
}
