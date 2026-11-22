// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Home with nothing in the library: "Create instance" is the primary action, "Browse modpacks" (Discover) the second
 * way to a first instance, and under them a row of popular modpacks to install right away (PopularModpacks; gone
 * when the listing cannot be had). As wide as the page's column; the text keeps its reading width.
 */
Column {
    id: root

    // QmlShell.modpackModel and Discover's installer, for the popular row; without them there is none.
    property var modpackModel: null
    property var modpackInstaller: null
    // The row may ask Modrinth: Home is showing and nothing covers it.
    property bool suggestionsActive: false

    signal createRequested()
    signal discoverRequested()
    // A popular pack was clicked: Discover shows it.
    signal packRequested(var pack)

    readonly property int readingWidth: 400

    spacing: Theme.space.lg

    EmptyState {
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, root.readingWidth)
        title: qsTr("No instances yet")
        body: qsTr("An instance is one Minecraft setup: a version, a mod loader and its mods.")

        MeshIcon {
            iconName: "cube"
            size: Theme.icon.xl
            color: Theme.palette.textTertiary
        }
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Theme.space.md

        Button {
            highlighted: true
            buttonSize: "lg"
            text: qsTr("Create instance")
            icon.source: Icons.url("plus")
            onClicked: root.createRequested()
        }
        Button {
            buttonSize: "lg"
            text: qsTr("Browse modpacks")
            icon.source: Icons.url("compass")
            onClicked: root.discoverRequested()
        }
    }

    // Its own gap: the row is a second thing, not a third button.
    Item {
        visible: popular.visible
        width: parent.width
        height: Theme.space.lg
    }

    PopularModpacks {
        id: popular
        width: parent.width
        model: root.modpackModel
        installer: root.modpackInstaller
        active: root.suggestionsActive && root.visible && !!root.modpackModel
        onOpenRequested: (pack) => root.packRequested(pack)
    }
}
