// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One row per installed file, reused by ContentTab for whichever kind is
 * selected (mods, core mods, resource/shader/texture packs, data packs) --
 * all a ContentFilterModel over a ModFolderModel underneath, with the same
 * roles, so a single delegate does for every kind. Each row shows the logo
 * the file declares for itself (iconUrl: a mod's fabric.mod.json/mods.toml
 * logo, a pack's pack.png); `kind` only picks the glyph shown when it has
 * none, and the noun used when confirming a delete.
 *
 * Laid out like every launcher's mod list: what it is on the left, its
 * on/off switch and a ⋯ menu (also on right-click) on the right. Clicking
 * a row opens its details (description, authors, version, where it came
 * from). Selecting works the way file lists do: hovering a row turns its
 * logo into a checkbox, and once anything is ticked every row shows one;
 * Cmd/Ctrl-click toggles a row, Shift-click ticks a range. Rows are kept
 * by `key` (the file name, ".disabled" or not), so a selection survives
 * searching, sorting and switching files on and off.
 */
Item {
    id: root

    // InstanceContentController and the ContentFilterModel it gave for
    // `kind`.
    property var controller: null
    property var model: null
    property string kind: "mods"
    property bool unlocked: false
    // Why changes are off right now, for tooltips; empty when they are not.
    property string lockedReason: ""
    property string iconName: "package"
    // Lower-case plural for confirmations: "mods", "resource packs".
    property string noun: qsTr("files")

    // Keys of the ticked rows. Always replaced, never changed in place, so
    // bindings on it update.
    property var selectedKeys: []
    readonly property bool selecting: selectedKeys.length > 0
    // Where a Shift-click range starts.
    property string anchorKey: ""
    // The one row whose details are open.
    property string expandedKey: ""

    readonly property int count: list.count
    readonly property int thumbSize: 36
    // The same frame InstanceListRow gives an instance icon.
    readonly property int thumbInset: Theme.space.xs
    // Details line up under the name, past the logo.
    readonly property int detailsIndent: Theme.space.md + thumbSize + Theme.space.md

    signal openFolderRequested(string path)

    function isSelected(key) {
        return root.selectedKeys.indexOf(key) >= 0
    }
    function toggle(key) {
        const keys = root.selectedKeys.slice()
        const at = keys.indexOf(key)
        if (at >= 0)
            keys.splice(at, 1)
        else
            keys.push(key)
        root.selectedKeys = keys
        root.anchorKey = key
    }
    // Ticks every row from the last one clicked to @p key, as shown.
    function selectRange(key) {
        if (!root.model)
            return
        const shown = root.model.visibleKeys()
        const from = shown.indexOf(root.anchorKey)
        const to = shown.indexOf(key)
        if (from < 0 || to < 0) {
            toggle(key)
            return
        }
        const keys = root.selectedKeys.slice()
        for (let i = Math.min(from, to); i <= Math.max(from, to); ++i) {
            if (keys.indexOf(shown[i]) < 0)
                keys.push(shown[i])
        }
        root.selectedKeys = keys
    }
    function selectAll() {
        root.selectedKeys = root.model ? root.model.visibleKeys() : []
    }
    function clearSelection() {
        root.selectedKeys = []
        root.anchorKey = ""
    }

    function confirmRemove(key, name) {
        confirm.key = key
        confirm.text = qsTr("Remove “%1” from this instance? The file is deleted.").arg(name)
        confirm.open()
    }

    onKindChanged: {
        clearSelection()
        root.expandedKey = ""
    }

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        // One rhythm for every list tab: Theme.space.sm between rows, rows
        // inset by sm on the right and spaced md inside.
        spacing: Theme.space.sm
        boundsBehavior: Flickable.StopAtBounds
        model: root.model
        // Overlays the rows' right edge rather than reserving a gutter, so
        // the rows end where the toolbar above them does.
        ScrollBar.vertical: ScrollBar {}

        delegate: PackRow {
            owner: root
            width: ListView.view.width
        }
    }

    // Rows fade out at the fold instead of being sliced by the play bar.
    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: list
    }

    // A search that matches nothing; the folder itself is not empty
    // (ContentTab shows its own empty state for that).
    EmptyState {
        anchors.centerIn: parent
        // Its own width, or it takes its title's.
        width: parent.width
        visible: !!root.model && root.model.totalCount > 0 && root.model.count === 0
        title: qsTr("No matches")
        body: root.model ? qsTr("Nothing here matches “%1”.").arg(root.model.filterText) : ""
        MeshIcon { iconName: "search"; size: 40; color: Theme.palette.textTertiary }
    }

    ConfirmDialog {
        id: confirm
        property string key: ""
        title: qsTr("Remove")
        confirmText: qsTr("Remove")
        onConfirmed: {
            if (root.controller)
                root.controller.remove(root.kind, [key])
            if (root.isSelected(key))
                root.toggle(key)
        }
    }
}
