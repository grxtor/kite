// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * "Export list…": the instance's mods (or packs) as text to paste or keep
 * -- HTML, Markdown, plain text, JSON, CSV, or a line of your own with
 * {name}, {version} and friends filled in. The QML replacement for
 * ExportListDialog; the text itself comes from ContentListExport through
 * InstanceContentController.exportText(), so both UIs write the same thing.
 *
 * The preview is exactly what is saved or copied.
 */
Dialog {
    id: root

    // InstanceContentController, and what to export from it: the rows with
    // these keys, or every row when empty.
    property var controller: null
    property string kind: "mods"
    property var keys: []
    property string noun: qsTr("mods")
    // The instance's name, for the suggested file name.
    property string listName: ""

    signal saved(string path)
    signal copied()
    signal failed(string message)

    property string format: "markdown"
    property var fields: ["version", "url"]
    property string lineTemplate: ""
    // Bumped on open, so the preview re-reads files added since.
    property int revision: 0
    readonly property bool custom: root.format === "custom"
    readonly property string output: {
        root.revision
        return root.controller ? root.controller.exportText(root.kind, root.keys, root.format,
                                                            root.fields, root.lineTemplate) : ""
    }

    readonly property int formatBoxWidth: 200
    readonly property int previewMinHeight: 120

    readonly property var formats: [
        { value: "html", label: qsTr("HTML") },
        { value: "markdown", label: qsTr("Markdown") },
        { value: "plaintext", label: qsTr("Plain text") },
        { value: "json", label: qsTr("JSON") },
        { value: "csv", label: qsTr("CSV") },
        { value: "custom", label: qsTr("Custom") }
    ]
    readonly property var fieldOptions: [
        { value: "version", label: qsTr("Version") },
        { value: "authors", label: qsTr("Authors") },
        { value: "url", label: qsTr("Link") },
        { value: "filename", label: qsTr("File name") }
    ]
    readonly property var placeholders: [
        { value: "{name}", label: qsTr("Name") },
        { value: "{version}", label: qsTr("Version") },
        { value: "{authors}", label: qsTr("Authors") },
        { value: "{url}", label: qsTr("Link") },
        { value: "{filename}", label: qsTr("File name") },
        { value: "{mod_id}", label: qsTr("Mod ID") }
    ]

    function toggleField(value, on) {
        const list = root.fields.filter((f) => f !== value)
        if (on)
            list.push(value)
        root.fields = list
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(640, parent ? parent.width - Theme.space.xxl * 2 : 640)
    height: Math.min(620, parent ? parent.height - Theme.space.xxl * 2 : 620)
    modal: true
    title: root.keys.length > 0 ? qsTr("Export %1 selected").arg(root.keys.length) : qsTr("Export list")

    header: DialogHeader {
        title: root.title
        icon: "list"
    }

    onOpened: root.revision++

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md
            Text {
                text: qsTr("Format")
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.weight: Theme.type.label.weight
            }
            ComboBox {
                id: formatBox
                Layout.preferredWidth: root.formatBoxWidth
                model: root.formats
                textRole: "label"
                valueRole: "value"
                Component.onCompleted: currentIndex = indexOfValue(root.format)
                onActivated: {
                    // A custom line starts from the format it replaces.
                    if (currentValue === "custom" && root.lineTemplate.length === 0 && root.controller)
                        root.lineTemplate = root.controller.exampleLine(root.format)
                    root.format = currentValue
                }
            }
            Item { Layout.fillWidth: true }
        }

        // What each line carries, for the built-in formats.
        Flow {
            Layout.fillWidth: true
            visible: !root.custom
            spacing: Theme.space.lg
            Repeater {
                model: root.fieldOptions
                CheckBox {
                    required property var modelData
                    // Box edge on the content edge, under "Format".
                    leftPadding: 0
                    text: modelData.label
                    checked: root.fields.indexOf(modelData.value) >= 0
                    onToggled: root.toggleField(modelData.value, checked)
                }
            }
        }

        // A line of your own.
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.custom
            spacing: Theme.space.sm
            TextField {
                id: templateField
                Layout.fillWidth: true
                text: root.lineTemplate
                placeholderText: qsTr("One line per item, for example {name} {version}")
                font.family: Theme.font.mono
                selectByMouse: true
                onTextEdited: root.lineTemplate = text
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.space.xs
                Repeater {
                    model: root.placeholders
                    Button {
                        required property var modelData
                        flat: true
                        text: modelData.label
                        icon.source: Icons.url("plus")
                        ToolTip.visible: hovered
                        ToolTip.delay: 400
                        ToolTip.text: qsTr("Insert %1").arg(modelData.value)
                        onClicked: {
                            templateField.insert(templateField.cursorPosition, modelData.value)
                            root.lineTemplate = templateField.text
                            templateField.forceActiveFocus()
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("Preview")
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.bodyStrong.pixelSize
                font.weight: Theme.type.bodyStrong.weight
            }
            Text {
                visible: !root.custom || root.lineTemplate.length > 0
                text: root.keys.length > 0 ? qsTr("%1 selected %2").arg(root.keys.length).arg(root.noun)
                                           : qsTr("All %1").arg(root.noun)
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.previewMinHeight
            radius: Theme.radius.sm
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            ScrollView {
                anchors.fill: parent
                anchors.margins: Theme.space.xxs
                TextArea {
                    readOnly: true
                    selectByMouse: true
                    text: root.output
                    wrapMode: TextEdit.NoWrap
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.mono
                    font.pixelSize: Theme.type.caption.pixelSize
                    background: null
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Built from what each file says about itself, so some may have no link or authors.")
            wrapMode: Text.Wrap
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: 0

        Button {
            highlighted: true
            enabled: root.output.length > 0
            text: qsTr("Save…")
            icon.source: Icons.url("download")
            onClicked: {
                if (root.controller)
                    saveDialog.selectedFile = root.controller.suggestedExportUrl(root.listName, root.format)
                saveDialog.open()
            }
        }
        Button {
            enabled: root.output.length > 0
            text: qsTr("Copy")
            icon.source: Icons.url("copy")
            onClicked: {
                clipboardHelper.text = root.output
                clipboardHelper.selectAll()
                clipboardHelper.copy()
                root.close()
                root.copied()
            }
        }
        Button {
            flat: true
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    }

    // QML has no clipboard API of its own; an invisible text edit does
    // (same as LogTab).
    TextEdit {
        id: clipboardHelper
        visible: false
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save list")
        fileMode: FileDialog.SaveFile
        defaultSuffix: root.controller ? root.controller.exportExtension(root.format).slice(1) : "txt"
        nameFilters: root.controller
                     ? [qsTr("%1 files (*%2)").arg(formatBox.currentText).arg(root.controller.exportExtension(root.format)),
                        qsTr("All files (*)")]
                     : [qsTr("All files (*)")]
        onAccepted: {
            if (!root.controller)
                return
            if (root.controller.saveText(selectedFile, root.output)) {
                root.close()
                root.saved(String(selectedFile))
            } else {
                root.failed(root.controller.lastError)
            }
        }
    }
}
