// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * A new, empty instance: pick a Minecraft version and, if wanted, a mod
 * loader; name it; create. Modpacks come from Discover, and importing a
 * zip or another launcher's pack is in the classic dialog ("More ways").
 *
 * The name follows the chosen version ("1.21.4", "Fabric 1.21.4") until
 * the user types one of their own.
 */
Dialog {
    id: root

    // NewInstanceController; set when the dialog opens, so the version list
    // is only fetched once someone actually wants a new instance.
    property var controller: null
    // TaskWatcher of the creation in progress.
    property var watcher: null
    property bool nameEdited: false
    // IconList model for the icon picker below, or null to hide it (the
    // integrator wires the shell's iconsModel in; without it the button
    // still shows the default icon, just with nothing to pick from).
    property var iconsModel: null
    property string selectedIcon: "default"

    signal moreWaysRequested()
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
    title: qsTr("New instance")

    onOpened: {
        root.watcher = null
        root.nameEdited = false
        root.selectedIcon = "default"
        groupField.text = ""
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
    }

    footer: RowLayout {
        spacing: Theme.space.sm

        Button {
            Layout.leftMargin: Theme.space.lg
            Layout.bottomMargin: Theme.space.lg
            flat: true
            visible: !root.creating
            text: qsTr("Import or more sources")
            icon.source: Icons.url("external-link")
            onClicked: {
                root.close()
                root.moreWaysRequested()
            }
        }

        Column {
            Layout.leftMargin: Theme.space.lg
            Layout.bottomMargin: Theme.space.lg
            Layout.fillWidth: true
            visible: root.creating || (!!root.watcher && root.watcher.failed)
            spacing: Theme.space.xs
            Text {
                width: parent.width
                text: root.watcher && root.watcher.failed ? (root.watcher.error || qsTr("Creating the instance failed."))
                    : root.watcher ? (root.watcher.status || qsTr("Creating…")) : ""
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

        Item { Layout.fillWidth: true; visible: !root.creating && !(root.watcher && root.watcher.failed) }

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
            text: root.creating ? qsTr("Creating…") : qsTr("Create")
            icon.source: Icons.url("plus")
            enabled: !root.creating && !!root.controller
                     && root.controller.selectedMinecraftVersion.length > 0
                     && nameField.text.trim().length > 0
            onClicked: {
                root.watcher = root.controller.create(nameField.text.trim(), groupField.text.trim(), root.selectedIcon)
            }
        }
    }
}
