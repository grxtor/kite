// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One instance another launcher has: a checkbox, the launcher's icon, its name
 * and a line of facts (game version, loader, size, last played), and a ⋯ menu
 * for how it comes over: copied (the default) or linked. One Kite cannot bring
 * over says why instead of the facts, and takes no tick.
 */
AbstractButton {
    id: root

    // LauncherImportModel's roles for this row.
    property var row: ({})
    // LauncherImportController.
    property var controller: null
    // While an import runs nothing is picked.
    property bool locked: false

    readonly property bool importable: (root.row.problem || "").length === 0
    readonly property bool ticked: root.row.checked === true

    function loaderName(loader) {
        switch (loader) {
        case "fabric": return "Fabric"
        case "quilt": return "Quilt"
        case "forge": return "Forge"
        case "neoforge": return "NeoForge"
        default: return ""
        }
    }

    function facts() {
        if (!root.importable)
            return root.row.problem
        var parts = [root.row.minecraftVersion]
        var loader = root.loaderName(root.row.loader)
        if (loader.length > 0)
            parts.push(loader + " " + root.row.loaderVersion)
        if ((root.row.sizeText || "").length > 0)
            parts.push(root.row.sizeText)
        parts.push(Format.lastPlayed(root.row.lastPlayed))
        return parts.join("  •  ")
    }

    enabled: root.importable && !root.locked
    hoverEnabled: true
    implicitHeight: Math.max(Theme.control.heightLg, content.implicitHeight + Theme.space.sm * 2)
    leftPadding: Theme.space.sm
    rightPadding: Theme.space.xs
    Accessible.role: Accessible.CheckBox
    Accessible.checked: root.ticked
    Accessible.name: root.row.name || ""
    Accessible.description: root.facts()
    onClicked: root.controller.setChecked(root.row.key, !root.ticked)

    background: StateFill {
        radius: Theme.radius.md
        hovered: root.hovered
        down: root.down
    }

    contentItem: RowLayout {
        id: content
        spacing: Theme.space.md

        CheckBox {
            enabled: root.enabled
            checked: root.ticked
            Accessible.name: root.row.name || ""
            onToggled: {
                root.controller.setChecked(root.row.key, checked)
                checked = Qt.binding(() => root.ticked)
            }
        }

        LauncherMark {
            launcher: root.row.launcher || ""
            size: Theme.icon.md
            opacity: root.importable ? 1 : Theme.opacity.disabled
        }

        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 1
            Copy {
                width: parent.width
                text: root.row.name || ""
                elide: Text.ElideRight
                tone: root.importable ? "primary" : "tertiary"
                font.weight: Font.Medium
            }
            Copy {
                width: parent.width
                text: root.facts()
                role: "caption"
                tone: "tertiary"
                elide: Text.ElideRight
            }
            Copy {
                width: parent.width
                visible: root.importable && (root.row.leftBehind || "").length > 0
                text: qsTr("Not brought over: %1").arg(root.row.leftBehind || "")
                role: "caption"
                tone: "warning"
                elide: Text.ElideRight
            }
        }

        Tag {
            visible: root.row.imported === true
            text: qsTr("In Kite")
        }
        Tag {
            visible: root.row.linked === true
            text: qsTr("Linked")
            iconName: "folder-input"
        }

        IconButton {
            id: moreButton
            visible: root.importable
            enabled: !root.locked
            iconName: "more"
            tip: qsTr("More")
            onClicked: menu.popup(moreButton, 0, moreButton.height + Theme.space.xs)

            Menu {
                id: menu
                MenuItem {
                    text: qsTr("Copy")
                    checkable: true
                    checked: root.row.linked !== true
                    onTriggered: {
                        root.controller.setMode(root.row.key, "copy")
                        checked = Qt.binding(() => root.row.linked !== true)
                    }
                }
                MenuItem {
                    text: root.row.linkable === true ? qsTr("Link")
                                                     : qsTr("Link: %1").arg(root.row.linkBlocker || "")
                    checkable: true
                    checked: root.row.linked === true
                    enabled: root.row.linkable === true
                    onTriggered: {
                        root.controller.setMode(root.row.key, "link")
                        checked = Qt.binding(() => root.row.linked === true)
                    }
                }
                MenuSeparator {}
                MenuItem {
                    text: qsTr("Open folder")
                    icon.source: Icons.url("folder")
                    onTriggered: root.controller.openFolder(root.row.key)
                }
            }
        }
    }
}
