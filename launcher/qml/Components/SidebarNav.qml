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
 *
 * Its right edge can be dragged to any width the labelled sidebar may have
 * (SidebarEdgeHandle), as the friends panel's left edge can: let go near the
 * rail's width and it folds to the rail, past that it stays labelled at the
 * width reached, and a double click on the edge switches. The width is
 * remembered (Main.qml keeps it as a setting; this only asks, with
 * `widthCommitted`). Dragging follows the pointer without any animation;
 * every other change of width slides.
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
        tip: root.railed ? qsTr("Expand sidebar") : qsTr("Collapse sidebar")
        rotation: root.railed ? 180 : 0

        Behavior on rotation { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

        onClicked: root.setCollapsed(!SettingsStore.bool("UiSidebarCollapsed"))
    }

    // Each entry: { id, icon, label }; icon is a MeshIcon name.
    property var items: []
    property var footerItems: []
    // Counts on the destinations, by id: { friends: { count, tone } } (see
    // NavItem.badge). A map of its own, so a count changing never rebuilds
    // `items` and the delegates with it.
    property var badges: ({})
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

    // The rail at rest. What is on show differs only while the edge is being
    // dragged (see railed).
    property bool collapsed: true
    // The width the user gave the labelled sidebar, as saved; it is brought
    // into range (see fullWidth) rather than trusted.
    property int expandedWidth: 240
    // What the sidebar may take of the window: its width less what the pages
    // and the friends panel need, which the caller knows. The sidebar never
    // leaves less than this makes it, down to its narrowest.
    property int roomWidth: 0
    // False while the host keeps the rail whatever the toggle says (a narrow
    // window): the expand toggle hides rather than do nothing.
    property bool canExpand: true
    // Whether the play bar is showing beside the foot band. The band keeps
    // its height either way, so the account never moves between pages; kept
    // for callers that still pass it.
    property bool barBeside: true
    // The colour the rail is drawn on (Main.qml's shell), for the corners of
    // an instance icon that fills its tile (RecentItem.surround).
    property color surround: "transparent"
    // The band at the top, on the top bar's line (Main.qml's shellBarHeight).
    property int bandHeight: Theme.control.heightLg
    // What the window's own buttons take at the band's left end: macOS's
    // traffic lights, once the title bar lies over the band (Main.qml's
    // windowControlsInset). The mark and the wordmark move past them, each
    // shown only while it fits before the fold toggle -- so the rail, too
    // narrow for anything past the lights, shows just the lights.
    property int controlsWidth: 0

    // The rail's geometry. Every item is one square (Modrinth's 48), inset
    // the same on both sides of the rail; the square's middle is the rail's
    // one vertical axis -- every icon, recent tile, the mark, the account
    // head and the selection sit on it, folded or not.
    readonly property int itemSize: Theme.control.heightLg
    readonly property int inset: Theme.space.sm
    readonly property int railWidth: root.itemSize + root.inset * 2
    // The labelled sidebar's range: wide enough for "Settings" and a short
    // instance name beside the icon, and not so wide that the names it has
    // are all there is to see.
    readonly property int minExpandedWidth: 200
    readonly property int maxExpandedWidth: 380
    // From the inner column's left edge; from the rail's own edge it is
    // inset + this, the rail's middle.
    readonly property int railAxis: root.itemSize / 2
    // Labels start here (from a row's left edge), after the recent tiles
    // (RecentItem.tileSize), the widest thing on the axis.
    readonly property int labelX: root.railAxis + Theme.control.heightSm / 2 + Theme.space.md
    readonly property int itemRadius: Theme.radius.lg
    // The Kite mark: as big as the recent tiles (RecentItem.tileSize), and
    // the rail's items start a gap below its band, so the mark floats on the
    // top bar's line instead of stacking on the first destination.
    readonly property int brandExtent: Theme.control.heightSm
    readonly property int brandGap: Theme.space.md

    signal itemActivated(string id)
    signal recentActivated(string id)
    signal recentPlayRequested(string id)
    signal newInstanceRequested()
    signal openAccountsRequested()
    // A drag of the edge ended with the sidebar labelled, at this width.
    signal widthCommitted(int width)

    function openProfileMenu() { accountButton.openMenu() }

    // How many recent instances fit between + and Settings without the
    // column overflowing -- on a short window the list gives way first.
    readonly property int recentShown: {
        var pitch = root.itemSize + column.spacing
        var fixed = root.bandHeight + root.brandGap + Theme.layout.barHeight
                  + (root.items.length + root.footerItems.length + 1) * pitch
                  + rule.height + rule.Layout.topMargin + rule.Layout.bottomMargin + column.spacing
                  + (root.railed && root.canExpand ? Theme.control.heightSm + column.spacing : 0)
                  + Theme.space.lg
        return Math.max(0, Math.min(root.recentLimit, Math.floor((root.height - fixed) / pitch)))
    }

    // The item whose id matches currentId -- or the open instance's recent
    // tile, or the account on Accounts -- or null on a page nothing here
    // lists, where the selection just hides rather than parking on the last
    // real destination.
    readonly property Item selectedNavItem: findNavItem(currentId, currentInstanceId)
    readonly property bool selectedIsInstance: !!root.selectedNavItem && root.selectedNavItem.instanceId !== undefined

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

    // The widest the labelled sidebar may be: not past maxExpandedWidth, nor
    // leaving less than roomWidth makes it -- except that it is never
    // narrower than its names can be laid out in.
    readonly property int maxWidth: Math.max(root.minExpandedWidth,
                                             Math.min(root.maxExpandedWidth,
                                                      root.roomWidth > 0 ? root.roomWidth : root.maxExpandedWidth))
    // The saved width, brought into range: the window may have shrunk since.
    readonly property int fullWidth: Math.min(Math.max(root.expandedWidth, root.minExpandedWidth), root.maxWidth)
    // What is on show: the rail or the labelled sidebar at rest, or -- while
    // dragging -- the one that letting go now would leave.
    readonly property bool railed: edge.showsRail

    // Fold or unfold, remembered (the same setting as the toggle's).
    function setCollapsed(collapsed) {
        SettingsStore.setValue("UiSidebarCollapsed", collapsed)
    }

    // True from the first turn of the event loop: the width is set at
    // start-up from the saved settings, and should appear in place.
    property bool settled: false
    Component.onCompleted: Qt.callLater(() => root.settled = true)

    // Which of the three widths the sidebar has. The slide belongs to a
    // change of this, not to a change of the width, which the window
    // changing size moves and which must be followed at once.
    readonly property string mode: edge.dragging ? "drag" : root.collapsed ? "rail" : "full"

    color: "transparent"
    // Only the move from one state to another slides, into a drag not at all
    // (the width follows the pointer) and out of one to where it was left.
    states: [
        State {
            name: "rail"
            when: root.mode === "rail"
            PropertyChanges { root.implicitWidth: root.railWidth }
        },
        State {
            name: "full"
            when: root.mode === "full"
            PropertyChanges { root.implicitWidth: root.fullWidth }
        },
        State {
            name: "drag"
            when: root.mode === "drag"
            PropertyChanges { root.implicitWidth: edge.liveWidth }
        }
    ]
    transitions: Transition {
        to: "rail,full"
        enabled: root.settled
        NumberAnimation {
            property: "implicitWidth"
            duration: SettingsStore.bool("UiReduceMotion") ? 0 : Theme.motion.normal
            easing.type: Theme.motion.easing
        }
    }

    // The selected destination's fill, as one shape that slides from the
    // previous item to the new one instead of each item owning its own
    // static highlight -- NavItem itself only ever paints hover/press.
    NavSelectionIndicator {
        // On the folded rail an instance's own tile shows it is selected
        // (RecentItem.selected, the nose in its cut): the square around it
        // would only frame a picture that already fills its tile.
        target: root.railed && root.selectedIsInstance ? null : root.selectedNavItem
        collapsed: root.railed
        railSquare: root.itemSize
        railAxis: root.railAxis
        cornerRadius: root.itemRadius
        live: edge.dragging
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
            x: root.controlsWidth > 0 ? root.controlsWidth : root.inset + root.railAxis - width / 2
            anchors.verticalCenter: parent.verticalCenter
            visible: root.controlsWidth === 0 || x + width + Theme.space.md <= bandToggle.x
            size: root.brandExtent
        }

        // On the labels' line, as the names below it are, its letters (not
        // its line box, which hangs low) centred on the mark: "Kite" has no
        // descender, so its ink is what rises above the baseline (the tight
        // rect's own height runs a pixel below it).
        Text {
            id: wordmark
            x: root.controlsWidth > 0 ? mark.x + mark.width + Theme.space.md : root.inset + root.labelX
            anchors.baseline: mark.verticalCenter
            anchors.baselineOffset: -wordmarkInk.tightBoundingRect.y / 2
            visible: !root.railed && (root.controlsWidth === 0 || x + width + Theme.space.md <= bandToggle.x)
            text: "Kite"
            color: Theme.palette.textPrimary
            font.family: Theme.font.heading
            font.pixelSize: Theme.type.subheading.pixelSize
            font.weight: Font.Bold
            font.letterSpacing: -0.2
        }
        TextMetrics {
            id: wordmarkInk
            font: wordmark.font
            text: wordmark.text
        }

        CollapseToggle {
            id: bandToggle
            visible: !root.railed
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
                surround: root.surround
                badge: root.badges[modelData.id] ? root.badges[modelData.id].count : 0
                badgeTone: root.badges[modelData.id] ? root.badges[modelData.id].tone : "accent"
                selected: root.selectedNavItem === navDelegate
                collapsed: root.railed
                onClicked: root.itemActivated(modelData.id)
            }
        }

        // A short rule where the destinations end and the instances begin:
        // no "Recent" heading, the icons say what they are.
        Rectangle {
            id: rule
            Layout.preferredWidth: root.railed ? root.itemSize - Theme.space.lg : -1
            Layout.fillWidth: !root.railed
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
                id: recentDelegate
                required property int index

                // Rows past what fits stay out of the layout entirely.
                visible: index < root.recentShown
                Layout.fillWidth: true
                implicitHeight: root.itemSize
                railSquare: root.itemSize
                iconAxis: root.railAxis
                labelX: root.labelX
                cornerRadius: root.itemRadius
                compact: root.railed
                selected: root.selectedNavItem === recentDelegate
                // The selection's fill when it sits under this one: only in
                // the labelled sidebar, the folded rail shows no square.
                surround: selected && !root.railed ? Theme.palette.surfaceRaised : root.surround
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
            collapsed: root.railed
            onClicked: root.newInstanceRequested()
        }

        Item { Layout.fillHeight: true }

        CollapseToggle {
            visible: root.railed && root.canExpand
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
                collapsed: root.railed
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
            compact: root.railed
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

    // The right edge, to be dragged; there is none where the window is too
    // narrow for the labelled sidebar at all.
    SidebarEdgeHandle {
        id: edge
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        visible: root.canExpand
        space: root.parent
        railWidth: root.railWidth
        minWidth: root.minExpandedWidth
        maxWidth: root.maxWidth
        panelWidth: root.width
        folded: root.collapsed
        onFoldRequested: (folded) => root.setCollapsed(folded)
        onWidthCommitted: (pixels) => root.widthCommitted(pixels)
    }
}
