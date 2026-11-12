// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Kite.Theme

/* A new instance: "create" (version and optional loader), "import" (a local .zip/.mrpack or a URL), "code" (a
 * friend's share code, ShareCodeImportPane) or "launchers" (instances other launchers have, LauncherImportPane). */
AppDialog {
    id: root

    // NewInstanceController; set when the dialog opens so the version list is fetched lazily.
    property var controller: null
    // TaskWatcher of the creation/import in progress.
    property var watcher: null
    property bool nameEdited: false
    // IconList model for the icon picker; null hides the picker.
    property var iconsModel: null
    property string selectedIcon: "default"

    // "create", "import", "code" or "launchers"; set before open().
    property string mode: "create"
    // QmlShell.shareCodes (ShareCodesController); null hides the "Share code" tab.
    property var shareCodes: null
    // The QML shell: Kite Performance version support, its bundle catalog and speedUp().
    property var shell: null
    // Kite Performance is the picked type (Fabric plus the performance mods); another type turns it
    // off.
    property bool performanceMode: false
    // Catalog mod project ids picked in the mod picker; null for the catalog defaults.
    property var modPicks: null
    property string importSource: ""
    // QmlShell.launcherImport; null hides the "From another launcher" tab.
    readonly property var launcherImport: root.shell ? root.shell.launcherImport : null
    // Starting value of the import field: a file or link handed in from outside (`kite --import`).
    property string presetSource: ""

    signal created()
    // A Kite Performance instance was created and its mods install in the background.
    signal openInstanceRequested(string id)

    readonly property bool creating: !!watcher && watcher.running
    // The Kite Performance pick only applies to creating.
    readonly property bool performanceCreate: root.mode === "create" && root.performanceMode
    // Version-list rows that always stay visible; a shorter window scrolls the dialog.
    readonly property int versionListRows: 4
    readonly property int versionListMinHeight: versionListRows * Theme.control.height + Theme.space.xs * 2 + 2
    readonly property int kindColumnWidth: 296
    // Why Create or Import is off, in a line; "" when it is on (or the drop zone already says why).
    readonly property string createBlocker: !root.controller ? ""
        : root.controller.selectedMinecraftVersion.length === 0 ? qsTr("Pick a Minecraft version.")
        : nameField.text.trim().length === 0 ? qsTr("Give the instance a name.") : ""
    readonly property string codeBlocker: !codePane.preview ? qsTr("Enter a share code.")
        : nameField.text.trim().length === 0 ? qsTr("Give the instance a name.") : ""
    readonly property string importBlocker: !root.controller ? ""
        : root.importSource.trim().length === 0 ? qsTr("Choose a modpack file or paste a link.")
        : nameField.text.trim().length === 0 ? qsTr("Give the instance a name.") : ""
    // Loader logos come from knownModLoaders() via the instanceicon provider; `fit` evens out how
    // big square and airy marks read.
    readonly property var loaders: [
        { value: "", label: qsTr("Vanilla"), iconKey: "grass", fit: 0.78 },
        { value: "fabric", label: "Fabric", iconKey: "fabricmc", fit: 1.0 },
        { value: "quilt", label: "Quilt", iconKey: "quilt_mark", fit: 0.86 },
        { value: "forge", label: "Forge", iconKey: "forge_mark", fit: 1.0 },
        { value: "neoforge", label: "NeoForge", iconKey: "neoforged", fit: 0.78 }
    ]

    TextMetrics {
        id: dateSample
        font.family: Theme.font.family
        font.pixelSize: Theme.type.caption.pixelSize
        text: Format.calendarDate(new Date(2025, 4, 30))
    }

    function loaderLabel(value) {
        for (var i = 0; i < root.loaders.length; ++i) {
            if (root.loaders[i].value === value)
                return root.loaders[i].label
        }
        return ""
    }

    // Start on the newest listed version so Create works at once; an explicit pick is never overridden.
    function selectDefaultVersion() {
        if (!root.controller || root.controller.selectedMinecraftVersion.length > 0)
            return
        var first = root.controller.minecraftVersions.firstVersionId
        if (first && first.length > 0)
            root.controller.selectMinecraftVersion(first)
    }

    // The controller names vanilla by its bare version ("26.3"), which reads as a number; it gets
    // the game's name in front.
    function refreshSuggestedName() {
        if (root.nameEdited || !root.controller || root.mode !== "create")
            return
        var suggested = root.controller.suggestedName()
        if (root.performanceMode)
            suggested = qsTr("Kite Performance %1").arg(root.controller.selectedMinecraftVersion)
        else if (suggested.length > 0 && root.controller.loader.length === 0)
            suggested = qsTr("Minecraft %1").arg(suggested)
        nameField.text = suggested
    }

    // Import names the instance after the pack once picked; until then the field stays empty.
    function refreshImportName() {
        if (root.nameEdited || !root.controller)
            return
        nameField.text = root.controller.suggestedNameForImportSource(root.importSource)
    }

    onPerformanceModeChanged: root.refreshSuggestedName()

    onModeChanged: {
        if (root.mode === "import")
            refreshImportName()
        else if (root.mode === "code")
            refreshCodeName()
        else
            refreshSuggestedName()
    }

    // A code names its instance after what its sharer called it, until a name is typed.
    function refreshCodeName() {
        if (!root.nameEdited)
            nameField.text = codePane.preview ? codePane.preview.name : ""
    }

    function createFromCode() {
        root.watcher = root.shareCodes.createInstance(nameField.text.trim(), groupField.text.trim(), root.selectedIcon)
    }

    // A share code typed into the "Share code" tab (KITE_QML_ROUTE "sharecode="); one handed in before the dialog
    // shows waits for it, as opening clears the tab.
    property string presetCode: ""
    function enterShareCode(code) {
        root.mode = "code"
        if (root.opened)
            codePane.enter(code)
        else
            root.presetCode = code
    }
    // KITE_QML_ROUTE "sharelist": the code's contents listed once they are named.
    function showShareContents() {
        codePane.contentsShown = true
    }
    onOpened: {
        if (root.presetCode.length > 0)
            codePane.enter(root.presetCode)
        root.presetCode = ""
    }

    function setImportSource(source) {
        root.importSource = source
        sourceField.text = source
        refreshImportName()
    }

    function pickKind(kind) {
        if (kind === kindList.performanceKind) {
            root.controller.loader = "fabric"
            root.performanceMode = true
        } else {
            root.performanceMode = false
            root.controller.loader = kind
        }
    }

    // Kite Performance supports releases from 1.16 on.
    function versionFits(version) {
        return !root.performanceMode || (!!root.shell && root.shell.performanceSupported(version))
    }

    function catalogMods() {
        return root.shell && root.shell.bundles ? root.shell.bundles.mods(["performance", "comfort"], "fabric") : []
    }
    function pickedProjects() {
        return root.modPicks !== null ? root.modPicks
             : root.catalogMods().filter((mod) => mod.isDefault === true).map((mod) => mod.project)
    }
    function pickedMods() {
        const picked = root.pickedProjects()
        return root.catalogMods().filter((mod) => picked.indexOf(mod.project) >= 0)
    }
    function openModPicker() {
        modPicker.mods = root.catalogMods()
        modPicker.openWith(root.pickedProjects())
    }

    // A Kite Performance instance leaves the game files for the first Play, so the dialog does not
    // wait on them.
    function create() {
        root.watcher = root.controller.create(nameField.text.trim(), groupField.text.trim(), root.selectedIcon,
                                              !root.performanceCreate)
    }

    // Sets memory and options.txt, then installs the mods in the background. The performance bundle's
    // turned-off defaults are excluded and its other picks included for good; other bundles' picks install once.
    function installPerformance() {
        const id = root.watcher.instanceId
        const picked = root.pickedProjects()
        const inPerformance = (mod) => mod.bundle === "performance"
        const isPicked = (mod) => picked.indexOf(mod.project) >= 0
        const mods = root.catalogMods()
        root.shell.bundles.applyPresetDefaults(id, "performance")
        root.shell.setPerformanceExcluded(id, mods.filter((mod) => inPerformance(mod) && mod.isDefault === true && !isPicked(mod))
                                                 .map((mod) => mod.project))
        root.shell.setPerformanceIncluded(id, mods.filter((mod) => inPerformance(mod) && mod.isDefault !== true && isPicked(mod))
                                                 .map((mod) => mod.project))
        root.shell.speedUp(id, mods.filter((mod) => !inPerformance(mod) && isPicked(mod))
                                   .map((mod) => mod.project))
        root.close()
        root.openInstanceRequested(id)
    }

    function typeLabel(type) {
        switch (type) {
        case "release": return qsTr("Release")
        case "snapshot": return qsTr("Snapshot")
        case "old_beta": return qsTr("Beta")
        case "old_alpha": return qsTr("Alpha")
        default: return type
        }
    }

    size: "xl"
    // Taller windows get a taller version list up to this; a short one scrolls the form.
    readonly property int maxHeight: 760
    height: Math.min(root.maxHeight, parent ? parent.height - Theme.space.xxl * 2 : root.maxHeight)
    closePolicy: root.creating ? Popup.NoAutoClose : Popup.CloseOnEscape
    title: root.mode === "import" ? qsTr("Import instance") : qsTr("New instance")

    // root.mode is left alone: a pick made right after open() must not be reset.
    onAboutToShow: {
        root.watcher = null
        root.performanceMode = false
        root.modPicks = null
        root.nameEdited = false
        root.selectedIcon = "default"
        groupField.text = ""
        root.importSource = root.presetSource
        sourceField.text = root.presetSource
        codePane.reset()
        selectDefaultVersion()
        if (root.mode === "import")
            refreshImportName()
        else
            refreshSuggestedName()
    }

    Connections {
        target: root.controller ? root.controller.minecraftVersions : null
        ignoreUnknownSignals: true
        function onFirstVersionIdChanged() { root.selectDefaultVersion() }
    }

    Connections {
        target: root.controller
        ignoreUnknownSignals: true
        function onSelectedMinecraftVersionChanged() { root.refreshSuggestedName() }
        function onSelectedLoaderVersionChanged() { root.refreshSuggestedName() }
        function onLoaderChanged() { root.refreshSuggestedName() }
    }

    Connections {
        target: root.watcher
        ignoreUnknownSignals: true
        function onFinished(ok) {
            if (!ok)
                return
            if (root.performanceCreate && root.watcher.instanceId)
                root.installPerformance()
            else {
                root.close()
                root.created()
            }
        }
    }

    // Scrolls on a short window so the version list keeps its rows.
    contentItem: Flickable {
        id: scroller
        clip: true
        contentWidth: width
        contentHeight: body.height
        boundsBehavior: Flickable.StopAtBounds
        // Always visible while the form overflows, so a short window shows there is more below.
        readonly property bool overflowing: body.implicitHeight > scroller.height
        ScrollBar.vertical: ScrollBar {
            policy: scroller.overflowing ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        }

    ColumnLayout {
        id: body
        // Leaves the always-on scroll bar its own strip.
        width: scroller.width - (scroller.overflowing ? Theme.space.md : 0)
        height: Math.max(scroller.height, implicitHeight)
        spacing: Theme.space.lg

        TabStrip {
            Layout.fillWidth: true
            enabled: !root.creating
            tabs: [
                { id: "create", label: qsTr("Create") },
                { id: "import", label: qsTr("Import") }
            ].concat(root.launcherImport ? [{ id: "launchers", label: qsTr("From another launcher") }] : [])
             .concat(root.shareCodes ? [{ id: "code", label: qsTr("Share code") }] : [])
            current: root.mode
            onActivated: (id) => root.mode = id
        }

        // Instances from other launchers keep their own names.
        RowLayout {
            Layout.fillWidth: true
            visible: root.mode !== "launchers"
            spacing: Theme.space.md

            AbstractButton {
                id: iconButton
                Layout.alignment: Qt.AlignBottom
                Layout.preferredWidth: nameColumn.height
                Layout.preferredHeight: nameColumn.height
                enabled: !root.creating
                hoverEnabled: true
                Accessible.name: qsTr("Instance icon")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Change icon")
                onClicked: iconPicker.open()

                background: InstanceTile {
                    radius: Theme.radius.sm
                    borderColor: iconButton.hovered ? Theme.palette.borderStrong : Theme.palette.border
                    Behavior on borderColor { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                }
                contentItem: Item {
                    Image {
                        anchors.centerIn: parent
                        width: Math.round(iconButton.height * 0.62)
                        height: width
                        source: Icons.instanceIcon(root.selectedIcon)
                        sourceSize: Qt.size(width, height)
                        fillMode: Image.PreserveAspectFit
                    }
                    BoxSurface {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: Theme.space.xxs
                        width: Theme.icon.md
                        height: width
                        radius: Theme.radius.sm
                        color: Theme.palette.surfaceOverlay
                        MeshIcon {
                            anchors.centerIn: parent
                            iconName: "edit"
                            size: Theme.icon.xxs
                            color: Theme.palette.textSecondary
                        }
                    }
                }
            }

            Column {
                id: nameColumn
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignBottom
                spacing: Theme.space.xs
                FieldLabel { text: qsTr("Name") }
                TextField {
                    id: nameField
                    width: parent.width
                    enabled: !root.creating
                    selectByMouse: true
                    placeholderText: root.mode === "import" ? qsTr("Named after the pack") : ""
                    onTextEdited: root.nameEdited = text.length > 0
                }
            }

            Column {
                Layout.preferredWidth: 220
                Layout.alignment: Qt.AlignBottom
                spacing: Theme.space.xs
                FieldLabel { text: qsTr("Group") }
                TextField {
                    id: groupField
                    width: parent.width
                    enabled: !root.creating
                    selectByMouse: true
                    placeholderText: qsTr("No group")
                    text: ""
                }
            }

        }

        IconPickerDialog {
            id: iconPicker
            iconsModel: root.iconsModel
            current: root.selectedIcon
            onPicked: (key) => root.selectedIcon = key
        }

        ModPickerSheet {
            id: modPicker
            onPicked: (projects) => root.modPicks = projects
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.mode === "launchers" ? 3 : root.mode === "code" ? 2 : root.mode === "import" ? 1 : 0

        RowLayout {
            spacing: Theme.space.xl

        ColumnLayout {
            Layout.fillWidth: false
            Layout.preferredWidth: root.kindColumnWidth
            Layout.fillHeight: true
            spacing: Theme.space.md

            // Matches the version column's heading height so both lists start level.
            Copy {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.control.heightSm
                verticalAlignment: Text.AlignVCenter
                text: qsTr("Type")
                role: "title"
                font.weight: Font.Bold
            }

            NewInstanceKindList {
                id: kindList
                Layout.fillWidth: true
                enabled: !root.creating && !!root.controller
                loaders: root.loaders
                current: root.performanceMode ? kindList.performanceKind
                       : root.controller ? root.controller.loader : ""
                performanceAvailable: !!root.shell && !!root.controller
                                      && root.shell.performanceSupported(root.controller.selectedMinecraftVersion)
                pickedMods: root.performanceMode ? root.pickedMods() : []
                onPicked: (kind) => root.pickKind(kind)
                onEditModsRequested: root.openModPicker()
            }

            ComboBox {
                id: loaderVersionBox
                Layout.fillWidth: true
                visible: !!root.controller && root.controller.loader.length > 0
                enabled: !root.creating && count > 0
                model: root.controller ? root.controller.loaderVersions : null
                textRole: "version"
                valueRole: "versionId"
                displayText: root.controller && root.controller.loaderLoading ? qsTr("Loading…")
                           : count === 0 ? qsTr("None for this version")
                           : root.loaderLabel(root.controller.loader) + " "
                             + (currentIndex >= 0 ? currentText : root.controller.selectedLoaderVersion)
                // The controller may pick before the rows arrive; line the two up whenever either
                // moves.
                function syncToController() {
                    if (root.controller)
                        currentIndex = indexOfValue(root.controller.selectedLoaderVersion)
                }
                onCountChanged: syncToController()
                onActivated: root.controller.selectLoaderVersion(currentValue)
                Connections {
                    target: root.controller
                    ignoreUnknownSignals: true
                    function onSelectedLoaderVersionChanged() { loaderVersionBox.syncToController() }
                }
            }

            Item { Layout.fillHeight: true }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.control.heightSm
            spacing: Theme.space.sm

            Copy {
                Layout.fillWidth: true
                text: qsTr("Minecraft version")
                role: "title"
                font.weight: Font.Bold
            }
            IconButton {
                id: filterButton
                readonly property bool filtering: !!root.controller
                    && (root.controller.minecraftVersions.showSnapshots || root.controller.minecraftVersions.showOldVersions)
                enabled: !!root.controller
                iconName: "filter"
                tip: qsTr("Snapshots and old versions")
                size: Theme.control.heightSm
                icon.color: filtering ? Theme.palette.accentText
                          : hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
                onClicked: filterMenu.popup(filterButton, 0, filterButton.height + Theme.space.xs)
            }
            Menu {
                id: filterMenu
                MenuItem {
                    text: qsTr("Snapshots")
                    checkable: true
                    checked: root.controller ? root.controller.minecraftVersions.showSnapshots : false
                    onToggled: root.controller.minecraftVersions.showSnapshots = checked
                }
                MenuItem {
                    text: qsTr("Old versions")
                    checkable: true
                    checked: root.controller ? root.controller.minecraftVersions.showOldVersions : false
                    onToggled: root.controller.minecraftVersions.showOldVersions = checked
                }
            }
        }

        BoxSurface {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.versionListMinHeight
            Layout.preferredHeight: root.versionListMinHeight
            color: "transparent"

            ListView {
                id: versions
                anchors.fill: parent
                anchors.margins: Theme.space.xs
                clip: true
                enabled: !root.creating
                boundsBehavior: Flickable.StopAtBounds
                model: root.controller ? root.controller.minecraftVersions : null
                ScrollBar.vertical: ScrollBar {}

                delegate: AbstractButton {
                    id: versionRow
                    required property string version
                    required property string type
                    required property bool recommended
                    required property var time
                    readonly property bool selected: !!root.controller && root.controller.selectedMinecraftVersion === version

                    // Full width: the scroll bar overlays the rows.
                    width: versions.width
                    height: Theme.control.height
                    hoverEnabled: true
                    enabled: root.versionFits(version)
                    onClicked: root.controller.selectMinecraftVersion(version)

                    background: StateFill {
                        radius: Theme.radius.md
                        hovered: versionRow.hovered
                        active: versionRow.selected
                        activeFill: Theme.palette.surfaceHover
                        border.width: 1
                        border.color: versionRow.selected ? Theme.neutral.borderHover : "transparent"
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space.md
                        Copy {
                            Layout.leftMargin: Theme.space.md
                            text: versionRow.version
                            tone: versionRow.enabled ? "primary" : "tertiary"
                            font.weight: versionRow.selected ? Font.Bold : Font.Medium
                        }
                        // Releases need no badge; a snapshot or old beta/alpha gets one.
                        StatusBadge {
                            visible: versionRow.type !== "release"
                            tone: versionRow.type === "snapshot" ? "warning" : "neutral"
                            text: root.typeLabel(versionRow.type)
                        }
                        // Neutral, so it does not turn green on the selected row.
                        StatusBadge {
                            visible: versionRow.recommended
                            text: qsTr("Recommended")
                        }
                        Item { Layout.fillWidth: true }
                        // One width for every date, so the tick stands in one column.
                        Copy {
                            Layout.preferredWidth: dateSample.advanceWidth
                            horizontalAlignment: Text.AlignRight
                            text: versionRow.time ? Format.calendarDate(new Date(versionRow.time)) : ""
                            role: "caption"
                            tone: "tertiary"
                        }
                        // Faded rather than hidden, so the dates keep their column.
                        MeshIcon {
                            Layout.rightMargin: Theme.space.md
                            opacity: versionRow.selected ? 1 : 0
                            iconName: "check"
                            size: Theme.icon.sm
                            color: Theme.palette.accentText
                        }
                    }
                }
            }

            ScrollFade {
                anchors.left: versions.left
                anchors.right: versions.right
                anchors.top: versions.top
                height: Theme.space.lg
                atTop: true
                flickable: versions
                fadeTo: Theme.glass.popup
            }
            ScrollFade {
                anchors.left: versions.left
                anchors.right: versions.right
                anchors.bottom: versions.bottom
                flickable: versions
                fadeTo: Theme.glass.popup
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: !!root.controller && root.controller.minecraftVersions.loading
                visible: running
            }
            Copy {
                anchors.centerIn: parent
                width: parent.width - Theme.space.xxl * 2
                horizontalAlignment: Text.AlignHCenter
                visible: !!root.controller && root.controller.minecraftVersions.error.length > 0
                text: root.controller ? root.controller.minecraftVersions.error : ""
                role: "label"
                tone: "danger"
                wrapMode: Text.Wrap
            }
        }

        } // version column
        } // create pane

        ColumnLayout {
            spacing: Theme.space.lg

            Rectangle {
                id: dropTarget
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radius.lg
                color: "transparent"

                // A Canvas draws the same on every render backend.
                Canvas {
                    anchors.fill: parent
                    readonly property real stroke: dropArea.containsDrag ? 2 : 1
                    readonly property color ink: dropArea.containsDrag ? Theme.palette.accent : Theme.palette.borderStrong
                    onStrokeChanged: requestPaint()
                    onInkChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        var h = stroke / 2
                        var r = dropTarget.radius
                        ctx.strokeStyle = ink
                        ctx.lineWidth = stroke
                        ctx.setLineDash([6 * stroke, 5 * stroke])
                        ctx.beginPath()
                        ctx.moveTo(h + r, h)
                        ctx.arcTo(width - h, h, width - h, height - h, r)
                        ctx.arcTo(width - h, height - h, h, height - h, r)
                        ctx.arcTo(h, height - h, h, h, r)
                        ctx.arcTo(h, h, width - h, h, r)
                        ctx.closePath()
                        ctx.stroke()
                    }
                }

                DropArea {
                    id: dropArea
                    anchors.fill: parent
                    enabled: !root.creating
                    onDropped: (drop) => {
                        if (drop.urls && drop.urls.length > 0)
                            root.setImportSource(drop.urls[0].toString())
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(460, dropTarget.width - Theme.space.xxl * 2)
                    spacing: Theme.space.md

                    MeshIcon {
                        Layout.alignment: Qt.AlignHCenter
                        iconName: "package"
                        size: Theme.icon.xl
                        color: Theme.palette.textTertiary
                    }

                    Copy {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Drop a .zip or .mrpack file here")
                        tone: "secondary"
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space.sm

                        TextField {
                            id: sourceField
                            Layout.fillWidth: true
                            enabled: !root.creating
                            selectByMouse: true
                            placeholderText: qsTr("Or paste a direct download link (https://…)")
                            onTextEdited: {
                                root.importSource = text
                                root.refreshImportName()
                            }
                        }
                        // A bordered button: a flat one's invisible side padding pulled the field
                        // and button left of the drop zone's middle.
                        Button {
                            enabled: !root.creating
                            text: qsTr("Browse…")
                            icon.source: Icons.url("folder")
                            onClicked: importFileDialog.open()
                        }
                    }

                    Copy {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        visible: !root.creating && root.importSource.trim().length > 0
                                 && !!root.controller
                                 && !root.controller.isImportSourceValid(root.importSource.trim())
                        text: qsTr("This doesn't look like a modpack file or link. Pick a .zip, .mrpack or .jar file, or paste a direct download link.")
                        role: "caption"
                        tone: "danger"
                    }
                }
            }

            Copy {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("Packs exported from CurseForge, FTB, ATLauncher or Technic import the same way, from their .zip file.")
                role: "caption"
                tone: "tertiary"
            }
        } // import pane

        ShareCodeImportPane {
            id: codePane
            controller: root.shareCodes
            locked: root.creating
            onPreviewChanged: root.refreshCodeName()
        }

        LauncherImportPane {
            id: launcherPane
            controller: root.launcherImport
        }
        } // StackLayout

        FileDialog {
            id: importFileDialog
            title: qsTr("Choose an instance to import")
            nameFilters: [qsTr("Modpack archives (*.zip *.mrpack)"), qsTr("All files (*)")]
            onAccepted: root.setImportSource(selectedFile.toString())
        }
    }
    } // scroller

    footer: RowLayout {
        spacing: Theme.space.sm

        Column {
            Layout.leftMargin: Theme.space.xl
            Layout.bottomMargin: Theme.space.xl
            Layout.fillWidth: true
            visible: root.creating || (!!root.watcher && root.watcher.failed)
            spacing: Theme.space.xs
            Copy {
                width: parent.width
                role: "label"
                tone: root.watcher && root.watcher.failed ? "danger" : "secondary"
                text: root.watcher && root.watcher.failed ? (root.watcher.error ||
                          (root.mode === "import" ? qsTr("Importing the instance failed.") : qsTr("Creating the instance failed.")))
                    : root.watcher ? (root.watcher.status ||
                          (root.mode === "import" ? qsTr("Importing…") : qsTr("Creating…"))) : ""
                elide: Text.ElideRight
            }
            LaunchProgressBar {
                width: Math.min(parent.width, 320)
                visible: root.creating
                progress: root.watcher ? root.watcher.progress : -1
            }
        }

        Copy {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space.xl
            Layout.bottomMargin: Theme.space.xl
            visible: !root.creating && !(root.watcher && root.watcher.failed)
            text: root.mode === "import" ? root.importBlocker : root.mode === "code" ? root.codeBlocker
                : root.mode === "launchers" ? launcherPane.summary : root.createBlocker
            role: "label"
            tone: "tertiary"
            elide: Text.ElideRight
        }

        // "Close" for other launchers: closing does not stop their import.
        Button {
            Layout.bottomMargin: Theme.space.xl
            text: root.mode === "launchers" ? qsTr("Close") : qsTr("Cancel")
            enabled: !root.creating
            onClicked: root.close()
        }
        Button {
            Layout.rightMargin: Theme.space.xl
            Layout.bottomMargin: Theme.space.xl
            highlighted: true
            visible: root.mode === "create"
            text: root.creating ? qsTr("Creating…") : qsTr("Create")
            icon.source: Icons.url("plus")
            enabled: !root.creating && !!root.controller
                     && root.controller.selectedMinecraftVersion.length > 0
                     && nameField.text.trim().length > 0
            onClicked: {
                root.create()
            }
        }
        Button {
            Layout.rightMargin: Theme.space.xl
            Layout.bottomMargin: Theme.space.xl
            highlighted: true
            visible: root.mode === "import"
            text: root.creating ? qsTr("Importing…") : qsTr("Import")
            icon.source: Icons.url("download")
            enabled: !root.creating && !!root.controller
                     && root.importSource.trim().length > 0
                     && nameField.text.trim().length > 0
                     && root.controller.isImportSourceValid(root.importSource.trim())
            onClicked: {
                root.watcher = root.controller.importFrom(root.importSource.trim(), nameField.text.trim(), groupField.text.trim(), root.selectedIcon)
            }
        }
        // The import carries on with the dialog closed; the pane shows its progress.
        Button {
            Layout.rightMargin: Theme.space.xl
            Layout.bottomMargin: Theme.space.xl
            highlighted: true
            visible: root.mode === "launchers"
            text: launcherPane.running ? qsTr("Importing…") : qsTr("Import")
            icon.source: Icons.url("download")
            enabled: launcherPane.canImport
            onClicked: launcherPane.start()
        }
        Button {
            Layout.rightMargin: Theme.space.xl
            Layout.bottomMargin: Theme.space.xl
            highlighted: true
            visible: root.mode === "code"
            text: root.creating ? qsTr("Creating…") : qsTr("Create")
            icon.source: Icons.url("plus")
            enabled: !root.creating && !!root.shareCodes && !!codePane.preview && nameField.text.trim().length > 0
            onClicked: root.createFromCode()
        }
    }
}
