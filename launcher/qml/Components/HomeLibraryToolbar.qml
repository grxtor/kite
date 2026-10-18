// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The Library section's toolbar on Home, as in Modrinth App: a wide search,
 * the sort, grid or list, and New instance, in one row under the "Library"
 * heading. The search field gives up its width first under pressure -- the
 * sort box and the labelled button keep theirs, since a shrunken combo box
 * reads as broken where a narrower search field still reads as one.
 *
 * New instance stays secondary: on Home the play bar's Play is the one
 * primary button.
 */
RowLayout {
    id: root

    property alias searchText: searchField.text
    // "grid" or "list", the page's; the toggle only asks.
    property string viewMode: "grid"

    signal viewModeActivated(string value)
    signal newInstanceRequested()

    function focusSearch() {
        searchField.forceActiveFocus()
    }

    readonly property int searchMinWidth: 160

    spacing: Theme.space.sm

    SearchBox {
        id: searchField
        Layout.fillWidth: true
        Layout.minimumWidth: root.searchMinWidth
        placeholderText: qsTr("Search instances")
    }

    ComboBox {
        id: sortBox
        model: [
            { value: "Name", label: qsTr("Name") },
            { value: "LastLaunch", label: qsTr("Last played") },
            { value: "TotalTimePlayed", label: qsTr("Time played") }
        ]
        textRole: "label"
        valueRole: "value"
        // InstSortMode is the launcher-wide setting Settings > General's
        // "Sort instances by" writes too; QmlShell re-sorts every instance
        // proxy when it changes. Followed rather than read once: it is still
        // empty when this completes, before Main.qml points SettingsStore at
        // the live adapter.
        readonly property string stored: SettingsStore.string("InstSortMode") || "Name"
        function sync() { currentIndex = Math.max(0, indexOfValue(stored)) }
        onStoredChanged: sync()
        Component.onCompleted: sync()
        onActivated: SettingsStore.setValue("InstSortMode", currentValue)

        Accessible.name: qsTr("Sort instances by")
    }

    // Grid or list: two icon buttons, the current view lit.
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
