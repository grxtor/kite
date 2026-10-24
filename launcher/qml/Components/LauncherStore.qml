// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton

import QtQuick

/*
 * Where the launcher-wide settings screens find their C++ side, the way
 * JavaStore does for Java: Main.qml sets these once at startup, so a
 * Settings section can use them without every page between passing them
 * down. Each stays null until Main.qml sets it, and the rows that need one
 * hide themselves meanwhile.
 */
QtObject {
    // FolderSettingsController: moving the data folders, extra instance
    // folders.
    property var folders: null
    // UpdateSettingsController: automatic checks, interval, "Check now".
    // `available` says whether this build has an updater at all.
    property var updates: null
    // PlayTimeSummary: total play time across every instance, in seconds.
    property var playTime: null
    // CatPackList: the cat pictures (packs, sourceFor(id)).
    property var catPacks: null
    // QmlShell::iconRevision: bumps when any instance icon's pixels may have
    // changed; Icons.instanceIcon() folds it into the image URL.
    property int iconRevision: 0
    // QmlShell, for iconFillsTile(key) -- see Icons.fillsTile().
    property var icons: null
}
