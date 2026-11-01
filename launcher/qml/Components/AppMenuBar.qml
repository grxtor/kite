// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * The macOS menu bar for the QML shell: new instance and import, Settings, the Help links and "Check for updates"
 * where this build can update itself. Main.qml uses it as the window's menuBar only on macOS with the native menu
 * bar setting on; every entry is a signal for Main.qml, which turns the bar off (`enabled`) while first-run setup
 * is on screen.
 */
MenuBar {
    id: root

    // AboutInfo, for the Help links; or null.
    property var about: null
    property bool updaterAvailable: false

    // mode: "create" or "import".
    signal newInstanceRequested(string mode)
    signal navigateRequested(string target)
    signal checkForUpdatesRequested()

    Menu {
        title: qsTr("File")
        enabled: root.enabled
        MenuItem {
            text: qsTr("New instance…")
            onTriggered: root.newInstanceRequested("create")
        }
        MenuItem {
            text: qsTr("Import instance…")
            onTriggered: root.newInstanceRequested("import")
        }
        MenuSeparator {}
        MenuItem {
            // Moved into the application menu by macOS, as "Settings…".
            text: qsTr("Settings…")
            onTriggered: root.navigateRequested("settings")
        }
    }

    Menu {
        id: helpMenu
        title: qsTr("Help")
        enabled: root.enabled

        Instantiator {
            model: root.about ? root.about.helpLinks : []
            delegate: MenuItem {
                required property var modelData
                text: modelData.label
                onTriggered: Qt.openUrlExternally(modelData.url)
            }
            onObjectAdded: (index, object) => helpMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => helpMenu.removeItem(object)
        }
        MenuSeparator {}
        MenuEntry {
            visible: root.updaterAvailable
            text: qsTr("Check for updates…")
            onTriggered: root.checkForUpdatesRequested()
        }
        MenuItem {
            // Moved into the application menu by macOS.
            text: qsTr("About Kite")
            onTriggered: root.navigateRequested("settings/about")
        }
    }
}
