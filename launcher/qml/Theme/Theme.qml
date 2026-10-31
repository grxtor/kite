// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton
import QtQuick

/*
 * The design tokens of the QML user interface. palette, dark, mode, scheme and
 * look forward to ThemeService; the rest are constants, or values derived from
 * the palette and mode for states the palette has no role of its own for.
 */
QtObject {
    id: root

    readonly property var palette: ThemeService.palette
    readonly property bool dark: ThemeService.dark
    property string mode: ThemeService.mode
    onModeChanged: ThemeService.mode = mode
    // "amethyst", "ember", "diamond" or "grass".
    property string scheme: ThemeService.scheme
    onSchemeChanged: ThemeService.scheme = scheme
    // "stage" (Sahne), "calm" (Sade), "night" (Gece) or "classic".
    property string look: ThemeService.look
    onLookChanged: ThemeService.look = look
    // False for Gece, whose sky shows through: a corner matte (KiteShape.matte)
    // would paint a halo over it, so pictures keep square corners there.
    readonly property bool solidShell: look !== "night"
    // A white knob on a near-white accent (Sahne, Gece) would vanish; the
    // knob takes the accent's own ink instead.
    readonly property bool lightAccent: palette.accent.hslLightness > 0.8
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

    // sm: small pieces (menu rows, tiles, tags); md: controls; lg: cards, menus,
    // popovers; xl: dialogs and other large surfaces. xs is for hairlines,
    // `indicator` for small boxes a 10 would turn into circles (a checkbox).
    // `pill` is for toggle/chip affordances only, never the Play verb or an
    // instance's identity, which cap at md/lg. No arithmetic on these
    // (no `md + 2`).
    readonly property QtObject radius: QtObject {
        readonly property int xs: 2
        readonly property int indicator: 5
        readonly property int sm: 8
        readonly property int md: 12
        readonly property int lg: 16
        readonly property int xl: 20
        readonly property int pill: 999
    }

    // On a wide window a page's content stops growing at this width and centres.
    readonly property QtObject layout: QtObject {
        readonly property int maxContentWidth: 1280
        // The bottom play bar; the sidebar's foot band matches it so their top
        // edges line up.
        readonly property int barHeight: 72
    }

    // sm: dense rows and toolbars; default: buttons and fields; lg: a page's
    // main action (Play is lg or larger).
    readonly property QtObject control: QtObject {
        readonly property int heightSm: 32
        readonly property int height: 40
        readonly property int heightLg: 48
        readonly property int menuItemHeight: 36
    }

    // A keyboard ring stands `offset` clear of the control, solid
    // palette.focusRing; a focused text field wears a soft halo (the same
    // colour at haloOpacity) instead.
    readonly property QtObject focus: QtObject {
        readonly property int width: 2
        readonly property int offset: 2
        readonly property real haloOpacity: 0.45
    }

    // Drop shadows (Shadow.qml): y offset, blur reach and strength. sm: a
    // hovered card; md: menus, tooltips, popovers; lg: dialogs. Light mode
    // needs far less strength.
    readonly property QtObject elevation: QtObject {
        // A trace of violet in light mode keeps the shadow from looking grey.
        readonly property color color: root.dark ? "#000000" : "#1A1230"
        readonly property QtObject sm: QtObject {
            readonly property int y: 2
            readonly property int blur: 10
            readonly property real strength: root.dark ? 0.34 : 0.08
        }
        readonly property QtObject md: QtObject {
            readonly property int y: 6
            readonly property int blur: 20
            readonly property real strength: root.dark ? 0.42 : 0.10
        }
        readonly property QtObject lg: QtObject {
            readonly property int y: 14
            readonly property int blur: 36
            readonly property real strength: root.dark ? 0.52 : 0.16
        }
    }

    // Only the sidebar, the play bar and popups/dialogs use it. Popups stay
    // opaque: without blur a see-through menu only shows ghosting.
    readonly property QtObject glass: QtObject {
        // The sidebar and the play bar.
        readonly property color fill: Qt.rgba(root.palette.surface.r, root.palette.surface.g,
                                              root.palette.surface.b, root.dark ? 0.88 : 0.86)
        // Menus, popups and dialogs.
        readonly property color popup: root.palette.surfaceOverlay
        // The top-edge highlight, and the sheen that fades down from it.
        readonly property color highlight: root.dark ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(1, 1, 1, 0.9)
        readonly property color sheen: root.dark ? Qt.rgba(1, 1, 1, 0.025) : Qt.rgba(1, 1, 1, 0)
        readonly property color border: root.palette.border
    }

    // Translucent, so they read the same on canvas, a card or a dialog.
    readonly property QtObject neutral: QtObject {
        // An "off" or empty track: a switch, a slider's groove, a progress bar.
        readonly property color track: root.dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0.09, 0.07, 0.16, 0.16)
        readonly property color trackHover: root.dark ? Qt.rgba(1, 1, 1, 0.20) : Qt.rgba(0.09, 0.07, 0.16, 0.24)
        // An outlined control's edge under the pointer: one step over
        // palette.border.
        readonly property color borderHover: root.dark ? Qt.rgba(1, 1, 1, 0.20) : Qt.rgba(0, 0, 0, 0.22)
    }

    // sm: beside a line of text; md: a control's own icon (nav item, toolbar
    // toggle); lg: a picture-like icon (an instance in the rail).
    readonly property QtObject icon: QtObject {
        readonly property int sm: 18
        readonly property int md: 22
        readonly property int lg: 26
    }

    readonly property QtObject opacity: QtObject {
        // A disabled control keeps its shape and fades.
        readonly property real disabled: 0.45
    }

    // The Gece look's night sky (SkyBackdrop); colorAt() is its colour at a
    // share of the window's height, for what has to fade into it (ScrollFade).
    readonly property QtObject sky: QtObject {
        readonly property color zenith: "#120C2E"
        readonly property color high: "#1A1148"
        readonly property color low: "#2B1C76"
        readonly property color horizon: "#4A2FC0"
        readonly property real highAt: 0.38
        readonly property real lowAt: 0.74

        function colorAt(position) {
            var stops = [[0, zenith], [highAt, high], [lowAt, low], [1, horizon]]
            var p = Math.max(0, Math.min(1, position))
            for (var i = 1; i < stops.length; ++i) {
                if (p <= stops[i][0]) {
                    var t = (p - stops[i - 1][0]) / (stops[i][0] - stops[i - 1][0])
                    var a = stops[i - 1][1], b = stops[i][1]
                    return Qt.rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1)
                }
            }
            return horizon
        }
    }

    // Text and shading over artwork. The art is the same in either theme, so
    // these do not follow the palette.
    readonly property QtObject media: QtObject {
        readonly property color text: "#FFFFFF"
        readonly property color textSecondary: Qt.rgba(1, 1, 1, 0.80)
        readonly property color scrim: Qt.rgba(0.02, 0.03, 0.05, 0.86)
        readonly property color chip: Qt.rgba(0, 0, 0, 0.38)
        readonly property color chipBorder: Qt.rgba(1, 1, 1, 0.12)
    }

    /* Motion contract: a hover/press transition changes exactly one visual
     * property over `fast` (colour, or one small scale/translate), never uses
     * an overshoot easing on a hover transform, and has no glow. An idle
     * animation loop is gated on a real busy/live state property, never on
     * "the page happens to be idle".
     *
     * Selection grammar, one look per control class:
     *   - rail/list nav (SidebarNav, Settings' section list): a sliding
     *     neutral shape in the kite's cut, nose lit in the accent, accentText
     *     icon and label.
     *   - tab strip (TabStrip, TabBar/TabButton): a soft pill behind the
     *     active tab, accentSubtle fill and accentText label.
     *   - swatch/tile picker (PalettePicker): an accent ring around the
     *     selected tile.
     *   - segmented control (SegmentedControl): a raised fill on the selected
     *     segment, no accent.
     * A control never falls back to a bare colour-only "selected" look.
     */
    readonly property QtObject motion: QtObject {
        readonly property int fast: 120
        readonly property int normal: 180
        readonly property int slow: 260
        readonly property int easing: Easing.OutCubic
    }

    // Inter 4.1 static TTFs, bundled as Kite.Theme resources
    // (Theme/CMakeLists.txt) so the type scale holds without a system Inter.
    readonly property bool _interReady: _interRegular.status === FontLoader.Ready
                                        && _interMedium.status === FontLoader.Ready
                                        && _interSemiBold.status === FontLoader.Ready
                                        && _interBold.status === FontLoader.Ready
    property FontLoader _interRegular: FontLoader {
        source: "qrc:/qt/qml/Kite/Theme/fonts/inter/Inter-Regular.ttf"
    }
    property FontLoader _interMedium: FontLoader {
        source: "qrc:/qt/qml/Kite/Theme/fonts/inter/Inter-Medium.ttf"
    }
    property FontLoader _interSemiBold: FontLoader {
        source: "qrc:/qt/qml/Kite/Theme/fonts/inter/Inter-SemiBold.ttf"
    }
    property FontLoader _interBold: FontLoader {
        source: "qrc:/qt/qml/Kite/Theme/fonts/inter/Inter-Bold.ttf"
    }
    // Montserrat, Kite's brand face, for headings only: too wide for small sizes.
    readonly property bool _montserratReady: _montserratRegular.status === FontLoader.Ready
                                             && _montserratSemiBold.status === FontLoader.Ready
                                             && _montserratBold.status === FontLoader.Ready
    property FontLoader _montserratRegular: FontLoader {
        source: "qrc:/qt/qml/Kite/Theme/fonts/montserrat/Montserrat-Regular.ttf"
    }
    property FontLoader _montserratSemiBold: FontLoader {
        source: "qrc:/qt/qml/Kite/Theme/fonts/montserrat/Montserrat-SemiBold.ttf"
    }
    property FontLoader _montserratBold: FontLoader {
        source: "qrc:/qt/qml/Kite/Theme/fonts/montserrat/Montserrat-Bold.ttf"
    }
    Component.onCompleted: {
        if (!root._interReady)
            console.warn("Theme: bundled Inter failed to load, falling back to",
                          Qt.application.font.family)
        if (!root._montserratReady)
            console.warn("Theme: bundled Montserrat failed to load; headings use",
                          root.font.family)
    }

    // The interface font picked in Settings, by family name; empty or "Inter"
    // means the bundled Inter. Set from the UiFontFamily setting.
    property string customFamily: ""

    readonly property QtObject font: QtObject {
        // The chosen family when installed, else the bundled Inter (the
        // platform font only if loading that failed).
        readonly property string family: root.customFamily.length > 0
                                         && root.customFamily !== "Inter"
                                         && Qt.fontFamilies().indexOf(root.customFamily) >= 0
                                         ? root.customFamily
                                         : root._interReady ? "Inter" : Qt.application.font.family
        // Titles, headings and display text. A font picked in Settings wins
        // here too.
        readonly property string heading: family !== "Inter" && family !== Qt.application.font.family
                                          ? family
                                          : root._montserratReady ? "Montserrat" : family
        readonly property string mono: Qt.fontFamilies().indexOf("JetBrains Mono") >= 0
                                       ? "JetBrains Mono"
                                       : Qt.platform.os === "osx" ? "Menlo"
                                       : Qt.platform.os === "windows" ? "Consolas"
                                       : "DejaVu Sans Mono"
    }

    // Nothing a user has to read goes under 13 (caption). heading (24) and
    // display (30) are page-header ceilings, not hero sizes: fix a "huge header,
    // tiny work area" page at the call site, never by raising these. Caps and
    // letter-spacing only on `overline`.
    readonly property QtObject type: QtObject {
        readonly property QtObject caption: QtObject {
            readonly property int pixelSize: 13
            readonly property int weight: Font.Normal
            readonly property real lineHeight: 1.33
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        // Small uppercase section labels ("RECENT", "CONTINUE PLAYING").
        readonly property QtObject overline: QtObject {
            readonly property int pixelSize: 12
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.30
            readonly property real letterSpacing: 0.8
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject label: QtObject {
            readonly property int pixelSize: 14
            readonly property int weight: Font.Medium
            readonly property real lineHeight: 1.30
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject body: QtObject {
            readonly property int pixelSize: 15
            readonly property int weight: Font.Normal
            readonly property real lineHeight: 1.45
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject bodyStrong: QtObject {
            readonly property int pixelSize: 15
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.45
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject title: QtObject {
            readonly property int pixelSize: 17
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.35
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        // A list's headline (a Discover result's name): louder than a row
        // title, below a section heading.
        readonly property QtObject subheading: QtObject {
            readonly property int pixelSize: 20
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.3
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject heading: QtObject {
            readonly property int pixelSize: 24
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.25
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject display: QtObject {
            readonly property int pixelSize: 30
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.15
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        // Lettering over artwork (Home's greeting), outside the page-header
        // ceiling.
        readonly property QtObject banner: QtObject {
            readonly property int pixelSize: 36
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.1
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
    }
}
