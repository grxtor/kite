// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The other ways to play one instance: offline, demo, as another account, the
 * Kite Performance mode and which profiler watches the game. A Menu, so it can
 * open on its own or nest as a submenu. What it offers is read from
 * optionsFor(instanceId) every time it opens; each choice is a signal, it
 * starts nothing itself. Performance and profiler are settings of the
 * instance, not of one launch.
 */
Menu {
    id: root

    property string instanceId: ""
    // function (id) returning {supportsDemo, performance, performanceAvailable, profiler,
    // profilers, accounts} (QmlShell::launchOptions()), or null.
    property var optionsFor: null
    property var options: ({})

    // mode: "offline" or "demo" -- QmlShell::launchInstanceMode().
    signal launchModeRequested(string id, string mode)
    // accountId: one of options.accounts' ids -- QmlShell::launchInstanceAs().
    signal launchAsRequested(string id, string accountId)
    // profilerKey: "" for none -- QmlShell::setInstanceProfiler().
    signal profilerChosen(string id, string profilerKey)
    // enabled: Kite Performance for the instance -- QmlShell::setPerformanceEnabled().
    signal performanceChosen(string id, bool enabled)
    // A Main.qml navigate() path: a profiler that is not set up sends the user to its settings.
    signal navigateRequested(string target)

    readonly property var accounts: root.options && root.options.accounts ? root.options.accounts : []
    readonly property var profilers: root.options && root.options.profilers ? root.options.profilers : []
    readonly property string profiler: root.options && root.options.profiler ? root.options.profiler : ""
    readonly property bool performance: !!(root.options && root.options.performance)
    readonly property bool performanceAvailable: !!(root.options && root.options.performanceAvailable)
    readonly property bool supportsDemo: !!(root.options && root.options.supportsDemo)
    readonly property bool offersPlayAs: root.accounts.length > 1

    title: qsTr("Play options")

    // Re-reads what to offer; runs on every open. Call it first to size the menu before positioning it.
    function refresh() {
        root.options = root.optionsFor && root.instanceId.length > 0
                ? root.optionsFor(root.instanceId) : ({})
    }
    onAboutToShow: root.refresh()

    // Opens submenu @p name ("playAs" or "profiler") beside its entry; for the "launchmenu=<name>"
    // dev route, the menu must be open.
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
    MenuEntry {
        text: qsTr("Play demo")
        icon.source: Icons.url("clock")
        visible: root.supportsDemo
        onTriggered: root.launchModeRequested(root.instanceId, "demo")
    }

    SubmenuEntry {
        menu: root
        submenu: playAsMenu
        shown: root.offersPlayAs
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
                icon.source: "image://accountface/" + modelData.id
                icon.color: "transparent"
                onTriggered: root.launchAsRequested(root.instanceId, modelData.id)
            }
            onObjectAdded: (index, object) => playAsMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => playAsMenu.removeItem(object)
        }
    }

    MenuSeparator {}

    // While on, Play installs the performance mods first. Only for an instance with a loader on a
    // version the mods exist for.
    MenuEntry {
        text: qsTr("Kite Performance")
        checkable: true
        checked: root.performance
        visible: root.performanceAvailable
        onTriggered: root.performanceChosen(root.instanceId, checked)
    }
    MenuDivider { visible: root.performanceAvailable }

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
                // One that is not set up yet leads to where it is set up.
                text: modelData.ready ? modelData.name : qsTr("Set up %1…").arg(modelData.name)
                // Checkable either way, so every entry's text lines up with "None"'s.
                checkable: true
                checked: modelData.ready && root.profiler === modelData.key
                onTriggered: modelData.ready ? root.profilerChosen(root.instanceId, modelData.key)
                                             : root.navigateRequested("settings/external-tools")
            }
            onObjectAdded: (index, object) => profilerMenu.insertItem(index + 1, object)
            onObjectRemoved: (index, object) => profilerMenu.removeItem(object)
        }
    }
}
