// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Pick one of the Java installs MeshMC found on this computer (the same
 * list the first-run setup shows, plus the runtimes MeshMC downloaded
 * itself). Choosing a row emits picked() and closes; the caller decides
 * what to do with the path.
 */
Dialog {
    id: root

    // The path in use now, marked in the list.
    property string currentPath
    readonly property var java: JavaStore.controller

    signal picked(string path, string version)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(600, parent ? parent.width - Theme.space.xxl * 2 : 600)
    height: Math.min(520, parent ? parent.height - Theme.space.xxl * 2 : 520)
    modal: true
    title: qsTr("Choose Java")

    header: DialogHeader {
        title: root.title
        icon: "cube"
    }

    onOpened: if (root.java && root.java.installs && list.count === 0 && !root.java.detecting) root.java.detect()

    contentItem: Rectangle {
        radius: Theme.radius.lg
        color: Theme.palette.surfaceSunken
        border.width: 1
        border.color: Theme.palette.border

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: Theme.space.xs
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.java ? root.java.installs : null
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property string path
                required property var version
                required property bool recommended
                readonly property bool chosen: root.currentPath === path

                width: list.width
                height: 52
                highlighted: chosen
                onClicked: {
                    root.picked(row.path, String(row.version))
                    root.close()
                }

                contentItem: RowLayout {
                    spacing: Theme.space.md
                    Column {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: qsTr("Java %1").arg(String(row.version))
                            color: row.chosen ? Theme.palette.accent : Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            font.weight: Font.DemiBold
                        }
                        Text {
                            width: parent.width
                            text: row.path
                            elide: Text.ElideMiddle
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.mono
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }
                    StatusBadge {
                        visible: row.recommended
                        tone: "success"
                        text: qsTr("Recommended")
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: !!root.java && root.java.detecting
                visible: running
            }
            Text {
                anchors.centerIn: parent
                width: parent.width - Theme.space.xxl * 2
                visible: list.count === 0 && !!root.java && !root.java.detecting
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("No Java found on this computer. Download one, or browse for a Java you installed yourself.")
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                lineHeight: Theme.type.label.lineHeight
            }
        }
    }

    footer: RowLayout {
        spacing: Theme.space.sm
        Button {
            Layout.leftMargin: Theme.space.lg
            Layout.bottomMargin: Theme.space.lg
            flat: true
            text: qsTr("Detect again")
            icon.source: Icons.url("refresh")
            enabled: !!root.java && !root.java.detecting
            onClicked: root.java.detect()
        }
        Item { Layout.fillWidth: true }
        Button {
            Layout.rightMargin: Theme.space.lg
            Layout.bottomMargin: Theme.space.lg
            text: qsTr("Close")
            onClicked: root.close()
        }
    }
}
