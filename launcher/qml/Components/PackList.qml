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
    // Of the card's inside edge, for whatever sits in its corners.
    readonly property real cornerRadius: card.radius - card.border.width
    // What the card lies on: its bottom corners are trimmed back to this. Over the Gece sky, the sky's colour
    // at about where the card ends.
    property color matte: Theme.look === "night" ? Theme.sky.colorAt(root.skyAt) : Theme.palette.canvas
    readonly property real skyAt: {
        card.height
        const win = root.Window.window
        return win && win.height > 0 ? card.mapToItem(null, 0, card.height).y / win.height : 0.9
    }
    readonly property int detailsIndent: rowPadding + checkSize + Theme.space.md + thumbSize + Theme.space.md

    property bool updatable: false
    // ContentUpdateList: what the last "Check for updates" found for this kind, or null. A row with an update
    // shows the new version and an Update button; every row then keeps room for the button.
    property var updates: null
    readonly property int updateCount: updates ? updates.count : 0
    readonly property int updateSlotWidth: updateCount > 0 ? updateMeasure.implicitWidth + Theme.space.md : 0
    signal updateRequested(string key)

    signal openFolderRequested(string path)
    signal changeVersionRequested(string key)
    signal resetMetadataRequested(string key, string name)
    signal removeRequested(string key, string name)

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

    // ContentTab removes it: at once when it goes to the trash, after a question when it does not.
    function confirmRemove(key, name) {
        root.removeRequested(key, name)
    }

    onKindChanged: {
        clearSelection()
        root.expandedKey = ""
    }

    BoxSurface {
        id: card
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.min(parent.height, tableHeader.height + list.contentHeight + list.topMargin
                                        + list.bottomMargin + border.width * 2)
        visible: list.count > 0
        clip: true

        Rectangle {
            id: tableHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: card.border.width
            height: Theme.control.heightLg
            // Rounded to sit in the card's corners (the card's clip is square).
            radius: root.cornerRadius
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
                Copy {
                    Layout.fillWidth: true
                    text: qsTr("Name")
                    role: "label"
                    tone: "secondary"
                    font.weight: Font.DemiBold
                }
                Copy {
                    visible: root.showVersionColumn
                    Layout.preferredWidth: root.versionColumnWidth
                    text: qsTr("Version")
                    role: "label"
                    tone: "secondary"
                    font.weight: Font.DemiBold
                }
                Copy {
                    Layout.preferredWidth: root.actionsWidth + root.healthSlotWidth + root.updateSlotWidth
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Actions")
                    role: "label"
                    tone: "secondary"
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
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.model
            // Overlays the rows' right edge instead of reserving a gutter.
            ScrollBar.vertical: ScrollBar {}

            // A removed file fades out and the rows under it close the gap.
            remove: Transition {
                NumberAnimation { property: "opacity"; to: 0; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
            }
            displaced: Transition {
                NumberAnimation { property: "y"; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
            }

            ListEntrance { id: entrance; view: list }
            delegate: PackRow {
                id: packRow
                owner: root
                width: ListView.view.width
                lastRow: index === ListView.view.count - 1
                RiseIn { target: packRow; index: packRow.index; gate: entrance }
            }
        }

        ScrollFade {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: card.border.width
            flickable: list
            fadeTo: Theme.palette.surface
        }

        // The card's clip is square, so a row's fill would poke through the rounded corners: a ring in the
        // colour behind the card covers them (only its four corners fall inside), and the border goes back on top.
        Rectangle {
            readonly property int ring: card.radius + 2
            anchors.fill: parent
            anchors.margins: -ring
            radius: card.radius + ring
            color: "transparent"
            border.width: ring
            border.color: root.matte
            z: 2
        }
        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: "transparent"
            border.width: card.border.width
            border.color: card.border.color
            z: 2
        }
    }

    StatusBadge { id: problemMeasure; visible: false; text: qsTr("Problem") }
    StatusBadge { id: warningMeasure; visible: false; text: qsTr("Warning") }
    Button { id: updateMeasure; visible: false; buttonSize: "sm"; text: qsTr("Update") }

    // A search that matches nothing (ContentTab shows its own empty state for an empty folder).
    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        width: parent.width
        visible: !!root.model && root.model.totalCount > 0 && root.model.count === 0
        title: qsTr("No matches")
        body: root.model ? qsTr("Nothing here matches “%1”.").arg(root.model.filterText) : ""
        MeshIcon { iconName: "search"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
    }
}
