// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One of the catalog's bundles for an existing instance, as a checklist of
 * its mods that exist for the instance's loader (a tab strip first, should
 * a dialog ever list more than one bundle). Mods the instance already has
 * are shown ticked and locked. Two dialogs, by `performance`:
 *  - Add-ons: the optional Comfort bundle. Install hands what is ticked to
 *    ContentUpdateFlows (installRequested), whose review lists what will be
 *    downloaded.
 *  - Kite Performance: a switch for the instance's mode over the
 *    Performance bundle, whose ticked mods Play keeps installed. Save
 *    stores the switch and the unticked mods (QmlShell), and hands the
 *    ticked ones that are missing to installRequested.
 *
 * OptiFine and Sodium or Iris do not run together; a warning says so
 * while both would be there.
 */
Dialog {
    id: root

    // Kite Performance rather than Add-ons.
    property bool performance: false
    // The QML shell and the instance, for Kite Performance's settings.
    property var shell: null
    property string instanceId: ""
    // BundleCatalog (shell.bundles), and the instance's ContentUpdateController
    // and primary mod loader.
    property var bundles: null
    property var updater: null
    property string loader: ""
    // Modrinth project ids the instance has, and whether OptiFine is among
    // its mods; read each time the dialog opens.
    property var installed: []
    property bool optiFine: false
    property string current: ""
    // The mods unticked when the dialog opened.
    property var excluded: []
    // Everything ticked, over all the tabs.
    property var checkedAll: []

    signal installRequested(var projectIds)

    readonly property var bundleIds: root.performance ? ["performance"] : ["comfort"]
    readonly property var tabs: root.bundleIds.map((id) => ({ id: id, label: root.bundleLabel(id) }))
    readonly property string sodiumId: "AANobbMI"
    readonly property string irisId: "YL57xq9U"
    readonly property bool optiFineClash: root.optiFine
        && (root.checkedAll.indexOf(root.sodiumId) >= 0 || root.checkedAll.indexOf(root.irisId) >= 0)
    readonly property int preferredWidth: 600
    readonly property int listHeight: 400

    // The catalog's own name for a bundle, in the selected language.
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
    function show() {
        root.installed = root.updater ? root.updater.installedProjects() : []
        root.optiFine = root.updater ? root.updater.hasOptiFine() : false
        if (root.performance && root.shell) {
            modeSwitch.checked = root.shell.performanceEnabled(root.instanceId)
            root.excluded = root.shell.performanceExcluded(root.instanceId)
        }
        root.current = root.bundleIds[0]
        root.open()
    }
    // Kite Performance's Save: the settings first, then the mods still to
    // install (none while the mode is off).
    function save() {
        root.shell.setPerformanceExcluded(root.instanceId, root.uncheckedDefaults())
        root.shell.setPerformanceEnabled(root.instanceId, modeSwitch.checked)
        root.close()
        if (modeSwitch.checked && root.checkedAll.length > 0)
            root.installRequested(root.checkedAll)
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.preferredWidth, parent ? parent.width - Theme.space.xxl * 2 : root.preferredWidth)
    height: Math.min(implicitHeight, parent ? parent.height - Theme.space.xxl * 2 : implicitHeight)
    modal: true
    // The title row sets the inset; the gap under it is the dialog's own spacing.
    topPadding: 0
    // The actions row sets the bottom inset; this and the spacing over it
    // leave one inset (xl) between the body and the actions.
    bottomPadding: Theme.space.sm
    closePolicy: Popup.CloseOnEscape
    title: root.performance ? qsTr("Kite Performance") : qsTr("Add-ons")

    header: DialogHeader {
        title: root.title
    }

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
            // Off, the mode installs nothing: the list is for looking at.
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
                    onCheckedProjectsChanged: root.recount()

                    Text {
                        width: list.width - Theme.space.xxl * 2
                        x: Theme.space.xxl
                        y: list.height / 3
                        visible: list.mods.length === 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        text: qsTr("Nothing in this bundle for this loader.")
                        color: Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                }
                onItemAdded: root.recount()
            }
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
