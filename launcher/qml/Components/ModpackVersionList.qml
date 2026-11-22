// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Every version of a modpack (ModpackDetail's "Versions" tab), newest first, in one card: its number, the loaders
 * and Minecraft versions it is for, and how long ago it came out. Picking a row picks that version in the page's
 * Install card (picked(index)); nothing installs from here.
 */
BoxSurface {
    id: root

    // The detail's versions: [{ id, name, versionNumber, gameVersions, loaders, datePublished }]
    property var versions: []
    property int current: -1
    // (version) -> the short label the Install card's picker shows.
    property var labelOf: null
    signal picked(int index)

    readonly property int dateWidth: 120

    implicitHeight: column.implicitHeight + Theme.space.xs * 2

    Column {
        id: column
        x: Theme.space.lg
        y: Theme.space.xs
        width: parent.width - Theme.space.lg * 2

        Repeater {
            model: root.versions
            delegate: VersionRow {
                id: row
                required property var modelData
                required property int index
                width: column.width
                rightPadding: root.dateWidth + Theme.space.md
                entry: ({ versionNumber: root.labelOf ? root.labelOf(modelData) : (modelData.versionNumber || modelData.name),
                          gameVersions: modelData.gameVersions, loaders: (modelData.loaders || []).map((l) => Format.titleCase(l)),
                          isCompatible: true })
                picked: root.current === index
                lastRow: index === root.versions.length - 1
                Accessible.name: qsTr("Version %1").arg(entry.versionNumber)
                onClicked: root.picked(index)

                Copy {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.dateWidth
                    horizontalAlignment: Text.AlignRight
                    text: Format.ageText(row.modelData.datePublished || "")
                    role: "label"
                    tone: "tertiary"
                    elide: Text.ElideRight
                }
            }
        }
    }
}
