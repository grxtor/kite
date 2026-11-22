// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The keyboard shortcuts sheet ("?"): every entry of KeyMap by group, the keys drawn as caps (KeyCap) in this
 * platform's own names. Two columns of groups; it scrolls where the window is short.
 */
AppDialog {
    id: root

    title: qsTr("Keyboard shortcuts")
    size: "xl"
    showPrimary: false
    cancelText: qsTr("Close")
    closeOnOutsidePress: true

    // Which groups stand in which column, top to bottom.
    readonly property var columns: [["navigation", "log"], ["library", "instance", "chat"]]
    readonly property int rowHeight: Theme.control.heightSm + Theme.space.xs

    function groupTitle(id) {
        var found = KeyMap.groups.find((one) => one.id === id)
        return found ? found.title : id
    }

    contentItem: ScrollView {
        id: scroll
        implicitHeight: Math.min(sheet.implicitHeight, root.parent ? root.parent.height - 220 : sheet.implicitHeight)
        contentWidth: availableWidth
        clip: true

        RowLayout {
            id: sheet
            width: scroll.availableWidth
            spacing: Theme.space.xxl

            Repeater {
                model: root.columns
                delegate: ColumnLayout {
                    id: column
                    required property var modelData
                    Layout.alignment: Qt.AlignTop
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    spacing: Theme.space.lg

                    Repeater {
                        model: column.modelData
                        delegate: ColumnLayout {
                            id: group
                            required property string modelData
                            Layout.fillWidth: true
                            spacing: 0

                            Copy {
                                Layout.bottomMargin: Theme.space.xs
                                text: root.groupTitle(group.modelData)
                                role: "bodyStrong"
                            }

                            Repeater {
                                model: KeyMap.entriesOf(group.modelData)
                                delegate: RowLayout {
                                    id: line
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.minimumHeight: root.rowHeight
                                    spacing: Theme.space.md

                                    Copy {
                                        Layout.fillWidth: true
                                        text: line.modelData.label
                                        tone: "secondary"
                                        role: "label"
                                        elide: Text.ElideRight
                                    }
                                    Copy {
                                        visible: text.length > 0
                                        text: line.modelData.also || ""
                                        role: "caption"
                                        tone: "tertiary"
                                    }
                                    Repeater {
                                        model: KeyMap.combos(line.modelData)
                                        delegate: Row {
                                            id: combo
                                            required property var modelData
                                            spacing: Theme.space.xs
                                            Repeater {
                                                model: combo.modelData
                                                delegate: KeyCap {
                                                    required property string modelData
                                                    text: modelData
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
