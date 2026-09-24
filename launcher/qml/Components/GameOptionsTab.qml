// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * This instance's options.txt, read-only - the widget-free replacement for
 * GameOptionsPage, which is read-only too (its own model has a save(), but
 * nothing on that page ever calls it - see its header comment).
 */
Item {
    id: root

    // KeyValueFilterModel over the instance's GameOptions (InstanceDetails.gameOptions).
    property var model: null

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            SearchBox {
                Layout.fillWidth: true
                placeholderText: qsTr("Search options")
                onTextChanged: if (root.model) root.model.filterText = text
            }
            Text {
                visible: !!root.model
                text: qsTr("%1 options").arg(list.count)
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius.lg
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: Theme.space.sm
                clip: true
                model: root.model
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: RowLayout {
                    required property string key
                    required property string value

                    width: list.width
                    height: Theme.control.height
                    spacing: Theme.space.md

                    Text {
                        Layout.preferredWidth: list.width * 0.5
                        text: key
                        elide: Text.ElideRight
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.mono
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                    Text {
                        Layout.fillWidth: true
                        text: value
                        elide: Text.ElideRight
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.mono
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                }
            }

            EmptyState {
                anchors.centerIn: parent
                visible: !root.model || list.count === 0
                title: qsTr("No options found")
                body: root.model && root.model.filterText.length > 0
                      ? qsTr("Nothing matches “%1”.").arg(root.model.filterText)
                      : qsTr("This instance has no options.txt yet - launch it once to create one.")
                MeshIcon { iconName: "settings"; size: 40; color: Theme.palette.textTertiary }
            }
        }
    }
}
