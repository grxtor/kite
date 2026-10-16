// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The "Updates" section of SettingsPage.qml: automatic update checks and a
 * manual "Check now". Empty for now, so it stays out of the section list
 * (`shown: false`) until its content is added here; flip `shown` then.
 */
SettingsScroll {
    // Read by SettingsPage.qml's section list.
    readonly property bool shown: false

    title: qsTr("Updates")
}
