// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * "Export…", "Create shortcut…", "Duplicate…" and "Share as code…" for any
 * instance, behind the library's and the instance page's ⋯ menus: the dialogs
 * and the toasts that follow them, in one place so both pages offer the same
 * thing. Until Main.qml sets the controllers, `canExport`, `canCreateShortcut`,
 * `canDuplicate` and `canShare` stay false.
 */
Item {
    id: root

    // id -> InstanceExportController / InstanceShortcutController
    // (QmlShell's exportController()/shortcutController()), or null.
    property var exportControllerFor: null
    property var shortcutControllerFor: null
    readonly property bool canExport: typeof root.exportControllerFor === "function"
    readonly property bool canCreateShortcut: typeof root.shortcutControllerFor === "function"
    property var library: null
    readonly property bool canDuplicate: !!root.library
    // QmlShell.shareCodes (ShareCodesController), or null in a build without friends.
    property var shareCodes: null
    readonly property bool canShare: !!root.shareCodes

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

    function shareAsCode(id, name) {
        if (root.canShare)
            shareDialog.openFor(id, name)
    }

    ShareCodeDialog {
        id: shareDialog
        controller: root.shareCodes
    }

    DuplicateInstanceDialog {
        id: duplicateDialog
        library: root.library
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
