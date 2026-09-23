// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * Maximum memory as a slider over the RAM this machine actually has, with
 * the amount spelled out and a warning once it eats into what the system
 * needs itself. Saved when the slider is let go.
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
    readonly property int step: 256
    readonly property int floor: 512
    readonly property int ceiling: Math.max(floor + step, Math.floor(systemMiB / step) * step)
    readonly property int stored: root.source.number(root.key)
    // Past three quarters of the machine's RAM the game starts competing
    // with the OS and everything else that's open.
    readonly property bool tooMuch: slider.value > systemMiB * 0.75

    property string hint
    description: tooMuch ? qsTr("That is most of this computer's %1 of memory; the system and other programs may slow down.").arg(describe(systemMiB))
                         : hint

    function describe(mib) {
        return mib >= 1024 ? qsTr("%1 GiB").arg(Number(mib / 1024).toLocaleString(Qt.locale(), "f", mib % 1024 === 0 ? 0 : 1))
                           : qsTr("%1 MiB").arg(mib)
    }

    Column {
        width: sliderRow.width
        spacing: Theme.space.sm

        Row {
            id: sliderRow
            spacing: Theme.space.md

            Slider {
                id: slider
                anchors.verticalCenter: parent.verticalCenter
                width: 240
                from: root.floor
                to: root.ceiling
                stepSize: root.step
                snapMode: Slider.SnapAlways
                value: root.stored
                Accessible.name: root.label
                onPressedChanged: {
                    if (pressed)
                        return
                    var max = Math.round(value)
                    root.source.setValue(root.key, max)
                    if (root.source.number(root.minKey) > max)
                        root.source.setValue(root.minKey, max)
                    value = Qt.binding(() => root.stored)
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 72
                horizontalAlignment: Text.AlignRight
                text: root.describe(Math.round(slider.value))
                color: root.tooMuch ? Theme.palette.warning : Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                font.weight: Font.DemiBold
            }
        }

        // How much of the machine's own RAM that allocation is, at a
        // glance -- the slider alone says "2.5 GiB" but not "of how much".
        Column {
            width: parent.width
            spacing: Theme.space.xxs

            Rectangle {
                width: parent.width
                height: Theme.space.xs
                radius: Theme.radius.pill
                color: Theme.palette.surfaceSunken

                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1, slider.value / root.systemMiB))
                    height: parent.height
                    radius: parent.radius
                    color: root.tooMuch ? Theme.palette.warning : Theme.palette.accent
                    Behavior on width {
                        NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
                    }
                }

                // Past this line the game starts competing with the OS for
                // memory -- the same threshold `tooMuch` itself uses.
                Rectangle {
                    x: parent.width * 0.75 - width / 2
                    width: 2
                    height: parent.height
                    color: Theme.palette.surface
                    opacity: 0.7
                }
            }

            Text {
                text: qsTr("%1 of %2 system memory").arg(root.describe(Math.round(slider.value))).arg(root.describe(root.systemMiB))
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }
    }
}
