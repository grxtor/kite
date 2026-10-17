// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The amount of memory as a typed value ("6 GB", "512 MB"), the one field
 * both memory rows use (MemorySetting's maximum, MemoryStartSetting's
 * starting amount) so the two read as the same kind of setting.
 *
 * `value` is in MiB. Typing accepts "6 GB", "6g", "6144" or "6144 MB"; a
 * bare number under `bareGigabytesBelow` is taken as GB (nobody means 8 MB).
 * Whatever is typed snaps to `step` and is clamped to [from, to] before
 * `committed` fires, and Up/Down step it. Text that is no amount at all
 * puts the old value back.
 */
TextField {
    id: root

    // MiB. The owner binds this to what it stores and saves on `committed`.
    property int value: 0
    property int from: 512
    property int to: 65536
    property int step: 512
    readonly property int mibPerGib: 1024
    readonly property int bareGigabytesBelow: 128
    readonly property int fieldWidth: Theme.control.heightLg * 2

    signal committed(int mib)

    // "512 MB" below a gigabyte, "6 GB" or "6.5 GB" above.
    function describe(mib) {
        if (mib < mibPerGib)
            return qsTr("%1 MB").arg(mib)
        var gib = mib / mibPerGib
        var decimals = mib % mibPerGib === 0 ? 0 : (mib % (mibPerGib / 2) === 0 ? 1 : 2)
        return qsTr("%1 GB").arg(Number(gib).toLocaleString(Qt.locale(), "f", decimals))
    }

    // The amount the text stands for, snapped and clamped; -1 for no amount.
    function parse(input) {
        var found = /^\s*([0-9]+(?:[.,][0-9]+)?)\s*([a-zA-Z]*)/.exec(input)
        if (!found)
            return -1
        var amount = parseFloat(found[1].replace(",", "."))
        var unit = found[2].toLowerCase()
        var mib = unit.charAt(0) === "g" ? amount * mibPerGib
                : unit.charAt(0) === "m" ? amount
                : amount < bareGigabytesBelow ? amount * mibPerGib : amount
        return clamp(Math.round(mib / step) * step)
    }

    function clamp(mib) {
        return Math.max(Math.min(from, to), Math.min(to, mib))
    }

    function refresh() {
        text = describe(value)
    }

    function commit(mib) {
        if (mib >= 0 && mib !== value)
            committed(mib)
        refresh()
    }

    width: fieldWidth
    horizontalAlignment: TextInput.AlignRight
    selectByMouse: true
    onValueChanged: if (!activeFocus) refresh()
    onActiveFocusChanged: if (!activeFocus) refresh()
    Component.onCompleted: refresh()
    onEditingFinished: commit(parse(text))
    Keys.onUpPressed: commit(clamp(value + step))
    Keys.onDownPressed: commit(clamp(value - step))
}
