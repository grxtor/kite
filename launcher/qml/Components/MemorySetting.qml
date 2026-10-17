// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * Maximum memory: one slider whose track runs from the least the game can
 * live with to the RAM this machine has, and a typed value beside it
 * (MemoryField). A single hint line under the label says how much is
 * installed and what suits most modpacks, and turns into a warning once
 * the amount eats into what the system needs itself.
 * The slider saves when let go, the field when edited.
 *
 * The minimum can never end up above the maximum: lowering the maximum
 * below it pulls the minimum down too -- the widget page swapped the two
 * instead, which silently turned the user's "max" into the min.
 */
SettingRow {
    id: root

    property string key: "MaxMemAlloc"
    // Where the value lives; the launcher-wide settings unless told otherwise.
    property var source: SettingsStore
    property string minKey: "MinMemAlloc"
    property int systemMiB: 8192
    // Replaces the default hint line when a screen has something shorter to say.
    property string hint
    readonly property int step: 512
    readonly property int floor: 512
    readonly property int ceiling: Math.max(floor + step, Math.floor(systemMiB / step) * step)
    readonly property int stored: root.source.number(root.key)
    // What the row shows right now: the knob's position while it is being
    // dragged, what is saved otherwise.
    readonly property int shown: slider.pressed ? Math.round(slider.value) : stored
    // Past three quarters of the machine's RAM the game starts competing
    // with the OS and everything else that's open.
    readonly property real comfortableShare: 0.75
    readonly property bool tooMuch: shown > systemMiB * comfortableShare
    // The band that suits most modpacks: up to 8 GB, never past the
    // comfortable share, and from half of that (4 GB on a 16 GB machine).
    readonly property int suitsHigh: Math.max(floor, Math.min(8192, Math.floor(ceiling * comfortableShare / step) * step))
    readonly property int suitsLow: Math.max(floor, Math.min(4096, Math.floor(suitsHigh / 2 / step) * step))

    // The slider's track and the typed amount sit side by side in a fixed
    // width, so the label column and the field line up with the other rows.
    readonly property int sliderWidth: 240

    function gib(mib) {
        return Number(mib / 1024).toLocaleString(Qt.locale(), "f", mib % 1024 === 0 ? 0 : 1)
    }

    function save(mib) {
        var max = Math.max(floor, Math.min(ceiling, mib))
        root.source.setValue(root.key, max)
        if (root.source.number(root.minKey) > max)
            root.source.setValue(root.minKey, max)
    }

    description: tooMuch ? qsTr("That is most of this computer's %1 GB; the system and other programs may slow down.").arg(gib(systemMiB))
               : hint.length > 0 ? hint
               : qsTr("%1 GB installed. %2–%3 GB suits most modpacks.").arg(gib(systemMiB)).arg(gib(suitsLow)).arg(gib(suitsHigh))

    Row {
        spacing: Theme.space.md

        // A plain track, like Modrinth App's and CurseForge's memory
        // sliders: the suggested range is said in the hint line. A tinted
        // band behind the knob read as a rendering glitch, not a range.
        Slider {
            id: slider
            anchors.verticalCenter: parent.verticalCenter
            width: root.sliderWidth
            // No side padding: the knob's edge lines up with the label
            // above when the row stacks, and the field to its right.
            leftPadding: 0
            rightPadding: 0
            from: root.floor
            to: root.ceiling
            stepSize: root.step
            snapMode: Slider.SnapAlways
            value: root.stored
            Accessible.name: root.label
            onPressedChanged: {
                if (pressed)
                    return
                root.save(Math.round(value))
                value = Qt.binding(() => root.stored)
            }
        }

        MemoryField {
            anchors.verticalCenter: parent.verticalCenter
            value: root.shown
            from: root.floor
            to: root.ceiling
            step: root.step
            color: root.tooMuch ? Theme.palette.warning : Theme.palette.textPrimary
            Accessible.name: root.label
            onCommitted: (mib) => root.save(mib)
        }
    }
}
