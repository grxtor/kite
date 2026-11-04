// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

// First run. Shows only the steps the shell still needs.
Rectangle {
    id: root

    property var shell: null
    // QmlShell.launcherImport: other launchers' instances, offered as a step when there are any.
    readonly property var importer: root.shell ? root.shell.launcherImport : null
    // macOS: the title bar's height when it lies over the window, else 0.
    property int topInset: 0
    // Fixed when the flow opens, so saving a choice does not reshuffle it.
    property var steps: []
    property int index: 0
    readonly property string step: steps.length > 0 ? steps[Math.min(index, steps.length - 1)] : ""
    property bool active: false
    readonly property int contentTop: Math.max(Theme.space.xxl, root.topInset + Theme.space.sm)
    readonly property int skyMaxWidth: 380
    readonly property real skyShare: 0.34
    readonly property int brandSize: 64
    readonly property int stepMarkSize: 28
    // The intro line under a step's title wraps sooner than the step's content.
    readonly property int introMaxWidth: 640
    readonly property int contentMaxWidth: 720

    signal finished()
    signal signInRequested()
    signal createInstanceRequested()
    signal discoverRequested()

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
        if (root.importer && !root.importer.scanned)
            root.importer.scan()
        else
            root.offerImport()
    }

    // The look for other launchers ends after the flow opened: its step joins before the account
    // (or the end) unless the person is already past that point.
    function offerImport() {
        if (!root.active || !root.importer || root.importer.availableCount === 0
                || root.steps.indexOf("import") >= 0)
            return
        var at = root.steps.indexOf("account")
        if (at < 0)
            at = root.steps.indexOf("done")
        if (at < 0 || root.index > at)
            return
        var steps = root.steps.slice()
        steps.splice(at, 0, "import")
        root.steps = steps
    }

    Connections {
        target: root.importer
        ignoreUnknownSignals: true
        function onScanningChanged() {
            if (!root.importer.scanning)
                root.offerImport()
        }
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

    function finishTo(request) {
        root.finish()
        request()
    }

    // Opens the list on the saved language. TranslationsModel has no key
    // lookup, so the rows are searched.
    function showChosenLanguage() {
        var model = languageList.model
        var saved = SettingsStore.string("Language")
        if (!model || saved.length === 0)
            return
        for (var row = 0; row < languageList.count; ++row) {
            if (model.data(model.index(row, 0), Qt.UserRole) === saved) {
                languageList.positionViewAtIndex(row, ListView.Center)
                return
            }
        }
    }

    function stepTitle(id) {
        switch (id) {
        case "language": return qsTr("Language")
        case "java": return qsTr("Java & memory")
        case "import": return qsTr("Other launchers")
        case "account": return qsTr("Account")
        case "done": return qsTr("Ready")
        default: return ""
        }
    }

    anchors.fill: parent
    z: 800
    visible: active
    color: Theme.palette.canvas

    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: sky
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(root.skyMaxWidth, root.width * root.skyShare)

            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.palette.surfaceRaised }
                GradientStop { position: 1.0; color: Theme.palette.surface }
            }

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.palette.border
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.space.xxl
                anchors.topMargin: root.contentTop
                spacing: Theme.space.lg

                BrandMark {
                    size: root.brandSize
                    Layout.preferredWidth: size
                    Layout.preferredHeight: size
                }
                Copy {
                    Layout.fillWidth: true
                    text: qsTr("Welcome to Kite")
                    wrapMode: Text.Wrap
                    role: "subheading"
                }
                Copy {
                    Layout.fillWidth: true
                    text: qsTr("A couple of choices and you're playing.")
                    wrapMode: Text.Wrap
                    tone: "secondary"
                }

                Column {
                    Layout.topMargin: Theme.space.xl
                    spacing: Theme.space.md
                    Repeater {
                        model: root.steps
                        delegate: Row {
                            id: stepRow
                            required property string modelData
                            required property int index
                            readonly property bool current: index === root.index
                            readonly property bool doneStep: index < root.index
                            spacing: Theme.space.md

                            Rectangle {
                                width: root.stepMarkSize
                                height: root.stepMarkSize
                                radius: root.stepMarkSize / 2
                                color: stepRow.current ? Theme.palette.accent
                                     : stepRow.doneStep ? Theme.palette.accentSubtle : "transparent"
                                border.width: stepRow.current || stepRow.doneStep ? 0 : 1
                                border.color: Theme.palette.borderStrong
                                Copy {
                                    anchors.centerIn: parent
                                    visible: !stepRow.doneStep
                                    text: stepRow.index + 1
                                    role: "label"
                                    tone: stepRow.current ? "onAccent" : "tertiary"
                                    font.weight: Font.Bold
                                }
                                MeshIcon {
                                    anchors.centerIn: parent
                                    visible: stepRow.doneStep
                                    iconName: "check"
                                    size: Theme.icon.sm
                                    color: Theme.palette.accentText
                                }
                            }
                            Copy {
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.stepTitle(stepRow.modelData)
                                tone: stepRow.current ? "primary" : "tertiary"
                                font.weight: stepRow.current ? Font.DemiBold : Font.Medium
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.space.xxl
                anchors.topMargin: root.contentTop + Theme.space.lg
                spacing: Theme.space.lg

                Copy {
                    text: root.step === "language" ? qsTr("Choose your language")
                        : root.step === "java" ? root.stepTitle("java")
                        : root.step === "import" ? qsTr("Bring your instances over")
                        : root.step === "account" ? qsTr("Sign in to play online")
                        : qsTr("You're all set")
                    role: "display"
                }
                Copy {
                    Layout.fillWidth: true
                    Layout.maximumWidth: root.introMaxWidth
                    wrapMode: Text.Wrap
                    text: root.step === "language" ? qsTr("Kite switches as soon as you pick one. You can change it later in Settings.")
                        : root.step === "java" ? qsTr("Minecraft runs on Java. Pick an installed one, or let Kite download the right version for each game automatically.")
                        : root.step === "import" ? qsTr("Kite found instances from other launchers on this computer. Tick the ones to bring over: they are copied, and the originals stay as they are. Your accounts stay there too; you sign in to Kite yourself.")
                        : root.step === "account" ? qsTr("Use the Microsoft account that owns Minecraft. You can also do this later from the account button at the bottom of the left sidebar.")
                        : qsTr("Start a new instance, or find a modpack in Discover.")
                    tone: "secondary"
                    lineHeight: Theme.type.label.lineHeight
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.maximumWidth: root.contentMaxWidth
                    // Every page counts toward a StackLayout's size; none may push the footer out of a short window.
                    Layout.minimumHeight: 0
                    currentIndex: ["language", "java", "account", "done", "import"].indexOf(root.step)

                    ColumnLayout {
                        spacing: Theme.space.md
                        SearchBox {
                            id: languageSearch
                            Layout.fillWidth: true
                            placeholderText: qsTr("Search languages")
                        }
                        BoxSurface {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: Theme.control.heightLg + Theme.space.xs * 2
                            Layout.maximumHeight: Math.max(Layout.minimumHeight, languageList.contentHeight + Theme.space.xs * 2)
                            ListView {
                                id: languageList
                                anchors.fill: parent
                                anchors.margins: Theme.space.xs
                                clip: true
                                model: root.shell ? root.shell.languages : null
                                ScrollBar.vertical: ScrollBar {}
                                onCountChanged: Qt.callLater(root.showChosenLanguage)
                                Component.onCompleted: Qt.callLater(root.showChosenLanguage)
                                delegate: AbstractButton {
                                    id: languageRow
                                    required property string languageKey
                                    required property string name
                                    required property var completeness
                                    readonly property bool matches: languageSearch.text.length === 0
                                        || name.toLowerCase().indexOf(languageSearch.text.toLowerCase()) >= 0
                                        || languageKey.toLowerCase().indexOf(languageSearch.text.toLowerCase()) >= 0
                                    readonly property bool chosen: SettingsStore.string("Language") === languageKey
                                    width: languageList.width
                                    height: matches ? Theme.control.heightLg : 0
                                    visible: matches
                                    hoverEnabled: true
                                    onClicked: root.shell.selectLanguage(languageKey)
                                    background: StateFill {
                                        hovered: languageRow.hovered
                                        active: languageRow.chosen
                                        radius: Theme.radius.md
                                    }
                                    contentItem: RowLayout {
                                        spacing: Theme.space.md
                                        Copy {
                                            Layout.leftMargin: Theme.space.md
                                            Layout.fillWidth: true
                                            text: languageRow.name
                                            // Left-aligned for right-to-left scripts too.
                                            horizontalAlignment: Text.AlignLeft
                                            elide: Text.ElideRight
                                            tone: languageRow.chosen ? "accent" : "primary"
                                            font.weight: languageRow.chosen ? Font.DemiBold : Font.Normal
                                        }
                                        Copy {
                                            visible: Number(languageRow.completeness) > 0 && Number(languageRow.completeness) < 1
                                            text: qsTr("%1% translated").arg(Math.round(Number(languageRow.completeness) * 100))
                                            role: "caption"
                                            tone: "tertiary"
                                        }
                                        MeshIcon {
                                            Layout.rightMargin: Theme.space.md
                                            visible: languageRow.chosen
                                            iconName: "check"
                                            size: Theme.icon.sm
                                            color: Theme.palette.accentText
                                        }
                                    }
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    Item {
                        // Scrolls when the window is too short for all of it, so the footer stays in view. It reaches
                        // past the column by what the flat "Detect again" hangs out, so its hover and focus are not clipped.
                        Flickable {
                            id: javaFlick
                            anchors.fill: parent
                            anchors.rightMargin: -Theme.space.sm
                            contentWidth: width
                            contentHeight: javaColumn.height
                            boundsBehavior: Flickable.StopAtBounds
                            clip: true
                            ScrollBar.vertical: ScrollBar {}

                            ColumnLayout {
                                id: javaColumn
                                width: javaFlick.width - Theme.space.sm
                                height: Math.max(javaFlick.height, implicitHeight)
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
                                        systemMiB: root.shell && root.shell.systemMemoryMiB ? root.shell.systemMemoryMiB : 8192
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    GroupHeading {
                                        Layout.fillWidth: true
                                        title: qsTr("Java on this computer")
                                    }
                                    Button {
                                        flat: true
                                        // Its text ends on the column's edge; the padding hangs out.
                                        rightPadding: Theme.space.sm
                                        Layout.rightMargin: -rightPadding
                                        text: qsTr("Detect again")
                                        icon.source: Icons.url("refresh")
                                        enabled: !!root.shell && !root.shell.javaDetecting
                                        onClicked: root.shell.detectJava()
                                    }
                                }
                                JavaInstallList {
                                    id: javaList
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    // As tall as its rows.
                                    Layout.minimumHeight: javaList.minHeight
                                    Layout.maximumHeight: javaList.fitHeight
                                    model: root.shell ? root.shell.javaInstalls : null
                                    detecting: !!root.shell && !!root.shell.javaDetecting
                                    currentPath: SettingsStore.string("JavaPath")
                                    emptyText: SettingsStore.bool("JavaAutoDownload")
                                               ? qsTr("No Java found on this computer. That's fine: Kite downloads the right one the first time you play.")
                                               : qsTr("No Java found on this computer. Turn on automatic downloads above, or install Java and detect again.")
                                    onPicked: (path) => root.shell.useJava(path)
                                }
                                Item { Layout.fillHeight: true }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 0
                        RowLayout {
                            Layout.topMargin: Theme.space.lg
                            spacing: Theme.space.sm
                            Button {
                                highlighted: true
                                text: qsTr("Sign in with Microsoft")
                                onClicked: {
                                    root.finish()
                                    root.signInRequested()
                                }
                            }
                            Button {
                                flat: true
                                text: qsTr("Later")
                                onClicked: root.next()
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: 0
                        // An import started a step earlier carries on here.
                        NoticeBar {
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.space.lg
                            visible: !!root.importer && root.importer.running
                            tone: "info"
                            text: root.importer ? root.importer.status : ""
                            progress: root.importer ? root.importer.progress : -1
                            actionText: qsTr("Stop")
                            onActionTriggered: root.importer.cancel()
                        }
                        RowLayout {
                            Layout.topMargin: Theme.space.lg
                            spacing: Theme.space.sm
                            Button {
                                highlighted: true
                                text: qsTr("New instance")
                                icon.source: Icons.url("plus")
                                onClicked: root.finishTo(root.createInstanceRequested)
                            }
                            Button {
                                text: qsTr("Discover")
                                icon.source: Icons.url("compass")
                                onClicked: root.finishTo(root.discoverRequested)
                            }
                            Button {
                                flat: true
                                text: qsTr("Go to Home")
                                onClicked: root.finish()
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    LauncherImportPane {
                        id: importPane
                        controller: root.importer
                        fadeTo: Theme.palette.canvas
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.maximumWidth: root.contentMaxWidth
                    visible: root.step !== "done"
                    spacing: Theme.space.sm
                    Button {
                        // Pulled left by its padding plus the chevron's side bearing, so the chevron's ink
                        // starts on the column's edge (as in ModpackDetail's "Back to results").
                        readonly property int chevronBearing: Math.round(Theme.icon.sm / 3)
                        flat: true
                        visible: root.index > 0
                        leftPadding: Theme.space.sm
                        Layout.leftMargin: -(leftPadding + chevronBearing)
                        text: qsTr("Back")
                        icon.source: Icons.url("chevron-left")
                        onClicked: root.index--
                    }
                    // The import step's pick, next to Back so the buttons keep their room.
                    Copy {
                        Layout.fillWidth: true
                        Layout.leftMargin: root.index > 0 ? Theme.space.md : 0
                        text: root.step === "import" ? importPane.summary : ""
                        role: "label"
                        tone: "tertiary"
                        elide: Text.ElideRight
                    }
                    Button {
                        highlighted: true
                        visible: root.step === "language" || root.step === "java"
                        text: qsTr("Continue")
                        onClicked: root.next()
                    }
                    // Skipping, or moving on while the import runs: it carries on in the background.
                    Button {
                        flat: true
                        visible: root.step === "import"
                        text: importPane.running ? qsTr("Continue") : qsTr("Skip")
                        onClicked: root.next()
                    }
                    Button {
                        highlighted: true
                        visible: root.step === "import" && !importPane.running
                        text: qsTr("Import")
                        icon.source: Icons.url("download")
                        enabled: importPane.canImport
                        onClicked: {
                            importPane.start()
                            root.next()
                        }
                    }
                }
            }
        }
    }

    WindowDragBand {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.topInset
        enabled: root.topInset > 0
    }
}
