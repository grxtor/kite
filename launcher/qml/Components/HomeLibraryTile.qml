// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One instance in the Home page's "Your library" strip: just its icon on a
 * small filled square, with the name as a tooltip. Clicking it opens the
 * instance page. Used directly as a Repeater delegate against the instance
 * model, so the required properties are filled from its roles -- the same
 * ones RecentItem reads, which this replaces on Home: RecentItem's compact
 * form is a sidebar rail item and keeps its 200px row width, so the strip
 * had to force its width from outside.
 *
 * Same tile grammar as HomeWorldTile: a filled surface square with no
 * outline, and hover lifts only that fill to surfaceRaised.
 */
AbstractButton {
    id: control

    // Filled straight from the model's roles; declared here rather than by
    // the caller, since a caller redeclaring them would shadow these.
    required property string instanceId
    required property string name
    required property string iconKey
    required property bool isRunning

    // The same square as RecentItem's/NavItem's rail item, so the strip
    // reads at the same scale as the sidebar beside it.
    readonly property int tileSize: 40
    readonly property int iconSize: 32
    // Same delay as HomeWorldTile's tooltip, the row above this one.
    readonly property int tipDelay: 500
    readonly property int runningDotSize: 8

    implicitWidth: control.tileSize
    implicitHeight: control.tileSize
    hoverEnabled: true

    Accessible.name: control.name

    ToolTip.visible: control.hovered
    ToolTip.delay: control.tipDelay
    ToolTip.text: control.name

    background: Rectangle {
        radius: Theme.radius.sm
        color: control.down ? Theme.palette.surfaceOverlay
             : control.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    contentItem: Item {
        Image {
            id: icon
            anchors.centerIn: parent
            width: control.iconSize
            height: control.iconSize
            source: control.iconKey.length > 0 ? "image://instanceicon/" + control.iconKey : ""
            sourceSize: Qt.size(width, height)
            fillMode: Image.PreserveAspectFit
            asynchronous: true
        }

        // Running: a static mark, not RecentItem's pulse -- this strip can
        // hold the whole library, and a quiet dot is enough to find the
        // running one in it.
        Rectangle {
            visible: control.isRunning
            anchors.right: icon.right
            anchors.bottom: icon.bottom
            width: control.runningDotSize
            height: control.runningDotSize
            radius: control.runningDotSize / 2
            color: Theme.palette.success
        }
    }

    // Keyboard focus ring, drawn inside the tile rather than outset like
    // HomeJumpCard's: the strip's Flickable clips exactly at the tiles' own
    // height, so an outset ring would lose its top and bottom edges.
    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.sm
        color: "transparent"
        border.width: 2
        border.color: Theme.palette.focusRing
        visible: control.visualFocus
    }
}
