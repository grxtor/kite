// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One row per installed file, reused by ContentTab for whichever kind is
 * selected (mods, core mods, resource/shader/texture packs, data packs) --
 * all a ContentFilterModel over a ModFolderModel underneath, with the same
 * roles, so a single delegate does for every kind. Each row shows the logo
 * the file declares for itself (iconUrl: a mod's fabric.mod.json/mods.toml
 * logo, a pack's pack.png); `kind` only picks the glyph shown when it has
 * none, and the noun used when confirming a delete.
 *
 * Laid out like Modrinth App's content table: one rounded card with a sticky
 * header (select all, Name, Version, Actions) over roomy rows split by
 * hairlines, as tall as they are up to the room there is -- a tick box, the
 * logo, what it is, its version (when there is room), its on/off switch and
 * a ⋯ menu (also on right-click). Clicking
 * a row opens its details (description, authors, version, where it came
 * from). Selecting works the way file lists do: every row has its tick box;
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
    readonly property int thumbSize: 44
    // The same frame InstanceListRow gives an instance icon.
    readonly property int thumbInset: Theme.space.xs + Theme.space.xxs
    // The tick box column, and the table's columns: the header and every
    // row are laid out from these, so they line up. Version only when the
    // card is wide enough to spare it, as in Modrinth App's table.
    readonly property int checkSize: 20
    readonly property bool showVersionColumn: width >= 760
    readonly property int versionColumnWidth: Math.round(width * 0.24)
    // Switch and ⋯ menu on the right.
    readonly property int actionsWidth: 40 + Theme.space.md + Theme.control.height
    readonly property int rowPadding: Theme.space.lg
    // Details line up under the name, past the tick box and the logo.
    readonly property int detailsIndent: rowPadding + checkSize + Theme.space.md + thumbSize + Theme.space.md

    // Whether files of `kind` can come from a provider (ContentTab).
    property bool updatable: false

    signal openFolderRequested(string path)
    signal changeVersionRequested(string key)
    signal resetMetadataRequested(string key, string name)

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
    readonly property bool allSelected: !!model && model.count > 0 && selectedKeys.length >= model.count
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

    Rectangle {
        id: card
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.min(parent.height, tableHeader.height + list.contentHeight + list.topMargin
                                        + list.bottomMargin + border.width * 2)
        visible: list.count > 0
        radius: Theme.radius.lg
        color: Theme.palette.surface
        border.width: 1
        border.color: Theme.palette.border
        clip: true

        // The sticky header: select-all, then a label for each column.
        Rectangle {
            id: tableHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: card.border.width
            height: Theme.control.heightLg
            // Rounded to sit in the card's corners (the card's clip is
            // square); the strip below squares its foot again.
            radius: card.radius - card.border.width
            color: Theme.palette.surfaceRaised
            z: 1

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.radius
                color: parent.color
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.palette.divider
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.rowPadding
                anchors.rightMargin: root.rowPadding
                spacing: Theme.space.md

                CheckBox {
                    Layout.preferredWidth: root.checkSize
                    padding: 0
                    tristate: true
                    checkState: root.allSelected ? Qt.Checked
                              : root.selecting ? Qt.PartiallyChecked : Qt.Unchecked
                    Accessible.name: qsTr("Select all")
                    onClicked: {
                        if (root.selecting)
                            root.clearSelection()
                        else
                            root.selectAll()
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Name")
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.weight: Font.DemiBold
                }
                Text {
                    visible: root.showVersionColumn
                    Layout.preferredWidth: root.versionColumnWidth
                    text: qsTr("Version")
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.preferredWidth: root.actionsWidth
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Actions")
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.weight: Font.DemiBold
                }
            }
        }

        ListView {
            id: list
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: tableHeader.bottom
            anchors.bottom: parent.bottom
            anchors.margins: card.border.width
            anchors.topMargin: 0
            bottomMargin: Theme.space.xs
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.model
            // Overlays the rows' right edge rather than reserving a gutter, so
            // the rows end where the toolbar above them does.
            ScrollBar.vertical: ScrollBar {}

            delegate: PackRow {
                owner: root
                width: ListView.view.width
                lastRow: index === ListView.view.count - 1
            }
        }

        // Rows fade out at the fold instead of being sliced by the play bar.
        ScrollFade {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: card.border.width
            flickable: list
            fadeTo: Theme.palette.surface
            // Rounded like the card, or its square foot would cover the
            // card's bottom corners.
            radius: Theme.radius.lg
        }
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
