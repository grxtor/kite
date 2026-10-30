// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The shell's top bar, Modrinth's: where you are -- the page's icon and
 * name, with an optional count (and, on the Library, the total play time),
 * or inside an instance a trail back up ("Home › Fabric Optimized"). A
 * breadcrumb at title size, not a page heading: the page's own section
 * headings carry the hierarchy below it. The account lives at the foot of
 * the sidebar (ProfileButton). It sits on the shell surface beside the
 * sidebar's brand band, the two forming one band, so it draws no background
 * of its own.
 *
 * Its optional second row, a search field and the actions passed in as
 * children (a PageToolbar), is for a TopBar used on its own; the shell keeps
 * that row off (searchVisible: false) and puts the Library's toolbar inside
 * the page area instead.
 *
 * What the shell wants at the bar's right end (the friends panel's toggle)
 * goes in `trailing`, at the window's edge. The bar runs on above a panel
 * that stands on the right (`rightReserve` is its width), so the title keeps
 * to the page's column, not the whole window's.
 */
Item {
    id: root

    property string title: ""
    // The page's own icon (a MeshIcon name), before the trail or the title.
    property string iconName: ""
    property int count: -1
    // The library's total play time after the title and count (see
    // TotalPlayTime; it hides itself while that setting is off).
    property bool showPlayTime: false
    // The way back up, before the title: labels, outermost first. Each one
    // is clickable (trailActivated(index)); the title itself is not.
    property var trail: []
    signal trailActivated(int index)

    property alias searchText: toolbar.searchText
    property alias searchPlaceholder: toolbar.searchPlaceholder
    property bool searchVisible: true

    default property alias actions: toolbar.actions
    // The shell's own buttons, at the right end of the title row.
    property alias trailing: trailingRow.data
    // The width of a panel that stands to the right of the page area, under
    // this bar: the page's column is centred in what is left of the window.
    property int rightReserve: 0
    // What stands over the bar's left end (macOS's traffic lights, when the
    // rail beside the bar is narrower than they are): the title starts
    // after it.
    property int leftReserve: 0

    readonly property int titleRowHeight: Theme.control.heightSm
    readonly property int toolbarRowHeight: Theme.control.heightLg
    implicitHeight: Theme.space.sm + titleRowHeight
                    + (root.searchVisible ? Theme.space.xs + toolbarRowHeight : 0)
                    + Theme.space.sm
    // The rows' padding: on a bar taller than they need (the shell's on
    // macOS, as tall as the title bar), half of what is left besides, so
    // they sit in its middle.
    readonly property int verticalInset: Theme.space.sm + Math.max(0, Math.floor((root.height - root.implicitHeight) / 2))

    function focusSearch() { toolbar.focusSearch() }

    // The same column every page's content uses: the page padding, or on a
    // wide window whatever centres a Theme.layout.maxContentWidth column --
    // so the title stays aligned with the page below it.
    readonly property int sideMargin: Math.max(Theme.space.xl + Theme.space.xs,
                                               Math.round((root.width - root.rightReserve - Theme.layout.maxContentWidth) / 2))

    // The trail and the title share one face and size; a step is only
    // quieter, and a link back to that level.
    readonly property int titleSize: Theme.type.title.pixelSize

    // One step of the trail: quieter than the title, and a link back to
    // that level.
    component TrailStep: AbstractButton {
        id: step
        required property string label
        required property int index
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        implicitWidth: stepText.implicitWidth
        implicitHeight: stepText.implicitHeight
        Accessible.role: Accessible.Link
        Accessible.name: step.label
        onClicked: root.trailActivated(step.index)

        contentItem: Item {}
        Text {
            id: stepText
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, step.width)
            text: step.label
            elide: Text.ElideRight
            color: step.hovered || step.visualFocus ? Theme.palette.textPrimary : Theme.palette.textSecondary
            font.family: Theme.font.heading
            font.pixelSize: root.titleSize
            font.weight: Font.DemiBold
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: root.verticalInset
        anchors.bottomMargin: root.verticalInset
        spacing: Theme.space.xs

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Math.max(root.sideMargin, root.leftReserve)
            // The page's column ends before the panel; and never under the
            // trailing buttons, which stand at the window's edge.
            Layout.rightMargin: Math.max(root.sideMargin + root.rightReserve,
                                         trailingRow.width + Theme.space.md * 2)
            Layout.preferredHeight: root.titleRowHeight
            spacing: Theme.space.sm

            MeshIcon {
                Layout.alignment: Qt.AlignVCenter
                visible: root.iconName.length > 0
                iconName: root.iconName
                size: Theme.icon.md
                color: root.trail.length > 0 ? Theme.palette.textSecondary : Theme.palette.textPrimary
            }

            Repeater {
                model: root.trail
                delegate: RowLayout {
                    id: crumb
                    required property var modelData
                    required property int index
                    Layout.minimumWidth: 0
                    spacing: Theme.space.sm

                    TrailStep {
                        Layout.alignment: Qt.AlignVCenter
                        // Long names give way before the title does.
                        Layout.maximumWidth: 220
                        label: crumb.modelData
                        index: crumb.index
                    }
                    MeshIcon {
                        Layout.alignment: Qt.AlignVCenter
                        iconName: "chevron-right"
                        size: Theme.icon.sm
                        color: Theme.palette.textTertiary
                    }
                }
            }

            Text {
                id: titleText
                Layout.alignment: Qt.AlignVCenter
                // Its own width, down to nothing under pressure: a long
                // instance name elides before the row overflows. (A layout
                // keeps an item that does not fill at its preferred width, so
                // it fills, and is held to the width it wants to stay put.)
                // That width is measured apart: an elided Text's own
                // implicitWidth follows the elided text, and held to that the
                // title elides itself down to "Ho…".
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.maximumWidth: Math.ceil(titleMetrics.advanceWidth)
                text: root.title
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.heading
                font.pixelSize: root.titleSize
                font.weight: Font.DemiBold

                TextMetrics {
                    id: titleMetrics
                    font: titleText.font
                    text: titleText.text
                }
            }

            Tag {
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: Theme.space.xs
                visible: root.count >= 0
                text: root.count
            }

            TotalPlayTime {
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: Theme.space.xs
                wanted: root.showPlayTime
            }

            Item { Layout.fillWidth: true }
        }

        PageToolbar {
            id: toolbar
            visible: root.searchVisible
            Layout.fillWidth: true
            Layout.preferredHeight: root.toolbarRowHeight
        }
    }

    // On the title row's line, whatever the toolbar row below does; over the
    // layout, so its buttons are first in line for the pointer.
    Row {
        id: trailingRow
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.md
        anchors.top: parent.top
        anchors.topMargin: root.verticalInset
        height: root.titleRowHeight
        spacing: Theme.space.xs
    }
}
