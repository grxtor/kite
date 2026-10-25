// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * A checklist of the catalog's mods (BundleCatalog::mods()), shared by the
 * Content tab's Add-ons and Kite Performance dialogs (BundleDialog). Each
 * row has the mod's icon, its name and a line about it. Rows are grouped
 * under a small heading per category, and per bundle too when the list
 * spans more than one.
 *
 * Ticks start from each mod's own default, bar the ones in `excluded`, and
 * are re-seeded whenever `mods` changes. A mod the instance already has is
 * shown ticked and locked, with an "Installed" tag, and is left out of
 * `checkedProjects`. A mod servers ban (`serverSafe: false`) carries a
 * warning under its summary.
 */
ListView {
    id: root

    // BundleCatalog::mods() rows.
    property var mods: []
    // Modrinth project ids the instance already has.
    property var installed: []
    // Project ids that start unticked although the catalog ticks them.
    property var excluded: []
    // What the user has ticked, installed mods excluded.
    readonly property var checkedProjects: {
        const out = []
        for (let i = 0; i < root.mods.length; ++i) {
            const id = root.mods[i].project
            if (root.ticks[id] === true && root.installed.indexOf(id) < 0 && out.indexOf(id) < 0)
                out.push(id)
        }
        return out
    }
    // The catalog's defaults the user unticked, installed ones aside.
    readonly property var uncheckedDefaults: {
        const out = []
        for (const mod of root.mods) {
            if (mod.isDefault === true && root.ticks[mod.project] !== true
                    && root.installed.indexOf(mod.project) < 0 && out.indexOf(mod.project) < 0)
                out.push(mod.project)
        }
        return out
    }
    // project id -> ticked; replaced whole on every change so bindings follow.
    property var ticks: ({})

    // The box (which has padding of its own) and the headings start on the
    // content edge.
    readonly property int rowPadding: 0
    readonly property int headingGap: Theme.space.md
    readonly property int iconSize: Theme.control.heightLg

    // Headings and mod rows in one flat list.
    readonly property var rows: {
        const out = []
        const bundles = new Set(root.mods.map((m) => m.bundle))
        let bundle = ""
        let category = ""
        for (const mod of root.mods) {
            const newBundle = mod.bundle !== bundle
            if (newBundle && bundles.size > 1)
                out.push({ kind: "bundle", text: mod.bundleName })
            if (newBundle || mod.category !== category)
                out.push({ kind: "category", text: mod.categoryName })
            bundle = mod.bundle
            category = mod.category
            out.push({ kind: "mod", mod: mod })
        }
        return out
    }

    function seed() {
        const next = {}
        for (const mod of root.mods)
            next[mod.project] = next[mod.project] === true
                || (mod.isDefault === true && root.excluded.indexOf(mod.project) < 0)
        root.ticks = next
    }
    function setTick(project, on) {
        const next = Object.assign({}, root.ticks)
        next[project] = on
        root.ticks = next
    }

    onModsChanged: root.seed()
    onExcludedChanged: root.seed()
    Component.onCompleted: root.seed()

    clip: true
    spacing: Theme.space.xs
    model: root.rows
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    delegate: Item {
        id: entry
        required property int index
        required property var modelData
        readonly property bool isMod: modelData.kind === "mod"
        readonly property var mod: isMod ? modelData.mod : null
        readonly property bool isInstalled: isMod && root.installed.indexOf(mod.project) >= 0
        readonly property bool unsafe: isMod && mod.serverSafe === false

        width: root.width
        implicitHeight: isMod ? modRow.implicitHeight : heading.implicitHeight + (entry.index === 0 ? 0 : root.headingGap)
        height: implicitHeight

        // "Performance" over "Speed": the bundle's name louder than the
        // category's.
        Text {
            id: heading
            visible: !entry.isMod
            anchors.left: parent.left
            anchors.leftMargin: root.rowPadding
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            text: entry.isMod ? "" : entry.modelData.text
            elide: Text.ElideRight
            color: entry.modelData.kind === "bundle" ? Theme.palette.textPrimary : Theme.palette.textTertiary
            font.family: entry.modelData.kind === "bundle" ? Theme.font.heading : Theme.font.family
            font.pixelSize: entry.modelData.kind === "bundle" ? Theme.type.title.pixelSize : Theme.type.caption.pixelSize
            font.weight: entry.modelData.kind === "bundle" ? Font.Bold : Font.DemiBold
        }

        RowLayout {
            id: modRow
            visible: entry.isMod
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Theme.space.sm

            CheckBox {
                id: box
                Layout.leftMargin: root.rowPadding - Theme.space.xs
                enabled: !entry.isInstalled
                checked: entry.isInstalled || (entry.isMod && root.ticks[entry.mod.project] === true)
                Accessible.name: entry.isMod ? entry.mod.name : ""
                onToggled: {
                    root.setTick(entry.mod.project, checked)
                    checked = Qt.binding(() => entry.isInstalled || root.ticks[entry.mod.project] === true)
                }
            }

            // The picture and the text tick the row too, not just the box.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space.xs
                Layout.bottomMargin: Theme.space.xs
                spacing: Theme.space.md

                TapHandler {
                    enabled: box.enabled
                    onTapped: root.setTick(entry.mod.project, root.ticks[entry.mod.project] !== true)
                }

                // The mod's own icon, on the neutral tile; the package glyph
                // while it loads, or when there is none.
                RowThumb {
                    Layout.preferredWidth: root.iconSize
                    Layout.preferredHeight: root.iconSize
                    source: entry.isMod && entry.mod.icon ? entry.mod.icon : ""
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.xxs

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space.sm
                        Text {
                            text: entry.isMod ? entry.mod.name : ""
                            elide: Text.ElideRight
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            font.weight: Font.DemiBold
                        }
                        Tag {
                            visible: entry.isInstalled
                            text: qsTr("Installed")
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: entry.isMod ? entry.mod.summary : ""
                        wrapMode: Text.Wrap
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: entry.unsafe
                        text: qsTr("Hypixel and other servers ban this. Using it is your responsibility.")
                        wrapMode: Text.Wrap
                        color: Theme.palette.warning
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                        font.weight: Font.Medium
                    }
                }
            }
        }
    }
}
