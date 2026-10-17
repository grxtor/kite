// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The other ways to play one instance: offline, the demo, as another
 * account, and which profiler watches the game -- what the widget launch
 * menu's Launch Offline, Launch Demo and profiler entries offered, plus
 * "Play as".
 *
 * A Menu, so a page can open it on its own (PlayDock's chevron next to
 * Play) or nest it inside a bigger one as a submenu (a card's ⋯ menu):
 *     LaunchOptionsMenu {
 *         instanceId: ...
 *         optionsFor: function (id) { return ... }   // Main.qml launchOptions()
 *         onLaunchModeRequested: (id, mode) => ...
 *     }
 *
 * What it offers is read from optionsFor(instanceId) -- Main.qml's
 * launchOptions(), in turn QmlShell::launchOptions() -- every time it
 * opens: the demo only where the game version has one, "Play as" only with
 * two or more accounts. It starts nothing itself; each choice is a signal
 * for Main.qml to act on, the way the pages' own Play buttons are.
 *
 * The profiler is a setting of the instance, not of one launch (the
 * widget menu treated it the same way): picking one here makes every later
 * Play use it, until "None" is picked.
 */
Menu {
    id: root

    property string instanceId: ""
    // function (id) returning {supportsDemo, profiler, profilers, accounts}
    // (see QmlShell::launchOptions()), or null.
    property var optionsFor: null
    property var options: ({})

    // mode: "offline" or "demo" -- QmlShell::launchInstanceMode().
    signal launchModeRequested(string id, string mode)
    // accountId: one of options.accounts' ids -- QmlShell::launchInstanceAs().
    signal launchAsRequested(string id, string accountId)
    // profilerKey: "" for none -- QmlShell::setInstanceProfiler().
    signal profilerChosen(string id, string profilerKey)
    // A Main.qml navigate() path: a profiler that is not set up yet sends
    // the user to the settings where it is.
    signal navigateRequested(string target)

    readonly property var accounts: root.options && root.options.accounts ? root.options.accounts : []
    readonly property var profilers: root.options && root.options.profilers ? root.options.profilers : []
    readonly property string profiler: root.options && root.options.profiler ? root.options.profiler : ""
    readonly property bool supportsDemo: !!(root.options && root.options.supportsDemo)
    // Choosing among one account is no choice.
    readonly property bool offersPlayAs: root.accounts.length > 1

    title: qsTr("Play options")

    // Re-reads what to offer. Runs on its own whenever the menu opens;
    // call it first to size the menu before positioning it (PlayDock).
    function refresh() {
        root.options = root.optionsFor && root.instanceId.length > 0
                ? root.optionsFor(root.instanceId) : ({})
        // A submenu has no `visible` of its own to bind: its entry in this
        // menu is a MenuItem created for it, found here by its subMenu.
        for (var i = 0; i < root.count; ++i) {
            var item = root.itemAt(i)
            if (item && item.subMenu === playAsMenu) {
                item.visible = root.offersPlayAs
                item.height = root.offersPlayAs ? item.implicitHeight : 0
            }
        }
    }
    onAboutToShow: root.refresh()

    // Opens submenu @p name ("playAs" or "profiler") beside its entry, as
    // hovering it would -- for review snapshots (Main.qml's
    // "launchmenu=<name>" dev route); the menu itself must be open.
    function openSubmenu(name) {
        var menu = name === "playAs" ? playAsMenu : name === "profiler" ? profilerMenu : null
        for (var i = 0; menu && i < root.count; ++i) {
            var item = root.itemAt(i)
            if (item && item.subMenu === menu && item.visible)
                menu.popup(item, item.width, 0)
        }
    }

    MenuItem {
        text: qsTr("Play offline")
        icon.source: Icons.url("play")
        onTriggered: root.launchModeRequested(root.instanceId, "offline")
    }
    MenuItem {
        text: qsTr("Play demo")
        icon.source: Icons.url("clock")
        visible: root.supportsDemo
        height: visible ? implicitHeight : 0
        onTriggered: root.launchModeRequested(root.instanceId, "demo")
    }

    Menu {
        id: playAsMenu
        title: qsTr("Play as")
        icon.source: Icons.url("user")
        Instantiator {
            model: root.accounts
            delegate: MenuItem {
                required property var modelData
                text: modelData.isDefault ? qsTr("%1 (default)").arg(modelData.name) : modelData.name
                // The account's own head, in its own colours.
                icon.source: "image://accountface/" + modelData.id
                icon.color: "transparent"
                onTriggered: root.launchAsRequested(root.instanceId, modelData.id)
            }
            onObjectAdded: (index, object) => playAsMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => playAsMenu.removeItem(object)
        }
    }

    MenuSeparator {}

    Menu {
        id: profilerMenu
        title: qsTr("Profiler")
        icon.source: Icons.url("sliders")
        MenuItem {
            text: qsTr("None")
            checkable: true
            checked: root.profiler.length === 0
            onTriggered: root.profilerChosen(root.instanceId, "")
        }
        Instantiator {
            model: root.profilers
            delegate: MenuItem {
                required property var modelData
                // One that is not set up yet leads to where it is set up
                // instead of being a dead, greyed-out entry.
                text: modelData.ready ? modelData.name : qsTr("Set up %1…").arg(modelData.name)
                // Checkable either way, so every entry's text lines up with
                // "None"'s past the check mark's column.
                checkable: true
                checked: modelData.ready && root.profiler === modelData.key
                onTriggered: modelData.ready ? root.profilerChosen(root.instanceId, modelData.key)
                                             : root.navigateRequested("settings/external-tools")
            }
            // After "None".
            onObjectAdded: (index, object) => profilerMenu.insertItem(index + 1, object)
            onObjectRemoved: (index, object) => profilerMenu.removeItem(object)
        }
    }
}
