// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * How MeshMC itself reaches the internet -- the QML equivalent of the
 * classic ProxyPage. Every field saves as it is set, same as the rest of
 * this window, so there is no separate Apply button; instead, whenever one
 * of the five proxy settings changes, applyProxySettings() below pushes the
 * whole configuration to the network layer immediately, the same effect
 * ProxyPage::apply() had when the classic dialog's OK button was pressed.
 */
SettingsScroll {
    id: root

    title: qsTr("Proxy")
    description: qsTr("How MeshMC reaches the internet. Only MeshMC's own downloads use it; Minecraft ignores proxy settings.")

    readonly property string proxyType: SettingsStore.string("ProxyType")
    readonly property bool needsAddress: root.proxyType === "SOCKS5" || root.proxyType === "HTTP"

    // A brief "Applied." confirmation after any push to the network layer,
    // since there is no Apply button here for it to have been the result of.
    property bool justApplied: false
    Timer {
        id: appliedTimer
        interval: 1600
        onTriggered: root.justApplied = false
    }

    Connections {
        target: SettingsStore.adapter
        function onValueChanged(id) {
            switch (id) {
                case "ProxyType":
                case "ProxyAddr":
                case "ProxyPort":
                case "ProxyUser":
                case "ProxyPass":
                    if (SettingsStore.adapter) {
                        SettingsStore.adapter.applyProxySettings(
                            SettingsStore.string("ProxyType"),
                            SettingsStore.string("ProxyAddr"),
                            SettingsStore.number("ProxyPort"),
                            SettingsStore.string("ProxyUser"),
                            SettingsStore.string("ProxyPass"))
                        root.justApplied = true
                        appliedTimer.restart()
                    }
                    break
            }
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Type")
        description: root.justApplied ? qsTr("Applied.")
                                       : qsTr("Changes apply immediately; there is no separate Apply button.")
        SettingChoice {
            key: "ProxyType"
            label: qsTr("Proxy type")
            options: [
                { value: "None", label: qsTr("None") },
                { value: "Default", label: qsTr("System") },
                { value: "SOCKS5", label: qsTr("SOCKS5") },
                { value: "HTTP", label: qsTr("HTTP") }
            ]
        }
    }

    SettingsGroup {
        width: parent.width
        visible: root.needsAddress
        title: qsTr("Address and port")
        SettingText {
            key: "ProxyAddr"
            label: qsTr("Host")
            placeholder: "127.0.0.1"
        }
        SettingNumber {
            key: "ProxyPort"
            label: qsTr("Port")
            from: 1
            to: 65535
        }
    }

    SettingsGroup {
        width: parent.width
        visible: root.needsAddress
        title: qsTr("Authentication")
        description: qsTr("Stored in plain text in MeshMC's configuration file.")
        SettingText {
            key: "ProxyUser"
            label: qsTr("Username")
        }
        SettingText {
            key: "ProxyPass"
            label: qsTr("Password")
            secret: true
        }
    }
}
