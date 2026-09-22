// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Page header: title with an optional count, a search field, and whatever
 * actions the page passes in as children. It sits on the page itself rather
 * than on its own band, so the page reads as one surface.
 */
Item {
    id: root

    property string title: ""
    property int count: -1
    property string searchText: ""
    property string searchPlaceholder: qsTr("Search")
    property bool searchVisible: true

    default property alias actions: actionsRow.data

    implicitHeight: Theme.control.heightLg + Theme.space.xl

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        anchors.topMargin: Theme.space.sm
        spacing: Theme.space.md

        Text {
            text: root.title
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.heading.pixelSize + 4
            font.weight: Font.Bold
            font.letterSpacing: -0.3
        }

        Tag {
            visible: root.count >= 0
            text: root.count
        }

        Item { Layout.fillWidth: true }

        SearchField {
            id: searchField
            visible: root.searchVisible
            Layout.preferredWidth: 300
            Layout.minimumWidth: 160
            Layout.fillWidth: false
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
