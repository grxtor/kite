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

    readonly property var motionStyles: [
        { value: "playful", label: qsTr("Playful") },
        { value: "calm", label: qsTr("Calm") },
        { value: "off", label: qsTr("Off") }
    ]

    // What UiMotionStyle means. Empty, it was never chosen: a config from before the setting keeps its
    // "Reduce motion" (UiReduceMotion) as "off", and everything else starts playful.
    function resolveMotionStyle(stored, legacyReduce) {
        if (stored === "playful" || stored === "calm" || stored === "off")
            return stored
        return legacyReduce ? "off" : "playful"
    }
    readonly property string motionStyle: store.resolveMotionStyle(store.string("UiMotionStyle"),
                                                                   store.bool("UiReduceMotion"))

    // Theme.motion reads it, so pages, dialogs and toasts follow the style without a check of their own.
    readonly property Binding _motionStyle: Binding {
        target: Theme
        property: "motionStyle"
        value: store.motionStyle
    }
}
