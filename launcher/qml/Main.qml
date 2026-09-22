// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * Root of the QML user interface.
 *
 * Deliberately unstyled for now: it exists to prove the path from the core's
 * models to the screen with real data, before the design system is layered on.
 * Everything it shows arrives as a required property set by QmlShell, so a
 * missing model is a load error rather than an empty window.
 */
ApplicationWindow {
    id: root

    /* The instance list, straight from the core (InstanceList). Its named
     * roles -- name, iconKey, group, instanceId, isRunning... -- are what the
     * delegate binds to. */
    required property var instanceModel

    /* Selection by instance id (IdSelectionModel). Ids rather than rows,
     * because rows move whenever the filter or the sort changes. */
    required property var selection

    // Read by QmlModule_test to prove this component, and not some default,
    // was instantiated.
    readonly property string moduleName: "MeshMC"

    width: 1100
    height: 700
    minimumWidth: 720
    minimumHeight: 480
    title: "MeshMC"

    GridView {
        id: grid

        anchors.fill: parent
        anchors.margins: 16
        model: root.instanceModel
        cellWidth: 168
        cellHeight: 128
        clip: true

        delegate: Item {
            id: tile

            required property string name
            required property string group
            required property string iconKey

            width: grid.cellWidth - 8
            height: grid.cellHeight - 8

            Column {
                anchors.centerIn: parent
                spacing: 4

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 40
                    height: 40
                    sourceSize: Qt.size(40, 40)
                    source: "image://instanceicon/" + tile.iconKey
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: tile.width - 16
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: tile.name
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    opacity: 0.6
                    font.pixelSize: 11
                    text: tile.group
                    visible: text.length > 0
                }
            }
        }

        Label {
            anchors.centerIn: parent
            visible: grid.count === 0
            opacity: 0.6
            text: qsTr("No instances yet")
        }
    }
}
