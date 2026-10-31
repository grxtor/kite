// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One row per installed file, reused by ContentTab for every kind (mods, packs,
 * ...). `kind` only picks the fallback glyph and the noun for delete
 * confirmations. Rows are kept by `key` (file name, ".disabled" or not), so a
 * selection survives searching, sorting and toggling.
 */
Item {
    id: root

    // InstanceContentController and its ContentFilterModel for `kind`.
    property var controller: null
    property var model: null
    property string kind: "mods"
    property bool unlocked: false
    // Why changes are off right now, for tooltips; empty when they are not.
    property string lockedReason: ""
    property string iconName: "package"
    // Lower-case plural for confirmations: "mods", "resource packs".
    property string noun: qsTr("files")

    // Keys of the ticked rows; always replaced, never changed in place, so bindings update.
    property var selectedKeys: []
    readonly property bool selecting: selectedKeys.length > 0
    property string anchorKey: ""
    property string expandedKey: ""

    readonly property int count: list.count
    readonly property int thumbSize: Theme.control.heightLg
    // The table's columns: the header and every row are laid out from these. Version only when the
    // card can spare it.
    readonly property int checkSize: 20
    readonly property bool showVersionColumn: width >= 760
    readonly property int versionColumnWidth: Math.round(width * 0.24)
    readonly property int actionsWidth: 40 + Theme.space.md + Theme.control.height
    // While the mods' health check has something to report, every row keeps room for the
    // "Problem"/"Warning" badge so the Version column lines up.
    readonly property bool showsHealth: kind === "mods" && !!controller && !!controller.health
                                        && controller.health.count > 0
    readonly property int healthBadgeWidth: showsHealth ? Math.max(problemMeasure.implicitWidth,
                                                                   warningMeasure.implicitWidth) : 0
    readonly property int healthSlotWidth: showsHealth ? healthBadgeWidth + Theme.space.md : 0
    readonly property int rowPadding: Theme.space.lg
    readonly property int detailsIndent: rowPadding + checkSize + Theme.space.md + thumbSize + Theme.space.md

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

        Rectangle {
            id: tableHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: card.border.width
            height: Theme.control.heightLg
            // Rounded to sit in the card's corners (the card's clip is square).
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
                    Layout.preferredWidth: root.actionsWidth + root.healthSlotWidth
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
            // Overlays the rows' right edge instead of reserving a gutter.
            ScrollBar.vertical: ScrollBar {}

            delegate: PackRow {
                owner: root
                width: ListView.view.width
                lastRow: index === ListView.view.count - 1
            }
        }

        ScrollFade {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: card.border.width
            flickable: list
            fadeTo: Theme.palette.surface
            radius: Theme.radius.lg
        }
    }

    StatusBadge { id: problemMeasure; visible: false; text: qsTr("Problem") }
    StatusBadge { id: warningMeasure; visible: false; text: qsTr("Warning") }

    // A search that matches nothing (ContentTab shows its own empty state for an empty folder).
    EmptyState {
        anchors.centerIn: parent
        upperThird: true
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
