// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One server in "Edit the bar": a handle to drag it up or down, its favicon, name and address, whether the bar
 * shows it, and Remove for one the player added. The drag is reported as a whole-row step (`stepRequested`), so
 * the list moves the row under the pointer as it goes.
 */
Item {
    id: root

    property string name: ""
    property string address: ""
    property bool own: false
    property bool featured: false
    property bool shown: true
    property string favicon: ""
    // The list's row pitch, for turning a drag into steps.
    property real step: height

    signal shownToggled(bool shown)
    signal removeRequested()
    // The drag that began on this row has gone @p steps rows from where it began.
    signal dragged(int steps)
    signal dragFinished()

    readonly property bool dragging: handleDrag.active

    implicitHeight: Theme.control.heightLg + Theme.space.xs
    Accessible.role: Accessible.ListItem
    Accessible.name: root.name

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.md
        color: root.dragging ? Theme.palette.surfaceHover : "transparent"
        border.width: root.dragging ? 1 : 0
        border.color: Theme.palette.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xs
        anchors.rightMargin: Theme.space.sm
        spacing: Theme.space.sm

        Item {
            id: handle
            objectName: "quickJoinEditHandle"
            Layout.preferredWidth: Theme.control.heightSm
            Layout.fillHeight: true
            Accessible.role: Accessible.Grip
            Accessible.name: qsTr("Drag to move %1").arg(root.name)

            MeshIcon {
                anchors.centerIn: parent
                iconName: "grip-vertical"
                size: Theme.icon.xs
                color: handleHover.hovered || root.dragging ? Theme.palette.textSecondary : Theme.palette.textTertiary
            }
            HoverHandler {
                id: handleHover
                cursorShape: root.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            }
            DragHandler {
                id: handleDrag
                target: null
                xAxis.enabled: false
                onTranslationChanged: root.dragged(Math.round(handleDrag.translation.y / Math.max(1, root.step)))
                onActiveChanged: if (!active) root.dragFinished()
            }
        }

        ServerFavicon {
            Layout.alignment: Qt.AlignVCenter
            source: root.favicon
            size: Theme.icon.mid
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignVCenter
            spacing: 0

            Copy {
                Layout.fillWidth: true
                text: root.name
                role: "label"
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                tone: root.shown ? "primary" : "tertiary"
            }
            Copy {
                Layout.fillWidth: true
                text: root.own && !root.featured ? qsTr("Your server · %1").arg(root.address) : root.address
                role: "caption"
                tone: "tertiary"
                elide: Text.ElideRight
            }
        }

        IconButton {
            objectName: "quickJoinEditRemove"
            Layout.alignment: Qt.AlignVCenter
            // A big server is hidden instead; only the player's own leave the list.
            visible: root.own && !root.featured
            size: Theme.control.heightSm
            iconName: "trash"
            tip: qsTr("Remove")
            onClicked: root.removeRequested()
        }

        CheckBox {
            objectName: "quickJoinEditShown"
            Layout.alignment: Qt.AlignVCenter
            checked: root.shown
            Accessible.name: qsTr("Show %1 in the bar").arg(root.name)
            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: root.shown ? qsTr("Shown in the bar") : qsTr("Hidden from the bar")
            onToggled: {
                root.shownToggled(checked)
                checked = Qt.binding(() => root.shown)
            }
        }
    }
}
