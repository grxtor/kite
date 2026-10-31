// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton

import QtQuick

/*
 * Where the Java settings UI finds its C++ side: Main.qml sets both once at
 * startup. Both stay null until then; every Java row treats that as "plain text
 * fields only" and hides what needs them.
 */
QtObject {
    // JavaController: detection, Test, downloaded runtimes, vendors.
    property var controller: null
    // JavaDownloadController: the Download Java flow.
    property var download: null

    // Asks Main.qml to open another page, e.g. "settings/java"; forwarded to its navigate().
    signal navigateRequested(string target)
}
