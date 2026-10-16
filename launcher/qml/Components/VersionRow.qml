// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One version of a project in a pick-one list -- Add content's version
 * panel and "Change version…" -- with what it runs on, greyed out (and
 * saying why) when it does not fit this instance. `entry` is one item of
 * ContentBrowser.versions / ContentUpdateController.versions. The picked
 * row is tinted and ticked; `installed` adds a tag for the version the
 * instance has now.
 */
AbstractButton {
    id: root

    // {versionNumber, name, gameVersions, loaders, isCompatible, ...}
    property var entry: ({})
    property bool picked: false
    property bool installed: false

    readonly property int rowHeight: 40

    height: root.rowHeight
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.radius.sm
        color: root.picked ? Theme.palette.accentSubtle
             : root.hovered ? Theme.palette.hoverOverlay : "transparent"
        border.width: root.picked ? 1 : 0
        border.color: Theme.palette.accent
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
                font.pixelSize: Theme.type.label.pixelSize
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
                color: root.entry.isCompatible ? Theme.palette.textTertiary : Theme.palette.warning
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
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
