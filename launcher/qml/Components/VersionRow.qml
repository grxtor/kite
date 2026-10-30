// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One version of a project in a pick-one list -- Add content's version
 * panel and "Change version…" -- with what it runs on, greyed out (and
 * saying why) when it does not fit this instance. `entry` is one item of
 * ContentBrowser.versions / ContentUpdateController.versions.
 *
 * No box of its own: the rows lie on their panel, split by hairlines. The
 * picked one is a filled radio ring and bold, the rest a hollow ring; hover
 * only brightens the ring. A filled strip for the pick, inside a framed
 * list, inside the panel, was a box in a box in a box. `installed` adds
 * "Installed" in the success colour, as words, not a chip.
 */
AbstractButton {
    id: root

    // {versionNumber, name, gameVersions, loaders, isCompatible, ...}
    property var entry: ({})
    property bool picked: false
    property bool installed: false
    // The last row draws no hairline under itself.
    property bool lastRow: false

    // Two lines at the list scale (body over label) with air around them.
    readonly property int rowHeight: Theme.control.heightLg + Theme.space.sm
    readonly property int ringSize: Theme.icon.md
    readonly property int ringWidth: 2
    readonly property int dotSize: 8

    height: root.rowHeight
    hoverEnabled: true

    background: Item {
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            visible: !root.lastRow
            color: Theme.palette.divider
        }
    }
    contentItem: RowLayout {
        spacing: Theme.space.md

        // The radio ring: filled with the accent and a dot when picked.
        Rectangle {
            Layout.preferredWidth: root.ringSize
            Layout.preferredHeight: root.ringSize
            radius: width / 2
            color: root.picked ? Theme.palette.accent : "transparent"
            border.width: root.picked ? 0 : root.ringWidth
            border.color: root.hovered ? Theme.palette.textSecondary : Theme.palette.borderStrong
            Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

            Rectangle {
                anchors.centerIn: parent
                visible: root.picked
                width: root.dotSize
                height: width
                radius: width / 2
                color: Theme.palette.textOnAccent
            }
        }
        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 0
            Text {
                width: parent.width
                text: root.entry.versionNumber || root.entry.name || ""
                elide: Text.ElideRight
                color: root.entry.isCompatible ? Theme.palette.textPrimary : Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                font.weight: root.picked ? Font.Bold : Font.Medium
            }
            Text {
                // "Fabric, Quilt for 1.21.4, 1.21.3", and why it is
                // greyed out when it does not fit this instance.
                readonly property string games: (root.entry.gameVersions || []).slice(0, 3).join(", ")
                readonly property string loaders: (root.entry.loaders || []).join(", ")
                readonly property string fits: loaders.length > 0 && games.length > 0
                                               ? qsTr("%1 for %2").arg(loaders).arg(games)
                                               : loaders + games
                width: parent.width
                text: root.entry.isCompatible ? fits
                      : fits.length > 0 ? qsTr("Not for this instance (%1)").arg(fits)
                      : qsTr("Not for this instance")
                elide: Text.ElideRight
                color: root.entry.isCompatible ? Theme.palette.textSecondary : Theme.palette.warning
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
        }
        Text {
            Layout.rightMargin: Theme.space.sm
            visible: root.installed
            text: qsTr("Installed")
            color: Theme.palette.success
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Font.DemiBold
        }
    }
}
