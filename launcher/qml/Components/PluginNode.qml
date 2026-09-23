// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One node of a plugin's "mmco-ui/1" document, drawn with the launcher's
 * own components -- a plugin names what it needs (a toggle, a list), never
 * how it looks. Containers draw their children through this same file.
 *
 * Every user action goes back to the plugin as (surface, node, event,
 * value) through `sink.sendEvent`; the plugin answers by patching its
 * document, which arrives here as a new `node`.
 */
Item {
    id: root

    property var node: ({})
    property string surfaceId
    // Has sendEvent(surfaceId, nodeId, event, value).
    property var sink: null

    readonly property string type: node && node.type ? node.type : ""
    readonly property var props: node && node.props ? node.props : ({})
    readonly property var children_: node && node.children ? node.children : []

    function send(event, value) {
        if (root.sink)
            root.sink.sendEvent(root.surfaceId, root.node.id || "", event, value)
    }

    // Children of a row keep their natural width; everything else fills.
    property bool stretch: true

    visible: props.visible !== false
    implicitWidth: loader.implicitWidth
    implicitHeight: visible ? loader.implicitHeight : 0
    width: stretch && parent ? parent.width : implicitWidth

    Loader {
        id: loader
        width: root.stretch ? parent.width : implicitWidth
        sourceComponent: {
            switch (root.type) {
            case "column": return columnNode
            case "row": return rowNode
            case "section": return sectionNode
            case "heading": return headingNode
            case "text": return textNode
            case "separator": return separatorNode
            case "progress": return progressNode
            case "button": return buttonNode
            case "toggle": return toggleNode
            case "text_field": return textFieldNode
            case "number_field": return numberFieldNode
            case "choice": return choiceNode
            case "list": return listNode
            case "link": return linkNode
            default: return null
            }
        }
    }

    Component {
        id: columnNode
        Column {
            spacing: Theme.space.md
            Repeater {
                model: root.children_
                delegate: Loader {
                    required property var modelData
                    width: parent ? parent.width : 0
                    source: Qt.resolvedUrl("PluginNode.qml")
                    onLoaded: { item.inCard = root.inCard; item.surfaceId = root.surfaceId; item.sink = root.sink; item.node = Qt.binding(() => modelData) }
                }
            }
        }
    }

    Component {
        id: rowNode
        Flow {
            spacing: Theme.space.sm
            Repeater {
                model: root.children_
                delegate: Loader {
                    required property var modelData
                    source: Qt.resolvedUrl("PluginNode.qml")
                    onLoaded: { item.stretch = false; item.inCard = root.inCard; item.surfaceId = root.surfaceId; item.sink = root.sink; item.node = Qt.binding(() => modelData) }
                }
            }
        }
    }

    Component {
        id: sectionNode
        SettingsGroup {
            title: root.props.title || ""
            Repeater {
                model: root.children_
                delegate: Loader {
                    required property var modelData
                    property bool showDivider: false
                    width: parent ? parent.width : 0
                    source: Qt.resolvedUrl("PluginNode.qml")
                    onLoaded: { item.surfaceId = root.surfaceId; item.sink = root.sink; item.node = Qt.binding(() => modelData); item.inCard = true }
                }
            }
        }
    }

    // Leaves inside a section card get row padding, like setting rows.
    property bool inCard: false
    readonly property int pad: inCard ? Theme.space.lg : 0

    Component {
        id: headingNode
        Text {
            leftPadding: root.pad
            text: root.props.text || ""
            wrapMode: Text.Wrap
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.title.pixelSize
            font.weight: Font.Bold
        }
    }

    Component {
        id: textNode
        Text {
            leftPadding: root.pad
            rightPadding: root.pad
            topPadding: root.inCard ? Theme.space.sm : 0
            bottomPadding: root.inCard ? Theme.space.sm : 0
            text: root.props.text || ""
            textFormat: root.props.format === "markdown" ? Text.MarkdownText : Text.PlainText
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            linkColor: Theme.palette.accent
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
            onLinkActivated: (link) => root.send("click", link)
        }
    }

    Component {
        id: separatorNode
        Rectangle {
            height: 1
            color: Theme.palette.divider
        }
    }

    Component {
        id: progressNode
        Item {
            implicitHeight: 24
            LaunchProgressBar {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: root.pad
                anchors.rightMargin: root.pad
                anchors.verticalCenter: parent.verticalCenter
                progress: root.props.value === undefined || root.props.value < 0 ? -1 : root.props.value / 100
            }
        }
    }

    Component {
        id: buttonNode
        Item {
            implicitWidth: button.implicitWidth + root.pad * 2
            implicitHeight: button.implicitHeight + (root.inCard ? Theme.space.sm * 2 : 0)
            Button {
                id: button
                x: root.pad
                anchors.verticalCenter: parent.verticalCenter
                text: root.props.label || ""
                enabled: root.props.enabled !== false
                highlighted: root.props.style === "primary"
                onClicked: root.send("click", null)
            }
        }
    }

    Component {
        id: toggleNode
        SettingRow {
            label: root.props.label || ""
            description: root.props.description || ""
            enabled: root.props.enabled !== false
            Switch {
                checked: root.props.value === true
                Accessible.name: root.props.label || ""
                onToggled: {
                    root.send("change", checked)
                    checked = Qt.binding(() => root.props.value === true)
                }
            }
        }
    }

    Component {
        id: textFieldNode
        SettingRow {
            label: root.props.label || ""
            wide: true
            enabled: root.props.enabled !== false
            TextField {
                width: parent ? parent.width : 300
                placeholderText: root.props.placeholder || ""
                selectByMouse: true
                Component.onCompleted: text = root.props.value || ""
                // Same moment the widget renderer reports it: when editing
                // finishes, so a plugin never sees half-typed values.
                onEditingFinished: root.send("change", text)
            }
        }
    }

    Component {
        id: numberFieldNode
        SettingRow {
            label: root.props.label || ""
            enabled: root.props.enabled !== false
            SpinBox {
                width: 168
                from: root.props.min !== undefined ? root.props.min : 0
                to: root.props.max !== undefined ? root.props.max : 1000000
                stepSize: root.props.step !== undefined ? root.props.step : 1
                editable: true
                value: root.props.value !== undefined ? root.props.value : 0
                onValueModified: root.send("change", value)
            }
        }
    }

    Component {
        id: choiceNode
        SettingRow {
            label: root.props.label || ""
            enabled: root.props.enabled !== false
            ComboBox {
                id: combo
                width: 220
                readonly property var options: (root.props.options || []).map(o => typeof o === "string" ? { id: o, label: o } : o)
                model: options
                textRole: "label"
                valueRole: "id"
                currentIndex: Math.max(0, options.findIndex(o => o.id === root.props.value))
                onActivated: root.send("change", currentValue)
            }
        }
    }

    Component {
        id: listNode
        Column {
            spacing: 2
            topPadding: root.inCard ? Theme.space.sm : 0
            bottomPadding: root.inCard ? Theme.space.sm : 0
            property string selectedId: ""

            // Column titles.
            Row {
                x: root.pad + Theme.space.sm
                visible: (root.props.columns || []).length > 1
                spacing: Theme.space.md
                Repeater {
                    model: root.props.columns || []
                    delegate: Text {
                        required property string modelData
                        width: 120
                        text: modelData
                        elide: Text.ElideRight
                        color: Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                        font.weight: Font.DemiBold
                    }
                }
            }

            Repeater {
                model: root.props.rows || []
                delegate: AbstractButton {
                    id: listRow
                    required property var modelData
                    readonly property bool selected: parent.selectedId === modelData.id
                    x: root.pad
                    width: parent.width - root.pad * 2
                    height: Theme.control.height
                    hoverEnabled: true
                    onClicked: { parent.selectedId = modelData.id; root.send("select", modelData.id) }
                    onDoubleClicked: root.send("activate", modelData.id)
                    background: Rectangle {
                        radius: Theme.radius.sm + 2
                        color: listRow.selected ? Theme.palette.accentSubtle
                             : listRow.hovered ? Theme.palette.hoverOverlay : "transparent"
                    }
                    contentItem: Row {
                        leftPadding: Theme.space.sm
                        spacing: Theme.space.md
                        Repeater {
                            model: listRow.modelData.cells || []
                            delegate: Text {
                                required property string modelData
                                required property int index
                                anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                                width: index === 0 && (listRow.modelData.cells || []).length === 1 ? listRow.width - Theme.space.md : 120
                                text: modelData
                                elide: Text.ElideRight
                                color: index === 0 ? Theme.palette.textPrimary : Theme.palette.textSecondary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.label.pixelSize
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: linkNode
        Item {
            implicitHeight: linkText.implicitHeight + (root.inCard ? Theme.space.sm * 2 : 0)
            Text {
                id: linkText
                x: root.pad
                anchors.verticalCenter: parent.verticalCenter
                text: root.props.text || root.props.href || ""
                color: Theme.palette.accent
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                font.underline: linkHover.hovered
                HoverHandler { id: linkHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.send("click", root.props.href || "") }
            }
        }
    }
}
