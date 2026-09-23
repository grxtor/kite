// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * First run: the few choices MeshMC cannot make on its own -- language,
 * which Java and how much memory, an account -- as one calm, full-window
 * flow instead of a wizard dialog. Only the steps still needed are shown
 * (the shell decides which, with the same rules the classic wizard had);
 * the account step appears when there is no account yet and can be left
 * for later.
 */
Rectangle {
    id: root

    property var shell: null
    // Shown steps, fixed when the flow opens so it does not reshuffle as
    // each choice is saved.
    property var steps: []
    property int index: 0
    readonly property string step: steps.length > 0 ? steps[Math.min(index, steps.length - 1)] : ""
    property bool active: false

    signal finished()
    signal signInRequested()

    function start() {
        var needed = root.shell && root.shell.setupSteps ? root.shell.setupSteps.slice() : []
        if (needed.length === 0)
            return
        if (root.shell.accountCount === 0)
            needed.push("account")
        needed.push("done")
        root.steps = needed
        root.index = 0
        root.active = true
        if (needed.indexOf("java") >= 0)
            root.shell.detectJava()
    }

    function next() {
        if (root.step === "language" || root.step === "java")
            root.shell.finishSetupStep(root.step)
        if (root.index < root.steps.length - 1)
            root.index++
        else
            finish()
    }

    function finish() {
        root.active = false
        root.finished()
    }

    function stepTitle(id) {
        switch (id) {
        case "language": return qsTr("Language")
        case "java": return qsTr("Java & memory")
        case "account": return qsTr("Account")
        case "done": return qsTr("Ready")
        default: return id
        }
    }

    anchors.fill: parent
    z: 800
    visible: active
    color: Theme.palette.canvas

    // Swallow everything underneath while the flow is open.
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Brand side: who we are and where we are in the flow.
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(380, root.width * 0.34)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0, 0.9, 1, Theme.dark ? 0.16 : 0.22) }
                GradientStop { position: 0.55; color: Theme.palette.surface }
                GradientStop { position: 1.0; color: Qt.rgba(1, 0, 0.24, Theme.dark ? 0.12 : 0.14) }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.space.xxl
                spacing: Theme.space.lg

                Image {
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                    source: "qrc:/icons/multimc/scalable/instances/meshmc.svg"
                    sourceSize: Qt.size(128, 128)
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Welcome to MeshMC")
                    wrapMode: Text.Wrap
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.display.pixelSize + 4
                    font.weight: Font.Bold
                    font.letterSpacing: -0.6
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("A couple of choices and you're playing.")
                    wrapMode: Text.Wrap
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize + 1
                }

                Column {
                    Layout.topMargin: Theme.space.xl
                    spacing: Theme.space.md
                    Repeater {
                        model: root.steps
                        delegate: Row {
                            required property string modelData
                            required property int index
                            readonly property bool current: index === root.index
                            readonly property bool doneStep: index < root.index
                            spacing: Theme.space.md

                            Rectangle {
                                width: 28; height: 28; radius: 14
                                color: parent.current ? Theme.palette.accent
                                     : parent.doneStep ? Theme.palette.accentSubtle : "transparent"
                                border.width: parent.current || parent.doneStep ? 0 : 1
                                border.color: Theme.palette.borderStrong
                                Text {
                                    anchors.centerIn: parent
                                    visible: !parent.parent.doneStep
                                    text: parent.parent.index + 1
                                    color: parent.parent.current ? Theme.palette.textOnAccent : Theme.palette.textTertiary
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.label.pixelSize
                                    font.weight: Font.Bold
                                }
                                MeshIcon {
                                    anchors.centerIn: parent
                                    visible: parent.parent.doneStep
                                    iconName: "check"
                                    size: Theme.icon.sm
                                    color: Theme.palette.accent
                                }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.stepTitle(modelData)
                                color: parent.current ? Theme.palette.textPrimary : Theme.palette.textTertiary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.body.pixelSize
                                font.weight: parent.current ? Font.DemiBold : Font.Medium
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // The step itself.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.space.xxl
                anchors.topMargin: Theme.space.xxl + Theme.space.lg
                spacing: Theme.space.lg

                Text {
                    text: root.step === "language" ? qsTr("Choose your language")
                        : root.step === "java" ? qsTr("Java and memory")
                        : root.step === "account" ? qsTr("Sign in to play online")
                        : qsTr("You're all set")
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.display.pixelSize
                    font.weight: Font.Bold
                }
                Text {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 640
                    wrapMode: Text.Wrap
                    text: root.step === "language" ? qsTr("MeshMC switches as soon as you pick one. You can change it later in Settings.")
                        : root.step === "java" ? qsTr("Minecraft runs on Java. Pick an installed one, or let MeshMC download the right version for each game automatically.")
                        : root.step === "account" ? qsTr("Use the Microsoft account that owns Minecraft. You can also do this later from the sidebar.")
                        : qsTr("Create an instance, or find a modpack in Discover.")
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize + 1
                    lineHeight: 1.3
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.maximumWidth: 720
                    currentIndex: ["language", "java", "account", "done"].indexOf(root.step)

                    // Language
                    ColumnLayout {
                        spacing: Theme.space.md
                        SearchField {
                            id: languageSearch
                            Layout.fillWidth: true
                            placeholderText: qsTr("Search languages")
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: Theme.radius.lg
                            color: Theme.palette.surface
                            border.width: 1
                            border.color: Theme.palette.border
                            ListView {
                                id: languageList
                                anchors.fill: parent
                                anchors.margins: Theme.space.xs
                                clip: true
                                model: root.shell ? root.shell.languages : null
                                ScrollBar.vertical: ScrollBar {}
                                delegate: AbstractButton {
                                    id: languageRow
                                    required property string languageKey
                                    required property string name
                                    required property var completeness
                                    readonly property bool matches: languageSearch.text.length === 0
                                        || name.toLowerCase().indexOf(languageSearch.text.toLowerCase()) >= 0
                                        || languageKey.toLowerCase().indexOf(languageSearch.text.toLowerCase()) >= 0
                                    readonly property bool chosen: SettingsStore.string("Language") === languageKey
                                    width: languageList.width - Theme.space.md
                                    height: matches ? Theme.control.heightLg : 0
                                    visible: matches
                                    hoverEnabled: true
                                    onClicked: root.shell.selectLanguage(languageKey)
                                    background: Rectangle {
                                        radius: Theme.radius.md
                                        color: languageRow.chosen ? Theme.palette.accentSubtle
                                             : languageRow.hovered ? Theme.palette.hoverOverlay : "transparent"
                                    }
                                    contentItem: RowLayout {
                                        spacing: Theme.space.md
                                        Text {
                                            Layout.leftMargin: Theme.space.md
                                            Layout.fillWidth: true
                                            text: languageRow.name
                                            // One edge for every name, right-to-left scripts too.
                                            horizontalAlignment: Text.AlignLeft
                                            elide: Text.ElideRight
                                            color: languageRow.chosen ? Theme.palette.accent : Theme.palette.textPrimary
                                            font.family: Theme.font.family
                                            font.pixelSize: Theme.type.body.pixelSize
                                            font.weight: languageRow.chosen ? Font.DemiBold : Font.Normal
                                        }
                                        Text {
                                            Layout.rightMargin: Theme.space.md
                                            visible: Number(languageRow.completeness) > 0 && Number(languageRow.completeness) < 1
                                            text: qsTr("%1% translated").arg(Math.round(Number(languageRow.completeness) * 100))
                                            color: Theme.palette.textTertiary
                                            font.family: Theme.font.family
                                            font.pixelSize: Theme.type.caption.pixelSize
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Java and memory
                    ColumnLayout {
                        spacing: Theme.space.md
                        SettingsGroup {
                            Layout.fillWidth: true
                            SettingSwitch {
                                key: "JavaAutoDownload"
                                label: qsTr("Download Java automatically")
                                description: qsTr("Recommended. Each Minecraft version gets the Java it needs.")
                            }
                            MemorySetting {
                                label: qsTr("Maximum memory")
                                hint: qsTr("How much memory games may use.")
                                systemMiB: root.shell && root.shell.systemMemoryMiB ? root.shell.systemMemoryMiB : 8192
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Java on this computer")
                                color: Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.title.pixelSize
                                font.weight: Font.Bold
                            }
                            Button {
                                flat: true
                                text: qsTr("Detect again")
                                icon.source: Icons.url("refresh")
                                enabled: !!root.shell && !root.shell.javaDetecting
                                onClicked: root.shell.detectJava()
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: Theme.radius.lg
                            color: Theme.palette.surface
                            border.width: 1
                            border.color: Theme.palette.border
                            ListView {
                                id: javaList
                                anchors.fill: parent
                                anchors.margins: Theme.space.xs
                                clip: true
                                model: root.shell ? root.shell.javaInstalls : null
                                ScrollBar.vertical: ScrollBar {}
                                delegate: AbstractButton {
                                    id: javaRow
                                    required property string path
                                    required property var version
                                    required property var architecture
                                    required property bool recommended
                                    readonly property bool chosen: SettingsStore.string("JavaPath") === path
                                    width: javaList.width - Theme.space.md
                                    height: 52
                                    hoverEnabled: true
                                    onClicked: root.shell.useJava(path)
                                    background: Rectangle {
                                        radius: Theme.radius.md
                                        color: javaRow.chosen ? Theme.palette.accentSubtle
                                             : javaRow.hovered ? Theme.palette.hoverOverlay : "transparent"
                                    }
                                    contentItem: RowLayout {
                                        spacing: Theme.space.md
                                        Column {
                                            Layout.leftMargin: Theme.space.md
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Text {
                                                text: qsTr("Java %1").arg(String(javaRow.version))
                                                color: javaRow.chosen ? Theme.palette.accent : Theme.palette.textPrimary
                                                font.family: Theme.font.family
                                                font.pixelSize: Theme.type.body.pixelSize
                                                font.weight: Font.DemiBold
                                            }
                                            Text {
                                                width: parent.width
                                                text: javaRow.path
                                                elide: Text.ElideMiddle
                                                color: Theme.palette.textTertiary
                                                font.family: Theme.font.mono
                                                font.pixelSize: Theme.type.caption.pixelSize
                                            }
                                        }
                                        Tag { text: String(javaRow.architecture) }
                                        StatusBadge {
                                            Layout.rightMargin: Theme.space.md
                                            visible: javaRow.recommended
                                            tone: "success"
                                            text: qsTr("Recommended")
                                        }
                                    }
                                }
                                BusyIndicator {
                                    anchors.centerIn: parent
                                    running: !!root.shell && root.shell.javaDetecting
                                    visible: running
                                }
                                Text {
                                    anchors.centerIn: parent
                                    width: parent.width - Theme.space.xxl * 2
                                    visible: javaList.count === 0 && !!root.shell && !root.shell.javaDetecting
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                    text: SettingsStore.bool("JavaAutoDownload")
                                          ? qsTr("No Java found on this computer. That's fine: MeshMC downloads the right one the first time you play.")
                                          : qsTr("No Java found on this computer. Turn on automatic downloads above, or install Java and detect again.")
                                    color: Theme.palette.textTertiary
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.label.pixelSize
                                    lineHeight: 1.3
                                }
                            }
                        }
                    }

                    // Account
                    ColumnLayout {
                        spacing: Theme.space.md
                        Button {
                            highlighted: true
                            text: qsTr("Sign in with Microsoft")
                            icon.source: Icons.url("user")
                            onClicked: {
                                root.finish()
                                root.signInRequested()
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    // Done
                    ColumnLayout {
                        spacing: Theme.space.md
                        MeshIcon {
                            iconName: "check"
                            size: 56
                            color: Theme.palette.success
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 720
                    spacing: Theme.space.sm
                    Button {
                        flat: true
                        visible: root.index > 0 && root.step !== "done"
                        text: qsTr("Back")
                        icon.source: Icons.url("chevron-left")
                        onClicked: root.index--
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        flat: true
                        visible: root.step === "account"
                        text: qsTr("Later")
                        onClicked: root.next()
                    }
                    Button {
                        highlighted: true
                        visible: root.step !== "account"
                        text: root.step === "done" ? qsTr("Start playing") : qsTr("Continue")
                        icon.source: root.step === "done" ? Icons.url("play") : ""
                        onClicked: root.next()
                    }
                }
            }
        }
    }
}
