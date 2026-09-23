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
    // "amethyst", "ember" or "diamond".
    property string scheme: ThemeService.scheme
    onSchemeChanged: ThemeService.scheme = scheme
    function previewPalette(name) { return ThemeService.previewPalette(name) }

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

    readonly property QtObject opacity: QtObject {
        // A disabled control keeps its shape and fades, rather than turning
        // into a different-looking control.
        readonly property real disabled: 0.45
    }

    // Text and shading laid over a screenshot or other artwork. The art is
    // what it is in either theme, so these do not follow the palette: light
    // text over a dark fade reads on any picture, in dark and light mode.
    readonly property QtObject media: QtObject {
        readonly property color text: "#FFFFFF"
        readonly property color textSecondary: Qt.rgba(1, 1, 1, 0.80)
        readonly property color scrim: Qt.rgba(0.02, 0.03, 0.05, 0.86)
        readonly property color chip: Qt.rgba(0, 0, 0, 0.38)
        readonly property color chipBorder: Qt.rgba(1, 1, 1, 0.12)
    }

    readonly property QtObject motion: QtObject {
        readonly property int fast: 120
        readonly property int normal: 180
        readonly property int slow: 260
        readonly property int easing: Easing.OutCubic
    }

    readonly property QtObject font: QtObject {
        // The design fonts when installed; otherwise the platform's own UI
        // and monospace fonts, never Qt's generic fallback (which on macOS
        // turns "JetBrains Mono" into a proportional font).
        readonly property string family: Qt.fontFamilies().indexOf("Inter") >= 0
                                         ? "Inter" : Qt.application.font.family
        readonly property string mono: Qt.fontFamilies().indexOf("JetBrains Mono") >= 0
                                       ? "JetBrains Mono"
                                       : Qt.platform.os === "osx" ? "Menlo"
                                       : Qt.platform.os === "windows" ? "Consolas"
                                       : "DejaVu Sans Mono"
    }

    readonly property QtObject type: QtObject {
        readonly property QtObject caption: QtObject {
            readonly property int pixelSize: 12
            readonly property int weight: Font.Normal
            readonly property real lineHeight: 1.33
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        // Small uppercase section labels ("RECENT", "CONTINUE PLAYING").
        readonly property QtObject overline: QtObject {
            readonly property int pixelSize: 11
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.30
            readonly property real letterSpacing: 0.8
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject label: QtObject {
            readonly property int pixelSize: 13
            readonly property int weight: Font.Medium
            readonly property real lineHeight: 1.30
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject body: QtObject {
            readonly property int pixelSize: 14
            readonly property int weight: Font.Normal
            readonly property real lineHeight: 1.45
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject bodyStrong: QtObject {
            readonly property int pixelSize: 14
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.45
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject title: QtObject {
            readonly property int pixelSize: 16
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.35
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject heading: QtObject {
            readonly property int pixelSize: 20
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.25
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject display: QtObject {
            readonly property int pixelSize: 28
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.15
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
    }
}
