// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One group of the library: a collapsible header and a grid of cards. The
 * page decides what rename, delete and drops mean; this only reports them.
 */
Column {
    id: root

    property string title
    // The group these instances are in; "" for ungrouped.
    property string group
    property bool showHeader: true
    property var model: null
    property int columns: 4
    property real cardWidth: 180
    property int gutter: Theme.space.md
    property string selectedId
    property bool collapsed: false
    // "grid" (InstanceCard tiles) or "list" (dense InstanceListRow rows).
    property string viewMode: "grid"
    // Where the cards end: whole-pixel card widths can stop short of the section's width.
    readonly property int contentRight: viewMode === "list" ? width
        : Math.min(width, Math.round(columns * cardWidth + (columns - 1) * gutter))

    signal selectRequested(string id)
    signal launchRequested(string id)
    signal stopRequested(string id)
    signal menuRequested(string id, bool running, string name, string iconKey, string group, Item anchor)
    signal toggleRequested()

    property bool manageable: false
    property bool dragEnabled: false
    signal renameGroupRequested(string group)
    signal deleteGroupRequested(string group)
    signal instanceDropped(string id, string group)
    // A drag of one of this section's instances began or ended.
    signal dragActiveChanged(bool active)
    signal dragMoved(real sceneY)

    readonly property string dragKey: "kite/instance"

    function openGroupMenu() {
        groupMenu.popup(groupMenuButton, groupMenuButton.width - groupMenu.width, groupMenuButton.height)
    }
    // Under the page Flickable's own threshold, so a drag on a card is claimed before the page scrolls.
    readonly property int dragThreshold: Theme.space.sm

    component InstanceDrag: Item {
        id: dragSource
        // An inline component does not see the file's ids.
        required property Item section
        property string instanceId
        property string name
        property string iconKey
        anchors.fill: parent
        // Under the card's content: on top, it took the ⋯ and Play clicks from the card's selection
        // tap. Its handler still sees presses that start on them.
        z: -1
        enabled: dragSource.section.dragEnabled

        DragHandler {
            id: dragger
            target: null
            dragThreshold: dragSource.section.dragThreshold
            // Takes the pointer from the card's taps and keeps it from the page's scrolling.
            grabPermissions: PointerHandler.CanTakeOverFromItems | PointerHandler.CanTakeOverFromHandlersOfDifferentType
            onActiveChanged: {
                if (active) {
                    chipLoader.active = true
                    chipLoader.item.Drag.start()
                } else if (chipLoader.item) {
                    chipLoader.item.Drag.drop()
                    chipLoader.active = false
                }
                dragSource.section.dragActiveChanged(active)
            }
            onCentroidChanged: if (active) dragSource.section.dragMoved(centroid.scenePosition.y)
        }

        // Built only while a drag is on; the chip lives in the window overlay.
        Loader {
            id: chipLoader
            active: false
            sourceComponent: Component {
                Rectangle {
                    id: chip
                    parent: Overlay.overlay
                    readonly property string instanceId: dragSource.instanceId
                    readonly property string group: dragSource.section.group
                    x: dragger.centroid.scenePosition.x - width / 2
                    y: dragger.centroid.scenePosition.y - height / 2
                    width: chipRow.implicitWidth + Theme.space.md * 2
                    height: Theme.control.height
                    radius: Theme.radius.sm
                    color: Theme.palette.surfaceOverlay
                    border.width: 1
                    border.color: Theme.palette.borderStrong
                    Drag.keys: [dragSource.section.dragKey]
                    Drag.hotSpot.x: width / 2
                    Drag.hotSpot.y: height / 2

                    Row {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: Theme.space.sm
                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Theme.icon.md
                            height: Theme.icon.md
                            sourceSize.width: width * 2
                            sourceSize.height: height * 2
                            source: Icons.instanceIcon(dragSource.iconKey)
                            smooth: true
                            mipmap: true
                        }
                        Copy {
                            anchors.verticalCenter: parent.verticalCenter
                            text: dragSource.name
                            font.weight: Font.Medium
                        }
                    }
                }
            }
        }
    }

    readonly property int headerHeight: Theme.control.height

    spacing: root.showHeader ? Theme.space.md : 0

    // Zero-height without a header (ungrouped instances), but its DropArea must still take an
    // instance dragged out of a group.
    Item {
        width: parent.width
        height: root.showHeader ? root.headerHeight : 0

        AbstractButton {
            id: header
            visible: root.showHeader
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: divider.top
            width: headerRow.implicitWidth
            hoverEnabled: true
            Accessible.name: root.title
            Accessible.description: root.collapsed ? qsTr("Collapsed") : qsTr("Expanded")
            onClicked: root.toggleRequested()

            TapHandler {
                acceptedButtons: Qt.RightButton
                enabled: root.manageable
                onTapped: groupMenu.popup()
            }

            contentItem: Row {
                id: headerRow
                spacing: Theme.space.sm

                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "chevron-down"
                    size: Theme.icon.md
                    rotation: root.collapsed ? -90 : 0
                    color: header.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
                    Behavior on rotation { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                }

                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.title
                    role: "title"
                }

                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !!root.model
                    text: root.model ? String(root.model.count) : ""
                    role: "caption"
                    tone: "tertiary"
                    font.weight: Font.Medium
                }
            }
            background: StateFill {
                x: -Theme.space.sm
                y: Theme.space.xs
                width: parent.width + Theme.space.sm * 2
                height: parent.height - Theme.space.xs * 2
                radius: Theme.radius.sm
                hovered: header.hovered
                down: header.down

                FocusFrame { around: 0; visible: header.visualFocus }
            }
        }

        IconButton {
            id: groupMenuButton
            visible: root.manageable && root.showHeader
            x: root.contentRight - width
            anchors.verticalCenter: header.verticalCenter
            size: Theme.control.heightSm
            iconName: "more"
            tip: qsTr("Group options")
            onClicked: root.openGroupMenu()
        }

        Rectangle {
            id: divider
            visible: root.showHeader
            anchors.bottom: parent.bottom
            width: root.contentRight
            height: 1
            color: Theme.palette.divider
        }

        // The whole section is the target, a folded group's header alone too. Lives in the header
        // so the Column does not lay it out as a row.
        DropArea {
            id: dropArea
            z: -1
            x: -Theme.space.sm
            y: -Theme.space.sm
            width: root.width + Theme.space.sm * 2
            height: root.height + Theme.space.sm * 2
            keys: [root.dragKey]
            readonly property bool wanted: containsDrag && !!drag.source && drag.source.group !== root.group
            onDropped: (drop) => {
                if (!drop.source || drop.source.group === root.group)
                    return
                root.instanceDropped(drop.source.instanceId, root.group)
                drop.accept()
            }

            Rectangle {
                anchors.fill: parent
                visible: dropArea.wanted
                radius: Theme.radius.lg
                color: Theme.palette.hoverOverlay
                border.width: 1
                border.color: Theme.palette.accent
            }
        }
    }

    Menu {
        id: groupMenu
        MenuItem {
            text: qsTr("Rename group\u2026")
            icon.source: Icons.url("edit")
            onTriggered: root.renameGroupRequested(root.group)
        }
        MenuItem {
            text: qsTr("Delete group\u2026")
            icon.source: Icons.url("trash")
            onTriggered: root.deleteGroupRequested(root.group)
        }
    }

    // Clipped to an animated height, not toggled by `visible`, so a fold reflows the sections below.
    Item {
        id: body
        readonly property real fullHeight: contentLoader.item ? contentLoader.item.implicitHeight : 0
        width: parent.width
        height: root.collapsed ? 0 : fullHeight
        // Only while folding: a permanent clip would cut off a card's focus ring on the top row.
        clip: height < fullHeight - 0.5

        // Off for the initial layout: height jumps from 0 to the real height when the Loader first
        // produces content, and animating that jump hung the app at startup on Windows.
        property bool settled: false
        Component.onCompleted: Qt.callLater(function () { body.settled = true })

        Behavior on height {
            enabled: body.settled
            NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing }
        }

        Loader {
            id: contentLoader
            // No explicit width: each Component sizes its own root item.
            sourceComponent: root.viewMode === "list" ? listComponent : gridComponent
        }
    }

    Component {
        id: gridComponent
        Grid {
            columns: root.columns
            columnSpacing: root.gutter
            rowSpacing: root.gutter

            Repeater {
                model: root.model
                delegate: InstanceCard {
                    id: card
                    // The row itself, for the one role (packArt) InstanceCard does not require.
                    required property var model
                    width: root.cardWidth
                    packArt: card.model.packArt || ""
                    selected: root.selectedId === instanceId
                    InstanceDrag {
                        section: root
                        instanceId: card.instanceId
                        name: card.name
                        iconKey: card.iconKey
                    }
                    onClicked: root.selectRequested(instanceId)
                    onPlayRequested: root.launchRequested(instanceId)
                    onStopRequested: root.stopRequested(instanceId)
                    onMenuRequested: (anchor) => {
                        root.selectRequested(instanceId)
                        root.menuRequested(instanceId, isRunning, name, iconKey, root.group, anchor)
                    }
                }
            }
        }
    }

    Component {
        id: listComponent
        Column {
            width: body.width
            spacing: Theme.space.sm

            Repeater {
                model: root.model
                delegate: InstanceListRow {
                    id: row
                    // The row itself, for the one role (packArt) InstanceListRow does not require.
                    required property var model
                    width: body.width
                    packArt: row.model.packArt || ""
                    selected: root.selectedId === instanceId
                    InstanceDrag {
                        section: root
                        instanceId: row.instanceId
                        name: row.name
                        iconKey: row.iconKey
                    }
                    onClicked: root.selectRequested(instanceId)
                    onPlayRequested: root.launchRequested(instanceId)
                    onStopRequested: root.stopRequested(instanceId)
                    onMenuRequested: (anchor) => {
                        root.selectRequested(instanceId)
                        root.menuRequested(instanceId, isRunning, name, iconKey, root.group, anchor)
                    }
                }
            }
        }
    }
}
