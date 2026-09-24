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
    id: root

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

    // Contract: `pill` is reserved for genuine toggle/chip affordances
    // (a switch track, a filter chip) -- never for the Play verb or an
    // instance's own identity (cover corners, the icon picker, PlayButton's
    // wide hero shape), which are capped at `md`/`lg` instead. A blocky game
    // launcher asking to look professional doesn't have to be as rounded as
    // a fintech app; see design-plan.md §1/§2.7.
    readonly property QtObject radius: QtObject {
        readonly property int xs: 2
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

    /* Motion contract: a hover/press transition changes exactly one visual
     * property over `fast` -- colour, or a single small scale/translate,
     * never both, and never an overshoot easing (Easing.OutBack) on a
     * hover-triggered transform. An idle animation loop (Animation.Infinite)
     * is only ever gated on a real busy/live state property -- the way
     * RecentItem.qml/InstanceListRow.qml gate their pulse on `isRunning`,
     * PlayDock.qml on `dockRunning`, and Skeleton.qml on its own visibility
     * while loading -- never bound to "the page happens to be idle" alone.
     * See design-plan.md §2.2-2.4.
     *
     * Selection grammar: one documented look per control class, not one
     * grammar total and not accidental variety --
     *   - rail/list nav (SidebarNav, and anything reusing its mechanism,
     *     e.g. Settings' section list): a sliding surfaceRaised pill plus a
     *     3px accent bar pinned to the leading edge.
     *   - tab strip (TabStrip): an accent underline beneath the active tab.
     *   - swatch/tile picker (PalettePicker and similar): an accent ring
     *     around the selected tile.
     *   - segmented control (SegmentedControl): a raised fill on the
     *     selected segment, no accent.
     * Every instance of a class uses its class's grammar; a control never
     * falls back to a bare colour-only "selected" look. See design-plan.md
     * §2.6.
     */
    readonly property QtObject motion: QtObject {
        readonly property int fast: 120
        readonly property int normal: 180
        readonly property int slow: 260
        readonly property int easing: Easing.OutCubic
    }

    // Inter 4.1 static TTFs, bundled as MeshMC.Theme module resources
    // (Theme/CMakeLists.txt) rather than probed for as a system font: the
    // whole type scale below is only true on a machine that happens to
    // already have Inter installed, unless the app ships its own copy.
    // Loaded once, here, since every consumer reaches Inter through
    // Theme.font.family rather than importing a FontLoader of its own.
    readonly property bool _interReady: _interRegular.status === FontLoader.Ready
                                        && _interMedium.status === FontLoader.Ready
                                        && _interSemiBold.status === FontLoader.Ready
                                        && _interBold.status === FontLoader.Ready
    property FontLoader _interRegular: FontLoader {
        source: "qrc:/qt/qml/MeshMC/Theme/fonts/inter/Inter-Regular.ttf"
    }
    property FontLoader _interMedium: FontLoader {
        source: "qrc:/qt/qml/MeshMC/Theme/fonts/inter/Inter-Medium.ttf"
    }
    property FontLoader _interSemiBold: FontLoader {
        source: "qrc:/qt/qml/MeshMC/Theme/fonts/inter/Inter-SemiBold.ttf"
    }
    property FontLoader _interBold: FontLoader {
        source: "qrc:/qt/qml/MeshMC/Theme/fonts/inter/Inter-Bold.ttf"
    }
    Component.onCompleted: {
        if (!root._interReady)
            console.warn("Theme: bundled Inter failed to load, falling back to",
                          Qt.application.font.family)
    }

    readonly property QtObject font: QtObject {
        // Unconditional now that Inter is bundled (see the FontLoaders
        // above) -- the platform font is only ever used if loading the
        // bundled resource itself failed, not merely because Inter isn't
        // separately installed system-wide.
        readonly property string family: root._interReady
                                         ? "Inter" : Qt.application.font.family
        readonly property string mono: Qt.fontFamilies().indexOf("JetBrains Mono") >= 0
                                       ? "JetBrains Mono"
                                       : Qt.platform.os === "osx" ? "Menlo"
                                       : Qt.platform.os === "windows" ? "Consolas"
                                       : "DejaVu Sans Mono"
    }

    // Contract: heading (20) and display (28) are page-header ceilings, not
    // hero sizes -- a page that reads as "huge header, tiny work area" is a
    // per-page layout bug to fix at the call site, never a reason to raise
    // these numbers further. Caps + letter-spacing stay allowed only on
    // `overline`.
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
