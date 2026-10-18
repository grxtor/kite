// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * A page's toolbar: a search field and whatever actions the page passes in
 * as children, on the same column as the page's own content. The search
 * field gives up its width first under pressure -- the actions keep their
 * own size, since a squeezed combo box or labelled button reads as broken
 * where a narrower search field still reads as a search field.
 *
 * The Library's sits at the top of the page area (Main.qml); TopBar uses
 * one as its own optional second row.
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

    // The same column every page's content uses: the page padding, or on a
    // wide window whatever centres a Theme.layout.maxContentWidth column --
    // so the toolbar stays aligned with the cards below it.
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
            Layout.maximumWidth: 320
            placeholderText: root.searchPlaceholder

            // TextField.text is a plain notifying property, not a
            // bindable one -- typing into it would otherwise permanently
            // sever a plain "text: root.searchText" binding. Both
            // directions are wired explicitly instead so external resets
            // of searchText still take.
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

        Item { Layout.fillWidth: true }

        Row {
            id: actionsRow
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space.sm
        }
    }
}
