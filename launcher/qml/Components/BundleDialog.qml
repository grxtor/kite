// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Recommended mods for an existing instance: the catalog's bundles
 * (Performance, Comfort, PvP) as tabs, each a checklist of its mods that
 * exist for the instance's loader. Mods the instance already has are
 * shown ticked and locked. Install hands what is ticked, across all the
 * tabs, to ContentUpdateFlows, whose review lists what will be downloaded.
 *
 * OptiFine and Sodium or Iris do not run together; a warning says so
 * while both would be there.
 */
Dialog {
    id: root

    // BundleCatalog (shell.bundles), and the instance's ContentUpdateController
    // and primary mod loader.
    property var bundles: null
    property var updater: null
    property string loader: ""
    // Modrinth project ids the instance has, and whether OptiFine is among
    // its mods; read each time the dialog opens.
    property var installed: []
    property bool optiFine: false
    property string current: "performance"
    // Everything ticked, over all the tabs.
    property var checkedAll: []

    signal installRequested(var projectIds)

    readonly property var bundleIds: ["performance", "comfort", "pvp"]
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
        return id === "performance" ? qsTr("Performance") : id === "comfort" ? qsTr("Comfort") : qsTr("PvP")
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
    function show() {
        root.installed = root.updater ? root.updater.installedProjects() : []
        root.optiFine = root.updater ? root.updater.hasOptiFine() : false
        root.current = root.bundleIds[0]
        root.open()
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
    title: qsTr("Recommended mods")

    header: DialogHeader {
        title: root.title
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        TabStrip {
            Layout.fillWidth: true
            tabs: root.tabs
            current: root.current
            onActivated: (id) => root.current = id
        }

        // One list per bundle, all alive, so ticks survive a tab switch.
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: root.listHeight
            currentIndex: root.bundleIds.indexOf(root.current)

            Repeater {
                id: lists
                model: root.bundleIds

                BundleModList {
                    id: list
                    required property string modelData
                    mods: root.bundles ? root.bundles.mods([modelData], root.loader) : []
                    installed: root.installed
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
            enabled: root.checkedAll.length > 0
            text: qsTr("Install")
            icon.source: Icons.url("download")
            onClicked: {
                root.close()
                root.installRequested(root.checkedAll)
            }
        }
    }
}
