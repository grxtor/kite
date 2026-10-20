// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton

import QtQuick
import Kite.Theme

// The launcher-wide settings; the shell sets `adapter` once at startup.
// Setting rows use this unless they are given another `source`.
SettingsSource {
    id: store

    // The orders InstSortMode takes, for every control that picks one
    // (Home's sort menu, Settings > General, the library toolbar).
    readonly property var instanceSortModes: [
        { value: "Name", label: qsTr("Name") },
        { value: "LastLaunch", label: qsTr("Last played") },
        { value: "TotalTimePlayed", label: qsTr("Time played") }
    ]

    // The interface font follows the UiFontFamily setting for as long as
    // the app runs, wherever it is changed from. Kept here, with the
    // settings themselves, so it holds even if no Settings page was ever
    // opened.
    readonly property Binding _interfaceFont: Binding {
        target: Theme
        property: "customFamily"
        value: store.string("UiFontFamily")
    }
}
