// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The Library section's toolbar on Home: a wide search, sort, grid or list, and New instance. The search field
 * gives up its width first. The sort button names the current order and opens a menu with "Group instances" (the
 * InstGrouped setting, handed by QmlShell to the library proxy).
 */
RowLayout {
    id: root

    property alias searchText: searchField.text
    property string viewMode: "grid"
    // The library proxy (QmlShell's instance model); whether it shows groups is its `grouped`.
    property var libraryModel: null

    signal viewModeActivated(string value)
    signal newInstanceRequested()

    function focusSearch() {
        searchField.forceActiveFocus()
    }

    readonly property int searchMinWidth: 160

    // InstSortMode is the launcher-wide setting Settings > General also writes; QmlShell re-sorts every instance
    // proxy when it changes. Followed rather than read once: it is still empty when this completes.
    readonly property var sortModes: SettingsStore.instanceSortModes
    readonly property string sortMode: SettingsStore.string("InstSortMode") || "Name"
    readonly property string sortLabel: {
        var mode = root.sortModes.find((entry) => entry.value === root.sortMode)
        return mode ? mode.label : root.sortModes[0].label
    }
    readonly property bool grouped: !root.libraryModel || root.libraryModel.grouped !== false

    function openSortMenu() {
        sortMenu.popup(sortButton, 0, sortButton.height + Theme.space.xs)
    }

    spacing: Theme.space.sm

    SearchBox {
        id: searchField
        Layout.fillWidth: true
        Layout.minimumWidth: root.searchMinWidth
        placeholderText: qsTr("Search instances")
    }

    Button {
        id: sortButton
        text: root.sortLabel
        icon.source: Icons.url("sort")
        onClicked: sortMenu.visible ? sortMenu.close() : root.openSortMenu()

        Accessible.name: qsTr("Sort instances by %1").arg(root.sortLabel)
    }

    // One order checked at a time; choosing the checked one again leaves
    // it checked.
    ButtonGroup { id: sortGroup }

    Menu {
        id: sortMenu

        Instantiator {
            model: root.sortModes
            delegate: MenuItem {
                required property var modelData
                text: modelData.label
                checkable: true
                checked: root.sortMode === modelData.value
                ButtonGroup.group: sortGroup
                onTriggered: SettingsStore.setValue("InstSortMode", modelData.value)
            }
            onObjectAdded: (index, object) => sortMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => sortMenu.removeItem(object)
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("Group instances")
            checkable: true
            checked: root.grouped
            onToggled: SettingsStore.setValue("InstGrouped", checked)
        }
    }

    ViewModeToggle {
        current: root.viewMode
        onActivated: (value) => root.viewModeActivated(value)
    }

    Button {
        text: qsTr("New instance")
        icon.source: Icons.url("plus")
        onClicked: root.newInstanceRequested()
    }
}
