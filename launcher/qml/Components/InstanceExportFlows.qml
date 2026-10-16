// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * "Export…", "Create shortcut…" and "Duplicate…" for any instance, behind
 * the library's and the instance page's ⋯ menus: the dialogs and the
 * toasts that follow them, in one place so the two pages offer exactly the
 * same thing.
 *
 * The controllers come from the shell, through the two functions and the
 * LibraryController below, which Main.qml sets on each page. Until it does,
 * `canExport`, `canCreateShortcut` and `canDuplicate` stay false and the
 * pages fall back to what they did before (or leave the entry out).
 */
Item {
    id: root

    // id -> InstanceExportController / InstanceShortcutController
    // (QmlShell's exportController()/shortcutController()), or null.
    property var exportControllerFor: null
    property var shortcutControllerFor: null
    readonly property bool canExport: typeof root.exportControllerFor === "function"
    readonly property bool canCreateShortcut: typeof root.shortcutControllerFor === "function"
    // LibraryController (QmlShell's `library`), for Duplicate.
    property var library: null
    readonly property bool canDuplicate: !!root.library

    // @p format ("zip", "modrinth", "curseforge") is optional: the dialog
    // opens on the zip export otherwise.
    function exportInstance(id, format) {
        var controller = root.canExport ? root.exportControllerFor(id) : null
        if (!controller)
            return
        if (format)
            controller.format = format
        exportDialog.controller = controller
        exportDialog.open()
    }

    function createShortcut(id) {
        var controller = root.canCreateShortcut ? root.shortcutControllerFor(id) : null
        if (!controller)
            return
        shortcutDialog.controller = controller
        shortcutDialog.open()
    }

    function duplicateInstance(id, name, group, iconKey) {
        if (!root.canDuplicate)
            return
        duplicateDialog.targetId = id
        duplicateDialog.targetGroup = group || ""
        duplicateDialog.sourceName = name || ""
        duplicateDialog.iconKey = iconKey || ""
        duplicateDialog.open()
    }

    DuplicateInstanceDialog {
        id: duplicateDialog
        library: root.library
        // The copy can take a while; say it started, then how it went.
        property var watcher: null
        onStarted: (name, watcher) => {
            duplicateDialog.watcher = watcher
            toast.show(qsTr("Copying \u201c%1\u201d\u2026").arg(name))
        }
        onFailed: toast.show(qsTr("The instance could not be copied."), "danger")
        Connections {
            target: duplicateDialog.watcher
            function onFinished(ok) {
                toast.show(ok ? qsTr("Copy ready in your library.")
                              : qsTr("Copy failed: %1").arg(duplicateDialog.watcher.error),
                           ok ? "success" : "danger")
                duplicateDialog.watcher = null
            }
        }
    }

    ExportInstanceDialog {
        id: exportDialog
        onExported: (message) => toast.show(message, "success", qsTr("Show in folder"), function () {
            if (exportDialog.controller)
                exportDialog.controller.showInFolder()
        })
    }

    CreateShortcutDialog {
        id: shortcutDialog
        onCreated: (message) => toast.show(message, "success", qsTr("Show in folder"), function () {
            if (shortcutDialog.controller)
                shortcutDialog.controller.showInFolder()
        })
    }

    Toast {
        id: toast
        parent: Overlay.overlay
    }
}
