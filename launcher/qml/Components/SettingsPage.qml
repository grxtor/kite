// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Launcher settings. Every change is saved the moment it is made -- there
 * is no OK/Cancel, as in any current desktop app's preferences.
 */
Item {
    id: root

    property int systemMemoryMiB: 8192
    // PluginSurfaceModel for the global settings anchor, or null.
    property var pluginSurfaces: null
    // TranslationsModel (languageKey, name, completeness) and the function
    // that switches to one of its keys, live.
    property var languages: null
    property var selectLanguage: null
    property string section: "general"

    // No longer emitted from within this file -- proxy/external-tools/
    // log-upload all have QML sections of their own now (below) instead of
    // a "More" escape hatch into the classic dialog. Left declared because
    // Main.qml still binds a handler to it; removing the signal outright is
    // for whoever next touches that file's own wiring.
    signal openClassicRequested(string page)
    signal openPathRequested(string path)

    readonly property var sections: [
        { id: "general", icon: "home", label: qsTr("General") },
        { id: "java", icon: "layers", label: qsTr("Java & memory") },
        { id: "minecraft", icon: "cube", label: qsTr("Minecraft") },
        { id: "console", icon: "terminal", label: qsTr("Console") },
        { id: "appearance", icon: "image", label: qsTr("Appearance") },
        { id: "commands", icon: "edit", label: qsTr("Custom commands") },
        { id: "plugins", icon: "package", label: qsTr("Plugins") },
        { id: "proxy", icon: "globe", label: qsTr("Proxy") },
        { id: "external-tools", icon: "settings", label: qsTr("External tools") },
        { id: "log-upload", icon: "copy", label: qsTr("Log upload") }
    ]
    readonly property int sectionIndex: {
        for (var i = 0; i < sections.length; ++i)
            if (sections[i].id === section)
                return i
        return 0
    }

    // A chrome-only screen (design-plan.md §5): no instance/pack art of its
    // own to bleed behind the header, unlike Library/PlayDock/Discover's
    // detail hero. The same quiet block-grid wash as Discover's own empty
    // state, behind this page's header region; every SettingsGroup panel
    // further down is a fully opaque surface and simply paints over it.
    AmbientPattern {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 260
        fadeBottom: true
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        spacing: Theme.space.xl

        // Same rail selection grammar as the main sidebar (design-plan.md
        // Principle 6/§4): a sliding pill + left accent bar behind the row,
        // not just NavItem's own bare accent-icon/bold-text fallback. Shares
        // SidebarNav's own mechanism via NavSelectionIndicator rather than
        // re-deriving the geometry here.
        Item {
            id: sectionNavHost
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 208
            Layout.topMargin: Theme.space.xs
            implicitHeight: sectionColumn.implicitHeight

            readonly property Item selectedItem: {
                for (var i = 0; i < sectionRepeater.count; ++i) {
                    var item = sectionRepeater.itemAt(i)
                    if (item && item.modelData.id === root.section)
                        return item
                }
                return null
            }

            NavSelectionIndicator {
                target: sectionNavHost.selectedItem
            }

            Column {
                id: sectionColumn
                width: parent.width
                spacing: Theme.space.xxs

                Repeater {
                    id: sectionRepeater
                    model: root.sections
                    delegate: NavItem {
                        required property var modelData
                        width: parent.width
                        // Only offered when some plugin put something there.
                        visible: modelData.id !== "plugins" || pluginsView.count > 0
                        iconName: modelData.icon
                        label: modelData.label
                        selected: root.section === modelData.id
                        onClicked: root.section = modelData.id
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.sectionIndex

            // General
            SettingsScroll {
                title: qsTr("General")
                description: qsTr("How the library sorts and where new instances start out.")
                SettingsGroup {
                    width: parent.width
                    title: qsTr("Library")
                    SettingChoice {
                        key: "InstSortMode"
                        label: qsTr("Sort instances by")
                        description: qsTr("Order inside each group of the library.")
                        options: [
                            { value: "Name", label: qsTr("Name") },
                            { value: "LastLaunch", label: qsTr("Last played") }
                        ]
                    }
                }
                SettingsGroup {
                    width: parent.width
                    title: qsTr("New instances")
                    SettingSwitch {
                        key: "DownloadGameFilesDuringInstanceCreation"
                        label: qsTr("Download game files right away")
                        description: qsTr("Fetch libraries and assets while the instance is created, instead of on its first launch.")
                    }
                    SettingSwitch {
                        key: "SkipModpackUpdatePrompt"
                        invert: true
                        label: qsTr("Offer to update an installed modpack")
                        description: qsTr("Installing a modpack you already have suggests updating that instance instead of making a second copy.")
                    }
                    SettingSwitch {
                        key: "BackupBeforeLaunch"
                        label: qsTr("Back up before every launch")
                        description: qsTr("Keeps a “pre-launch” snapshot in the instance's .backups folder.")
                    }
                }
                SettingsGroup {
                    width: parent.width
                    title: qsTr("Folders")
                    description: qsTr("Relative paths are inside MeshMC's data folder. Moving them is done in the classic settings, which check the new place first.")
                    Repeater {
                        model: [
                            { key: "InstanceDir", label: qsTr("Instances") },
                            { key: "CentralModsDir", label: qsTr("Mods") },
                            { key: "IconsDir", label: qsTr("Icons") },
                            { key: "SkinsDir", label: qsTr("Skins") },
                            { key: "JavaDir", label: qsTr("Java runtimes") }
                        ]
                        delegate: SettingRow {
                            required property var modelData
                            label: modelData.label
                            description: SettingsStore.string(modelData.key)
                            IconButton {
                                iconName: "folder"
                                tip: qsTr("Open folder")
                                flat: false
                                onClicked: root.openPathRequested(SettingsStore.string(modelData.key))
                            }
                        }
                    }
                }
            }

            // Java & memory
            SettingsScroll {
                title: qsTr("Java & memory")
                description: qsTr("How much memory Minecraft gets, and which Java runtime runs it.")
                SettingsGroup {
                    width: parent.width
                    title: qsTr("Memory")
                    MemorySetting {
                        label: qsTr("Maximum memory")
                        hint: qsTr("How much memory Minecraft may use. Big modpacks want 6–8 GiB; vanilla is happy with 2–4.")
                        systemMiB: root.systemMemoryMiB
                    }
                    SettingNumber {
                        key: "MinMemAlloc"
                        label: qsTr("Starting memory")
                        description: qsTr("Memory reserved when the game starts. Never above the maximum.")
                        from: 128
                        to: SettingsStore.number("MaxMemAlloc")
                        stepSize: 128
                        suffix: qsTr("MiB")
                    }
                }
                SettingsGroup {
                    width: parent.width
                    title: qsTr("Java runtime")
                    SettingSwitch {
                        key: "JavaAutoDownload"
                        label: qsTr("Download Java automatically")
                        description: qsTr("Fetches the Java version each Minecraft release needs, so you never have to install one yourself.")
                    }
                    SettingText {
                        key: "JavaPath"
                        label: qsTr("Java executable")
                        description: qsTr("Used when an instance does not choose its own. Leave empty to let MeshMC pick.")
                        placeholder: qsTr("Automatic")
                        monospace: true
                    }
                    SettingText {
                        key: "JvmArgs"
                        label: qsTr("JVM arguments")
                        description: qsTr("Extra flags for every launch. MeshMC already sets memory and the classpath.")
                        placeholder: "-XX:+UseG1GC"
                        monospace: true
                    }
                }
            }

            // Minecraft
            SettingsScroll {
                title: qsTr("Minecraft")
                description: qsTr("The game window, play time tracking, and native library overrides.")
                SettingsGroup {
                    width: parent.width
                    title: qsTr("Game window")
                    SettingSwitch {
                        id: maximized
                        key: "LaunchMaximized"
                        label: qsTr("Start maximized")
                    }
                    SettingNumber {
                        key: "MinecraftWinWidth"
                        enabled: !maximized.checked
                        label: qsTr("Window width")
                        from: 1
                        to: 65536
                        suffix: qsTr("px")
                    }
                    SettingNumber {
                        key: "MinecraftWinHeight"
                        enabled: !maximized.checked
                        label: qsTr("Window height")
                        from: 1
                        to: 65536
                        suffix: qsTr("px")
                    }
                }
                SettingsGroup {
                    width: parent.width
                    title: qsTr("Play time")
                    SettingSwitch {
                        key: "RecordGameTime"
                        label: qsTr("Record play time")
                    }
                    SettingSwitch {
                        key: "ShowGameTime"
                        label: qsTr("Show play time per instance")
                    }
                    SettingSwitch {
                        key: "ShowGlobalGameTime"
                        label: qsTr("Show total play time")
                    }
                }
                SettingsGroup {
                    width: parent.width
                    title: qsTr("Native libraries")
                    description: qsTr("Only for systems where the bundled libraries don't work, such as some Linux setups.")
                    SettingSwitch {
                        key: "UseNativeGLFW"
                        label: qsTr("Use the system's GLFW")
                    }
                    SettingSwitch {
                        key: "UseNativeOpenAL"
                        label: qsTr("Use the system's OpenAL")
                    }
                }
            }

            // Console
            SettingsScroll {
                title: qsTr("Console")
                description: qsTr("What the game's log window does while you play, and how much of it is kept.")
                SettingsGroup {
                    width: parent.width
                    title: qsTr("Game console")
                    SettingSwitch {
                        key: "ShowConsole"
                        label: qsTr("Show the console while playing")
                    }
                    SettingSwitch {
                        key: "AutoCloseConsole"
                        label: qsTr("Close the console when the game quits")
                    }
                    SettingSwitch {
                        key: "ShowConsoleOnError"
                        label: qsTr("Show the console when the game crashes")
                    }
                }
                SettingsGroup {
                    width: parent.width
                    title: qsTr("Log history")
                    SettingNumber {
                        key: "ConsoleMaxLines"
                        label: qsTr("Lines to keep")
                        from: 10000
                        to: 1000000
                        stepSize: 10000
                    }
                    SettingSwitch {
                        key: "ConsoleOverflowStop"
                        label: qsTr("Stop logging when the limit is reached")
                        description: qsTr("Otherwise the oldest lines are dropped to make room.")
                    }
                }
            }

            // Appearance
            SettingsScroll {
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
                            function syncToSetting() {
                                currentIndex = indexOfValue(SettingsStore.string("Language"))
                            }
                            onCountChanged: syncToSetting()
                            Component.onCompleted: syncToSetting()
                            onActivated: root.selectLanguage(currentValue)
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
            }

            // Custom commands
            SettingsScroll {
                title: qsTr("Custom commands")
                description: qsTr("Shell commands run around every launch, launcher-wide.")
                SettingsGroup {
                    width: parent.width
                    description: qsTr("Run around every launch. Available variables: $INST_NAME, $INST_ID, $INST_DIR, $INST_MC_DIR, $INST_JAVA and $INST_JAVA_ARGS.")
                    SettingText {
                        key: "PreLaunchCommand"
                        label: qsTr("Before launch")
                        monospace: true
                    }
                    SettingText {
                        key: "WrapperCommand"
                        label: qsTr("Wrapper")
                        description: qsTr("Runs the game through another program, such as prime-run or gamemoderun.")
                        monospace: true
                    }
                    SettingText {
                        key: "PostExitCommand"
                        label: qsTr("After exit")
                        monospace: true
                    }
                }
            }

            // Plugins
            SettingsScroll {
                title: qsTr("Plugins")
                description: qsTr("Settings the installed plugins have added.")
                PluginSurfaces {
                    id: pluginsView
                    width: parent.width
                    model: root.pluginSurfaces
                }
            }

            // Proxy
            SettingsProxySection {}

            // External tools
            SettingsExternalToolsSection {}

            // Log upload
            SettingsLogUploadSection {}
        }
    }
}
