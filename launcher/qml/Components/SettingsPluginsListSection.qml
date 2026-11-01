// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The "Installed plugins" section of SettingsPage.qml: every plugin Kite found,
 * whether it loaded (or why not), its signature, and a switch to turn it on or
 * off from the next start. Hidden in a build without the plugin host, where
 * QmlShell.pluginList() is null.
 */
SettingsScroll {
    id: root

    // QmlShell.pluginList() (PluginListModel), or null.
    property var pluginList: null
    readonly property bool shown: !!root.pluginList
    readonly property int count: root.pluginList ? root.pluginList.count : 0

    title: qsTr("Installed plugins")
    description: qsTr("Plugins add features to Kite. Turning one on or off takes effect the next time Kite starts.")

    Copy {
        width: parent.width
        visible: root.count === 0
        text: qsTr("No plugins are installed.")
        wrapMode: Text.Wrap
        tone: "secondary"
    }

    SettingsGroup {
        width: parent.width
        visible: root.count > 0
        description: root.pluginList && root.pluginList.restartNeeded
                     ? qsTr("Restart Kite to apply your changes.") : ""

        Repeater {
            model: root.pluginList
            delegate: SettingRow {
                id: pluginRow
                required property int index
                // Roles through `model`: several (enabled, description) are
                // also SettingRow properties of their own.
                required property var model

                label: model.version.length > 0 ? qsTr("%1 %2").arg(model.name).arg(model.version) : model.name
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
                            // A click assigns `checked` and drops the binding; put it back so the
                            // switch shows the plugin's state.
                            checked = Qt.binding(() => pluginRow.model.enabled)
                        }
                    }
                }
            }
        }
    }
}
