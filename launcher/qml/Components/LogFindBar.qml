// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The find controls of a log's toolbar: a search box and, once something is
 * typed, "3 of 12" with previous and next. It only edits the search text; the log
 * it sits over owns the matches (see LogLines.qml). "Wrap lines" is a setting of
 * the view, kept here only as the value LogLines reads. Enter steps to the next
 * match, Shift+Enter to the previous.
 */
RowLayout {
    id: root

    property alias text: field.text
    // How many lines contain the text, and which of them is current (0-based,
    // -1 when none).
    property int matchCount: 0
    property int current: -1
    property bool wrap: true
    property bool searchEnabled: true

    signal next()
    signal previous()

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
        visible: root.text.length > 0 && root.searchEnabled
        enabled: root.matchCount > 0
        flat: false
        iconName: "arrow-up"
        tip: qsTr("Previous match")
        onClicked: root.previous()
    }
    IconButton {
        visible: root.text.length > 0 && root.searchEnabled
        enabled: root.matchCount > 0
        flat: false
        iconName: "arrow-down"
        tip: qsTr("Next match")
        onClicked: root.next()
    }
}
