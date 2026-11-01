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

    readonly property var instanceSortModes: [
        { value: "Name", label: qsTr("Name") },
        { value: "LastLaunch", label: qsTr("Last played") },
        { value: "TotalTimePlayed", label: qsTr("Time played") }
    ]

    // The interface font follows the UiFontFamily setting for as long as the app runs, even if no
    // Settings page was ever opened.
    readonly property Binding _interfaceFont: Binding {
        target: Theme
        property: "customFamily"
        value: store.string("UiFontFamily")
    }

    // Theme.motion reads it, so pages, dialogs and toasts stop animating without a check of their own.
    readonly property Binding _reduceMotion: Binding {
        target: Theme
        property: "reduceMotion"
        value: store.bool("UiReduceMotion")
    }
}
