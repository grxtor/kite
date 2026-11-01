// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The bar ContentTab shows over its list while rows are ticked: how many, tick or untick all, and what can be done
 * with them. Enable, Disable and Remove are off while the game holds the files (with the reason on hover); Export
 * always works. Its ⋯ menu checks just these for updates or forgets where they came from.
 */
BoxSurface {
    id: root

    property var list: null
    property var listModel: null
    property bool unlocked: false
    property string lockedReason: ""

    property bool updatable: false

    signal enableRequested(bool enabled)
    signal removeRequested()
    signal exportRequested()
    signal checkUpdatesRequested()
    signal resetMetadataRequested()

    readonly property bool allShown: {
        if (!root.list)
            return false
        root.list.selectedKeys
        const shown = root.listModel && root.listModel.count >= 0 ? root.listModel.visibleKeys() : []
        return shown.length > 0 && shown.every((k) => root.list.isSelected(k))
    }

    implicitHeight: Theme.control.height + Theme.space.sm * 2
    color: Theme.palette.surfaceRaised

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.list ? root.list.rowPadding : Theme.space.lg
        anchors.rightMargin: Theme.space.sm
        spacing: Theme.space.sm

        Item {
            Layout.preferredWidth: root.list ? root.list.checkSize : 0
            Layout.preferredHeight: root.list ? root.list.checkSize : 0
            CheckBox {
                anchors.centerIn: parent
                padding: 0
                checkState: root.allShown ? Qt.Checked : Qt.PartiallyChecked
                Accessible.name: qsTr("Select all")
                onClicked: {
                    if (root.allShown)
                        root.list.clearSelection()
                    else
                        root.list.selectAll()
                    checkState = Qt.binding(() => root.allShown ? Qt.Checked : Qt.PartiallyChecked)
                }
            }
        }
        Copy {
            Layout.leftMargin: Theme.space.xs
            Layout.fillWidth: true
            text: qsTr("%1 selected").arg(root.list ? root.list.selectedKeys.length : 0)
            elide: Text.ElideRight
            role: "bodyStrong"
        }

        HoverReason {
            reason: root.unlocked ? "" : root.lockedReason
            RowLayout {
                spacing: Theme.space.sm
                enabled: root.unlocked
                Button {
                    text: qsTr("Enable")
                    onClicked: root.enableRequested(true)
                }
                Button {
                    text: qsTr("Disable")
                    onClicked: root.enableRequested(false)
                }
                Button {
                    text: qsTr("Remove…")
                    icon.source: Icons.url("trash")
                    onClicked: root.removeRequested()
                }
            }
        }
        Button {
            text: qsTr("Export…")
            onClicked: root.exportRequested()
        }
        IconButton {
            id: moreButton
            visible: root.updatable
            iconName: "more"
            tip: qsTr("More")
            onClicked: moreMenu.popup(moreButton, moreButton.width - moreMenu.width, moreButton.height)

            Menu {
                id: moreMenu
                MenuItem {
                    enabled: root.unlocked
                    text: qsTr("Check these for updates")
                    icon.source: Icons.url("refresh")
                    onTriggered: root.checkUpdatesRequested()
                }
                MenuItem {
                    text: qsTr("Reset metadata…")
                    icon.source: Icons.url("refresh")
                    onTriggered: root.resetMetadataRequested()
                }
            }
        }
        IconButton {
            iconName: "x"
            tip: qsTr("Clear selection")
            onClicked: root.list.clearSelection()
        }
    }
}
