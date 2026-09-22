// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Launcher settings. Every change is saved the moment it is made -- there
 * is no OK/Cancel, as in any current desktop app's preferences. Pages that
 * have not moved to QML yet (proxy, language, external tools, accounts,
 * log upload) are one click away in the classic dialog, under "More".
 */
Item {
    id: root

    property int systemMemoryMiB: 8192
    property string section: "general"

    signal openClassicRequested(string page)
    signal openPathRequested(string path)

    readonly property var sections: [
        { id: "general", icon: "home", label: qsTr("General") },
        { id: "java", icon: "layers", label: qsTr("Java & memory") },
        { id: "minecraft", icon: "cube", label: qsTr("Minecraft") },
        { id: "console", icon: "terminal", label: qsTr("Console") },
        { id: "appearance", icon: "image", label: qsTr("Appearance") },
        { id: "commands", icon: "edit", label: qsTr("Custom commands") },
        { id: "more", icon: "more", label: qsTr("More") }
    ]
    readonly property int sectionIndex: {
        for (var i = 0; i < sections.length; ++i)
            if (sections[i].id === section)
                return i
        return 0
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        spacing: Theme.space.xl

        Column {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 208
            Layout.topMargin: Theme.space.xs
            spacing: Theme.space.xxs

            Repeater {
                model: root.sections
                delegate: NavItem {
                    required property var modelData
                    width: parent.width
                    iconName: modelData.icon
                    label: modelData.label
                    selected: root.section === modelData.id
                    onClicked: root.section = modelData.id
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
                SettingsGroup {
                    width: parent.width
                    title: qsTr("Theme")
                    SettingChoice {
                        key: "UiThemeMode"
                        label: qsTr("Colour scheme")
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

            // More (classic pages)
            SettingsScroll {
                title: qsTr("More")
                SettingsGroup {
                    width: parent.width
                    description: qsTr("These still open in the classic settings window.")
                    Repeater {
                        model: [
                            { page: "accounts", label: qsTr("Accounts"), text: qsTr("Add, remove and switch Minecraft accounts; skins.") },
                            { page: "language-settings", label: qsTr("Language"), text: qsTr("The language MeshMC is shown in.") },
                            { page: "proxy-settings", label: qsTr("Proxy"), text: qsTr("How MeshMC reaches the internet.") },
                            { page: "external-tools", label: qsTr("External tools"), text: qsTr("Profilers, MCEdit and the JSON editor.") },
                            { page: "log-upload", label: qsTr("Log upload"), text: qsTr("The paste.ee key used to share logs.") }
                        ]
                        delegate: SettingRow {
                            required property var modelData
                            label: modelData.label
                            description: modelData.text
                            Button {
                                text: qsTr("Open")
                                icon.source: Icons.url("external-link")
                                onClicked: root.openClassicRequested(modelData.page)
                            }
                        }
                    }
                }
            }
        }
    }
}
