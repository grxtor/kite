// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The shell's top bar: the page's icon and name with an optional count, or a
 * breadcrumb trail inside an instance. It sits on the shell surface and draws
 * no background. The optional second row (search + children) is for a TopBar
 * used on its own; `trailing` goes at the window's edge; `rightReserve` is the
 * width of a panel on the right.
 */
Item {
    id: root

    property string title: ""
    property string iconName: ""
    property int count: -1
    // The library's total play time after the title and count.
    property bool showPlayTime: false
    // The way back up, outermost first; each step is clickable (trailActivated(index)), the title
    // is not.
    property var trail: []
    signal trailActivated(int index)

    property alias searchText: toolbar.searchText
    property alias searchPlaceholder: toolbar.searchPlaceholder
    property bool searchVisible: true

    default property alias actions: toolbar.actions
    property alias trailing: trailingRow.data
    // Width of a panel right of the page area: the page's column centres in what is left.
    property int rightReserve: 0
    // Width covered at the bar's left end (macOS traffic lights over a narrow rail); the title
    // starts after it.
    property int leftReserve: 0

    readonly property int titleRowHeight: Theme.control.heightSm
    readonly property int toolbarRowHeight: Theme.control.heightLg
    implicitHeight: Theme.space.sm + titleRowHeight
                    + (root.searchVisible ? Theme.space.xs + toolbarRowHeight : 0)
                    + Theme.space.sm

    function focusSearch() { toolbar.focusSearch() }

    readonly property int sideMargin: Math.max(Theme.space.xl + Theme.space.xs,
                                               Math.round((root.width - root.rightReserve - Theme.layout.maxContentWidth) / 2))

    readonly property int titleSize: Theme.type.title.pixelSize

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
        id: rows
        anchors.left: parent.left
        anchors.right: parent.right
        // A bar taller than the rows (macOS: as tall as the title bar) shares the rest above and below.
        anchors.verticalCenter: parent.verticalCenter
        height: root.implicitHeight - 2 * Theme.space.sm
        spacing: Theme.space.xs

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Math.max(root.sideMargin, root.leftReserve)
            // The page's column ends before the panel and never under the trailing buttons.
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
                // Fills, then held to the width it wants: an elided Text's implicitWidth follows
                // the elided text (down to "Ho…"), so that width is measured apart.
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

    // Over the layout, so its buttons are first in line for the pointer.
    Row {
        id: trailingRow
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.md
        anchors.top: rows.top
        height: root.titleRowHeight
        spacing: Theme.space.xs
    }
}
