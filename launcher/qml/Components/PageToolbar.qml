// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * A page's toolbar: a wide search field and the actions the page passes in as
 * children, on the same column as the page's content. The search field takes the
 * width the actions leave and gives it up first under pressure; the actions keep
 * their size. The Library's sits at the top of the page area; TopBar uses one as
 * its optional second row.
 */
Item {
    id: root

    property string searchText: ""
    property string searchPlaceholder: qsTr("Search")
    property bool searchVisible: true

    default property alias actions: actionsRow.data

    implicitHeight: Theme.control.height

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    readonly property int sideMargin: Math.max(Theme.space.xl + Theme.space.xs,
                                               Math.round((root.width - Theme.layout.maxContentWidth) / 2))

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.sideMargin
        anchors.rightMargin: root.sideMargin
        spacing: Theme.space.sm

        SearchBox {
            id: searchField
            visible: root.searchVisible
            Layout.fillWidth: true
            Layout.minimumWidth: 120
            placeholderText: root.searchPlaceholder

            // TextField.text is not bindable: typing would sever a plain "text: root.searchText"
            // binding, so both directions are wired explicitly.
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

        // Only without a search field: the actions still sit on the right.
        Item { Layout.fillWidth: true; visible: !root.searchVisible }

        Row {
            id: actionsRow
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space.sm
        }
    }
}
