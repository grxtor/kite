// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kite.Theme

/*
 * "Export…": one instance out as a file, the instance folder as a plain zip or a Modrinth or CurseForge pack. The
 * work is InstanceExportController's. Stays up with progress and a way to stop while the archive is written, and
 * closes once the file is there (`exported(message)` cues a toast); a failure keeps it open with the reason.
 */
AppDialog {
    id: root

    // InstanceExportController.
    property var controller: null
    signal exported(string message)

    property var watcher: null
    property bool stopping: false
    property string failure: ""
    readonly property bool exporting: !!root.watcher && root.watcher.running
    readonly property string format: root.controller ? root.controller.format : "zip"
    readonly property bool pack: root.format !== "zip"

    readonly property int dialogHeight: 700
    readonly property int treeMinHeight: 140
    readonly property int treeIndent: Theme.icon.md
    readonly property int treeRowHeight: Theme.control.heightSm + Theme.space.xs
    readonly property int memoryFieldWidth: 168
    readonly property int memoryStepMiB: 512
    readonly property int memoryMinMiB: 512
    readonly property int memoryMaxMiB: 65536

    readonly property var formats: [
        { id: "zip", label: qsTr("Zip") },
        { id: "modrinth", label: qsTr("Modrinth") },
        { id: "curseforge", label: qsTr("CurseForge") }
    ]

    function start(fileUrl) {
        root.failure = ""
        root.stopping = false
        var watcher = root.controller.exportTo(String(fileUrl))
        if (!watcher) {
            root.failure = root.controller.error
            return
        }
        root.watcher = watcher
    }

    size: "lg"
    height: Math.min(root.dialogHeight, parent ? parent.height - Theme.space.xxl * 2 : root.dialogHeight)
    // A stray click outside should not throw away typing, and an export in progress only stops through its button.
    closePolicy: root.exporting ? Popup.NoAutoClose : Popup.CloseOnEscape
    title: root.controller ? qsTr("Export “%1”").arg(root.controller.instanceName) : qsTr("Export")

    onOpened: {
        root.failure = ""
        root.stopping = false
        root.watcher = null
    }
    onClosed: if (root.controller) root.controller.saveChoices()

    Connections {
        target: root.watcher
        function onFinished(ok) {
            var watcher = root.watcher
            root.watcher = null
            if (ok) {
                var path = root.controller.lastOutput
                root.close()
                root.exported(qsTr("Saved “%1”.").arg(path.substring(path.lastIndexOf("/") + 1)))
            } else if (!root.stopping) {
                root.failure = watcher.error
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space.md
            enabled: !root.exporting

            NoticeBar {
                Layout.fillWidth: true
                visible: !!root.controller && root.controller.running
                text: qsTr("The game is running. Files it is writing now may be left out or cut short, so quit it first for a clean export.")
            }

            TabStrip {
                Layout.fillWidth: true
                visible: !!root.controller && root.controller.packFormats
                tabs: root.formats
                current: root.format
                onActivated: (id) => root.controller.format = id
            }

            Copy {
                Layout.fillWidth: true
                role: "label"
                tone: "secondary"
                wrapMode: Text.Wrap
                text: root.format === "modrinth"
                      ? qsTr("A Modrinth pack (.mrpack). Mods from Modrinth are linked for download, and everything else is packed in.")
                      : root.format === "curseforge"
                        ? qsTr("A CurseForge pack (.zip). Mods from CurseForge are linked for download, and everything else is packed in.")
                        : qsTr("The whole instance in one .zip file, ready to import into Kite or another launcher.")
            }

            GridLayout {
                Layout.fillWidth: true
                visible: root.pack
                columns: 2
                columnSpacing: Theme.space.md
                rowSpacing: Theme.space.sm

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.xs
                    FieldLabel { text: qsTr("Name") }
                    TextField {
                        Layout.fillWidth: true
                        text: root.controller ? root.controller.packName : ""
                        placeholderText: root.controller ? root.controller.instanceName : ""
                        selectByMouse: true
                        onTextEdited: root.controller.packName = text
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.xs
                    FieldLabel { text: qsTr("Version") }
                    TextField {
                        Layout.fillWidth: true
                        text: root.controller ? root.controller.packVersion : ""
                        placeholderText: "1.0.0"
                        selectByMouse: true
                        onTextEdited: root.controller.packVersion = text
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    visible: root.format === "curseforge"
                    spacing: Theme.space.xs
                    FieldLabel { text: qsTr("Author") }
                    TextField {
                        Layout.fillWidth: true
                        text: root.controller ? root.controller.packAuthor : ""
                        selectByMouse: true
                        onTextEdited: root.controller.packAuthor = text
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    visible: root.format === "modrinth"
                    spacing: Theme.space.xs
                    FieldLabel { text: qsTr("Summary") }
                    TextField {
                        Layout.fillWidth: true
                        text: root.controller ? root.controller.packSummary : ""
                        placeholderText: qsTr("One line about the pack")
                        selectByMouse: true
                        onTextEdited: root.controller.packSummary = text
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.pack
                spacing: 0

                CheckBox {
                    leftPadding: 0
                    text: qsTr("Mark disabled mods as optional")
                    checked: !!root.controller && root.controller.includeOptional
                    onToggled: root.controller.includeOptional = checked
                }
                RowLayout {
                    visible: root.format === "curseforge"
                    spacing: Theme.space.md
                    CheckBox {
                        id: memoryBox
                        leftPadding: 0
                        text: qsTr("Recommend memory")
                        checked: !!root.controller && root.controller.recommendMemory
                        onToggled: root.controller.recommendMemory = checked
                    }
                    SpinBox {
                        Layout.preferredWidth: root.memoryFieldWidth
                        enabled: memoryBox.checked
                        editable: true
                        from: root.memoryMinMiB
                        to: root.memoryMaxMiB
                        stepSize: root.memoryStepMiB
                        value: root.controller ? root.controller.recommendedMemory : root.memoryMinMiB
                        textFromValue: (value, locale) => qsTr("%1 MiB").arg(Number(value).toLocaleString(locale, "f", 0))
                        valueFromText: (text, locale) => Number.fromLocaleString(locale, text.replace(/[^0-9.,]/g, ""))
                        onValueModified: root.controller.recommendedMemory = value
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Copy {
                    Layout.fillWidth: true
                    text: qsTr("Files")
                    role: "bodyStrong"
                }
                Copy {
                    text: qsTr("Unchecked files are left out")
                    role: "caption"
                    tone: "tertiary"
                }
            }

            BoxSurface {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: root.treeMinHeight
                radius: Theme.radius.sm
                color: "transparent"

                TreeView {
                    id: tree
                    anchors.fill: parent
                    anchors.margins: Theme.space.xxs
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.controller ? root.controller.files : null
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Item {
                        id: node
                        required property TreeView treeView
                        required property bool expanded
                        required property bool hasChildren
                        required property int depth
                        required property int row
                        required property var model
                        required property string display
                        required property int checkState
                        required property bool isDir

                        implicitWidth: node.treeView.width
                        implicitHeight: root.treeRowHeight

                        HoverHandler { id: hover }

                        StateFill {
                            anchors.fill: parent
                            radius: Theme.radius.sm
                            hovered: hover.hovered
                        }

                        // Under the row's content, so a click on the checkbox stays the checkbox's.
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: node.isDir ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: if (node.isDir) node.treeView.toggleExpanded(node.row)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.space.xs + node.depth * root.treeIndent
                            anchors.rightMargin: Theme.space.sm
                            spacing: Theme.space.xs

                            MeshIcon {
                                iconName: "chevron-right"
                                size: Theme.icon.sm
                                color: Theme.palette.textTertiary
                                opacity: node.isDir && node.hasChildren ? 1 : 0
                                rotation: node.expanded ? 90 : 0
                            }
                            CheckBox {
                                id: box
                                padding: 0
                                checkState: node.checkState
                                nextCheckState: function () {
                                    return node.checkState === Qt.Checked ? Qt.Unchecked : Qt.Checked
                                }
                                // The click set the box itself; the model has the final say, and the binding comes
                                // back so later changes still reach it.
                                onToggled: {
                                    node.model.checkState = box.checkState
                                    box.checkState = Qt.binding(() => node.checkState)
                                }
                            }
                            MeshIcon {
                                iconName: node.isDir ? "folder" : "file"
                                size: Theme.icon.sm
                                color: Theme.palette.textTertiary
                            }
                            Copy {
                                Layout.fillWidth: true
                                text: node.display
                                tone: node.checkState === Qt.Unchecked ? "tertiary" : "primary"
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                ScrollFade {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 1
                    flickable: tree
                    fadeTo: Theme.glass.popup
                }

                // The tree starts inside the instance (or its game folder), not at the top of the disk; applied once
                // there is a controller.
                Binding {
                    target: tree
                    property: "rootIndex"
                    when: !!root.controller
                    value: root.controller ? root.controller.filesRoot : null
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.exporting
            spacing: Theme.space.xs

            Copy {
                Layout.fillWidth: true
                text: root.watcher && root.watcher.status.length > 0 ? root.watcher.status : qsTr("Exporting…")
                role: "label"
                tone: "secondary"
                elide: Text.ElideMiddle
            }
            ProgressBar {
                Layout.fillWidth: true
                indeterminate: !root.watcher || root.watcher.progress < 0
                value: root.watcher && root.watcher.progress >= 0 ? root.watcher.progress : 0
            }
        }

        Copy {
            Layout.fillWidth: true
            visible: root.failure.length > 0 || (root.format === "modrinth" && !!root.controller && !root.controller.ready)
            text: root.failure.length > 0 ? root.failure : qsTr("A Modrinth pack needs a version.")
            role: "caption"
            tone: "danger"
            wrapMode: Text.Wrap
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.xl
        topPadding: 0

        Button {
            visible: !root.exporting
            highlighted: true
            enabled: !!root.controller && root.controller.ready
            text: qsTr("Export…")
            onClicked: {
                saveDialog.selectedFile = root.controller.suggestedFileUrl()
                saveDialog.open()
            }
        }
        Button {
            visible: !root.exporting
            text: qsTr("Cancel")
            onClicked: root.close()
        }
        Button {
            visible: root.exporting
            text: qsTr("Stop")
            enabled: !root.stopping
            onClicked: {
                root.stopping = true
                root.controller.cancel()
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Export to")
        fileMode: FileDialog.SaveFile
        defaultSuffix: root.format === "modrinth" ? "mrpack" : "zip"
        nameFilters: root.format === "modrinth" ? [qsTr("Modrinth packs (*.mrpack)")]
                                                : [qsTr("Zip archives (*.zip)")]
        onAccepted: root.start(selectedFile)
    }
}
