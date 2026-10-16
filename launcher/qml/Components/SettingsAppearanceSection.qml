// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The "Appearance" section of SettingsPage.qml: the launcher's own
 * language, palette and mode, motion, and the cat.
 */
SettingsScroll {
    id: root

    // TranslationsModel (languageKey, name, completeness) and the function
    // that switches to one of its keys, live.
    property var languages: null
    property var selectLanguage: null
    // Whether launcher/qml/Cat was built at all (Main.qml's own
    // catAvailable, in turn MESHMC_HAS_CAT) -- hides the Cat group below
    // rather than offering switches that would do nothing on a build
    // without Qt Quick3D.
    property bool catAvailable: false

    // One tile in the cat variant picker below -- the coat's face, cropped
    // out of the same texture the 3D cat wears (see Cat/CatRig.qml), plus a label,
    // styled like SkinCapeEditor's CapeTile but simple enough not to
    // warrant its own file for the one row that uses it.
    component CatVariantTile: Item {
        id: tile
        required property string value
        required property string label
        readonly property bool selected: SettingsStore.string("CatVariant") === tile.value

        implicitWidth: 56
        implicitHeight: 76

        Rectangle {
            id: swatch
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: 48
            height: 48
            radius: Theme.radius.md
            color: tile.selected ? Theme.palette.accentSubtle : Theme.palette.surface
            border.width: tile.selected ? 2 : 1
            border.color: tile.selected ? Theme.palette.accent : Theme.palette.border

            // The front of the head in the model's 64x64 UV layout: five
            // pixels wide, four high, eyes on its second row.
            Image {
                anchors.centerIn: parent
                width: 35
                height: 28
                smooth: false
                sourceClipRect: Qt.rect(5, 33, 5, 4)
                source: "qrc:/qt/qml/MeshMC/Cat/textures/cat_" + tile.value + ".png"
            }
        }

        Text {
            anchors.top: swatch.bottom
            anchors.topMargin: Theme.space.xs
            anchors.horizontalCenter: parent.horizontalCenter
            text: tile.label
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
        }

        TapHandler {
            onTapped: SettingsStore.setValue("CatVariant", tile.value)
        }
    }

    title: qsTr("Appearance")
    description: qsTr("The launcher's own language and colour scheme.")
    SettingsGroup {
        width: parent.width
        title: qsTr("Language")
        SettingRow {
            label: qsTr("Display language")
            description: qsTr("Applied immediately.")
            ComboBox {
                id: languageBox
                width: 260
                model: root.languages
                textRole: "name"
                valueRole: "languageKey"
                enabled: !!root.languages && !!root.selectLanguage
                // Follows the setting (changed here, in the old settings
                // window or by a plugin), not only the first time.
                readonly property string stored: SettingsStore.string("Language")
                function syncToSetting() {
                    currentIndex = indexOfValue(stored)
                }
                onStoredChanged: syncToSetting()
                onCountChanged: syncToSetting()
                Component.onCompleted: syncToSetting()
                onActivated: {
                    root.selectLanguage(currentValue)
                    syncToSetting()
                }
                Accessible.name: qsTr("Display language")
            }
        }
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Theme")
        SettingRow {
            wide: true
            label: qsTr("Palette")
            description: qsTr("The launcher's colours. Each palette has a dark and a light variant.")
            PalettePicker {
                current: Theme.scheme
                onPicked: (scheme) => {
                    Theme.scheme = scheme
                    SettingsStore.setValue("UiPalette", scheme)
                }
            }
        }
        SettingChoice {
            key: "UiThemeMode"
            label: qsTr("Mode")
            showDivider: true
            options: [
                { value: "dark", label: qsTr("Dark") },
                { value: "light", label: qsTr("Light") }
            ]
            onChanged: (value) => Theme.mode = value
        }
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Font")
        FontFamilyRow {
            key: "UiFontFamily"
            defaultFamily: "Inter"
            label: qsTr("Interface font")
            description: qsTr("The font every screen of the launcher is drawn in. Sizes stay the same. Inter comes with MeshMC; the rest are installed on this computer.")
        }
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Motion")
        SettingSwitch {
            key: "UiReduceMotion"
            label: qsTr("Reduce motion")
            description: qsTr("Turns off decorative animation across the launcher, including the cat.")
        }
    }
    // Hidden outright rather than shown disabled: a build
    // without Qt Quick3D has nothing these switches could turn
    // on, and a visible-but-inert row reads as broken, not as
    // "not for you". See root.catAvailable's own comment.
    SettingsGroup {
        width: parent.width
        visible: root.catAvailable
        title: qsTr("Cat")
        SettingSwitch {
            key: "CatEnabled"
            label: qsTr("Show the cat")
            description: qsTr("A small Minecraft cat that lives on the play bar. Click to pet it, double-click and it hops.")
            // One cat in that corner: the picture below steps aside.
            onSwitched: (on) => { if (on) SettingsStore.setValue("TheCat", false) }
        }
        SettingRow {
            wide: true
            showDivider: true
            label: qsTr("Variant")
            Row {
                spacing: Theme.space.md
                CatVariantTile { value: "calico"; label: qsTr("Calico") }
                CatVariantTile { value: "ginger"; label: qsTr("Ginger") }
                CatVariantTile { value: "black"; label: qsTr("Black") }
                CatVariantTile { value: "white"; label: qsTr("White") }
                CatVariantTile { value: "siamese"; label: qsTr("Siamese") }
            }
        }
    }
    // The still cat pictures of the classic window, for builds with or
    // without the 3D cat. Shares the corner with the 3D cat, so turning one
    // on turns the other off.
    SettingsGroup {
        width: parent.width
        title: qsTr("Cat picture")
        SettingSwitch {
            key: "TheCat"
            label: qsTr("Show a cat picture")
            description: root.catAvailable
                         ? qsTr("A quiet picture of a cat in the corner of every page, instead of the 3D cat.")
                         : qsTr("A quiet picture of a cat in the corner of every page.")
            onSwitched: (on) => { if (on) SettingsStore.setValue("CatEnabled", false) }
        }
        SettingRow {
            wide: true
            showDivider: true
            enabled: SettingsStore.bool("TheCat")
            label: qsTr("Picture")
            description: qsTr("Add your own by putting a picture in the catpacks folder inside MeshMC's data folder.")
            visible: !!LauncherStore.catPacks
            CatPackPicker {
                width: parent.width
            }
        }
    }
}
