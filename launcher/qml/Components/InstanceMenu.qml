// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * An instance's ⋯ menu in the library (LibrarySections): Play or Stop and
 * its options, Open, the folders, Rename, Change icon, Change group,
 * Duplicate, Export, Create shortcut and Delete. What each means is the
 * host's: the menu only says what was asked for, except the Export,
 * Create shortcut and Duplicate dialogs, which `exportFlows`
 * (InstanceExportFlows) opens itself.
 */
Menu {
    id: root

    // Main.qml's launchOptions(): what the Play options submenu offers.
    property var launchOptionsFor: null
    // LibraryController: the instance's folders, and opening one.
    property var library: null
    // The host's InstanceExportFlows.
    property Item exportFlows: null

    signal launchRequested(string id)
    signal stopRequested(string id)
    signal editRequested(string id)
    signal folderRequested(string id)
    signal renameRequested(string id, string name)
    signal iconRequested(string id, string iconKey)
    signal groupRequested(string id, string group)
    signal duplicateRequested(string id, string name, string group)
    signal deleteRequested(string id, string name)
    signal launchModeRequested(string id, string mode)
    signal launchAsRequested(string id, string accountId)
    signal profilerChosen(string id, string profilerKey)
    signal navigateRequested(string target)

    property string targetId
    property bool targetRunning: false
    property string targetName
    property string targetIcon
    property string targetGroup
    // This instance's .minecraft and mods folders; empty where there is
    // no such folder to open (see LibraryController.folders()).
    property string gameFolder
    property string modsFolder

    function openFor(id, running, name, iconKey, group, anchor) {
        var folders = root.library ? root.library.folders(id) : ({})
        gameFolder = folders.game && folders.game !== folders.instance ? folders.game : ""
        modsFolder = folders.mods || ""
        targetId = id
        targetRunning = running
        targetName = name || ""
        targetIcon = iconKey || ""
        targetGroup = group || ""
        // Play options make no sense for a running instance. A submenu
        // has no `visible` of its own: its entry is a MenuItem Qt makes,
        // found here by its subMenu.
        var offerOptions = !running && !!root.launchOptionsFor
        for (var i = 0; i < root.count; ++i) {
            var item = root.itemAt(i)
            if (item && item.subMenu === playOptionsMenu) {
                item.visible = offerOptions
                item.height = offerOptions ? item.implicitHeight : 0
                // An icon like its neighbours', or its label would
                // start where theirs' icons do.
                item.icon.source = Icons.url("sliders")
            }
        }
        if (!anchor) {
            popup()
            return
        }
        // Under the ⋯ button and right-aligned to it, like the instance
        // page's own ⋯, and held there while the menu settles its width.
        popup(anchor, anchor.width - root.width, anchor.height + Theme.space.xs)
        root.x = Qt.binding(() => anchor.width - root.width)
    }

    MenuItem {
        text: root.targetRunning ? qsTr("Stop") : qsTr("Play")
        icon.source: Icons.url(root.targetRunning ? "stop" : "play")
        onTriggered: root.targetRunning ? root.stopRequested(root.targetId)
                                        : root.launchRequested(root.targetId)
    }
    LaunchOptionsMenu {
        id: playOptionsMenu
        instanceId: root.targetId
        optionsFor: root.launchOptionsFor
        onLaunchModeRequested: (id, mode) => root.launchModeRequested(id, mode)
        onLaunchAsRequested: (id, accountId) => root.launchAsRequested(id, accountId)
        onProfilerChosen: (id, key) => root.profilerChosen(id, key)
        onNavigateRequested: (target) => root.navigateRequested(target)
    }
    // An arrow, "go to its page": the gear it had read as "settings".
    MenuItem {
        text: qsTr("Open")
        icon.source: Icons.url("arrow-right")
        onTriggered: root.editRequested(root.targetId)
    }
    MenuItem {
        text: qsTr("Open folder")
        icon.source: Icons.url("folder")
        onTriggered: root.folderRequested(root.targetId)
    }
    MenuItem {
        // A hidden item still takes its row in a Menu; zero it as well.
        visible: root.gameFolder.length > 0
        height: visible ? implicitHeight : 0
        text: qsTr("Open .minecraft folder")
        icon.source: Icons.url("folder")
        onTriggered: root.library.openFolder(root.gameFolder)
    }
    MenuItem {
        visible: root.modsFolder.length > 0
        height: visible ? implicitHeight : 0
        text: qsTr("Open mods folder")
        icon.source: Icons.url("folder")
        onTriggered: root.library.openFolder(root.modsFolder)
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("Rename…")
        icon.source: Icons.url("edit")
        onTriggered: root.renameRequested(root.targetId, root.targetName)
    }
    MenuItem {
        text: qsTr("Change icon…")
        icon.source: Icons.url("image")
        onTriggered: root.iconRequested(root.targetId, root.targetIcon)
    }
    // The instance page's ⋯ menu words it the same way.
    MenuItem {
        text: qsTr("Change group…")
        icon.source: Icons.url("folder-input")
        onTriggered: root.groupRequested(root.targetId, root.targetGroup)
    }
    MenuItem {
        text: qsTr("Duplicate…")
        icon.source: Icons.url("copy")
        onTriggered: !!root.exportFlows && root.exportFlows.canDuplicate
                     ? root.exportFlows.duplicateInstance(root.targetId, root.targetName,
                                                          root.targetGroup, root.targetIcon)
                     : root.duplicateRequested(root.targetId, root.targetName, root.targetGroup)
    }
    MenuItem {
        // A hidden item still takes its row in a Menu; zero it as well.
        visible: !!root.exportFlows && root.exportFlows.canExport
        height: visible ? implicitHeight : 0
        text: qsTr("Export…")
        icon.source: Icons.url("archive")
        onTriggered: root.exportFlows.exportInstance(root.targetId)
    }
    MenuItem {
        visible: !!root.exportFlows && root.exportFlows.canCreateShortcut
        height: visible ? implicitHeight : 0
        text: qsTr("Create shortcut…")
        icon.source: Icons.url("external-link")
        onTriggered: root.exportFlows.createShortcut(root.targetId)
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("Delete…")
        icon.source: Icons.url("trash")
        enabled: !root.targetRunning
        onTriggered: root.deleteRequested(root.targetId, root.targetName)
    }
}
