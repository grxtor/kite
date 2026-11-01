// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The shell's top bar: the page's icon and name, or a breadcrumb trail inside an
 * instance. It sits on the shell surface and draws no background. `trailing`
 * goes at the window's edge; `rightReserve` is the width of a panel on the right.
 */
Item {
    id: root

    property string title: ""
    property string iconName: ""
    // The way back up, outermost first; each step is clickable (trailActivated(index)), the title
    // is not.
    property var trail: []
    signal trailActivated(int index)

    property alias trailing: trailingRow.data
    // Width of a panel right of the page area: the page's column centres in what is left.
    property int rightReserve: 0
    // The page column's margin (Main.pageSideMargin).
    property int sideMargin: Theme.space.xl + Theme.space.xs
    // Width covered at the bar's left end (macOS traffic lights over a narrow rail); the title
    // starts after it.
    property int leftReserve: 0

    readonly property int titleRowHeight: Theme.control.heightSm
    implicitHeight: Theme.space.sm + titleRowHeight + Theme.space.sm

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
        Copy {
            id: stepText
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, step.width)
            text: step.label
            role: "title"
            elide: Text.ElideRight
            color: step.hovered || step.visualFocus ? Theme.palette.textPrimary : Theme.palette.textSecondary
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
    }

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        // A bar taller than the row (macOS: as tall as the title bar) shares the rest above and below.
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Math.max(root.sideMargin, root.leftReserve)
        // The page's column ends before the panel and never under the trailing buttons.
        anchors.rightMargin: Math.max(root.sideMargin + root.rightReserve,
                                      trailingRow.width + Theme.space.md * 2)
        height: root.titleRowHeight
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

        Copy {
            id: titleText
            Layout.alignment: Qt.AlignVCenter
            // Fills, then held to the width it wants: an elided Text's implicitWidth follows
            // the elided text (down to "Ho…"), so that width is measured apart.
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.maximumWidth: Math.ceil(titleMetrics.advanceWidth)
            text: root.title
            role: "title"
            elide: Text.ElideRight

            TextMetrics {
                id: titleMetrics
                font: titleText.font
                text: titleText.text
            }
        }

        Item { Layout.fillWidth: true }
    }

    // Over the layout, so its buttons are first in line for the pointer.
    Row {
        id: trailingRow
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.md
        anchors.top: row.top
        height: root.titleRowHeight
        spacing: Theme.space.xs
    }
}
