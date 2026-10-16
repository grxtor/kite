// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * "Export…" and "Create shortcut…" for any instance, behind the library's
 * and the instance page's ⋯ menus: both dialogs and the toast that follows
 * them, in one place so the two pages offer exactly the same thing.
 *
 * The controllers come from the shell through the two functions below,
 * which Main.qml sets on each page. Until it does, `canExport` and
 * `canCreateShortcut` stay false and the menus leave their entries out.
 */
Item {
    id: root

    // id -> InstanceExportController / InstanceShortcutController
    // (QmlShell's exportController()/shortcutController()), or null.
    property var exportControllerFor: null
    property var shortcutControllerFor: null
    readonly property bool canExport: typeof root.exportControllerFor === "function"
    readonly property bool canCreateShortcut: typeof root.shortcutControllerFor === "function"

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
