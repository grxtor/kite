// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * Starting memory: the amount the game reserves at launch, as the same typed
 * value MemorySetting shows for the maximum (MemoryField), so the two memory
 * rows read as one kind of setting. Saved when edited, never above the
 * maximum, which is read from `maxKey` in the same source.
 */
SettingRow {
    id: root

    property string key: "MinMemAlloc"
    property string maxKey: "MaxMemAlloc"
    // Where the values live; the launcher-wide settings unless told otherwise.
    property var source: SettingsStore
    readonly property int step: 512
    readonly property int floor: 512
    readonly property int stored: root.source.number(root.key)
    readonly property int limit: Math.max(floor, root.source.number(root.maxKey))

    MemoryField {
        value: root.stored
        from: root.floor
        to: root.limit
        step: root.step
        Accessible.name: root.label
        onCommitted: (mib) => root.source.setValue(root.key, mib)
    }
}
