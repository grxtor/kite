// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The JVM arguments field. Memory has its own settings, so arguments that
 * set it by hand (-Xmx, -Xms, PermSize) are called out right under the
 * field while you type, the way the old page warned on save.
 */
SettingRow {
    id: root

    property string key: "JvmArgs"
    property var source: SettingsStore
    property string placeholder: "-XX:+UseG1GC"
    readonly property string warning: JavaStore.controller ? JavaStore.controller.argsWarning(field.text) : ""

    wide: true

    ColumnLayout {
        width: parent.width
        spacing: Theme.space.sm

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: root.placeholder
            font.family: Theme.font.mono
            selectByMouse: true
            Accessible.name: root.label

            readonly property string stored: root.source.string(root.key)
            onStoredChanged: if (!activeFocus) { text = stored; cursorPosition = 0 }
            Component.onCompleted: { text = stored; cursorPosition = 0 }
            onWidthChanged: if (!activeFocus) cursorPosition = 0
            onActiveFocusChanged: if (!activeFocus) cursorPosition = 0
            onEditingFinished: if (text !== stored) root.source.setValue(root.key, text)
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.warning.length > 0
            spacing: Theme.space.sm

            MeshIcon {
                Layout.alignment: Qt.AlignTop
                iconName: "alert-triangle"
                size: Theme.icon.sm
                color: Theme.palette.warning
            }
            Text {
                Layout.fillWidth: true
                text: root.warning
                wrapMode: Text.Wrap
                color: Theme.palette.warning
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                lineHeight: Theme.type.label.lineHeight
            }
        }
    }
}
