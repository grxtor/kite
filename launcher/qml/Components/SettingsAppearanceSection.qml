// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The "Appearance" section of SettingsPage.qml: the launcher's own
 * language, palette and mode, motion, and the cat picture.
 */
SettingsScroll {
    id: root

    // TranslationsModel (languageKey, name, completeness) and the function
    // that switches to one of its keys, live.
    property var languages: null
    property var selectLanguage: null

    // The language box, as wide as the longest language name needs.
    readonly property int languageBoxWidth: 260

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
                width: root.languageBoxWidth
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
                { value: "system", label: qsTr("System") },
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
            description: qsTr("The font every screen of the launcher is drawn in. Sizes stay the same. Inter comes with Kite; the rest are installed on this computer.")
        }
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Motion")
        SettingSwitch {
            key: "UiReduceMotion"
            label: qsTr("Reduce motion")
            description: qsTr("Turns off decorative animation across the launcher.")
        }
    }
    // The still cat pictures of the classic window.
    SettingsGroup {
        width: parent.width
        title: qsTr("Cat picture")
        SettingSwitch {
            key: "TheCat"
            label: qsTr("Show a cat picture")
            description: qsTr("A quiet picture of a cat in the corner of every page.")
        }
        SettingRow {
            wide: true
            showDivider: true
            enabled: SettingsStore.bool("TheCat")
            label: qsTr("Picture")
            description: qsTr("Add your own by putting a picture in the catpacks folder inside Kite's data folder.")
            visible: !!LauncherStore.catPacks
            CatPackPicker {
                width: parent.width
            }
        }
    }
}
