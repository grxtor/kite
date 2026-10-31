// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Home with nothing in the library: "Create instance" is the primary action, "Browse modpacks" (Discover) the second
 * way to a first instance.
 */
Column {
    id: root

    signal createRequested()
    signal discoverRequested()

    spacing: Theme.space.lg

    EmptyState {
        width: parent.width
        title: qsTr("No instances yet")
        body: qsTr("An instance is one Minecraft setup: a version, a mod loader and its mods.")

        MeshIcon {
            iconName: "cube"
            size: Theme.icon.lg + Theme.space.md
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
}
