// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

pragma Singleton
import QtQuick

/*
 * The single source of design tokens for the QML user interface: every size,
 * colour and timing a component asks for comes from here, never a literal
 * baked into that component.
 *
 * palette/dark/mode forward straight to ThemeService -- this module's other
 * QML singleton, registered alongside this file in C++ -- so the palette-swap
 * logic lives in exactly one place and this file only has to relay it.
 * Everything below that (space, radius, control, icon, motion, font, type) is
 * a plain constant this file alone owns.
 */
QtObject {
    // ThemeService is this module's own C++ singleton; referring to it by
    // name is enough; the module makes it visible without an import, same as
    // any other type declared in it.
    readonly property var palette: ThemeService.palette
    readonly property bool dark: ThemeService.dark
    property string mode: ThemeService.mode
    onModeChanged: ThemeService.mode = mode

    readonly property QtObject space: QtObject {
        readonly property int xxs: 2
        readonly property int xs: 4
        readonly property int sm: 8
        readonly property int md: 12
        readonly property int lg: 16
        readonly property int xl: 24
        readonly property int xxl: 32
    }

    readonly property QtObject radius: QtObject {
        readonly property int sm: 4
        readonly property int md: 8
        readonly property int lg: 12
        readonly property int xl: 16
        readonly property int pill: 999
    }

    readonly property QtObject control: QtObject {
        readonly property int heightSm: 28
        readonly property int height: 36
        readonly property int heightLg: 44
    }

    readonly property QtObject icon: QtObject {
        readonly property int sm: 16
        readonly property int md: 20
        readonly property int lg: 24
    }

    readonly property QtObject motion: QtObject {
        readonly property int fast: 120
        readonly property int normal: 180
        readonly property int slow: 260
        readonly property int easing: Easing.OutCubic
    }

    readonly property QtObject font: QtObject {
        // Qt falls back to a system font when these are not installed; the
        // actual font files are bundled in a later phase.
        readonly property string family: "Inter"
        readonly property string mono: "JetBrains Mono"
    }

    readonly property QtObject type: QtObject {
        readonly property QtObject caption: QtObject {
            readonly property int pixelSize: 12
            readonly property int weight: Font.Normal
            readonly property real lineHeight: 1.33
        }
        readonly property QtObject label: QtObject {
            readonly property int pixelSize: 13
            readonly property int weight: Font.Medium
            readonly property real lineHeight: 1.30
        }
        readonly property QtObject body: QtObject {
            readonly property int pixelSize: 14
            readonly property int weight: Font.Normal
            readonly property real lineHeight: 1.45
        }
        readonly property QtObject bodyStrong: QtObject {
            readonly property int pixelSize: 14
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.45
        }
        readonly property QtObject title: QtObject {
            readonly property int pixelSize: 16
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.35
        }
        readonly property QtObject heading: QtObject {
            readonly property int pixelSize: 20
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.25
        }
        readonly property QtObject display: QtObject {
            readonly property int pixelSize: 28
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.15
        }
    }
}
