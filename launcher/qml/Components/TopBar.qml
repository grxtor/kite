// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Page header: title, search, trailing actions. Trailing actions are a
 * default-property slot (a Row) rather than a fixed "New instance" button,
 * since the brief only gives that button as an example of what can go there.
 */
Rectangle {
    id: root

    property string title: ""
    property string searchText: ""
    property string searchPlaceholder: qsTr("Search")

    default property alias actions: actionsRow.data

    implicitHeight: Theme.control.heightLg + Theme.space.md * 2
    color: Theme.palette.surface

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.lg
        anchors.rightMargin: Theme.space.lg
        spacing: Theme.space.md

        Text {
            text: root.title
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.heading.pixelSize
            font.weight: Theme.type.heading.weight
        }

        Item { Layout.fillWidth: true }

        TextField {
            id: searchField
            Layout.preferredWidth: 280
            placeholderText: root.searchPlaceholder

            // TextField.text is a plain notifying property, not a
            // bindable one -- typing into it would otherwise permanently
            // sever a plain "text: root.searchText" binding. Both directions
            // are wired explicitly instead so external resets of
            // searchText still take.
            onTextChanged: if (text !== root.searchText) root.searchText = text
            Component.onCompleted: text = root.searchText

            Connections {
                target: root
                function onSearchTextChanged() {
                    if (searchField.text !== root.searchText)
                        searchField.text = root.searchText
                }
            }
        }

        Row {
            id: actionsRow
            spacing: Theme.space.sm
        }
    }
}
