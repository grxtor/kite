// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

pragma Singleton

import QtQuick

/*
 * Where the Java settings UI finds its C++ side, the way SettingsStore holds
 * the settings adapter: Main.qml sets both once at startup, so the Java
 * rows work on any page (Settings, an instance's settings) without every
 * page between Main.qml and them passing the controllers down.
 *
 * Both stay null until Main.qml sets them; every Java row treats that as
 * "plain text fields only" and hides what needs them.
 */
QtObject {
    // JavaController: detection, Test, downloaded runtimes, vendors.
    property var controller: null
    // JavaDownloadController: the Download Java flow.
    property var download: null

    // Ask Main.qml to open another page, e.g. "settings/java" from an
    // instance's settings. Main.qml forwards it to its navigate().
    signal navigateRequested(string target)
}
