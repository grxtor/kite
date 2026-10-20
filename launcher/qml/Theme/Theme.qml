// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
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
 * a plain constant this file alone owns; focus, elevation, glass and neutral
 * are the few values derived from the palette and the mode here, for the
 * states and surfaces the palette has no role of its own for. Shadow.qml and
 * GlassSurface.qml, in this module too, draw the elevation and glass ones.
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
    // "amethyst", "ember", "diamond" or "grass".
    property string scheme: ThemeService.scheme
    onSchemeChanged: ThemeService.scheme = scheme
    function previewPalette(name) { return ThemeService.previewPalette(name) }

    // Modrinth's own rhythm (0.5 / 0.75 / 1 / 1.5 rem), measured off its
    // app at 1x: the phase-3 scale pass kept these as they were and grew the
    // type, the controls and the icons around them instead.
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
    // (a switch track, a filter chip, a tab's soft pill) -- never for the
    // Play verb or an instance's own identity (cover corners, the icon
    // picker, PlayButton's wide hero shape), which are capped at `md`/`lg`
    // instead.
    // The "soft" scale, grown with the phase-3 controls (designs/
    // phase3-scale.md): 8 for small pieces (menu rows, tiles, tags), 12 for
    // controls (buttons, fields), 16 for cards, menus and popovers, 20 for
    // dialogs and other large surfaces. xs is for hairline details only, `indicator` for the small
    // square boxes a 10 would turn into circles (a checkbox). Never
    // arithmetic on these (no `md + 2`): an off-scale corner is exactly the
    // inconsistency this scale exists to prevent.
    readonly property QtObject radius: QtObject {
        readonly property int xs: 2
        readonly property int indicator: 5
        readonly property int sm: 8
        readonly property int md: 12
        readonly property int lg: 16
        readonly property int xl: 20
        readonly property int pill: 999
    }

    // Page layout: on a wide window a page's content stops growing at this
    // width and centres, instead of hugging the left edge with an empty
    // band on the right.
    readonly property QtObject layout: QtObject {
        readonly property int maxContentWidth: 1280
        // The bottom play bar's height -- and the sidebar's foot band, which
        // matches it so their top edges form one line across the window.
        readonly property int barHeight: 72
    }

    // Control heights: sm for dense rows and toolbars, the default for
    // ordinary buttons and fields, lg for a page's main action (Play is lg
    // or larger). Modrinth's scale: its buttons and search fields stand
    // 36-40 px tall at 1x, a nav item 48.
    readonly property QtObject control: QtObject {
        readonly property int heightSm: 32
        readonly property int height: 40
        readonly property int heightLg: 48
        // A menu row (Menu/MenuItem): denser than a button, roomier than a
        // list line.
        readonly property int menuItemHeight: 36
    }

    // Keyboard focus and a focused field -- see Style/FocusRing.qml. A
    // keyboard ring stands `offset` clear of the control, solid
    // palette.focusRing; a focused text field instead wears a soft halo
    // (the same colour at haloOpacity) hugging its accent border.
    readonly property QtObject focus: QtObject {
        readonly property int width: 2
        readonly property int offset: 2
        readonly property real haloOpacity: 0.45
    }

    // Soft drop shadows (Theme/Shadow.qml draws them): y offset, blur
    // reach and strength. sm lifts a hovered card, md carries menus,
    // tooltips and popovers, lg carries dialogs. Light mode needs far less
    // strength for the same lift.
    readonly property QtObject elevation: QtObject {
        // A trace of the neutrals' violet in light mode keeps the shadow
        // from reading as grey dirt under a white card.
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

    /* Glass is an accent, not the style: only the sidebar, the bottom play
     * bar and popups/dialogs use it -- a 1 px light line along the top edge
     * and a faint sheen down from it, over a surface that is slightly
     * translucent where only the canvas sits behind it (sidebar, play bar).
     * Popups stay opaque: with no blur to give (the 6.4 floor has none) a
     * see-through menu only shows the text behind it as ghosting. */
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

    // Neutral state colours that are not a palette role of their own:
    // translucent, so they read the same on canvas, a card or a dialog.
    readonly property QtObject neutral: QtObject {
        // The calm grey of an "off" or empty track: a switch that is off,
        // a slider's groove, a progress bar's track.
        readonly property color track: root.dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0.09, 0.07, 0.16, 0.16)
        readonly property color trackHover: root.dark ? Qt.rgba(1, 1, 1, 0.20) : Qt.rgba(0.09, 0.07, 0.16, 0.24)
        // An outlined control's edge under the pointer: one step over
        // palette.border, well short of borderStrong's 3:1.
        readonly property color borderHover: root.dark ? Qt.rgba(1, 1, 1, 0.20) : Qt.rgba(0, 0, 0, 0.22)
    }

    // sm beside a line of text (a button's glyph, a meta line's clock), md
    // for a control's own icon (a nav item, a toolbar toggle), lg for a
    // picture-like icon (an instance in the rail).
    readonly property QtObject icon: QtObject {
        readonly property int sm: 18
        readonly property int md: 22
        readonly property int lg: 26
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
     * hover-triggered transform. There is no glow: a primary button's hover
     * is its lighter fill alone. An idle animation loop (Animation.Infinite)
     * is only ever gated on a real busy/live state property -- the way
     * RecentItem.qml/InstanceListRow.qml gate their pulse on `isRunning`,
     * PlayDock.qml on `dockRunning`, and Skeleton.qml on its own visibility
     * while loading -- never bound to "the page happens to be idle" alone.
     * See design-plan.md §2.2-2.4.
     *
     * Selection grammar: one documented look per control class, not one
     * grammar total and not accidental variety --
     *   - rail/list nav (SidebarNav, and anything reusing its mechanism,
     *     e.g. Settings' section list): a sliding neutral shape in the
     *     kite's cut -- a square on the icon rail, a full-width pill in a
     *     labelled list -- with its nose lit in the accent and an
     *     accentText icon and label.
     *   - tab strip (TabStrip, TabBar/TabButton): a soft pill behind the
     *     active tab -- accentSubtle fill, accentText label -- the rest plain
     *     text with a hover overlay (Modrinth's tabs; phase-02 brief).
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

    // Inter 4.1 static TTFs, bundled as Kite.Theme module resources
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
    // Montserrat, Kite's brand face, for headings only: it is too wide for
    // the small body and label sizes, which stay on Inter.
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

    // The interface font the user picked, by family name; empty or "Inter"
    // means the bundled Inter. Set from the UiFontFamily setting (see
    // SettingsStore.qml). Sizes stay on the type scale below whatever it is.
    property string customFamily: ""

    readonly property QtObject font: QtObject {
        // Unconditional now that Inter is bundled (see the FontLoaders
        // above) -- the platform font is only ever used if loading the
        // bundled resource itself failed, not merely because Inter isn't
        // separately installed system-wide.
        // The family the user chose in Settings > Appearance, when it is
        // installed (see Theme.customFamily); else the bundled Inter.
        readonly property string family: root.customFamily.length > 0
                                         && root.customFamily !== "Inter"
                                         && Qt.fontFamilies().indexOf(root.customFamily) >= 0
                                         ? root.customFamily
                                         : root._interReady ? "Inter" : Qt.application.font.family
        // Titles, headings and display text. A font the user picked in
        // Settings wins here too, so the whole interface stays in one face.
        readonly property string heading: family !== "Inter" && family !== Qt.application.font.family
                                          ? family
                                          : root._montserratReady ? "Montserrat" : family
        readonly property string mono: Qt.fontFamilies().indexOf("JetBrains Mono") >= 0
                                       ? "JetBrains Mono"
                                       : Qt.platform.os === "osx" ? "Menlo"
                                       : Qt.platform.os === "windows" ? "Consolas"
                                       : "DejaVu Sans Mono"
    }

    // Modrinth's scale (designs/phase3-scale.md, measured off its app at
    // 1x): a section heading ("Jump in", "Library") 24, a list's headline
    // (a Discover result) 20, a row or card title 17, body and meta lines 15, labels and buttons 14-15, and nothing a
    // user has to read under 13 (caption). overline, the one size below
    // that, is for a caps tag only and has no call site today.
    //
    // Contract: heading (24) and display (30) are page-header ceilings, not
    // hero sizes -- a page that reads as "huge header, tiny work area" is a
    // per-page layout bug to fix at the call site, never a reason to raise
    // these numbers further. Caps + letter-spacing stay allowed only on
    // `overline`.
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
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject label: QtObject {
            readonly property int pixelSize: 14
            readonly property int weight: Font.Medium
            readonly property real lineHeight: 1.30
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject body: QtObject {
            readonly property int pixelSize: 15
            readonly property int weight: Font.Normal
            readonly property real lineHeight: 1.45
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject bodyStrong: QtObject {
            readonly property int pixelSize: 15
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.45
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject title: QtObject {
            readonly property int pixelSize: 17
            readonly property int weight: Font.DemiBold
            readonly property real lineHeight: 1.35
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        // A list's own headline: a Discover result's name, a project
        // panel's subsection -- louder than a row title, short of a page
        // section's heading.
        readonly property QtObject subheading: QtObject {
            readonly property int pixelSize: 20
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.3
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject heading: QtObject {
            readonly property int pixelSize: 24
            // Bold: Montserrat's heavy cut is Kite's voice (Modrinth sets
            // its headings in a semibold sans).
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.25
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        readonly property QtObject display: QtObject {
            readonly property int pixelSize: 30
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.15
            // lineHeight is a multiplier for Text; this is the same line in pixels,
            // for anything that has to reserve room for text before it exists.
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
        // Lettering laid over artwork (Home's header greeting) -- outside
        // the page-header ceiling above because it is part of the picture,
        // not a header over a work area.
        readonly property QtObject banner: QtObject {
            readonly property int pixelSize: 36
            readonly property int weight: Font.Bold
            readonly property real lineHeight: 1.1
            readonly property real lineHeightPx: pixelSize * lineHeight
        }
    }
}
