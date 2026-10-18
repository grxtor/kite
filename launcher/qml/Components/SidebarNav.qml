// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

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

    // The collapse toggle -- persisted, so a manual choice survives a
    // restart; SidebarNav.collapsed itself still ORs this with the caller's
    // own narrow-window check (see Main.qml), so a small window keeps
    // auto-collapsing regardless of what was last chosen. Used in two
    // places: at the end of the brand row when expanded (away from
    // Settings, which it used to sit beside and read as part of), and just
    // above the foot band on the icon rail, where the brand row has room for
    // only the mark.
    component CollapseToggle: IconButton {
        size: Theme.control.heightSm
        iconName: "chevron-left"
        tip: root.collapsed ? qsTr("Expand sidebar") : qsTr("Collapse sidebar")
        rotation: root.collapsed ? 180 : 0

        Behavior on rotation { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

        onClicked: SettingsStore.setValue("UiSidebarCollapsed", !SettingsStore.bool("UiSidebarCollapsed"))
    }

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
    // Whether the play bar is showing beside the foot band: its top border
    // only makes sense continuing the bar's own. The band keeps its height
    // either way, so Settings never moves between pages.
    property bool barBeside: true
    readonly property int expandedWidth: 244
    // The rail's one vertical axis, from the inner column's left edge:
    // every nav icon's centre (NavItem.iconAxis), the recent instances'
    // icons, the brand mark, the collapse toggle and the selection square
    // sit on it, collapsed or not. The icon rail is exactly the column's
    // margins plus twice that wide, so on it the axis is also the middle --
    // nothing has to move or re-anchor when the rail folds.
    readonly property int railAxis: Theme.space.md + Theme.icon.md / 2
    readonly property int railWidth: Theme.space.md * 2 + root.hairline + root.railAxis * 2
    readonly property int hairline: 1
    readonly property int brandExtent: Theme.control.heightSm + 2

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
        width: root.hairline
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
        railAxis: root.railAxis
        // Rows live in two containers (the main column and the foot band),
        // so positions are summed up to this rail rather than read off one
        // layout.
        space: root
        barInset: Theme.space.md
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footBand.top
        anchors.margins: Theme.space.md
        anchors.rightMargin: Theme.space.md + root.hairline
        spacing: Theme.space.xs

        // Brand: the mark centred on the rail axis in both modes, so it
        // stays put while the rail folds and sits over the icons below.
        RowLayout {
            Layout.fillWidth: !root.collapsed
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            // Centred on the same line as every page's title row (TopBar:
            // Theme.space.sm down, Theme.control.height tall), so the brand
            // and the page title read as one header across the window: the
            // column's own md top margin is pulled back up to sm.
            Layout.topMargin: Theme.space.sm - Theme.space.md
            Layout.preferredHeight: Theme.control.height
            Layout.leftMargin: root.railAxis - root.brandExtent / 2
            Layout.bottomMargin: Theme.space.xl
            spacing: Theme.space.sm + 2

            Image {
                Layout.preferredWidth: root.brandExtent
                Layout.preferredHeight: root.brandExtent
                source: "qrc:/icons/multimc/scalable/instances/kite.svg"
                // From a constant, not from width: the layout sizes this
                // item from its implicit size, which sourceSize sets.
                sourceSize: Qt.size(root.brandExtent * 2, root.brandExtent * 2)
                fillMode: Image.PreserveAspectFit
            }

            Text {
                visible: !root.collapsed
                text: "Kite"
                color: Theme.palette.textPrimary
                font.family: Theme.font.heading
                font.pixelSize: Theme.type.title.pixelSize + 2
                font.weight: Font.Bold
                font.letterSpacing: -0.2
            }

            // Gone, not just zero-width, on the icon rail: a visible spacer
            // still takes the row's spacing, which pushed the mark off the
            // nav icons' centre line.
            Item { visible: !root.collapsed; Layout.fillWidth: true }

            CollapseToggle {
                visible: !root.collapsed
                Layout.alignment: Qt.AlignVCenter
            }
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

        // A quiet rule where the destinations end, then the recent
        // instances under a plain "Recent" -- sentence case in the tertiary
        // text colour, not a letter-spaced caps overline.
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
            text: qsTr("Recent")
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
            font.weight: Font.Medium
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

        CollapseToggle {
            visible: root.collapsed
            Layout.alignment: Qt.AlignHCenter
        }
    }

    // The foot band: exactly the play bar's height (Theme.layout.barHeight),
    // with a top border in the same colour as the bar's own, so the rail's
    // foot and the bar read as one strip across the window instead of two
    // dividers at different heights. Settings has its one row to itself,
    // centred like the bar's own content (the collapse toggle lives in the
    // brand row, or just above this band on the icon rail -- see
    // CollapseToggle).
    Item {
        id: footBand
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: root.hairline
        anchors.bottom: parent.bottom
        height: Theme.layout.barHeight

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            visible: root.barBeside
            color: Theme.palette.border
        }

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.md
            spacing: Theme.space.xs

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
        }
    }
}
