// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Settings › Friends › "Share codes": every code you made (an instance's ⋯ › Share as code), newest first, with
 * what it holds and when it runs out; each row's ⋯ copies or deletes it. Read when the group is shown.
 */
SettingsGroup {
    id: root

    // QmlShell.shareCodes (ShareCodesController), or null.
    property var controller: null
    readonly property bool on: !!root.controller && root.controller.available
    readonly property var codes: root.on ? root.controller.codes : []
    readonly property var loaderNames: ({ fabric: "Fabric", quilt: "Quilt", forge: "Forge", neoforge: "NeoForge" })

    function refresh() {
        if (root.on && root.visible)
            root.controller.refreshCodes()
    }

    title: qsTr("Share codes")
    description: !root.on ? qsTr("Turn on Friends to make and see share codes.")
        : root.controller.codesLoaded && root.codes.length === 0
          ? qsTr("You have no share codes. Make one from an instance's ⋯ menu with Share as code.") : ""

    onVisibleChanged: root.refresh()
    onOnChanged: root.refresh()
    Component.onCompleted: root.refresh()

    Repeater {
        model: root.codes
        delegate: SettingRow {
            id: row
            required property var modelData

            label: row.modelData.name
            description: row.modelData.code + " · " + Format.versionLine(
                             root.loaderNames[row.modelData.loader] || "", row.modelData.minecraftVersion)
                         + " · " + qsTr("Runs out %1").arg(Format.calendarDate(new Date(row.modelData.expiresAt)))
            IconButton {
                id: more
                iconName: "more"
                tip: qsTr("More")
                Accessible.name: qsTr("More options for %1").arg(row.modelData.code)
                onClicked: menu.popup(more, more.width - menu.width, more.height + Theme.space.xs)
            }
            Menu {
                id: menu
                MenuItem {
                    text: qsTr("Copy code")
                    icon.source: Icons.url("copy")
                    onTriggered: clipboard.copyText(row.modelData.code)
                }
                MenuItem {
                    text: qsTr("Delete")
                    icon.source: Icons.url("trash")
                    onTriggered: root.controller.deleteCode(row.modelData.code)
                }
            }
        }
    }

    ClipboardText { id: clipboard }
}
