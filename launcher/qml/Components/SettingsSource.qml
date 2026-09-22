// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * What setting rows read and write through: one SettingsAdapter (the
 * launcher's, or one instance's). `revision` moves on every change, so a
 * binding that reads value() through here re-evaluates when anything --
 * this page, the widget dialog, a plugin -- changes a setting.
 */
QtObject {
    id: store

    property var adapter: null
    property int revision: 0

    readonly property Connections watcher: Connections {
        target: store.adapter
        ignoreUnknownSignals: true
        function onValueChanged() { store.revision++ }
    }

    function value(key) {
        // Reading revision makes every caller's binding depend on it.
        return store.revision >= 0 && store.adapter ? store.adapter.value(key) : undefined
    }

    // Settings read back from the config file can be strings ("false" is
    // truthy in JS), so rows go through these instead of value() directly.
    function bool(key) {
        var v = store.value(key)
        return v === true || v === "true" || v === 1 || v === "1"
    }

    function number(key) {
        var n = Number(store.value(key))
        return isNaN(n) ? 0 : n
    }

    function string(key) {
        var v = store.value(key)
        return v === undefined || v === null ? "" : String(v)
    }

    function setValue(key, value) {
        if (store.adapter)
            store.adapter.setValue(key, value)
    }

    function reset(key) {
        if (store.adapter)
            store.adapter.reset(key)
    }

    function isDefault(key) {
        return store.revision >= 0 && store.adapter
                ? String(store.adapter.value(key)) === String(store.adapter.defaultValue(key))
                : true
    }
}
