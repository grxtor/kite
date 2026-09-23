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
 * QML yet -- see the note in the import pane.
 *
 * In create mode the name follows the chosen version ("1.21.4", "Fabric
 * 1.21.4") until the user types one of their own; in import mode it follows
 * the picked file/URL the same way.
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

    signal created()

    readonly property bool creating: !!watcher && watcher.running
    // No trademarked logos, so each loader gets a plain shape instead: a
    // block for the unmodified game, a woven grid for Fabric's own name and
    // its fork Quilt, a gear for Forge and its fork NeoForge -- tinted
    // apart so the four modded options don't read as one another.
    readonly property var loaders: [
        { value: "", label: qsTr("Vanilla"), icon: "cube", tint: Theme.palette.textSecondary },
        { value: "fabric", label: "Fabric", icon: "grid", tint: Theme.palette.accent },
        { value: "quilt", label: "Quilt", icon: "copy", tint: Theme.palette.info },
        { value: "forge", label: "Forge", icon: "settings", tint: Theme.palette.warning },
        { value: "neoforge", label: "NeoForge", icon: "refresh", tint: Theme.palette.danger }
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

    function refreshSuggestedName() {
        if (!root.nameEdited && root.controller)
            nameField.text = root.controller.suggestedName()
    }

    function refreshImportName() {
        if (root.nameEdited || !root.controller)
            return
        var suggested = root.controller.suggestedNameForImportSource(root.importSource)
        if (suggested.length > 0)
            nameField.text = suggested
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
    height: Math.min(640, parent ? parent.height - Theme.space.xxl * 2 : 640)
    modal: true
    closePolicy: root.creating ? Popup.NoAutoClose : Popup.CloseOnEscape
    title: root.mode === "import" ? qsTr("Import instance") : qsTr("New instance")

    // root.mode itself is left alone here -- the integrator sets it right
    // before open(), e.g. Main.qml's openNewInstance(mode).
    onOpened: {
        root.watcher = null
        root.nameEdited = false
        root.selectedIcon = "default"
        groupField.text = ""
        root.importSource = ""
        sourceField.text = ""
        selectDefaultVersion()
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

    contentItem: ColumnLayout {
        spacing: Theme.space.lg

        SegmentedControl {
            Layout.alignment: Qt.AlignLeft
            enabled: !root.creating
            options: [
                { value: "create", label: qsTr("Create") },
                { value: "import", label: qsTr("Import") }
            ]
            current: root.mode
            onActivated: (value) => root.mode = value
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

                background: Rectangle {
                    radius: Theme.radius.lg
                    color: iconButton.hovered ? Theme.palette.hoverOverlay : Theme.palette.surfaceRaised
                    border.width: 1
                    border.color: iconButton.hovered ? Theme.palette.borderStrong : Theme.palette.border
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
                        radius: width / 2
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

                    width: versions.width - Theme.space.md
                    height: Theme.control.height + 4
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
                            Layout.preferredWidth: 140
                            text: versionRow.version
                            color: versionRow.selected ? Theme.palette.accent : Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            font.weight: versionRow.selected ? Font.Bold : Font.Medium
                        }
                        StatusBadge {
                            tone: versionRow.type === "release" ? "success"
                                : versionRow.type === "snapshot" ? "warning" : "neutral"
                            text: root.typeLabel(versionRow.type)
                        }
                        StatusBadge {
                            visible: versionRow.recommended
                            tone: "info"
                            text: qsTr("Recommended")
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            Layout.rightMargin: Theme.space.md
                            text: versionRow.time ? Qt.formatDate(versionRow.time, Qt.locale().dateFormat(Locale.ShortFormat)) : ""
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }
                }
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

                        Layout.fillWidth: true
                        Layout.preferredHeight: 92
                        enabled: !root.creating
                        hoverEnabled: true
                        checkable: true
                        checked: selected
                        Accessible.name: modelData.label
                        onClicked: if (root.controller) root.controller.loader = modelData.value

                        background: Rectangle {
                            radius: Theme.radius.lg
                            color: loaderCard.selected ? Theme.palette.accentSubtle
                                 : loaderCard.hovered ? Theme.palette.hoverOverlay : Theme.palette.surfaceRaised
                            border.width: loaderCard.selected ? 2 : 1
                            border.color: loaderCard.selected ? Theme.palette.accent : Theme.palette.border
                            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                        }

                        contentItem: Item {
                            Column {
                                anchors.centerIn: parent
                                spacing: Theme.space.xs

                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: Theme.icon.lg + Theme.space.md
                                    height: width
                                    radius: Theme.radius.md
                                    color: Qt.rgba(loaderCard.modelData.tint.r, loaderCard.modelData.tint.g,
                                                   loaderCard.modelData.tint.b, loaderCard.selected ? 0.24 : 0.14)

                                    MeshIcon {
                                        anchors.centerIn: parent
                                        iconName: loaderCard.modelData.icon
                                        size: Theme.icon.lg
                                        color: loaderCard.modelData.tint
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
                        Button {
                            flat: true
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
                        text: qsTr("Doesn't look like a modpack file or link yet -- pick a .zip/.mrpack/.jar that exists, or paste a direct download URL.")
                        color: Theme.palette.danger
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("Packs from CurseForge, FTB, ATLauncher and Technic can be imported the same way, from their exported .zip file -- browsing those sites here is coming later.")
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
