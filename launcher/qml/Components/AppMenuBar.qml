// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * The macOS menu bar at the top of the screen for the QML shell -- the
 * useful part of the widget MacMenuBar: new instance and import, Settings
 * (which macOS files under the application menu by its name, as it does
 * "About"), the Help links, and "Check for updates" where this build can
 * update itself. Main.qml makes it the window's menuBar only on macOS with
 * the native menu bar setting on (MacNativeMenuBar, the widget bar's own
 * switch), where Qt shows it natively instead of inside the window.
 *
 * It starts nothing itself: every entry is a signal for Main.qml to act on.
 */
MenuBar {
    id: root

    // QmlShell.about (AboutInfo), for the Help links; or null.
    property var about: null
    property bool updaterAvailable: false

    // mode: "create" or "import" -- Main.qml's openNewInstance().
    signal newInstanceRequested(string mode)
    // A Main.qml navigate() path.
    signal navigateRequested(string target)
    signal checkForUpdatesRequested()

    Menu {
        title: qsTr("File")
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
        MenuItem {
            visible: root.updaterAvailable
            height: visible ? implicitHeight : 0
            text: qsTr("Check for updates…")
            onTriggered: root.checkForUpdatesRequested()
        }
        MenuItem {
            // Moved into the application menu by macOS.
            text: qsTr("About MeshMC")
            onTriggered: root.navigateRequested("settings/about")
        }
    }
}
