// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One group of the library: a collapsible header and a grid of cards.
 * Column count and card width come from the page so every section lines
 * up with every other one.
 *
 * A named group's header carries a ⋯ menu (also on right-click) to rename
 * or delete the group, and the whole section takes an instance dragged
 * from another one: the card stays put while a small chip with its icon
 * and name follows the pointer. What either means is the page's business;
 * this only says what was asked for.
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
    // Where the cards end: whole-pixel card widths can stop a pixel or two
    // short of the section's own width, and the header's divider ends with
    // them rather than poking past the grid's right edge.
    readonly property int contentRight: viewMode === "list" ? width
        : Math.min(width, Math.round(columns * cardWidth + (columns - 1) * gutter))

    signal selectRequested(string id)
    signal launchRequested(string id)
    signal stopRequested(string id)
    signal menuRequested(string id, bool running, string name, string iconKey, string group, Item anchor)
    signal toggleRequested()

    // Rename/delete, offered only for a named group the page can manage.
    property bool manageable: false
    // Whether instances can be dragged out of this section, onto another.
    property bool dragEnabled: false
    signal renameGroupRequested(string group)
    signal deleteGroupRequested(string group)
    // An instance from another group, dropped here.
    signal instanceDropped(string id, string group)
    // A drag of one of this section's instances began or ended, and where
    // the pointer is (window y), so the page can hold its scrolling still
    // under it and scroll near its edges.
    signal dragActiveChanged(bool active)
    signal dragMoved(real sceneY)

    readonly property string dragKey: "kite/instance"

    // The group ⋯ menu, under its button.
    function openGroupMenu() {
        groupMenu.popup(groupMenuButton, groupMenuButton.width - groupMenu.width, groupMenuButton.height)
    }
    // Under the page Flickable's own threshold, so a drag on a card is
    // claimed as a drag before the page starts to scroll.
    readonly property int dragThreshold: Theme.space.sm

    // A card or row's drag: the handler over it, and the chip that follows
    // the pointer, so the grid itself never moves.
    component InstanceDrag: Item {
        id: dragSource
        // The InstanceSection it belongs to (an inline component does not
        // see the file's ids).
        required property Item section
        property string instanceId
        property string name
        property string iconKey
        anchors.fill: parent
        // Under the card's own content. On top, it left the card's
        // selection tap to take the ⋯ and Play buttons' clicks: ⋯ only
        // selected the card and opened no menu. Its handler still sees a
        // press that starts on them, so a drag can begin anywhere on the
        // card.
        z: -1
        enabled: dragSource.section.dragEnabled

        DragHandler {
            id: dragger
            target: null
            dragThreshold: dragSource.section.dragThreshold
            // May take the pointer from the card's own taps, and does not
            // hand it to the page's scrolling once the drag is on.
            grabPermissions: PointerHandler.CanTakeOverFromItems | PointerHandler.CanTakeOverFromHandlersOfDifferentType
            onActiveChanged: {
                if (active)
                    chip.Drag.start()
                else
                    chip.Drag.drop()
                dragSource.section.dragActiveChanged(active)
            }
            onCentroidChanged: if (active) dragSource.section.dragMoved(centroid.scenePosition.y)
        }

        Rectangle {
            id: chip
            parent: Overlay.overlay
            visible: dragger.active
            // What a drop reads off its source.
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
                    source: "image://instanceicon/" + (dragSource.iconKey.length > 0 ? dragSource.iconKey : "default")
                    smooth: true
                    mipmap: true
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: dragSource.name
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    font.weight: Font.Medium
                }
            }
        }
    }

    // Modrinth App's group header height.
    readonly property int headerHeight: 40

    // No gap above the cards when there is no header to be apart from.
    spacing: root.showHeader ? Theme.space.md : 0

    // Always there, zero-height without a header (the ungrouped instances,
    // as in MultiMC and Prism): the section's DropArea lives in it, and an
    // ungrouped section must still take an instance dragged out of a group.
    // Modrinth App's group header: 40px, a fold chevron leading the name
    // and its count, the group's ⋯ at the far right, and a hairline under
    // the whole row.
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

            // The chevron flush with the cards' left edge; the hover plate
            // reaches a little past the row on both sides.
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

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.title
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.heading
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Theme.type.title.weight
                }

                Tag {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !!root.model
                    text: root.model ? String(root.model.count) : ""
                }
            }
            background: Rectangle {
                x: -Theme.space.sm
                y: Theme.space.xs
                width: parent.width + Theme.space.sm * 2
                height: parent.height - Theme.space.xs * 2
                radius: Theme.radius.sm
                color: header.down ? Theme.palette.pressedOverlay
                     : header.hovered ? Theme.palette.hoverOverlay : "transparent"
                border.width: header.visualFocus ? Theme.focus.width : 0
                border.color: Theme.palette.focusRing
                Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
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

        // Under the whole row, ending where the cards end.
        Rectangle {
            id: divider
            visible: root.showHeader
            anchors.bottom: parent.bottom
            width: root.contentRight
            height: 1
            color: Theme.palette.divider
        }

        // The whole section is the target, header and cards alike, and a
        // folded group's header alone still is one. Lives in the header so
        // the Column does not lay it out as a row of its own; drawn under
        // the cards, around them.
        DropArea {
            id: dropArea
            // Under the header's own title and button too.
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

            // A neutral wash with an accent edge: no tinted fills.
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

    // Clipped to an animated height rather than toggling `visible` outright,
    // so a fold/unfold reflows the sections below it instead of jumping.
    Item {
        id: body
        readonly property real fullHeight: contentLoader.item ? contentLoader.item.implicitHeight : 0
        width: parent.width
        height: root.collapsed ? 0 : fullHeight
        // Only while folded or folding: a permanent clip would also cut off
        // a card's focus ring along the top row.
        clip: height < fullHeight - 0.5

        // Off for the section's own initial layout -- height jumps straight
        // from 0 to the grid/list's real height as soon as the Loader below
        // first produces content, and animating that first jump (rather
        // than a later, real fold/unfold click) is exactly the kind of
        // startup work that made the app hang on Windows. Qt.callLater runs
        // after that first layout pass has settled, so a genuine click a
        // moment later still gets the fold animation.
        property bool settled: false
        Component.onCompleted: Qt.callLater(function () { body.settled = true })

        Behavior on height {
            enabled: body.settled
            NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing }
        }

        Loader {
            id: contentLoader
            // No explicit width: each Component below sizes its own root
            // item (the grid packs to its own columns*cardWidth, the list
            // binds to `body.width` itself), so the Loader is left free to
            // just take whichever one it is showing rather than fight it.
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
                    // The row itself, for the one role InstanceCard does
                    // not require (packArt).
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
                    // The row itself, for the one role InstanceListRow
                    // does not require (packArt).
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
