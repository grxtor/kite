// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * Three sample log lines in the console font and size from the settings, so
 * a change shows before the game is ever started. The colours are the ones
 * the log uses for errors, warnings and plain messages.
 */
Item {
    id: root

    // Same hairline rule as SettingRow, so SettingsGroup treats it as a row.
    property bool showDivider: false
    readonly property string family: SettingsStore.string("ConsoleFont")
    // Points, like the console itself (QFont(family, size)).
    readonly property int pointSize: Math.max(6, SettingsStore.number("ConsoleFontSize"))

    width: parent ? parent.width : implicitWidth
    implicitHeight: sample.height + Theme.space.lg * 2

    Rectangle {
        visible: root.showDivider
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.space.lg
        anchors.rightMargin: Theme.space.lg
        height: 1
        color: Theme.palette.divider
    }

    Rectangle {
        id: sample
        x: Theme.space.lg
        y: Theme.space.lg
        width: parent.width - Theme.space.lg * 2
        height: lines.height + Theme.space.md * 2
        radius: Theme.radius.sm
        color: Theme.palette.surfaceSunken
        border.width: 1
        border.color: Theme.palette.border

        Column {
            id: lines
            x: Theme.space.md
            y: Theme.space.md
            width: parent.width - Theme.space.md * 2
            spacing: 2

            Repeater {
                model: [
                    { text: qsTr("[Something/ERROR] A spooky error!"), color: Theme.palette.danger },
                    { text: qsTr("[Something/WARN] A not so spooky warning."), color: Theme.palette.warning },
                    { text: qsTr("[Test/INFO] A harmless message…"), color: Theme.palette.textSecondary }
                ]
                delegate: Text {
                    required property var modelData
                    width: lines.width
                    text: modelData.text
                    elide: Text.ElideRight
                    color: modelData.color
                    font.family: root.family
                    font.pointSize: root.pointSize
                }
            }
        }
    }
}
