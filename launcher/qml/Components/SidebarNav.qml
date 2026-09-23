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
    Rectangle {
        id: activeIndicator
        visible: !!root.selectedNavItem
        x: Theme.space.md
        y: Theme.space.md + (root.selectedNavItem ? root.selectedNavItem.y : 0)
        width: root.selectedNavItem ? root.selectedNavItem.width : 0
        height: root.selectedNavItem ? root.selectedNavItem.height : 0
        radius: Theme.radius.md
        color: Theme.palette.surfaceRaised

        Behavior on y { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        Behavior on width { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: parent.height - Theme.space.md * 2 + 4
            radius: 2
            color: Theme.palette.accent
        }
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

        AccountChip {
            visible: !root.collapsed
            Layout.fillWidth: true
            Layout.topMargin: Theme.space.sm
            name: root.accountName
            kind: root.accountKind
            avatarSource: root.accountAvatarSource
            onClicked: root.accountClicked()
        }

        // The chip's own slot is the accounts agent's, left untouched above;
        // the rail just needs *something* tappable in its place, so this is
        // a second, independent control rather than a squeezed copy of it.
        AbstractButton {
            id: railAccountButton
            visible: root.collapsed
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.space.sm
            implicitWidth: Theme.control.heightLg - 4
            implicitHeight: implicitWidth
            hoverEnabled: true
            Accessible.name: root.accountName.length > 0 ? qsTr("Account: %1").arg(root.accountName) : qsTr("Sign in")

            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: root.accountName.length > 0 ? root.accountName : qsTr("Sign in")

            background: Rectangle {
                radius: Theme.radius.md
                color: root.accountName.length > 0 ? Theme.palette.accentSubtle : Theme.palette.surfaceOverlay
                border.width: railAccountButton.hovered ? 1 : 0
                border.color: Theme.palette.borderStrong
            }

            contentItem: Item {
                Text {
                    anchors.centerIn: parent
                    visible: root.accountName.length > 0 && avatarImage.status !== Image.Ready
                    text: root.accountName.charAt(0).toUpperCase()
                    color: Theme.palette.accent
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }

                Image {
                    id: avatarImage
                    anchors.fill: parent
                    anchors.margins: 2
                    source: root.accountAvatarSource
                    visible: root.accountName.length > 0 && root.accountAvatarSource.length > 0
                    smooth: false
                    sourceSize: Qt.size(width, height)
                }

                MeshIcon {
                    anchors.centerIn: parent
                    visible: root.accountName.length === 0
                    iconName: "user"
                    size: Theme.icon.md
                    color: Theme.palette.textSecondary
                }
            }

            onClicked: root.accountClicked()
        }
    }
}
