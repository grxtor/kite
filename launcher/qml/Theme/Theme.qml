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

    // The UiMotionStyle setting, set by SettingsStore (Theme cannot see the
    // settings): "playful", "calm" or "off". Every `motion` token resolves
    // from it; "off" zeroes every duration and sets `motion.reduced`.
    // Anything else reads as "playful".
    property string motionStyle: "playful"

    // The ink and the soft fill of a status colour, for a badge, a notice or
    // an icon disc: "neutral", "success", "warning", "danger", "info" or
    // "accent". Anything else reads as neutral.
    function tone(name) {
        switch (name) {
        case "success": return { ink: root.palette.success, soft: root.palette.successSubtle }
        case "warning": return { ink: root.palette.warning, soft: root.palette.warningSubtle }
        case "danger": return { ink: root.palette.danger, soft: root.palette.dangerSubtle }
        case "info": return { ink: root.palette.info, soft: root.palette.infoSubtle }
        case "accent": return { ink: root.palette.accentText, soft: root.palette.accentSubtle }
        default: return { ink: root.palette.textSecondary, soft: root.palette.surfaceOverlay }
        }
    }

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
        // The Play verb's plate (PlayButton, HomeRowPlay): a step rounder than
        // a control's sm, still well under the md/lg cap above.
        readonly property int play: 10
    }

    // On a wide window a page's content stops growing at this width and centres.
    readonly property QtObject layout: QtObject {
        readonly property int maxContentWidth: 1280
        // The bottom play bar; the sidebar's foot band matches it so their top
        // edges line up.
        readonly property int barHeight: 72
        // A dialog's preferred width (AppDialog `size`); it never exceeds the
        // window less a margin.
        readonly property int dialogSm: 440
        readonly property int dialogMd: 560
        readonly property int dialogLg: 680
        readonly property int dialogXl: 860
    }

    // sm: dense rows and toolbars; default: buttons and fields; lg: a page's
    // main action (Play is lg or larger).
    readonly property QtObject control: QtObject {
        readonly property int heightSm: 32
        readonly property int height: 40
        readonly property int heightLg: 48
        readonly property int menuItemHeight: 32
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
        // The edge of an outlined control (secondary Button, text field,
        // combo box, chip, segmented group), and that edge under the pointer.
        // Lighter than palette.borderStrong, which is opaque and too loud for
        // a whole toolbar of outlines.
        readonly property color outline: root.dark ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(0, 0, 0, 0.20)
        readonly property color outlineHover: root.dark ? Qt.rgba(1, 1, 1, 0.34) : Qt.rgba(0, 0, 0, 0.32)
        // The hairline round a white knob (Switch, Slider) so it holds on a pale track.
        readonly property color knobEdge: Qt.rgba(0, 0, 0, 0.10)
    }

    // sm: beside a line of text; md: a control's own icon (nav item, toolbar
    // toggle); lg: a picture-like icon (an instance in the rail). xxs and xs
    // sit inside a chip, a tag or a small row; mid is a control's glyph one
    // step over sm; xl is an empty state's picture. No arithmetic on these.
    readonly property QtObject icon: QtObject {
        readonly property int xxs: 14
        readonly property int xs: 16
        readonly property int sm: 18
        readonly property int mid: 20
        readonly property int md: 22
        readonly property int lg: 26
        readonly property int xl: 40
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
        // Frosted glass over the night sky or a photo (no blur in Qt 6.4): a
        // faint white fill, one step lighter when lit or under the pointer,
        // and its edge. `glassStrong` is a glass button's resting fill.
        readonly property color glass: Qt.rgba(1, 1, 1, 0.07)
        readonly property color glassLit: Qt.rgba(1, 1, 1, 0.11)
        readonly property color glassStrong: Qt.rgba(1, 1, 1, 0.12)
        readonly property color glassEdge: Qt.rgba(1, 1, 1, 0.09)
        // The dark counterpart over a photo (the Sahne stage's world panel).
        readonly property color shade: Qt.rgba(0.04, 0.04, 0.05, 0.58)
        readonly property color shadeLit: Qt.rgba(0.04, 0.04, 0.05, 0.68)
        // Small print over glass or a photo; a bar track on media.
        readonly property color textFaint: Qt.rgba(1, 1, 1, 0.55)
        readonly property color track: Qt.rgba(1, 1, 1, 0.18)
    }

    /* Motion contract. Two styles share one grammar; a component reads the
     * tokens below and never asks which style is on.
     *   - playful (the default): longer, larger, springy. A transform
     *     (translate, scale, rotation, the sliding marker, the switch knob)
     *     eases with `popEasing`, a back-out that overshoots its end by about
     *     a tenth of the distance and settles.
     *   - calm: short, small, quiet. `popEasing` is the same OutCubic as
     *     `easing`, so nothing overshoots.
     * The grammar:
     *   - press: a button scales to `pressScale` over `press` and back (the
     *     big Play to `pressScaleLarge`, a tab, chip or segment to
     *     `pressScaleLight`, a card to `cardPressScale`).
     *   - hover: a colour fades over `fast`. An instance card is the one
     *     hover that moves: it lifts `lift` px over `normal`, its border
     *     brightens over `fast` and its Play grows in from `appearScale`.
     *   - selection: one marker (rail shape, tab underline, a segmented
     *     toggle's fill) slides to the new item over `slow`; it never pops
     *     from item to item, and it stays inside its row while it overshoots.
     *   - entrance: a page fades in over `normal` and rises `shift` px over
     *     `slow`; a list's first rows fade in and rise `rise` px, `stagger`
     *     apart (RiseIn); a menu or popup scales up from `popScale` over
     *     `pop`, a dialog over `slow` while it rises `dialogRise` px.
     *   - exit: always the quick way out, a fade over `fast`, no overshoot.
     * Overshoot is for transforms only. Opacity and colour always use
     * `easing`; so does anything that drives layout (a bar's height, a panel's
     * or button's width, a row collapsing), so no sibling is ever pushed back
     * and forth. Inside a clipped container an overshooting child must stay
     * within the clip.
     * No glow. An idle animation loop is gated on a real busy/live state
     * property, never on "the page happens to be idle". Every transform ends
     * at rest: nothing is left faded, scaled or offset when an animation is
     * cut short or motion is off.
     *
     * Selection grammar, one look per control class:
     *   - rail/list nav (SidebarNav, Settings' section list): a sliding
     *     neutral shape in the kite's cut, nose lit in the accent, accentText
     *     icon and label.
     *   - tab strip (TabStrip, TabBar/TabButton, ChipGroup `tabs`): plain
     *     text over a 1 px hairline under the whole row; one 2 px accent
     *     underline slides to the active tab, which takes textPrimary, the
     *     rest textTertiary. No pill behind any tab.
     *   - segmented toggle (ViewModeToggle, SegmentedChoice): one outlined
     *     group with 1 px dividers; the active segment takes hoverOverlay and
     *     textPrimary.
     *   - filter chip (ChipGroup): an outline; selected, the outline and the
     *     label turn textPrimary.
     *   - swatch/tile picker (PalettePicker): an accent ring around the
     *     selected tile.
     * A control never falls back to a bare colour-only "selected" look.
     *
     * With motion off (the Motion setting's "Off") every duration is 0 and
     * `reduced` is true; an endless animation must also gate `running` on
     * !reduced.
     */
    readonly property QtObject motion: QtObject {
        readonly property bool reduced: root.motionStyle === "off"
        // "playful" or "calm"; while `reduced` it is moot.
        readonly property string style: root.motionStyle === "calm" ? "calm" : "playful"
        readonly property bool _playful: style === "playful"

        readonly property int fast: reduced ? 0 : _playful ? 140 : 120
        readonly property int normal: reduced ? 0 : _playful ? 240 : 180
        readonly property int slow: reduced ? 0 : _playful ? 400 : 260
        // A button going down under the pointer, and coming back.
        readonly property int press: reduced ? 0 : _playful ? 100 : 80
        // A menu or popup growing open.
        readonly property int pop: reduced ? 0 : _playful ? 240 : 120
        // Between one row and the next as a list comes in, for the first
        // `staggerRows` rows; the rest come with the last of them.
        readonly property int stagger: reduced ? 0 : _playful ? 45 : 30
        readonly property int staggerRows: 12
        // One half of the running dot's breathing loop.
        readonly property int pulse: reduced ? 0 : 900

        // Opacity, colour and anything that drives layout.
        readonly property int easing: Easing.OutCubic
        // Transforms: `easing.type: popEasing; easing.overshoot: popOvershoot`
        // (the overshoot is ignored by OutCubic). The switch knob takes
        // `knobOvershoot`.
        readonly property int popEasing: _playful ? Easing.OutBack : Easing.OutCubic
        readonly property real popOvershoot: _playful ? 1.7 : 0
        readonly property real knobOvershoot: _playful ? 2.0 : 0
        // How far past its end the pop easing goes, as a share of the travel
        // (about a tenth), and the most a sliding marker may pass its row, in
        // px: a marker crossing a whole rail would otherwise fly well past.
        readonly property real _popPeak: {
            const s = popOvershoot
            const u = 2 * s / (3 * (s + 1))
            return u * u * s / 3
        }
        readonly property int slideOvershootMax: 6
        // A sliding marker's animated position `value` on its way to `target`
        // over the signed distance `travel`: unchanged until it passes the
        // target, then scaled so its furthest point is slideOvershootMax px.
        function tame(value, target, travel) {
            const peak = Math.abs(travel) * _popPeak
            const over = value - target
            if (peak <= slideOvershootMax || over * travel <= 0)
                return value
            return target + over * slideOvershootMax / peak
        }

        // Distances, in px: what a page rises into place, a list row, a
        // dialog, and what a hovered card lifts.
        readonly property int shift: _playful ? 18 : 10
        readonly property int rise: _playful ? 14 : 8
        readonly property int dialogRise: _playful ? 16 : 10
        readonly property int lift: _playful ? 6 : 3
        // A pressed button; the big Play; a tab, chip or segment; a card.
        readonly property real pressScale: _playful ? 0.93 : 0.97
        readonly property real pressScaleLarge: _playful ? 0.95 : 0.97
        readonly property real pressScaleLight: _playful ? 0.97 : 0.985
        readonly property real cardPressScale: _playful ? 0.985 : 1.0
        // A menu, popup or dialog as it starts to open.
        readonly property real popScale: _playful ? 0.88 : 0.96
        // A small piece as it starts to appear: a card's Play, a tick.
        readonly property real appearScale: _playful ? 0.7 : 0.85
        readonly property real tickScale: _playful ? 0.5 : 0.85
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
        // The instance's name on Home's hero (heroSm: Gece's).
        readonly property QtObject hero: QtObject {
            readonly property int pixelSize: 48
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.1
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject heroSm: QtObject {
            readonly property int pixelSize: 46
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.1
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        // Home's section name in the Sahne look, between title and subheading.
        readonly property QtObject section: QtObject {
            readonly property int pixelSize: 18
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.3
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        // The name on a Home card (Sahne), between bodyStrong and title.
        readonly property QtObject cardName: QtObject {
            readonly property int pixelSize: 16
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.35
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
    }
}
