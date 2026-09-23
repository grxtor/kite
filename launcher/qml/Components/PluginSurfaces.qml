// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * Every plugin surface for one place in the UI (the settings, one
 * instance's page or settings), one after another, each under its title.
 * `model` is the shell's PluginSurfaceModel for that anchor.
 */
Column {
    id: root

    property var model: null
    property bool showTitles: true
    readonly property int count: repeater.count
    // The title a single surface would give a tab; "" with none.
    readonly property string firstTitle: repeater.count > 0 && repeater.itemAt(0) ? repeater.itemAt(0).title : ""

    spacing: Theme.space.xl

    Repeater {
        id: repeater
        model: root.model
        delegate: Item {
            id: surface
            required property string surfaceId
            required property string title
            required property var document
            required property int revision

            // revision is read so the node tree follows every patch.
            readonly property var rootNode: revision >= 0 && document ? (document.root || document) : ({})
            // A document that is a section already brings its own card;
            // anything else is put in one, titled with the surface.
            readonly property bool ownCard: rootNode.type === "section"

            width: root.width
            implicitHeight: ownCard ? bare.implicitHeight : card.implicitHeight

            PluginNode {
                id: bare
                visible: surface.ownCard
                width: parent.width
                surfaceId: surface.surfaceId
                sink: root.model
                node: surface.ownCard ? surface.rootNode : ({})
            }

            SettingsGroup {
                id: card
                visible: !surface.ownCard
                width: parent.width
                title: root.showTitles ? surface.title : ""

                PluginNode {
                    width: parent ? parent.width : 0
                    property bool showDivider: false
                    inCard: true
                    surfaceId: surface.surfaceId
                    sink: root.model
                    node: surface.ownCard ? ({}) : surface.rootNode
                }
            }
        }
    }
}
