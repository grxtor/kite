// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kite.Theme

/*
 * "Export list…": the instance's mods (or packs) as text to paste or keep -- HTML, Markdown, plain text, JSON, CSV or
 * a line of your own with {name}, {version} and friends. The text comes from ContentListExport through
 * InstanceContentController.exportText(); the preview is exactly what is saved or copied.
 */
AppDialog {
    id: root

    // InstanceContentController. `keys` picks the rows to export; every row when empty.
    property var controller: null
    property string kind: "mods"
    property var keys: []
    property string noun: qsTr("mods")
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

    size: "lg"
    height: Math.min(620, parent ? parent.height - Theme.space.xxl * 2 : 620)
    closeOnOutsidePress: true
    title: root.keys.length > 0 ? qsTr("Export %1 selected").arg(root.keys.length) : qsTr("Export list")

    onOpened: root.revision++

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md
            FieldLabel { text: qsTr("Format") }
            ComboBox {
                id: formatBox
                Layout.preferredWidth: root.formatBoxWidth
                model: root.formats
                textRole: "label"
                valueRole: "value"
                Component.onCompleted: currentIndex = indexOfValue(root.format)
                onActivated: {
                    if (currentValue === "custom" && root.lineTemplate.length === 0 && root.controller)
                        root.lineTemplate = root.controller.exampleLine(root.format)
                    root.format = currentValue
                }
            }
            Item { Layout.fillWidth: true }
        }

        Flow {
            Layout.fillWidth: true
            visible: !root.custom
            spacing: Theme.space.lg
            Repeater {
                model: root.fieldOptions
                CheckBox {
                    required property var modelData
                    leftPadding: 0
                    text: modelData.label
                    checked: root.fields.indexOf(modelData.value) >= 0
                    onToggled: root.toggleField(modelData.value, checked)
                }
            }
        }

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
            Copy {
                Layout.fillWidth: true
                text: qsTr("Preview")
                role: "bodyStrong"
            }
            Copy {
                visible: !root.custom || root.lineTemplate.length > 0
                text: root.keys.length > 0 ? qsTr("%1 selected %2").arg(root.keys.length).arg(root.noun)
                                           : qsTr("All %1").arg(root.noun)
                role: "caption"
                tone: "tertiary"
            }
        }

        BoxSurface {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.previewMinHeight
            radius: Theme.radius.sm
            color: "transparent"

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

        Copy {
            Layout.fillWidth: true
            text: qsTr("Built from what each file says about itself, so some may have no link or authors.")
            role: "caption"
            tone: "tertiary"
            wrapMode: Text.Wrap
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.xl
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
                clipboard.copyText(root.output)
                root.close()
                root.copied()
            }
        }
        Button {
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    }

    ClipboardText { id: clipboard }

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
