// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import MeshMC.Theme

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
    // Raw text of the chosen file/URL in import mode.
    property string importSource: ""
    // What import mode's file/URL field starts with when the dialog opens:
    // a file or link handed in from outside (`meshmc --import`, see
    // Main.qml's openImport()); empty for a dialog opened from a button.
    property string presetSource: ""

    signal created()

    readonly property bool creating: !!watcher && watcher.running
    readonly property int loaderTileHeight: 96
    // Rows of the version list that always stay visible; a shorter window
    // scrolls the dialog instead of squeezing the list.
    readonly property int versionListRows: 4
    readonly property int versionListMinHeight: versionListRows * Theme.control.height + Theme.space.xs * 2 + 2
    readonly property int logoWell: Theme.control.heightLg
    readonly property int logoSize: Theme.control.height
    // Each loader wears the logo the launcher already ships for it (the
    // iconName column of knownModLoaders(), served by the instanceicon
    // provider); the unmodified game gets the grass block, the default
    // instance icon.
    readonly property var loaders: [
        { value: "", label: qsTr("Vanilla"), iconKey: "grass" },
        { value: "fabric", label: "Fabric", iconKey: "fabricmc" },
        { value: "quilt", label: "Quilt", iconKey: "quilt_mark" },
        { value: "forge", label: "Forge", iconKey: "forge_mark" },
        { value: "neoforge", label: "NeoForge", iconKey: "neoforged" }
    ]

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
        if (suggested.length > 0 && root.controller.loader.length === 0)
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
            if (ok) {
                root.close()
                root.created()
            }
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

                // A control, so a control's corner; hover moves the border
                // alone.
                background: Rectangle {
                    radius: Theme.radius.sm
                    color: Theme.palette.surfaceRaised
                    border.width: 1
                    border.color: iconButton.hovered ? Theme.palette.borderStrong : Theme.palette.border
                    Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                }
                contentItem: Item {
                    Image {
                        anchors.centerIn: parent
                        width: Math.round(iconButton.height * 0.62)
                        height: width
                        source: "image://instanceicon/" + root.selectedIcon
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
                font.family: Theme.font.family
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
            color: Theme.palette.surfaceSunken
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
                    onClicked: root.controller.selectMinecraftVersion(version)

                    background: Rectangle {
                        radius: Theme.radius.md
                        color: versionRow.selected ? Theme.palette.accentSubtle
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
                        Text {
                            Layout.rightMargin: Theme.space.md
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
                fadeTo: Theme.palette.surfaceSunken
            }
            ScrollFade {
                anchors.left: versions.left
                anchors.right: versions.right
                anchors.bottom: versions.bottom
                flickable: versions
                fadeTo: Theme.palette.surfaceSunken
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

        // Mod loader
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space.md

                Text {
                    text: qsTr("Mod loader")
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
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
                    displayText: root.controller && root.controller.loaderLoading ? qsTr("Loading\u2026")
                               : count === 0 ? qsTr("None for this version")
                               : currentIndex >= 0 ? currentText
                               : root.controller ? root.controller.selectedLoaderVersion : ""
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

            // One selectable card per loader instead of a segmented control,
            // so the showpiece dialog leads with imagery here too.
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
                        onClicked: if (root.controller) root.controller.loader = modelData.value

                        // One surface for every tile; the chosen one gets
                        // the tile picker's accent ring, hover only lifts
                        // the border.
                        background: Rectangle {
                            radius: Theme.radius.lg
                            color: Theme.palette.surfaceRaised
                            border.width: loaderCard.selected ? 2 : 1
                            border.color: loaderCard.selected ? Theme.palette.accent
                                        : loaderCard.hovered ? Theme.palette.borderStrong : Theme.palette.border
                            Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                        }

                        contentItem: Item {
                            Column {
                                anchors.centerIn: parent
                                spacing: Theme.space.sm

                                // Every logo sits in the same well at the
                                // same size, so a mark that ships on its
                                // own square (Forge's navy, Quilt's
                                // purple) reads as a logo on a plate like
                                // the rest, not as a box only it has.
                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: root.logoWell
                                    height: width
                                    radius: Theme.radius.sm
                                    color: Theme.palette.surfaceSunken
                                    border.width: 1
                                    border.color: Theme.palette.border

                                    Image {
                                        anchors.centerIn: parent
                                        width: root.logoSize
                                        height: width
                                        source: "image://instanceicon/" + loaderCard.modelData.iconKey
                                        sourceSize: Qt.size(width * 2, height * 2)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        mipmap: true
                                    }
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: loaderCard.modelData.label
                                    color: loaderCard.selected ? Theme.palette.textPrimary : Theme.palette.textSecondary
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
                color: Theme.palette.surfaceSunken
                border.width: dropArea.containsDrag ? 2 : 1
                border.color: dropArea.containsDrag ? Theme.palette.accent : Theme.palette.border
                Behavior on border.color { ColorAnimation { duration: Theme.motion.fast } }

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
            Layout.leftMargin: Theme.space.lg
            Layout.bottomMargin: Theme.space.lg
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

        Item { Layout.fillWidth: true; Layout.leftMargin: Theme.space.lg; visible: !root.creating && !(root.watcher && root.watcher.failed) }

        Button {
            Layout.bottomMargin: Theme.space.lg
            flat: true
            text: qsTr("Cancel")
            enabled: !root.creating
            onClicked: root.close()
        }
        Button {
            Layout.rightMargin: Theme.space.lg
            Layout.bottomMargin: Theme.space.lg
            highlighted: true
            visible: root.mode !== "import"
            text: root.creating ? qsTr("Creating…") : qsTr("Create")
            icon.source: Icons.url("plus")
            enabled: !root.creating && !!root.controller
                     && root.controller.selectedMinecraftVersion.length > 0
                     && nameField.text.trim().length > 0
            onClicked: {
                root.watcher = root.controller.create(nameField.text.trim(), groupField.text.trim(), root.selectedIcon)
            }
        }
        Button {
            Layout.rightMargin: Theme.space.lg
            Layout.bottomMargin: Theme.space.lg
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
