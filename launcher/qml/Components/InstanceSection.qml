// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One group of the library: a collapsible header and a grid of cards.
 * Column count and card width come from the page so every section lines
 * up with every other one.
 */
Column {
    id: root

    property string title
    // The group these instances are in; "" for ungrouped.
    property string group
    property bool showHeader: true
    property var model: null
    property int columns: 4
    property real cardWidth: 208
    property int gutter: Theme.space.lg
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
    signal menuRequested(string id, bool running, string name, string iconKey, string group)
    signal toggleRequested()

    spacing: Theme.space.md

    Item {
        visible: root.showHeader
        width: parent.width
        height: Theme.control.height

        AbstractButton {
            id: header
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height
            width: headerRow.implicitWidth
            hoverEnabled: true
            Accessible.name: root.title
            onClicked: root.toggleRequested()

            // The title first, flush with the page title and the cards'
            // left edge; the fold chevron trails the count rather than
            // pushing the title in by its own width.
            contentItem: Row {
                id: headerRow
                spacing: Theme.space.sm

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.title
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }

                Tag {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !!root.model
                    text: root.model ? String(root.model.count) : ""
                }

                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "chevron-down"
                    size: Theme.icon.sm
                    rotation: root.collapsed ? -90 : 0
                    color: header.hovered ? Theme.palette.textPrimary : Theme.palette.textTertiary
                    Behavior on rotation { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                }
            }
            background: Item {}
        }

        // On whole pixels: the header's text width is fractional, and a
        // hairline starting mid-pixel was drawn a pixel past its end.
        Rectangle {
            x: Math.ceil(header.width) + Theme.space.md
            y: Math.floor((parent.height - height) / 2)
            width: Math.max(0, root.contentRight - x)
            height: 1
            color: Theme.palette.divider
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
                    width: root.cardWidth
                    selected: root.selectedId === instanceId
                    onClicked: root.selectRequested(instanceId)
                    onPlayRequested: root.launchRequested(instanceId)
                    onStopRequested: root.stopRequested(instanceId)
                    onMenuRequested: {
                        root.selectRequested(instanceId)
                        root.menuRequested(instanceId, isRunning, name, iconKey, root.group)
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
                    width: body.width
                    selected: root.selectedId === instanceId
                    onClicked: root.selectRequested(instanceId)
                    onPlayRequested: root.launchRequested(instanceId)
                    onStopRequested: root.stopRequested(instanceId)
                    onMenuRequested: {
                        root.selectRequested(instanceId)
                        root.menuRequested(instanceId, isRunning, name, iconKey, root.group)
                    }
                }
            }
        }
    }
}
