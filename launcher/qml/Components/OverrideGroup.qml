// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * A group of instance settings that follow the launcher-wide ones until the
 * instance asks for its own. The first row is that choice; the rows the
 * caller adds below show the value in effect either way and should be
 * `enabled: group.overriding`, so they only become editable once the
 * instance overrides them. Turning the override off drops the instance's
 * values, so it follows the launcher again -- the classic page did the same
 * on save.
 */
SettingsGroup {
    id: root

    property var source
    property string gateKey
    // The keys this group overrides, reset when the override is turned off.
    property var keys: []
    property string gateLabel: qsTr("Use custom settings for this instance")
    readonly property bool overriding: gate.checked

    SettingSwitch {
        id: gate
        source: root.source
        key: root.gateKey
        label: root.gateLabel
        onSwitched: (on) => {
            if (on)
                return
            for (var i = 0; i < root.keys.length; ++i)
                root.source.reset(root.keys[i])
        }
    }
}
