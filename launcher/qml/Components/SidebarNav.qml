// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/* The left rail; `collapsed: false` widens it into the labelled sidebar. */
Rectangle {
    id: root

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
    // Counts by id: { friends: { count, tone } }; a map of its own so a count changing does not
    // rebuild `items`.
    property var badges: ({})
    property string currentId: ""
    // The open instance: its recent tile takes the selection while listed, over currentId.
    property string currentInstanceId: ""

    // Any model with instanceId/name/iconKey/isRunning roles; at most `recentLimit` rows, fewer on
    // a short window.
    property var recentModel: null
    property int recentLimit: 5
    // On Home a recent tile picks the instance for the hero (one click), opens its page (two) or launches it
    // (three); elsewhere one click opens the page.
    property bool pickOnTap: false

    property string accountName: ""
    property string accountKind: ""
    property string accountAvatarSource: ""
    property var accountsController: null

    // The rail at rest; `railed` differs only while the edge is dragged.
    property bool collapsed: true
    // The saved sidebar width; brought into range (see fullWidth), not trusted.
    property int expandedWidth: 240
    // What the window can give the sidebar: its width less what pages and the friends panel need.
    property int roomWidth: 0
    // False while the host keeps the rail (narrow window): the expand toggle hides.
    property bool canExpand: true
    // Colour behind the rail, for the corners of a full-tile instance icon.
    property color surround: "transparent"
    property int bandHeight: Theme.control.heightLg
    // Width taken by the window's own buttons (macOS traffic lights) at the band's left end; the
    // mark and wordmark move past them.
    property int controlsWidth: 0

    // Every item is one square, inset the same on both sides; the square's middle is the rail's
    // vertical axis.
    readonly property int itemSize: Theme.control.heightLg
    readonly property int inset: Theme.space.sm
    readonly property int railWidth: root.itemSize + root.inset * 2
    // Wide enough for "Settings" and a short instance name beside the icon.
    readonly property int minExpandedWidth: 200
    readonly property int maxExpandedWidth: 380
    // From the inner column's left edge; inset + this from the rail's edge.
    readonly property int railAxis: root.itemSize / 2
    // Labels start here, after the recent tiles (RecentItem.tileSize).
    readonly property int labelX: root.railAxis + Theme.control.heightSm / 2 + Theme.space.md
    readonly property int itemRadius: Theme.radius.lg
    readonly property int brandExtent: Theme.control.heightSm
    readonly property int brandGap: Theme.space.md

    signal itemActivated(string id)
    signal recentSelectRequested(string id)
    signal recentActivated(string id)
    signal recentPlayRequested(string id)
    signal newInstanceRequested()
    signal openAccountsRequested()
    signal widthCommitted(int width)

    function openProfileMenu() { accountButton.openMenu() }

    // Recent instances that fit between + and Settings; on a short window the list gives way first.
    readonly property int recentShown: {
        var pitch = root.itemSize + column.spacing
        var fixed = root.bandHeight + root.brandGap + Theme.layout.barHeight
                  + (root.items.length + root.footerItems.length + 1) * pitch
                  + rule.height + rule.Layout.topMargin + rule.Layout.bottomMargin + column.spacing
                  + (root.railed && root.canExpand ? Theme.control.heightSm + column.spacing : 0)
                  + Theme.space.lg
        return Math.max(0, Math.min(root.recentLimit, Math.floor((root.height - fixed) / pitch)))
    }

    // The item matching currentId, the open instance's recent tile or the account; null hides the
    // selection.
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

    // Not past maxExpandedWidth, nor less than roomWidth leaves, but never narrower than its names
    // need.
    readonly property int maxWidth: Math.max(root.minExpandedWidth,
                                             Math.min(root.maxExpandedWidth,
                                                      root.roomWidth > 0 ? root.roomWidth : root.maxExpandedWidth))
    // The saved width brought into range: the window may have shrunk.
    readonly property int fullWidth: Math.min(Math.max(root.expandedWidth, root.minExpandedWidth), root.maxWidth)
    // What is on show: rail or sidebar at rest; while dragging, the one letting go now would leave.
    readonly property bool railed: edge.showsRail

    function setCollapsed(collapsed) {
        SettingsStore.setValue("UiSidebarCollapsed", collapsed)
    }

    // True from the first turn of the event loop: the start-up width should appear in place.
    property bool settled: false
    Component.onCompleted: Qt.callLater(() => root.settled = true)

    // The slide belongs to a change of this, not of the width, which window resizes move and must
    // follow at once.
    readonly property string mode: edge.dragging ? "drag" : root.collapsed ? "rail" : "full"

    color: "transparent"
    // Only a change of state slides; a drag follows the pointer.
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
            duration: Theme.motion.normal
            easing.type: Theme.motion.easing
        }
    }

    NavSelectionIndicator {
        // An instance's own tile shows the selection on the folded rail; a square around it would
        // frame a picture that already fills its tile.
        target: root.railed && root.selectedIsInstance ? null : root.selectedNavItem
        collapsed: root.railed
        railSquare: root.itemSize
        railAxis: root.railAxis
        cornerRadius: root.itemRadius
        live: edge.dragging
        // Rows live in two containers, so positions are summed up to this rail.
        space: root
    }

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

        // Centred on the mark by its letters, not its line box, which hangs low.
        Copy {
            id: wordmark
            x: root.controlsWidth > 0 ? mark.x + mark.width + Theme.space.md : root.inset + root.labelX
            anchors.baseline: mark.verticalCenter
            anchors.baselineOffset: -wordmarkInk.tightBoundingRect.y / 2
            visible: !root.railed && (root.controlsWidth === 0 || x + width + Theme.space.md <= bandToggle.x)
            text: "Kite"
            role: "subheading"
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

                visible: index < root.recentShown
                Layout.fillWidth: true
                implicitHeight: root.itemSize
                railSquare: root.itemSize
                iconAxis: root.railAxis
                labelX: root.labelX
                cornerRadius: root.itemRadius
                compact: root.railed
                selected: root.selectedNavItem === recentDelegate
                // The selection's fill under this tile, only in the labelled sidebar.
                surround: selected && !root.railed ? Theme.palette.surfaceRaised : root.surround
                onClicked: root.pickOnTap ? taps.tap() : root.recentActivated(instanceId)
                Keys.onReturnPressed: root.recentActivated(instanceId)
                TapSequence {
                    id: taps
                    onSingle: root.recentSelectRequested(recentDelegate.instanceId)
                    onTwice: root.recentActivated(recentDelegate.instanceId)
                    onThrice: if (!recentDelegate.isRunning) root.recentPlayRequested(recentDelegate.instanceId)
                }
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

    // Absent where the window is too narrow for the labelled sidebar.
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
