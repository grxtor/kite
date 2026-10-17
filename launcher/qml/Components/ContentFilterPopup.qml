// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Add content's filters, the panel the widget download dialog had down its
 * left side (ContentFilterWidget): Minecraft version, loader, environment,
 * categories, and whether to show what is already installed. Opened from
 * the Filters button beside the sort box; each change searches again at
 * once, so there is no Apply. Groups a provider cannot answer (environment
 * and licence on CurseForge, loaders for resource packs) are left out
 * rather than shown doing nothing.
 *
 * One control per kind of choice, as in Modrinth's and Prism's filter
 * lists: check boxes where several can be on (loaders, categories), radio
 * buttons where exactly one is (where it runs), a drop-down for the long
 * version list.
 */
Popup {
    id: root

    // ContentBrowser.
    property var browser: null

    readonly property int panelWidth: 380
    readonly property int maxPanelHeight: 520
    // Room between the panel's top and the play bar, set as it opens: the
    // panel stops above the bar rather than running under it or off the
    // window.
    property real roomBelow: maxPanelHeight

    width: panelWidth
    height: Math.min(maxPanelHeight, roomBelow, column.implicitHeight + topPadding + bottomPadding)
    padding: Theme.space.lg
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    function measureRoom() {
        if (root.parent && root.parent.Window.window) {
            const top = root.parent.mapToItem(null, 0, root.y).y
            root.roomBelow = root.parent.Window.height - top - Theme.layout.barHeight - Theme.space.md
        }
    }
    onAboutToShow: measureRoom()
    onOpened: {
        // Again once shown, in case the page was still settling.
        measureRoom()
        if (root.browser)
            root.browser.loadFilterOptions()
    }

    // Loaders and categories list in two columns: short words, and a
    // single column of twenty categories would make the panel scroll.
    readonly property int optionColumns: 2

    component GroupTitle: Text {
        Layout.fillWidth: true
        color: Theme.palette.textPrimary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.bodyStrong.pixelSize
        font.weight: Theme.type.bodyStrong.weight
    }

    function toggled(list, id, on) {
        const out = list.filter((x) => x !== id)
        if (on)
            out.push(id)
        return out
    }

    contentItem: Flickable {
        id: flick
        clip: true
        contentWidth: width
        contentHeight: column.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        // On the view itself, not its scrolling content, so it stays on
        // the panel's bottom edge.
        ScrollFade {
            parent: flick
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            flickable: flick
            fadeTo: Theme.palette.surfaceOverlay
        }

        ColumnLayout {
            id: column
            width: flick.width
            spacing: Theme.space.lg

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Filters")
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Theme.type.title.weight
                }
                Button {
                    flat: true
                    enabled: !!root.browser && root.browser.activeFilterCount > 0
                    text: qsTr("Reset")
                    onClicked: root.browser.resetFilters()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space.sm
                GroupTitle { text: qsTr("Minecraft version") }
                ComboBox {
                    id: versionBox
                    Layout.fillWidth: true
                    readonly property var options: [{ value: "", label: qsTr("Any version") }]
                        .concat((root.browser ? root.browser.gameVersionOptions : [])
                                .map((v) => ({ value: v, label: v })))
                    model: options
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.browser ? Math.max(0, options.findIndex((o) => o.value === root.browser.gameVersionFilter)) : 0
                    onActivated: {
                        root.browser.gameVersionFilter = currentValue
                        currentIndex = Qt.binding(() => root.browser ? Math.max(0, options.findIndex((o) => o.value === root.browser.gameVersionFilter)) : 0)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: !!root.browser && root.browser.loaderFilterAvailable
                spacing: Theme.space.sm
                GroupTitle { text: qsTr("Loader") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.optionColumns
                    columnSpacing: Theme.space.md
                    rowSpacing: 0
                    Repeater {
                        model: root.browser ? root.browser.loaderOptions : []
                        CheckBox {
                            // The box on the group title's edge, not 4px in.
                            leftPadding: 0
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            text: modelData.label
                            checked: root.browser.loaderFilter.indexOf(modelData.id) >= 0
                            onToggled: {
                                root.browser.loaderFilter = root.toggled(root.browser.loaderFilter, modelData.id, checked)
                                checked = Qt.binding(() => root.browser.loaderFilter.indexOf(modelData.id) >= 0)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: !!root.browser && root.browser.extendedFilters && root.browser.loaderFilterAvailable
                spacing: Theme.space.sm
                GroupTitle { text: qsTr("Runs on") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.optionColumns
                    columnSpacing: Theme.space.md
                    rowSpacing: 0
                    Repeater {
                        model: [
                            { value: "", label: qsTr("Anywhere") },
                            { value: "client", label: qsTr("Client") },
                            { value: "server", label: qsTr("Server") },
                            { value: "both", label: qsTr("Client and server") }
                        ]
                        RadioButton {
                            // The box on the group title's edge, not 4px in.
                            leftPadding: 0
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            text: modelData.label
                            checked: !!root.browser && root.browser.environmentFilter === modelData.value
                            onToggled: {
                                root.browser.environmentFilter = modelData.value
                                checked = Qt.binding(() => !!root.browser && root.browser.environmentFilter === modelData.value)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space.sm
                GroupTitle { text: qsTr("Categories") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.optionColumns
                    columnSpacing: Theme.space.md
                    rowSpacing: 0
                    Repeater {
                        model: root.browser ? root.browser.categories : []
                        CheckBox {
                            // The box on the group title's edge, not 4px in.
                            leftPadding: 0
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            text: modelData.label
                            checked: root.browser.categoryFilter.indexOf(modelData.id) >= 0
                            onToggled: {
                                root.browser.categoryFilter = root.toggled(root.browser.categoryFilter, modelData.id, checked)
                                checked = Qt.binding(() => root.browser.categoryFilter.indexOf(modelData.id) >= 0)
                            }
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    visible: !!root.browser && root.browser.categories.length === 0
                    text: qsTr("Loading categories…")
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space.xs
                CheckBox {
                    // The box on the group title's edge, not 4px in.
                    leftPadding: 0
                    text: qsTr("Hide what is already installed")
                    checked: !!root.browser && root.browser.hideInstalled
                    onToggled: {
                        root.browser.hideInstalled = checked
                        checked = Qt.binding(() => !!root.browser && root.browser.hideInstalled)
                    }
                }
                CheckBox {
                    // The box on the group title's edge, not 4px in.
                    leftPadding: 0
                    visible: !!root.browser && root.browser.extendedFilters
                    text: qsTr("Open source only")
                    checked: !!root.browser && root.browser.openSourceOnly
                    onToggled: {
                        root.browser.openSourceOnly = checked
                        checked = Qt.binding(() => !!root.browser && root.browser.openSourceOnly)
                    }
                }
            }
        }
    }
}
