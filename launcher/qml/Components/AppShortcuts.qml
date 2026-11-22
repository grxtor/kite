// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * The window's own keyboard shortcuts and the mouse's back and forward buttons, each reported as
 * triggered(action): "back", "forward", "home", "discover", "settings", "play" or "help". The keys come from
 * KeyMap. Fill the window with it, above the pages.
 *
 * None fires while a dialog or menu is open: Qt Quick Controls holds a window's shortcuts back under a modal
 * popup, and the popup takes the mouse. `active` is for what covers the window without being one (first-run
 * setup, the busy overlay). Chords no text field uses work while typing; "?" and Play do not.
 */
Item {
    id: root

    property bool active: true
    // The menu bar has Settings' key while it shows the item (AppMenuBar); two owners would cancel each other.
    property bool settingsInMenu: false

    signal triggered(string action)

    readonly property bool typing: {
        var item = root.Window.activeFocusItem
        return item instanceof TextInput || item instanceof TextEdit
    }

    component Key: Shortcut {
        required property string action
        // Not for a text field: plain characters, and Play.
        property bool awayFromText: false
        sequences: KeyMap.keys(action)
        enabled: root.active && !(awayFromText && root.typing)
        onActivated: root.triggered(action)
    }

    Key { action: "back" }
    Key { action: "forward" }
    Key { action: "home" }
    Key { action: "discover" }
    Key { action: "settings"; enabled: root.active && !root.settingsInMenu }
    Key { action: "play"; awayFromText: true }
    Key { action: "help"; awayFromText: true }

    MouseArea {
        anchors.fill: parent
        enabled: root.active
        acceptedButtons: Qt.BackButton | Qt.ForwardButton
        onPressed: (mouse) => root.triggered(mouse.button === Qt.BackButton ? "back" : "forward")
    }
}
