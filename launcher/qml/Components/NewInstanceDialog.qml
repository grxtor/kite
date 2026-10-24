// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Kite.Theme

/*
 * A new instance, in one of two modes:
 *  - "create": pick a Minecraft version and, if wanted, a mod loader; name
 *    it; create.
 *  - "import": a local .zip/.mrpack export (CurseForge, Modrinth, MultiMC,
 *    Prism...) or a direct download URL, staged through the same
 *    InstanceImportTask the widget dialog used.
 * Modpacks browsed by project (Modrinth) come from Discover instead; other
 * catalogue browsing (CurseForge/FTB/ATLauncher/Technic) is not built in
 * QML yet -- their exported .zip files import here (the import pane's note).
 *
 * In create mode the name follows the chosen version ("Minecraft 1.21.4",
 * "1.21.4 Fabric") until the user types one of their own; in import mode it
 * follows the picked file/URL the same way.
 */
Dialog {
    id: root

    // NewInstanceController; set when the dialog opens, so the version list
    // is only fetched once someone actually wants a new instance.
    property var controller: null
    // TaskWatcher of the creation/import in progress.
    property var watcher: null
    property bool nameEdited: false
    // IconList model for the icon picker below, or null to hide it (the
    // integrator wires the shell's iconsModel in; without it the button
    // still shows the default icon, just with nothing to pick from).
    property var iconsModel: null
    property string selectedIcon: "default"

    // "create" or "import" -- set by the integrator before open(), e.g.
    // Main.qml's openNewInstance(mode).
    property string mode: "create"
    // The QML shell: which versions have Kite Performance, and its bundle
    // catalog and speedUp() to set an instance up once it exists.
    property var shell: null
    // Where a Kite Performance instance is: "creating" it, then
    // "installing" the performance mods; empty otherwise.
    property string performanceStep: ""
    // The selected version was picked with its ⚡: Fabric, the performance
    // mods and its own name. A plain row or loader click turns it off.
    property bool performanceMode: false
    // Raw text of the chosen file/URL in import mode.
    property string importSource: ""
    // What import mode's file/URL field starts with when the dialog opens:
    // a file or link handed in from outside (`kite --import`, see
    // Main.qml's openImport()); empty for a dialog opened from a button.
    property string presetSource: ""

    signal created()

    readonly property bool creating: !!watcher && watcher.running
    readonly property int loaderTileHeight: Theme.control.heightLg
    // Rows of the version list that always stay visible; a shorter window
    // scrolls the dialog instead of squeezing the list.
    readonly property int versionListRows: 4
    readonly property int versionListMinHeight: versionListRows * Theme.control.height + Theme.space.xs * 2 + 2
    readonly property int logoSize: Theme.icon.lg
    // Each loader wears the logo the launcher already ships for it (the
    // iconName column of knownModLoaders(), served by the instanceicon
    // provider); the unmodified game gets the grass block, the default
    // instance icon. `fit` evens out how big they read: a solid square (the
    // grass block, NeoForge's framed pixel art) looks larger than a mark
    // with air around it at the same box.
    readonly property var loaders: [
        { value: "", label: qsTr("Vanilla"), iconKey: "grass", fit: 0.78 },
        { value: "fabric", label: "Fabric", iconKey: "fabricmc", fit: 1.0 },
        { value: "quilt", label: "Quilt", iconKey: "quilt_mark", fit: 0.86 },
        { value: "forge", label: "Forge", iconKey: "forge_mark", fit: 1.0 },
        { value: "neoforge", label: "NeoForge", iconKey: "neoforged", fit: 0.78 }
    ]

    // The widest date a row shows (a long month name and a two-digit day).
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

    // Start on the newest version of the list shown, so Create works
    // straight away; an explicit pick is never overridden.
    function selectDefaultVersion() {
        if (!root.controller || root.controller.selectedMinecraftVersion.length > 0)
            return
        var first = root.controller.minecraftVersions.firstVersionId
        if (first && first.length > 0)
            root.controller.selectMinecraftVersion(first)
    }

    // The controller names a vanilla instance by its bare version ("26.3"),
    // which reads as a number in the library rather than a name; it gets
    // the game's name in front. A modded one ("26.3 Fabric") already says
    // what it is.
    function refreshSuggestedName() {
        if (root.nameEdited || !root.controller || root.mode === "import")
            return
        var suggested = root.controller.suggestedName()
        if (root.performanceMode)
            suggested = qsTr("Kite Performance %1").arg(root.controller.selectedMinecraftVersion)
        else if (suggested.length > 0 && root.controller.loader.length === 0)
            suggested = qsTr("Minecraft %1").arg(suggested)
        nameField.text = suggested
    }

    // Import names the instance after the pack once one is picked; until
    // then the field stays empty (its placeholder says so) instead of
    // carrying over the create pane's "Minecraft 26.3".
    function refreshImportName() {
        if (root.nameEdited || !root.controller)
            return
        nameField.text = root.controller.suggestedNameForImportSource(root.importSource)
    }

    onPerformanceModeChanged: root.refreshSuggestedName()

    onModeChanged: {
        if (root.mode === "import")
            refreshImportName()
        else
            refreshSuggestedName()
    }

    // Called from the URL field, the file picker and the drop area alike,
    // so all three keep the field, the property and the suggested name in
    // sync with each other regardless of which one changed.
    function setImportSource(source) {
        root.importSource = source
        sourceField.text = source
        refreshImportName()
    }

    // A plain pick of @p version: Kite Performance is not part of it.
    function pickVersion(version) {
        root.performanceMode = false
        root.controller.selectMinecraftVersion(version)
    }
    // A version's ⚡: that version, on Fabric, as a Kite Performance instance.
    function pickPerformance(version) {
        root.controller.selectMinecraftVersion(version)
        root.controller.loader = "fabric"
        root.performanceMode = true
    }
    function pickLoader(value) {
        root.performanceMode = false
        root.controller.loader = value
    }

    function create() {
        root.performanceStep = root.performanceMode ? "creating" : ""
        root.watcher = root.controller.create(nameField.text.trim(), groupField.text.trim(), root.selectedIcon)
    }

    // The instance exists: its memory and options.txt first, then the mods.
    // Their download takes over the progress UI; without one the dialog is
    // done.
    function installPerformance() {
        const id = root.watcher.instanceId
        root.shell.bundles.applyPresetDefaults(id, "performance")
        const install = root.shell.speedUp(id)
        if (install) {
            root.performanceStep = "installing"
            root.watcher = install
            return
        }
        root.performanceStep = ""
        root.close()
        root.created()
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

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(860, parent ? parent.width - Theme.space.xxl * 2 : 860)
    // Taller windows get a taller version list (it fills the height), up
    // to this; a short one scrolls the form.
    readonly property int maxHeight: 760
    height: Math.min(root.maxHeight, parent ? parent.height - Theme.space.xxl * 2 : root.maxHeight)
    // The tabs belong to the title: right under it, not a padding's width
    // further down like a dialog's body.
    topPadding: 0
    // The actions row sets the bottom inset; this and the spacing over it
    // leave one inset (xl) between the body and the actions.
    bottomPadding: Theme.space.sm
    modal: true
    closePolicy: root.creating ? Popup.NoAutoClose : Popup.CloseOnEscape
    title: root.mode === "import" ? qsTr("Import instance") : qsTr("New instance")

    // The plain title every dialog has; no logo or icon tile in front.
    header: DialogHeader {
        title: root.title
    }

    // root.mode itself is left alone here -- the integrator sets it right
    // before open(), e.g. Main.qml's openNewInstance(mode).
    onOpened: {
        root.watcher = null
        root.performanceStep = ""
        root.performanceMode = false
        root.nameEdited = false
        root.selectedIcon = "default"
        groupField.text = ""
        root.importSource = root.presetSource
        sourceField.text = root.presetSource
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
            // A failed install leaves the instance made: the first Play
            // tries again.
            if (!ok && root.performanceStep !== "installing")
                return
            if (ok && root.performanceStep === "creating") {
                root.installPerformance()
                return
            }
            root.performanceStep = ""
            root.close()
            root.created()
        }
    }

    // Scrolls when the window is too short for the whole form (860x540),
    // so the version list never has to give up its rows to fit it.
    contentItem: Flickable {
        id: scroller
        clip: true
        contentWidth: width
        contentHeight: body.height
        boundsBehavior: Flickable.StopAtBounds
        // Always showing while the form is taller than the dialog, so a
        // short window says there is more (the loader tiles) below.
        readonly property bool overflowing: body.implicitHeight > scroller.height
        ScrollBar.vertical: ScrollBar {
            policy: scroller.overflowing ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        }

    ColumnLayout {
        id: body
        // Leaves the always-on scroll bar its own strip instead of
        // drawing it over the switches and the group field.
        width: scroller.width - (scroller.overflowing ? Theme.space.md : 0)
        height: Math.max(scroller.height, implicitHeight)
        spacing: Theme.space.lg

        TabStrip {
            Layout.fillWidth: true
            enabled: !root.creating
            tabs: [
                { id: "create", label: qsTr("Create") },
                { id: "import", label: qsTr("Import") }
            ]
            current: root.mode
            onActivated: (id) => root.mode = id
        }

        // Name and group
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            // A tile as tall as the labelled name field beside it.
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

                // A control, so a control's corner, on the tile every
                // instance icon sits on; hover moves the border alone.
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
                    Rectangle {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: Theme.space.xxs
                        width: Theme.icon.md
                        height: width
                        radius: Theme.radius.sm
                        color: Theme.palette.surfaceOverlay
                        border.width: 1
                        border.color: Theme.palette.border
                        MeshIcon {
                            anchors.centerIn: parent
                            iconName: "edit"
                            size: Theme.icon.sm - 4
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
                Text {
                    text: qsTr("Name")
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.weight: Font.Medium
                }
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
                Text {
                    text: qsTr("Group")
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.weight: Font.Medium
                }
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

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.mode === "import" ? 1 : 0

        // Create: pick a Minecraft version and, optionally, a mod loader.
        ColumnLayout {
            spacing: Theme.space.lg

        // Minecraft version
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.lg

            Text {
                Layout.fillWidth: true
                text: qsTr("Minecraft version")
                color: Theme.palette.textPrimary
                font.family: Theme.font.heading
                font.pixelSize: Theme.type.title.pixelSize
                font.weight: Font.Bold
            }
            Switch {
                text: qsTr("Snapshots")
                enabled: !!root.controller
                checked: root.controller ? root.controller.minecraftVersions.showSnapshots : false
                onToggled: root.controller.minecraftVersions.showSnapshots = checked
            }
            Switch {
                text: qsTr("Old versions")
                enabled: !!root.controller
                checked: root.controller ? root.controller.minecraftVersions.showOldVersions : false
                onToggled: root.controller.minecraftVersions.showOldVersions = checked
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.versionListMinHeight
            Layout.preferredHeight: root.versionListMinHeight
            radius: Theme.radius.lg
            // The dialog's own surface inside a hairline: a dark well read
            // as a hole in it.
            color: "transparent"
            border.width: 1
            border.color: Theme.palette.border

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

                    // Full width: the scroll bar overlays the rows rather
                    // than keeping a gutter beside them.
                    width: versions.width
                    height: Theme.control.height
                    hoverEnabled: true
                    onClicked: root.pickVersion(version)

                    background: Rectangle {
                        radius: Theme.radius.md
                        color: versionRow.selected ? Theme.palette.surfaceHover
                             : versionRow.hovered ? Theme.palette.hoverOverlay : "transparent"
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space.md
                        Text {
                            Layout.leftMargin: Theme.space.md
                            text: versionRow.version
                            color: versionRow.selected ? Theme.palette.accentText : Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            font.weight: versionRow.selected ? Font.Bold : Font.Medium
                        }
                        // Releases are the list's default and need no
                        // badge on every row; a snapshot or an old beta/
                        // alpha does, once the switches above show them.
                        StatusBadge {
                            visible: versionRow.type !== "release"
                            tone: versionRow.type === "snapshot" ? "warning" : "neutral"
                            text: root.typeLabel(versionRow.type)
                        }
                        // The suggestion, in the same badge as the type
                        // right after the version; neutral, so it does not
                        // turn green on the green selected row.
                        StatusBadge {
                            visible: versionRow.recommended
                            text: qsTr("Recommended")
                        }
                        Item { Layout.fillWidth: true }
                        // Where Kite Performance exists, its own pick: the
                        // version on Fabric with the performance mods.
                        IconButton {
                            visible: root.shell ? root.shell.performanceSupported(versionRow.version) : false
                            iconName: "zap"
                            tip: qsTr("Kite Performance")
                            size: Theme.control.heightSm
                            icon.color: versionRow.selected && root.performanceMode ? Theme.palette.accentText
                                      : hovered ? Theme.palette.textPrimary : Theme.palette.textTertiary
                            onClicked: root.pickPerformance(versionRow.version)
                        }
                        // One width for every date, so the ⚡ stands in one column.
                        Text {
                            Layout.rightMargin: Theme.space.md
                            Layout.preferredWidth: dateSample.advanceWidth
                            horizontalAlignment: Text.AlignRight
                            text: versionRow.time ? Format.calendarDate(new Date(versionRow.time)) : ""
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }
                }
            }

            // The first and last visible versions fade into the list's own
            // surface instead of being cut through mid-row by its border.
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
            Text {
                anchors.centerIn: parent
                width: parent.width - Theme.space.xxl * 2
                horizontalAlignment: Text.AlignHCenter
                visible: !!root.controller && root.controller.minecraftVersions.error.length > 0
                text: root.controller ? root.controller.minecraftVersions.error : ""
                wrapMode: Text.Wrap
                color: Theme.palette.danger
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
        }

        // What the \u26a1 pick brings, only while it is the pick.
        Text {
            Layout.fillWidth: true
            Layout.topMargin: -Theme.space.sm
            visible: root.performanceMode
            wrapMode: Text.Wrap
            text: qsTr("Fabric with Sodium and the other performance mods, as its own instance.")
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
        }

        // Mod loader
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            // As tall as the loader version box with or without it, so
            // picking a loader does not shorten the version list above.
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.control.height
                spacing: Theme.space.md

                Text {
                    text: qsTr("Mod loader")
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.heading
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }
                Item { Layout.fillWidth: true }
                ComboBox {
                    id: loaderVersionBox
                    Layout.preferredWidth: 220
                    visible: !!root.controller && root.controller.loader.length > 0
                    enabled: !root.creating && count > 0
                    model: root.controller ? root.controller.loaderVersions : null
                    textRole: "version"
                    valueRole: "versionId"
                    // Names the loader as well ("Fabric 0.20.1"), so the box
                    // says whose version it is.
                    displayText: root.controller && root.controller.loaderLoading ? qsTr("Loading\u2026")
                               : count === 0 ? qsTr("None for this version")
                               : root.loaderLabel(root.controller.loader) + " "
                                 + (currentIndex >= 0 ? currentText : root.controller.selectedLoaderVersion)
                    // The controller may have picked a version before the rows
                    // reached this box; line the two up whenever either moves.
                    function syncToController() {
                        if (root.controller)
                            currentIndex = indexOfValue(root.controller.selectedLoaderVersion)
                    }
                    onCountChanged: syncToController()
                    onActivated: root.controller.selectLoaderVersion(currentValue)
                    // Follow the controller's pick (it defaults to the newest).
                    Connections {
                        target: root.controller
                        ignoreUnknownSignals: true
                        function onSelectedLoaderVersionChanged() { loaderVersionBox.syncToController() }
                    }
                }
            }

            // One selectable tile per loader, its logo beside its name: a
            // control-sized row, so the version list above keeps the height.
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space.sm

                Repeater {
                    model: root.loaders

                    delegate: AbstractButton {
                        id: loaderCard
                        required property var modelData
                        readonly property bool selected: (root.controller ? root.controller.loader : "") === modelData.value

                        // Equal shares of the row, whatever the label.
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: root.loaderTileHeight
                        enabled: !root.creating
                        hoverEnabled: true
                        checkable: true
                        checked: selected
                        Accessible.name: modelData.label
                        onClicked: if (root.controller) root.pickLoader(modelData.value)

                        // The chosen one wears the active tab's and the
                        // filter chips' look (accentSubtle fill, accent
                        // hairline); the rest are plain outlines on the
                        // dialog, so the row reads as one control.
                        background: Rectangle {
                            radius: Theme.radius.md
                            color: loaderCard.selected ? Theme.palette.accentSubtle
                                 : loaderCard.hovered ? Theme.palette.hoverOverlay : "transparent"
                            border.width: 1
                            border.color: loaderCard.selected ? Theme.palette.accent
                                        : loaderCard.hovered ? Theme.palette.borderStrong : Theme.palette.border
                            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                            Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                        }

                        contentItem: Item {
                            Row {
                                anchors.centerIn: parent
                                spacing: Theme.space.sm

                                // The logo stands on the tile itself: a
                                // dark well around it read as a hole in
                                // the tile. One box for all, scaled by
                                // the loader's `fit`.
                                Item {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: root.logoSize
                                    height: width

                                    Image {
                                        anchors.centerIn: parent
                                        width: Math.round(root.logoSize * loaderCard.modelData.fit)
                                        height: width
                                        source: Icons.instanceIcon(loaderCard.modelData.iconKey)
                                        sourceSize: Qt.size(width * 2, height * 2)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        mipmap: true
                                    }
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: loaderCard.modelData.label
                                    color: loaderCard.selected ? Theme.palette.accentText : Theme.palette.textSecondary
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.label.pixelSize
                                    font.weight: loaderCard.selected ? Font.DemiBold : Font.Medium
                                }
                            }
                        }
                    }
                }
            }
        }

        } // create pane

        // Import: a local archive/export, or a direct download URL.
        ColumnLayout {
            spacing: Theme.space.lg

            Rectangle {
                id: dropTarget
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radius.lg
                // Transparent with a dashed outline -- the usual look of a
                // drop target -- that turns accent while a file is over it.
                color: "transparent"

                // A Canvas, not a Shape: it draws the same on every render
                // backend, and this one outline is drawn rarely.
                Canvas {
                    id: dashes
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
                        size: Theme.icon.lg + Theme.space.md
                        color: Theme.palette.textTertiary
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Drop a .zip or .mrpack file here")
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.body.pixelSize
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
                        // A bordered (default) button, not a flat one: a
                        // flat button's invisible side padding counted in
                        // the centring and pulled the field + button group
                        // visibly left of the drop zone's middle.
                        Button {
                            enabled: !root.creating
                            text: qsTr("Browse…")
                            icon.source: Icons.url("folder")
                            onClicked: importFileDialog.open()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        visible: !root.creating && root.importSource.trim().length > 0
                                 && !!root.controller
                                 && !root.controller.isImportSourceValid(root.importSource.trim())
                        text: qsTr("This doesn't look like a modpack file or link. Pick a .zip, .mrpack or .jar file, or paste a direct download link.")
                        color: Theme.palette.danger
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("Packs exported from CurseForge, FTB, ATLauncher or Technic import the same way, from their .zip file.")
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        } // import pane
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
            Text {
                width: parent.width
                text: root.watcher && root.watcher.failed ? (root.watcher.error ||
                          (root.mode === "import" ? qsTr("Importing the instance failed.") : qsTr("Creating the instance failed.")))
                    : root.watcher ? (root.watcher.status ||
                          (root.mode === "import" ? qsTr("Importing…") : qsTr("Creating…"))) : ""
                elide: Text.ElideRight
                color: root.watcher && root.watcher.failed ? Theme.palette.danger : Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            LaunchProgressBar {
                width: Math.min(parent.width, 320)
                visible: root.creating
                progress: root.watcher ? root.watcher.progress : -1
            }
        }

        Item { Layout.fillWidth: true; Layout.leftMargin: Theme.space.xl; visible: !root.creating && !(root.watcher && root.watcher.failed) }

        Button {
            Layout.bottomMargin: Theme.space.xl
            text: qsTr("Cancel")
            enabled: !root.creating
            onClicked: root.close()
        }
        Button {
            Layout.rightMargin: Theme.space.xl
            Layout.bottomMargin: Theme.space.xl
            highlighted: true
            visible: root.mode !== "import"
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
    }
}
