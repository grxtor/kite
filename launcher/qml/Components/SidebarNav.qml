// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The left rail: brand, the main destinations, the last few instances
 * played, and the account. Pages that open elsewhere (Settings, for now a
 * dialog) are still listed here -- where they open is the shell's business,
 * not the rail's.
 *
 * Below `collapseWidth` the rail folds down to an icon-only strip (labels
 * become tooltips); the caller drives that from the window's own width via
 * `collapsed`, since only it knows what the window is competing for space
 * with.
 */
Rectangle {
    id: root

    // Each entry: { id, icon, label }; icon is a MeshIcon name.
    property var items: []
    property var footerItems: []
    property string currentId: ""

    // Any model with instanceId/name/iconKey/isRunning roles; only the
    // first `recentLimit` rows are shown.
    property var recentModel: null
    property int recentLimit: 4

    // The account chip used to live at the foot of this rail; it is gone
    // now that the top bar's ProfileButton covers every page (including the
    // instance page, which had no chip to show it on). These three stay
    // declared, unused, only because Gallery.qml -- outside this change --
    // still binds them on its own SidebarNav preview.
    property string accountName: ""
    property string accountKind: ""
    property string accountAvatarSource: ""

    property bool collapsed: false
    readonly property int expandedWidth: 244
    readonly property int railWidth: 72

    signal itemActivated(string id)
    signal recentActivated(string id)
    signal recentPlayRequested(string id)
    signal accountClicked()

    // The item (from either nav Repeater) whose id matches currentId, or
    // null on a page (Accounts) neither one lists -- activeIndicator just
    // hides itself then, rather than parking on the last real destination.
    readonly property Item selectedNavItem: findNavItem(currentId)

    function findNavItem(id) {
        if (!id || id.length === 0)
            return null
        for (var i = 0; i < navRepeater.count; ++i) {
            var item = navRepeater.itemAt(i)
            if (item && item.modelData.id === id)
                return item
        }
        for (var j = 0; j < footerRepeater.count; ++j) {
            var footerItem = footerRepeater.itemAt(j)
            if (footerItem && footerItem.modelData.id === id)
                return footerItem
        }
        return null
    }

    implicitWidth: collapsed ? railWidth : expandedWidth
    color: Theme.palette.surface

    Behavior on implicitWidth { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

    // Hairline against the page, instead of a contrasting fill.
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.palette.divider
    }

    // The selected destination's fill, as one pill that slides from the
    // previous item to the new one instead of each item owning its own
    // static highlight -- NavItem itself only ever paints hover/press.
    // The shared mechanism (also used by Settings' own section list) lives
    // in NavSelectionIndicator.qml; collapsed mode's centred square instead
    // of a full-width bar is documented there.
    readonly property int railSquare: 40

    NavSelectionIndicator {
        target: root.selectedNavItem
        collapsed: root.collapsed
        railSquare: root.railSquare
        insetX: Theme.space.md
        insetY: Theme.space.md
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.md
        anchors.rightMargin: Theme.space.md + 1
        spacing: Theme.space.xs

        // Brand
        RowLayout {
            Layout.fillWidth: !root.collapsed
            Layout.alignment: root.collapsed ? Qt.AlignHCenter : (Qt.AlignLeft | Qt.AlignVCenter)
            Layout.preferredHeight: Theme.control.heightLg
            Layout.leftMargin: root.collapsed ? 0 : Theme.space.xs
            Layout.bottomMargin: Theme.space.md
            spacing: Theme.space.sm + 2

            Image {
                readonly property int extent: Theme.control.heightSm + 2
                Layout.preferredWidth: extent
                Layout.preferredHeight: extent
                source: "qrc:/icons/multimc/scalable/instances/meshmc.svg"
                // From a constant, not from width: the layout sizes this
                // item from its implicit size, which sourceSize sets.
                sourceSize: Qt.size(extent * 2, extent * 2)
                fillMode: Image.PreserveAspectFit
            }

            Text {
                visible: !root.collapsed
                text: "MeshMC"
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.title.pixelSize + 2
                font.weight: Font.Bold
                font.letterSpacing: -0.2
            }

            Item { Layout.fillWidth: !root.collapsed }
        }

        Repeater {
            id: navRepeater
            model: root.items
            delegate: NavItem {
                required property var modelData
                Layout.fillWidth: true
                iconName: modelData.icon
                label: modelData.label
                selected: modelData.id === root.currentId
                collapsed: root.collapsed
                onClicked: root.itemActivated(modelData.id)
            }
        }

        // A quiet rule instead of a second "RECENT"-style label: the
        // section below already names itself, this just says where the
        // destinations end.
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space.md
            Layout.bottomMargin: Theme.space.xxs
            visible: recentRepeater.count > 0
            height: 1
            color: Theme.palette.divider
        }

        Text {
            Layout.topMargin: Theme.space.sm
            Layout.leftMargin: Theme.space.md
            Layout.bottomMargin: Theme.space.xs
            visible: recentRepeater.count > 0 && !root.collapsed
            text: qsTr("RECENT")
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.overline.pixelSize
            font.weight: Theme.type.overline.weight
            font.letterSpacing: Theme.type.overline.letterSpacing
        }

        Repeater {
            id: recentRepeater
            model: root.recentModel
            delegate: RecentItem {
                required property int index

                // Rows past the limit stay out of the layout entirely.
                visible: index < root.recentLimit
                Layout.fillWidth: true
                Layout.topMargin: root.collapsed && index === 0 ? Theme.space.xs : 0
                compact: root.collapsed
                onClicked: root.recentActivated(instanceId)
                onPlayRequested: root.recentPlayRequested(instanceId)
            }
        }

        Item { Layout.fillHeight: true }

        Rectangle {
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.space.xs
            height: 1
            color: Theme.palette.divider
        }

        Repeater {
            id: footerRepeater
            model: root.footerItems
            delegate: NavItem {
                required property var modelData
                Layout.fillWidth: true
                iconName: modelData.icon
                label: modelData.label
                selected: modelData.id === root.currentId
                collapsed: root.collapsed
                onClicked: root.itemActivated(modelData.id)
            }
        }

        // The collapse toggle, grouped with Settings at the sidebar's foot
        // rather than floating in its own row up near the logo -- persisted,
        // so a manual choice survives a restart; SidebarNav.collapsed itself
        // still ORs this with the caller's own narrow-window check (see
        // Main.qml), so a small window keeps auto-collapsing regardless of
        // what was last chosen.
        IconButton {
            id: collapseToggle
            Layout.topMargin: Theme.space.xxs
            Layout.alignment: root.collapsed ? Qt.AlignHCenter : Qt.AlignRight
            size: Theme.control.heightSm
            iconName: "chevron-left"
            tip: root.collapsed ? qsTr("Expand sidebar") : qsTr("Collapse sidebar")
            rotation: root.collapsed ? 180 : 0

            Behavior on rotation { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

            onClicked: SettingsStore.setValue("UiSidebarCollapsed", !SettingsStore.bool("UiSidebarCollapsed"))
        }
    }
}
