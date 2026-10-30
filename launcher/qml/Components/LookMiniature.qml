// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * One look drawn as a small launcher in its own colours and layout, for
 * LookPicker: the rail with real instance icons, then Home as that look lays
 * it out -- Sahne's blurred stage with a white Play, the worlds on glass and
 * a row of cards; Sade's flat hero, green Play, hairline rows and side
 * column; Gece's night sky with glass rows and pane; Klasik's framed hero
 * and rows in the palette picked for it. Words are bars at this size, so
 * the choice is made by looking.
 *
 * Drawn once in a fixed design space (designWidth x designHeight) and
 * scaled to the tile, so every coordinate below is in those units.
 */
Rectangle {
    id: root

    // "stage", "calm", "night" or "classic".
    required property string look
    // That look's palette (Theme.previewPalette()).
    required property var colors

    readonly property int designWidth: 320
    readonly property int designHeight: 180
    // The height a host gives this for a width, to keep the design's shape.
    readonly property real aspect: designHeight / designWidth

    readonly property bool night: root.look === "night"
    readonly property color ink: root.colors.textPrimary
    readonly property color inkSoft: Qt.rgba(root.ink.r, root.ink.g, root.ink.b, 0.55)
    readonly property color inkFaint: Qt.rgba(root.ink.r, root.ink.g, root.ink.b, 0.3)
    readonly property color glass: Qt.rgba(1, 1, 1, 0.07)
    readonly property color glassEdge: Qt.rgba(1, 1, 1, 0.1)

    radius: Theme.radius.md
    color: root.night ? Theme.sky.zenith : root.colors.canvas
    border.width: 1
    border.color: root.colors.border
    clip: true

    // -- Pieces, in design units -------------------------------------------
    // A line of words.
    component Bar: Rectangle {
        height: 3
        radius: 1.5
    }
    // An instance's real icon (Icons), pixel-sharp.
    component Icon: Image {
        property string key: "default"
        width: 12
        height: 12
        source: Icons.instanceIcon(key)
        sourceSize: Qt.size(48, 48)
        smooth: false
    }
    // Play: the look's filled button with its glyph and a word.
    component Play: Rectangle {
        property color ink: "black"
        width: 34
        height: 11
        radius: 3
        MeshIcon {
            x: 6
            anchors.verticalCenter: parent.verticalCenter
            iconName: "play"
            size: 6
            color: parent.ink
        }
        Bar {
            x: 15
            width: 13
            height: 2.5
            anchors.verticalCenter: parent.verticalCenter
            color: parent.ink
        }
    }
    // A world in a side list: its picture and two lines.
    component World: Item {
        property color picture: "gray"
        property color ink: "white"
        width: 70
        height: 12
        Rectangle { width: 10; height: 10; radius: 2; color: parent.picture }
        Bar { x: 14; y: 1.5; width: 36; height: 2.5; color: parent.ink }
        Bar { x: 14; y: 6.5; width: 24; height: 2; color: Qt.rgba(parent.ink.r, parent.ink.g, parent.ink.b, 0.5) }
    }

    Item {
        id: design
        width: root.designWidth
        height: root.designHeight
        transformOrigin: Item.TopLeft
        scale: root.width / root.designWidth

        // -- The shell: sky or frame, rail and top bar ------------------
        SkyBackdrop {
            anchors.fill: parent
            visible: root.night
            starCount: 0
        }
        Repeater {
            model: root.night ? [[150, 6], [176, 10], [205, 4], [238, 9], [262, 5], [290, 11]] : []
            delegate: Rectangle {
                required property var modelData
                x: modelData[0]; y: modelData[1]
                width: 1.5; height: 1.5
                color: "white"
                opacity: 0.7
            }
        }
        Rectangle {
            // The shell around the page.
            anchors.fill: parent
            visible: !root.night
            color: root.colors.surface
        }
        Rectangle {
            // The page panel, its corner against the shell rounded.
            x: 26; y: 18
            width: parent.width - x + 8
            height: parent.height - y + 8
            radius: 8
            visible: !root.night
            color: root.colors.canvas
        }
        Rectangle {
            x: 7; y: 4; width: 12; height: 12; radius: 3
            gradient: Gradient {
                GradientStop { position: 0; color: "#9B7CFF" }
                GradientStop { position: 1; color: "#5A3AD6" }
            }
        }
        Bar { x: 34; y: 8; width: 30; height: 3.5; color: root.ink }
        Column {
            x: 6; y: 30
            spacing: 6
            Icon { key: "creeper"; width: 14; height: 14 }
            Icon { key: "fox"; width: 14; height: 14 }
            Icon { key: "diamond"; width: 14; height: 14 }
        }

        // -- Home, as the look lays it out ------------------------------
        Loader {
            anchors.fill: parent
            sourceComponent: root.look === "stage" ? stageHome
                           : root.look === "calm" ? calmHome
                           : root.look === "night" ? nightHome : classicHome
        }
    }

    Component {
        id: stageHome
        Item {
            // The stage: the hero's screenshot as colour, fading to the page.
            Rectangle {
                x: 26; y: 18; width: 294; height: 84
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#0B0B0D" }
                    GradientStop { position: 0.45; color: "#3A2622" }
                    GradientStop { position: 0.75; color: "#5C2E2E" }
                    GradientStop { position: 1.0; color: "#3E4A2C" }
                }
            }
            Rectangle {
                x: 26; y: 60; width: 294; height: 42
                gradient: Gradient {
                    GradientStop { position: 0; color: "transparent" }
                    GradientStop { position: 1; color: root.colors.canvas }
                }
            }
            Bar { x: 36; y: 38; width: 22; height: 2; color: root.inkSoft }
            Bar { x: 36; y: 44; width: 76; height: 7; radius: 2; color: root.ink }
            Bar { x: 36; y: 56; width: 46; height: 2.5; color: root.inkSoft }
            Play { x: 36; y: 65; color: root.colors.accent; ink: root.colors.textOnAccent }
            Rectangle { x: 73; y: 65; width: 11; height: 11; radius: 3; color: root.glass }
            // Recent worlds on dark glass.
            Rectangle {
                x: 236; y: 24; width: 76; height: 50; radius: 6
                color: Qt.rgba(0, 0, 0, 0.35)
                Bar { x: 7; y: 6; width: 26; height: 2; color: root.inkSoft }
                Column {
                    x: 7; y: 13
                    spacing: 3
                    World { picture: "#5E8C46"; ink: root.ink }
                    World { picture: "#55606B"; ink: root.ink }
                    World { picture: "#8E7650"; ink: root.ink }
                }
            }
            Bar { x: 36; y: 106; width: 44; height: 3.5; color: root.ink }
            Row {
                x: 36; y: 115
                spacing: 5
                Repeater {
                    model: 4
                    delegate: Rectangle {
                        required property int index
                        width: 64; height: 44; radius: 5
                        color: root.colors.surfaceRaised
                        gradient: index === 0 ? cardArt : null
                        Bar { x: 5; y: 5; width: 13; height: 4; radius: 1; color: Qt.rgba(1, 1, 1, 0.12) }
                        Bar { x: 5; y: 31; width: 34; height: 3; color: root.ink }
                        Bar { x: 5; y: 37; width: 22; height: 2; color: root.inkSoft }
                    }
                }
            }
            Gradient {
                id: cardArt
                GradientStop { position: 0; color: "#2C3D4E" }
                GradientStop { position: 1; color: "#141418" }
            }
        }
    }

    Component {
        id: calmHome
        Item {
            Icon { key: "creeper"; x: 36; y: 26; width: 22; height: 22 }
            Bar { x: 64; y: 29; width: 72; height: 6; radius: 2; color: root.ink }
            Bar { x: 64; y: 40; width: 44; height: 2.5; color: root.inkSoft }
            Play { x: 170; y: 31; color: root.colors.accent; ink: root.colors.textOnAccent }
            Rectangle { x: 207; y: 31; width: 11; height: 11; radius: 3; color: root.glass }
            Bar { x: 36; y: 62; width: 40; height: 2; color: root.inkFaint }
            Column {
                x: 36; y: 70
                Repeater {
                    model: ["fox", "diamond", "tnt"]
                    delegate: Item {
                        required property string modelData
                        required property int index
                        width: 182; height: 20
                        Icon { key: modelData; y: 3 }
                        Bar { x: 18; y: 4.5; width: 56; height: 3; color: root.ink }
                        Bar { x: 18; y: 10.5; width: 34; height: 2; color: root.inkSoft }
                        MeshIcon { x: 170; y: 4; iconName: "play"; size: 7; color: root.inkFaint }
                        Rectangle {
                            visible: index < 2
                            anchors.bottom: parent.bottom
                            width: parent.width; height: 0.75
                            color: root.colors.divider
                        }
                    }
                }
            }
            // The side column, set off by a hairline.
            Rectangle { x: 228; y: 18; width: 0.75; height: 162; color: root.colors.divider }
            Bar { x: 236; y: 26; width: 34; height: 2; color: root.inkFaint }
            Column {
                x: 236; y: 34
                spacing: 4
                World { picture: "#5E8C46"; ink: root.ink }
                World { picture: "#55606B"; ink: root.ink }
                World { picture: "#8E7650"; ink: root.ink }
            }
        }
    }

    Component {
        id: nightHome
        Item {
            Bar { x: 36; y: 30; width: 22; height: 2; color: root.colors.accentText }
            Bar { x: 36; y: 36; width: 76; height: 7; radius: 2; color: root.ink }
            Bar { x: 36; y: 48; width: 46; height: 2.5; color: root.inkSoft }
            Play { x: 36; y: 57; color: root.colors.accent; ink: root.colors.textOnAccent }
            Rectangle { x: 73; y: 57; width: 11; height: 11; radius: 3; color: root.glass }
            Bar { x: 36; y: 84; width: 40; height: 2; color: root.colors.accentText }
            Grid {
                x: 36; y: 92
                columns: 2
                spacing: 5
                Repeater {
                    model: ["fox", "diamond", "tnt", "creeper"]
                    delegate: Rectangle {
                        required property string modelData
                        width: 92; height: 20; radius: 4
                        color: root.glass
                        border.width: 0.75
                        border.color: root.glassEdge
                        Icon { key: parent.modelData; x: 5; y: 4 }
                        Bar { x: 22; y: 6; width: 40; height: 2.5; color: root.ink }
                        Bar { x: 22; y: 11; width: 26; height: 2; color: root.inkSoft }
                        MeshIcon { x: 80; y: 6.5; iconName: "play"; size: 6; color: root.inkSoft }
                    }
                }
            }
            // The side pane, one glass.
            Rectangle {
                x: 236; y: 24; width: 76; height: 60; radius: 6
                color: root.glass
                border.width: 0.75
                border.color: root.glassEdge
                Bar { x: 7; y: 7; width: 30; height: 2; color: root.colors.accentText }
                Column {
                    x: 7; y: 15
                    spacing: 4
                    World { picture: "#5E8C46"; ink: root.ink }
                    World { picture: "#55606B"; ink: root.ink }
                    World { picture: "#8E7650"; ink: root.ink }
                }
            }
        }
    }

    Component {
        id: classicHome
        Item {
            Rectangle {
                x: 36; y: 26; width: 182; height: 46; radius: 6
                color: root.colors.surface
                border.width: 0.75
                border.color: root.colors.border
                Icon { key: "default"; x: 7; y: 7; width: 32; height: 32 }
                Bar { x: 46; y: 12; width: 58; height: 5; radius: 2; color: root.ink }
                Bar { x: 46; y: 22; width: 36; height: 2.5; color: root.inkSoft }
                Play { x: 140; y: 17; color: root.colors.accent; ink: root.colors.textOnAccent }
            }
            Bar { x: 36; y: 82; width: 52; height: 4; color: root.ink }
            Column {
                x: 36; y: 92
                spacing: 5
                Repeater {
                    model: ["default", "default", "default"]
                    delegate: Rectangle {
                        required property string modelData
                        width: 182; height: 20; radius: 4
                        color: root.colors.surface
                        border.width: 0.75
                        border.color: root.colors.border
                        Icon { key: parent.modelData; x: 6; y: 4 }
                        Bar { x: 24; y: 6; width: 50; height: 2.5; color: root.ink }
                        Bar { x: 24; y: 11; width: 30; height: 2; color: root.inkSoft }
                        Rectangle { x: 150; y: 5; width: 24; height: 10; radius: 2.5; color: root.colors.surfaceRaised }
                    }
                }
            }
            Rectangle { x: 228; y: 18; width: 0.75; height: 162; color: root.colors.divider }
            Rectangle {
                x: 236; y: 24; width: 76; height: 34; radius: 5
                color: root.colors.surface
                border.width: 0.75
                border.color: root.colors.border
                Bar { x: 7; y: 7; width: 30; height: 3; color: root.ink }
                Bar { x: 7; y: 16; width: 50; height: 2; color: root.inkSoft }
                Bar { x: 7; y: 23; width: 44; height: 2; color: root.inkSoft }
            }
            Column {
                x: 236; y: 66
                spacing: 4
                World { picture: "#5E8C46"; ink: root.ink }
                World { picture: "#55606B"; ink: root.ink }
            }
        }
    }
}
