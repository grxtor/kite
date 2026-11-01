// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One version of a project in a pick-one list, greyed out (and saying why) when
 * it does not fit this instance. `entry` is one item of ContentBrowser.versions /
 * ContentUpdateController.versions. No box of its own: rows lie on their panel,
 * split by hairlines. `installed` adds "Installed" in the success colour.
 */
AbstractButton {
    id: root

    // {versionNumber, name, gameVersions, loaders, isCompatible, ...}
    property var entry: ({})
    property bool picked: false
    property bool installed: false
    property bool lastRow: false

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
            Copy {
                width: parent.width
                text: root.entry.versionNumber || root.entry.name || ""
                tone: root.entry.isCompatible ? "primary" : "tertiary"
                elide: Text.ElideRight
                font.weight: root.picked ? Font.Bold : Font.Medium
            }
            Copy {
                readonly property string games: (root.entry.gameVersions || []).slice(0, 3).join(", ")
                readonly property string loaders: (root.entry.loaders || []).join(", ")
                readonly property string fits: loaders.length > 0 && games.length > 0
                                               ? qsTr("%1 for %2").arg(loaders).arg(games)
                                               : loaders + games
                width: parent.width
                text: root.entry.isCompatible ? fits
                      : fits.length > 0 ? qsTr("Not for this instance (%1)").arg(fits)
                      : qsTr("Not for this instance")
                role: "label"
                tone: root.entry.isCompatible ? "secondary" : "warning"
                elide: Text.ElideRight
            }
        }
        Copy {
            Layout.rightMargin: Theme.space.sm
            visible: root.installed
            text: qsTr("Installed")
            role: "label"
            tone: "success"
            font.weight: Font.DemiBold
        }
    }
}
