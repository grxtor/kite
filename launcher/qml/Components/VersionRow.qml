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
 * ContentBrowser.versions / ContentUpdateController.versions. The picked
 * row is ticked and keeps the hover look (a lifted fill and a hairline, no
 * accent); `installed` adds a tag for the version the instance has now.
 */
AbstractButton {
    id: root

    // {versionNumber, name, gameVersions, loaders, isCompatible, ...}
    property var entry: ({})
    property bool picked: false
    property bool installed: false

    // Two lines at the list scale (body over label) with air around them.
    readonly property int rowHeight: Theme.control.heightLg + Theme.space.sm

    height: root.rowHeight
    hoverEnabled: true

    background: Rectangle {
        // Picked looks like hovered. Both fade in from their own colour at
        // zero alpha, so nothing greys on the way from "transparent".
        readonly property bool lifted: root.picked || root.hovered
        readonly property color fill: Theme.palette.surfaceHover
        readonly property color edge: Theme.neutral.borderHover
        radius: Theme.radius.md
        color: lifted ? fill : Qt.rgba(fill.r, fill.g, fill.b, 0)
        border.width: 1
        border.color: lifted ? edge : Qt.rgba(edge.r, edge.g, edge.b, 0)
        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast } }
    }
    contentItem: RowLayout {
        spacing: Theme.space.sm
        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: Theme.space.sm
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
        Tag {
            visible: root.installed
            text: qsTr("Installed")
        }
        MeshIcon {
            Layout.rightMargin: Theme.space.sm
            visible: root.picked
            iconName: "check"
            size: Theme.icon.sm
            color: Theme.palette.accentText
        }
    }
}
