// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The "Installed plugins" section of SettingsPage.qml: every plugin MeshMC
 * found, whether it loaded (or why not), its signature, and a switch to
 * turn it on or off from the next start -- the widget PluginsDialog. Not
 * the same as the "Plugins" section (SettingsPluginsSection.qml), which
 * shows the settings plugins add. Hidden in a build without the plugin
 * host, where QmlShell.pluginList() is null.
 */
SettingsScroll {
    id: root

    // QmlShell.pluginList() (PluginListModel), or null.
    property var pluginList: null
    // Read by SettingsPage.qml's section list.
    readonly property bool shown: !!root.pluginList
    readonly property int count: root.pluginList ? root.pluginList.count : 0

    title: qsTr("Installed plugins")
    description: qsTr("Plugins add features to MeshMC. Turning one on or off takes effect the next time MeshMC starts.")

    Text {
        width: parent.width
        visible: root.count === 0
        text: qsTr("No plugins are installed.")
        wrapMode: Text.Wrap
        color: Theme.palette.textSecondary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
    }

    SettingsGroup {
        width: parent.width
        visible: root.count > 0
        description: root.pluginList && root.pluginList.restartNeeded
                     ? qsTr("Restart MeshMC to apply your changes.") : ""

        Repeater {
            model: root.pluginList
            delegate: SettingRow {
                id: pluginRow
                required property int index
                // Roles through `model`: several (enabled, description) are
                // also SettingRow properties of their own.
                required property var model

                label: model.version.length > 0 ? qsTr("%1 %2").arg(model.name).arg(model.version) : model.name
                // What it does, then who made it and under which licence,
                // what it needs, and why it did not load when it did not.
                description: [model.description,
                              model.author.length > 0 ? qsTr("By %1").arg(model.author) : "",
                              model.license.length > 0 ? qsTr("Licence: %1").arg(model.license) : "",
                              model.dependencies.length > 0 ? qsTr("Needs %1").arg(model.dependencies) : "",
                              model.statusDetail].filter(s => s.length > 0).join("\n")

                Row {
                    spacing: Theme.space.sm
                    Tag {
                        anchors.verticalCenter: parent.verticalCenter
                        text: pluginRow.model.status
                    }
                    Tag {
                        anchors.verticalCenter: parent.verticalCenter
                        text: pluginRow.model.signature
                    }
                    IconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: pluginRow.model.sourceUrl.length > 0
                        iconName: "external-link"
                        tip: qsTr("Source code")
                        onClicked: Qt.openUrlExternally(pluginRow.model.sourceUrl)
                    }
                    Switch {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: pluginRow.model.enabled
                        enabled: pluginRow.model.canToggle
                        Accessible.name: qsTr("Use %1").arg(pluginRow.model.name)
                        onToggled: {
                            root.pluginList.setEnabled(pluginRow.index, checked)
                            // A click assigns `checked` and drops the binding; put it
                            // back so the switch shows the plugin's state, not the
                            // click, whatever the plugin made of it.
                            checked = Qt.binding(() => pluginRow.model.enabled)
                        }
                    }
                }
            }
        }
    }
}
