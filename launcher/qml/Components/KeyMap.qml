// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton

import QtQuick

/*
 * Every keyboard shortcut of the launcher, in one list: AppShortcuts and Main.qml take their key sequences from
 * it (keys()) and ShortcutsDialog draws it (groups, combos()), so the sheet cannot name a key the window does
 * not answer to. An entry with `elsewhere` belongs to the control that has the focus (a card's Enter, the log's
 * find field) and is listed for the sheet only.
 *
 * Sequences are Qt's portable text: "Ctrl" is Command on macOS. `mac` replaces `keys` there; `also` is drawn
 * beside the keys as it stands ("Mouse back button"); `loose` keys are drawn side by side and are no chord.
 */
QtObject {
    id: root

    // Writable so a test can read the other platform's keys.
    property bool mac: Qt.platform.os === "osx"

    readonly property var groups: [
        { id: "navigation", title: qsTr("Navigation") },
        { id: "library", title: qsTr("Library") },
        { id: "instance", title: qsTr("Instance") },
        { id: "log", title: qsTr("Log") },
        { id: "chat", title: qsTr("Chat") }
    ]

    readonly property var entries: [
        { id: "back", group: "navigation", label: qsTr("Back"), keys: ["Alt+Left"], mac: ["Ctrl+["],
          also: qsTr("Mouse back button") },
        { id: "forward", group: "navigation", label: qsTr("Forward"), keys: ["Alt+Right"], mac: ["Ctrl+]"],
          also: qsTr("Mouse forward button") },
        { id: "home", group: "navigation", label: qsTr("Go to Home"), keys: ["Ctrl+1"] },
        { id: "discover", group: "navigation", label: qsTr("Go to Discover"), keys: ["Ctrl+2"] },
        { id: "settings", group: "navigation", label: qsTr("Settings"), keys: ["Ctrl+,"] },
        { id: "help", group: "navigation", label: qsTr("Keyboard shortcuts"), keys: ["?"] },
        { id: "close", group: "navigation", label: qsTr("Close a dialog or menu"), keys: ["Esc"], elsewhere: true },

        { id: "find", group: "library", label: qsTr("Search the library"), keys: ["Ctrl+F"] },
        { id: "new", group: "library", label: qsTr("New instance"), keys: ["Ctrl+N"] },
        { id: "play", group: "library", label: qsTr("Play the selected instance"), keys: ["Ctrl+Return"] },
        { id: "move", group: "library", label: qsTr("Move between instances"), loose: ["Left", "Up", "Down", "Right"],
          elsewhere: true },
        { id: "open", group: "library", label: qsTr("Open the instance"), keys: ["Return"], elsewhere: true },
        { id: "select", group: "library", label: qsTr("Select the instance"), keys: ["Space"], elsewhere: true },
        { id: "menu", group: "library", label: qsTr("Open the instance's menu"), keys: ["Menu"], mac: [],
          elsewhere: true },

        { id: "playOpen", group: "instance", label: qsTr("Play this instance"), keys: ["Ctrl+Return"], elsewhere: true },
        { id: "clear", group: "instance", label: qsTr("Clear the selection, leave Add content"), keys: ["Esc"],
          elsewhere: true },

        { id: "logFind", group: "log", label: qsTr("Find in the log"), keys: ["Ctrl+F"], elsewhere: true },
        { id: "logNext", group: "log", label: qsTr("Next match"), keys: ["Return"], elsewhere: true },
        { id: "logPrevious", group: "log", label: qsTr("Previous match"), keys: ["Shift+Return"], elsewhere: true },

        { id: "send", group: "chat", label: qsTr("Send the message"), keys: ["Return"], elsewhere: true },
        { id: "newLine", group: "chat", label: qsTr("New line"), keys: ["Shift+Return"], elsewhere: true },
        { id: "closeChat", group: "chat", label: qsTr("Close the conversation"), keys: ["Esc"], elsewhere: true }
    ]

    function entry(id) {
        for (var i = 0; i < root.entries.length; ++i)
            if (root.entries[i].id === id)
                return root.entries[i]
        return null
    }

    // The sequences a Shortcut for `id` listens to on this platform.
    function keys(id) {
        var one = root.entry(id)
        if (!one)
            return []
        return root.mac && one.mac !== undefined ? one.mac : (one.keys || [])
    }

    // The entries of a group that exist on this platform.
    function entriesOf(group) {
        return root.entries.filter((one) => one.group === group && root.combos(one).length > 0)
    }

    // What the sheet draws for an entry: a list of alternatives, each a list of key caps.
    function combos(one) {
        if (one.loose)
            return [one.loose.map(root.cap)]
        var shown = root.mac && one.mac !== undefined ? one.mac : (one.keys || [])
        return shown.map((sequence) => sequence.split("+").map(root.cap))
    }

    // One key as its cap reads on this platform.
    function cap(key) {
        switch (key) {
        case "Ctrl": return root.mac ? "⌘" : qsTr("Ctrl")
        case "Alt": return root.mac ? "⌥" : qsTr("Alt")
        case "Shift": return root.mac ? "⇧" : qsTr("Shift")
        case "Return": return root.mac ? qsTr("Return") : qsTr("Enter")
        case "Space": return qsTr("Space")
        case "Esc": return qsTr("Esc")
        case "Menu": return qsTr("Menu")
        case "Left": return "←"
        case "Right": return "→"
        case "Up": return "↑"
        case "Down": return "↓"
        }
        return key
    }
}
