// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * A group of instance settings that follow the launcher-wide ones until the
 * instance asks for its own. The first row is that choice; rows added below
 * should be `enabled: group.overriding`. Turning the override off drops the
 * instance's values.
 */
SettingsGroup {
    id: root

    property var source
    property string gateKey
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
