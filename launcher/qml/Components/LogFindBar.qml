// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The find row above a log: a search box, "3 of 12", previous and next, and
 * the wrap switch. It only edits the search text and says which way to step;
 * the log it sits over owns the matches (see LogLines.qml).
 *
 * Enter steps to the next match and Shift+Enter to the previous one, the way
 * a browser's find bar does.
 */
RowLayout {
    id: root

    property alias text: field.text
    // How many lines contain the text, and which of them is current (0-based,
    // -1 when none).
    property int matchCount: 0
    property int current: -1
    property alias wrap: wrapSwitch.checked
    // False while there is nothing to search (no log yet); the wrap switch
    // stays usable.
    property bool searchEnabled: true

    signal next()
    signal previous()

    readonly property int fieldWidth: 300
    // The box shrinks, down to this, before the row pushes past its column.
    readonly property int fieldMinWidth: 120

    function focusField() {
        field.forceActiveFocus()
        field.selectAll()
    }

    spacing: Theme.space.sm

    SearchBox {
        id: field
        enabled: root.searchEnabled
        Layout.fillWidth: true
        Layout.maximumWidth: root.fieldWidth
        Layout.minimumWidth: root.fieldMinWidth
        placeholderText: qsTr("Find in log")
        Accessible.name: qsTr("Find in log")

        Keys.onReturnPressed: (event) => {
            if (event.modifiers & Qt.ShiftModifier)
                root.previous()
            else
                root.next()
        }
        Keys.onEnterPressed: (event) => {
            if (event.modifiers & Qt.ShiftModifier)
                root.previous()
            else
                root.next()
        }
    }

    Text {
        visible: root.text.length > 0 && root.searchEnabled
        text: root.matchCount === 0 ? qsTr("No matches")
                                    : qsTr("%1 of %2").arg(root.current + 1).arg(root.matchCount)
        color: Theme.palette.textTertiary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
    }

    IconButton {
        enabled: root.matchCount > 0
        flat: false
        iconName: "arrow-up"
        tip: qsTr("Previous match")
        onClicked: root.previous()
    }
    IconButton {
        enabled: root.matchCount > 0
        flat: false
        iconName: "arrow-down"
        tip: qsTr("Next match")
        onClicked: root.next()
    }

    Item { Layout.fillWidth: true }

    Switch {
        id: wrapSwitch
        checked: true
        text: qsTr("Wrap lines")
    }
}
