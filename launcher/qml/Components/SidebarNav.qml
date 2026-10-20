// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The left rail, Modrinth's: the Kite mark on the top bar's line, the main
 * destinations, a rule, the last few instances played (their own icons),
 * + for a new instance, then Settings and the account at the foot. No
 * headings: every item is its own icon, its name a tooltip. Pages that open
 * elsewhere are still listed here -- where they open is the shell's
 * business, not the rail's.
 *
 * Part of the shell surface (Main.qml paints it, with the top bar and the
 * play bar), so the rail draws no background or edge of its own: the page
 * panel's hairline beside it is the edge.
 *
 * The icon rail is the default; `collapsed: false` widens it into the
 * labelled sidebar (names beside the icons, the Kite wordmark in the band
 * at the top), the same items on the same lines, so nothing jumps while it
 * folds. The caller drives that, since only it knows what the window is
 * competing for space with.
 */
Rectangle {
    id: root

    // The fold toggle -- persisted, so a manual choice survives a restart;
    // the caller ORs it with its own narrow-window check (see Main.qml), so
    // a small window keeps the rail regardless of what was last chosen. At
    // the end of the brand band in the labelled sidebar, just above
    // Settings on the icon rail, where the band has room for the mark only.
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
    // The open instance, if any: its recent tile takes the selection while
    // it is listed, over currentId.
    property string currentInstanceId: ""

    // Any model with instanceId/name/iconKey/isRunning roles; at most the
    // first `recentLimit` rows are shown, fewer when the window is too short
    // for them (see recentShown).
    property var recentModel: null
    property int recentLimit: 5

    // The account at the foot (ProfileButton).
    property string accountName: ""
    property string accountKind: ""
    property string accountAvatarSource: ""
    property var accountsController: null

    property bool collapsed: true
    // Whether the play bar is showing beside the foot band. The band keeps
    // its height either way, so the account never moves between pages; kept
    // for callers that still pass it.
    property bool barBeside: true
    // The band at the top, on the top bar's line (Main.qml's shellBarHeight).
    property int bandHeight: Theme.control.heightLg

    // The rail's geometry. Every item is one square (Modrinth's 48), inset
    // the same on both sides of the rail; the square's middle is the rail's
    // one vertical axis -- every icon, recent tile, the mark, the account
    // head and the selection sit on it, folded or not.
    readonly property int itemSize: Theme.control.heightLg
    readonly property int inset: Theme.space.sm
    readonly property int railWidth: root.itemSize + root.inset * 2
    readonly property int expandedWidth: 240
    // From the inner column's left edge; from the rail's own edge it is
    // inset + this, the rail's middle.
    readonly property int railAxis: root.itemSize / 2
    // Labels start here (from a row's left edge), after the recent tiles
    // (RecentItem.tileSize), the widest thing on the axis.
    readonly property int labelX: root.railAxis + Theme.control.heightSm / 2 + Theme.space.md
    readonly property int itemRadius: Theme.radius.lg
    // The Kite mark: a step under the recent tiles (RecentItem.tileSize), and
    // the rail's items start a gap below its band, so the mark floats on the
    // top bar's line instead of stacking on the first destination.
    readonly property int brandExtent: Theme.control.heightSm - Theme.space.xs
    readonly property int brandGap: Theme.space.md

    signal itemActivated(string id)
    signal recentActivated(string id)
    signal recentPlayRequested(string id)
    signal newInstanceRequested()
    signal openAccountsRequested()

    function openProfileMenu() { accountButton.openMenu() }

    // How many recent instances fit between + and Settings without the
    // column overflowing -- on a short window the list gives way first.
    readonly property int recentShown: {
        var pitch = root.itemSize + column.spacing
        var fixed = root.bandHeight + root.brandGap + Theme.layout.barHeight
                  + (root.items.length + root.footerItems.length + 1) * pitch
                  + rule.height + rule.Layout.topMargin + rule.Layout.bottomMargin + column.spacing
                  + (root.collapsed ? Theme.control.heightSm + column.spacing : 0)
                  + Theme.space.lg
        return Math.max(0, Math.min(root.recentLimit, Math.floor((root.height - fixed) / pitch)))
    }

    // The item whose id matches currentId -- or the open instance's recent
    // tile, or the account on Accounts -- or null on a page nothing here
    // lists, where the selection just hides rather than parking on the last
    // real destination.
    readonly property Item selectedNavItem: findNavItem(currentId, currentInstanceId)

    function findNavItem(id, instanceId) {
        if (instanceId && instanceId.length > 0) {
            for (var r = 0; r < recentRepeater.count; ++r) {
                var recent = recentRepeater.itemAt(r)
                if (recent && recent.visible && recent.instanceId === instanceId)
                    return recent
            }
        }
        if (!id || id.length === 0)
            return null
        if (id === "accounts")
            return accountButton
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
    color: "transparent"

    Behavior on implicitWidth { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

    // The selected destination's fill, as one shape that slides from the
    // previous item to the new one instead of each item owning its own
    // static highlight -- NavItem itself only ever paints hover/press.
    NavSelectionIndicator {
        target: root.selectedNavItem
        collapsed: root.collapsed
        railSquare: root.itemSize
        railAxis: root.railAxis
        cornerRadius: root.itemRadius
        // Rows live in two containers (the main column and the foot band),
        // so positions are summed up to this rail rather than read off one
        // layout.
        space: root
    }

    // The band at the top, on the top bar's line: the mark on the rail's
    // axis in both modes, so it stays put while the rail folds; the wordmark
    // and the fold toggle beside it in the labelled sidebar.
    Item {
        id: brandBand
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.bandHeight

        BrandMark {
            id: mark
            x: root.inset + root.railAxis - width / 2
            anchors.verticalCenter: parent.verticalCenter
            size: root.brandExtent
        }

        // On the labels' line, as the names below it are, its letters (not
        // its line box, which hangs low) centred on the mark: "Kite" has no
        // descender, so its ink is what rises above the baseline (the tight
        // rect's own height runs a pixel below it).
        Text {
            id: wordmark
            x: root.inset + root.labelX
            anchors.baseline: mark.verticalCenter
            anchors.baselineOffset: -wordmarkInk.tightBoundingRect.y / 2
            visible: !root.collapsed
            text: "Kite"
            color: Theme.palette.textPrimary
            font.family: Theme.font.heading
            font.pixelSize: Theme.type.title.pixelSize + 2
            font.weight: Font.Bold
            font.letterSpacing: -0.2
        }
        TextMetrics {
            id: wordmarkInk
            font: wordmark.font
            text: wordmark.text
        }

        CollapseToggle {
            visible: !root.collapsed
            anchors.right: parent.right
            anchors.rightMargin: root.inset
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: brandBand.bottom
        anchors.topMargin: root.brandGap
        anchors.bottom: footBand.top
        anchors.leftMargin: root.inset
        anchors.rightMargin: root.inset
        spacing: Theme.space.xs

        Repeater {
            id: navRepeater
            model: root.items
            delegate: NavItem {
                id: navDelegate
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: root.itemSize
                railSquare: root.itemSize
                iconAxis: root.railAxis
                labelX: root.labelX
                cornerRadius: root.itemRadius
                iconName: modelData.icon
                label: modelData.label
                selected: root.selectedNavItem === navDelegate
                collapsed: root.collapsed
                onClicked: root.itemActivated(modelData.id)
            }
        }

        // A short rule where the destinations end and the instances begin:
        // no "Recent" heading, the icons say what they are.
        Rectangle {
            id: rule
            Layout.preferredWidth: root.collapsed ? root.itemSize - Theme.space.lg : -1
            Layout.fillWidth: !root.collapsed
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.space.xs
            Layout.bottomMargin: Theme.space.xs
            implicitHeight: 1
            color: Theme.palette.divider
        }

        Repeater {
            id: recentRepeater
            model: root.recentModel
            delegate: RecentItem {
                required property int index

                // Rows past what fits stay out of the layout entirely.
                visible: index < root.recentShown
                Layout.fillWidth: true
                implicitHeight: root.itemSize
                railSquare: root.itemSize
                iconAxis: root.railAxis
                labelX: root.labelX
                cornerRadius: root.itemRadius
                compact: root.collapsed
                onClicked: root.recentActivated(instanceId)
                onPlayRequested: root.recentPlayRequested(instanceId)
            }
        }

        NavItem {
            Layout.fillWidth: true
            implicitHeight: root.itemSize
            railSquare: root.itemSize
            iconAxis: root.railAxis
            labelX: root.labelX
            cornerRadius: root.itemRadius
            iconName: "plus"
            label: qsTr("New instance")
            collapsed: root.collapsed
            onClicked: root.newInstanceRequested()
        }

        Item { Layout.fillHeight: true }

        CollapseToggle {
            visible: root.collapsed
            Layout.alignment: Qt.AlignHCenter
        }

        Repeater {
            id: footerRepeater
            model: root.footerItems
            delegate: NavItem {
                id: footerDelegate
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: root.itemSize
                railSquare: root.itemSize
                iconAxis: root.railAxis
                labelX: root.labelX
                cornerRadius: root.itemRadius
                iconName: modelData.icon
                label: modelData.label
                selected: root.selectedNavItem === footerDelegate
                collapsed: root.collapsed
                onClicked: root.itemActivated(modelData.id)
            }
        }
    }

    // The foot band: exactly the play bar's height (Theme.layout.barHeight),
    // so the account sits on the same line as the bar's own content and the
    // rail's foot and the bar read as one strip across the window.
    Item {
        id: footBand
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.inset
        anchors.rightMargin: root.inset
        height: Theme.layout.barHeight

        ProfileButton {
            id: accountButton
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            compact: root.collapsed
            railSquare: root.itemSize
            iconAxis: root.railAxis
            labelX: root.labelX
            cornerRadius: root.itemRadius
            name: root.accountName
            kind: root.accountKind
            avatarSource: root.accountAvatarSource
            controller: root.accountsController
            onOpenAccountsRequested: root.openAccountsRequested()
        }
    }
}
