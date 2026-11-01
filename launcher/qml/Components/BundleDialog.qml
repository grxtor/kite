// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * A checklist of the catalog's bundle mods for an instance's loader; installed ones are ticked and locked. By
 * `performance`: Add-ons (the Comfort bundle) or Kite Performance (the instance's mode switch, plus a Graphics API
 * row on Minecraft 26.2+ that edits options.txt). Warns when OptiFine and Sodium or Iris would meet.
 */
AppDialog {
    id: root

    property bool performance: false
    property var shell: null
    property string instanceId: ""
    // BundleCatalog (shell.bundles).
    property var bundles: null
    property var updater: null
    property string loader: ""
    // Modrinth project ids the instance has; read each time the dialog opens.
    property var installed: []
    property bool optiFine: false
    property bool running: false
    // "default", "vulkan" or "opengl"; "" when the version has no such setting.
    property string graphicsBackend: ""
    property bool graphicsFailed: false
    property string current: ""
    property var excluded: []
    // Extras ticked before; they start ticked.
    property var included: []
    property var checkedAll: []

    signal installRequested(var projectIds)

    readonly property var bundleIds: root.performance ? ["performance"] : ["comfort"]
    readonly property var tabs: root.bundleIds.map((id) => ({ id: id, label: root.bundleLabel(id) }))
    readonly property string sodiumId: "AANobbMI"
    readonly property string irisId: "YL57xq9U"
    readonly property bool optiFineClash: root.optiFine
        && (root.checkedAll.indexOf(root.sodiumId) >= 0 || root.checkedAll.indexOf(root.irisId) >= 0)
    readonly property int listHeight: 400
    readonly property int graphicsBoxWidth: Theme.control.heightLg * 4
    readonly property var graphicsChoices: [
        { id: "default", label: qsTr("Automatic") },
        { id: "vulkan", label: "Vulkan" },
        { id: "opengl", label: "OpenGL" }
    ]

    function bundleLabel(id) {
        const mods = root.bundles ? root.bundles.mods([id], root.loader) : []
        if (mods.length > 0)
            return mods[0].bundleName
        return id === "performance" ? qsTr("Performance") : qsTr("Comfort")
    }
    function recount() {
        const out = []
        for (let i = 0; i < lists.count; ++i) {
            const list = lists.itemAt(i)
            for (const id of list ? list.checkedProjects : []) {
                if (out.indexOf(id) < 0)
                    out.push(id)
            }
        }
        root.checkedAll = out
    }
    function uncheckedDefaults() {
        const out = []
        for (let i = 0; i < lists.count; ++i) {
            const list = lists.itemAt(i)
            for (const id of list ? list.uncheckedDefaults : []) {
                if (out.indexOf(id) < 0)
                    out.push(id)
            }
        }
        return out
    }
    function keptExtras() {
        const out = []
        for (let i = 0; i < lists.count; ++i) {
            const list = lists.itemAt(i)
            for (const id of list ? list.keptExtras : []) {
                if (out.indexOf(id) < 0)
                    out.push(id)
            }
        }
        return out
    }
    function show() {
        root.installed = root.updater ? root.updater.installedProjects() : []
        root.optiFine = root.updater ? root.updater.hasOptiFine() : false
        if (root.performance && root.shell) {
            modeSwitch.checked = root.shell.performanceEnabled(root.instanceId)
            root.excluded = root.shell.performanceExcluded(root.instanceId)
            root.included = root.shell.performanceIncluded(root.instanceId)
            root.graphicsBackend = root.shell.graphicsBackend(root.instanceId)
            graphicsBox.currentIndex = graphicsBox.indexOfValue(root.graphicsBackend)
        }
        root.graphicsFailed = false
        root.current = root.bundleIds[0]
        root.open()
    }
    // Settings first (a failed Graphics API keeps the dialog open), then the mods still to install.
    function save() {
        if (graphicsRow.visible && graphicsBox.currentValue !== root.graphicsBackend
                && !root.shell.setGraphicsBackend(root.instanceId, graphicsBox.currentValue)) {
            root.graphicsFailed = true
            return
        }
        root.shell.setPerformanceExcluded(root.instanceId, root.uncheckedDefaults())
        root.shell.setPerformanceIncluded(root.instanceId, root.keptExtras())
        root.shell.setPerformanceEnabled(root.instanceId, modeSwitch.checked)
        root.close()
        if (modeSwitch.checked && root.checkedAll.length > 0)
            root.installRequested(root.checkedAll)
    }

    height: Math.min(implicitHeight, parent ? parent.height - Theme.space.xxl * 2 : implicitHeight)
    title: root.performance ? qsTr("Kite Performance") : qsTr("Add-ons")

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        SettingRow {
            id: modeRow
            Layout.fillWidth: true
            // The row's own side padding would indent it past the list.
            Layout.leftMargin: -Theme.space.lg
            Layout.rightMargin: -Theme.space.lg
            visible: root.performance
            label: qsTr("Kite Performance")
            description: qsTr("Checked mods are kept installed every time you play.")
            Switch {
                id: modeSwitch
                Accessible.name: modeRow.label
            }
        }

        SettingRow {
            id: graphicsRow
            Layout.fillWidth: true
            Layout.leftMargin: -Theme.space.lg
            Layout.rightMargin: -Theme.space.lg
            visible: root.performance && root.graphicsBackend.length > 0
            label: qsTr("Graphics API")
            description: root.running
                ? qsTr("The game is running. This can be changed once it closes.")
                : qsTr("Vulkan is often smoother, most of all on Windows. Automatic leaves it to the game.")
            ComboBox {
                id: graphicsBox
                width: root.graphicsBoxWidth
                model: root.graphicsChoices
                textRole: "label"
                valueRole: "id"
                enabled: !root.running
                Accessible.name: graphicsRow.label
            }
        }

        TabStrip {
            Layout.fillWidth: true
            visible: root.bundleIds.length > 1
            tabs: root.tabs
            current: root.current
            onActivated: (id) => root.current = id
        }

        // One list per bundle, all alive, so ticks survive a tab switch.
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: root.listHeight
            enabled: !root.performance || modeSwitch.checked
            currentIndex: root.bundleIds.indexOf(root.current)

            Repeater {
                id: lists
                model: root.bundleIds

                BundleModList {
                    id: list
                    required property string modelData
                    mods: root.bundles ? root.bundles.mods([modelData], root.loader) : []
                    installed: root.installed
                    excluded: root.excluded
                    included: root.included
                    onCheckedProjectsChanged: root.recount()

                    Copy {
                        width: list.width - Theme.space.xxl * 2
                        x: Theme.space.xxl
                        y: list.height / 3
                        visible: list.mods.length === 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        text: qsTr("Nothing in this bundle for this loader.")
                        role: "label"
                        tone: "tertiary"
                    }
                }
                onItemAdded: root.recount()
            }
        }

        NoticeBar {
            Layout.fillWidth: true
            visible: root.graphicsFailed
            text: qsTr("Could not save the graphics API.")
        }

        NoticeBar {
            Layout.fillWidth: true
            visible: root.optiFineClash
            text: qsTr("OptiFine is installed. Sodium and Iris don't run alongside it: remove OptiFine, or untick them.")
        }
    }

    footer: RowLayout {
        spacing: Theme.space.sm

        Item { Layout.fillWidth: true; Layout.leftMargin: Theme.space.xl }
        Button {
            Layout.bottomMargin: Theme.space.xl
            text: qsTr("Cancel")
            onClicked: root.close()
        }
        Button {
            Layout.rightMargin: Theme.space.xl
            Layout.bottomMargin: Theme.space.xl
            highlighted: true
            enabled: root.performance || root.checkedAll.length > 0
            text: root.performance ? qsTr("Save") : qsTr("Install")
            icon.source: Icons.url(root.performance ? "check" : "download")
            onClicked: {
                if (root.performance) {
                    root.save()
                    return
                }
                root.close()
                root.installRequested(root.checkedAll)
            }
        }
    }
}
