// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The game's output, live. Follows the newest line only while "Follow" is
 * on -- scrolling up to read something turns it off, instead of the view
 * yanking the reader back down on every new line.
 */
Item {
    id: root

    // InstanceLogBridge: model (LogModel: line, level), hasLog, clear(),
    // text().
    property var log: null
    readonly property var model: log ? log.model : null

    // MessageLevel::Enum
    function levelColor(level) {
        switch (level) {
        case 2: case 8: case 9: return Theme.palette.danger   // StdErr, Error, Fatal
        case 7: return Theme.palette.warning                  // Warning
        case 3: return Theme.palette.accent                   // MeshMC
        case 4: return Theme.palette.textTertiary             // Debug
        default: return Theme.palette.textSecondary
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            Switch {
                id: follow
                checked: true
                text: qsTr("Follow")
            }
            Item { Layout.fillWidth: true }
            Button {
                enabled: !!root.model
                text: qsTr("Copy all")
                icon.source: Icons.url("copy")
                onClicked: {
                    clipboardHelper.text = root.log.text()
                    clipboardHelper.selectAll()
                    clipboardHelper.copy()
                }
            }
            Button {
                enabled: !!root.model
                flat: true
                text: qsTr("Clear")
                icon.source: Icons.url("x")
                onClicked: root.log.clear()
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
                id: lines
                anchors.fill: parent
                anchors.margins: Theme.space.sm
                clip: true
                model: root.model
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true
                ScrollBar.vertical: ScrollBar {}

                onCountChanged: if (follow.checked) positionViewAtEnd()
                onMovementStarted: follow.checked = false
                onAtYEndChanged: if (atYEnd && moving) follow.checked = true

                delegate: Text {
                    required property string line
                    required property int level
                    width: lines.width - Theme.space.md
                    text: line
                    wrapMode: Text.WrapAnywhere
                    color: root.levelColor(level)
                    font.family: Theme.font.mono
                    font.pixelSize: Theme.type.label.pixelSize - 1
                    textFormat: Text.PlainText
                }
            }

            EmptyState {
                anchors.centerIn: parent
                visible: !root.model || lines.count === 0
                title: qsTr("No log yet")
                body: qsTr("Start the instance and its output appears here, live.")
                MeshIcon { iconName: "terminal"; size: 40; color: Theme.palette.textTertiary }
            }
        }
    }

    // QML has no clipboard API of its own; an invisible text edit does.
    TextEdit {
        id: clipboardHelper
        visible: false
    }
}
