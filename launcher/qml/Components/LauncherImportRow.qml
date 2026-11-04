// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One instance another launcher has: a checkbox, the instance's own icon
 * (Kite's default when it has none), its name and a line of facts (game
 * version, loader, size, last played), and a ⋯ menu for how it comes over:
 * copied (the default) or linked. One Kite cannot bring over says why instead
 * of the facts, and takes no tick; a note like that, or what a copy leaves
 * behind, is a muted line with a small warning mark.
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
    readonly property int iconSize: 32
    readonly property real noteMarkEmphasis: 0.7

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
        var parts = [root.row.minecraftVersion]
        var loader = root.loaderName(root.row.loader)
        if (loader.length > 0)
            parts.push(loader + " " + root.row.loaderVersion)
        if ((root.row.sizeText || "").length > 0)
            parts.push(root.row.sizeText)
        parts.push(Format.lastPlayed(root.row.lastPlayed))
        return parts.join("  •  ")
    }

    // One already in Kite takes no second tick.
    enabled: root.importable && root.row.imported !== true && !root.locked
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

        Image {
            Layout.preferredWidth: root.iconSize
            Layout.preferredHeight: root.iconSize
            source: (root.row.iconFile || "").length > 0 ? root.row.iconFile
                                                        : Icons.instanceIcon(root.row.iconKey || "default")
            sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            smooth: true
            mipmap: true
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
                visible: root.importable
                text: root.facts()
                role: "caption"
                tone: "tertiary"
                elide: Text.ElideRight
            }
            NoteLine {
                width: parent.width
                visible: !root.importable
                text: root.row.problem || ""
            }
            NoteLine {
                width: parent.width
                visible: root.importable && (root.row.leftBehind || "").length > 0
                text: qsTr("Not brought over: %1").arg(root.row.leftBehind || "")
            }
        }

        Tag {
            visible: root.row.imported === true
            text: qsTr("In Kite")
        }
        // The way it comes over when not the default copy.
        Tag {
            visible: root.row.linked === true && root.row.imported !== true
            text: qsTr("Will link")
            iconName: "folder-input"
        }

        IconButton {
            id: moreButton
            visible: root.importable
            enabled: !root.locked
            iconName: "more"
            tip: qsTr("More")
            // Right-aligned to the button, so it opens inside the list.
            onClicked: menu.popup(moreButton, moreButton.width - menu.width, moreButton.height + Theme.space.xs)

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

    // A note in the muted text colour behind a small mark in the warning colour, held back.
    component NoteLine: Row {
        id: note
        property string text: ""
        spacing: Theme.space.xs
        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            iconName: "alert-triangle"
            size: Theme.icon.xxs
            color: Theme.palette.warning
            opacity: root.noteMarkEmphasis
        }
        Copy {
            width: note.width - Theme.icon.xxs - note.spacing
            text: note.text
            role: "caption"
            tone: "tertiary"
            elide: Text.ElideRight
        }
    }
}
