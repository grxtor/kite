// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * What a new instance runs on, as a pick-one list in the list-nav grammar:
 * Kite Performance first (Fabric with the performance mods; its row shows the
 * icons of the picked mods and offers to edit them, also on right-click;
 * greyed out for a version it has no mods for), then the unmodified game and
 * each mod loader.
 */
Item {
    id: root

    // NewInstanceDialog.loaders rows: {value, label, iconKey, fit}.
    property var loaders: []
    // The picked row: performanceKind, or a loader's value ("" is vanilla).
    property string current: ""
    // Whether the version picked beside the list has Kite Performance.
    property bool performanceAvailable: true
    // BundleCatalog::mods() rows of the mods picked for Kite Performance.
    property var pickedMods: []

    signal picked(string kind)
    // The performance row's "Edit mods", or a right click on that row.
    signal editModsRequested()

    readonly property string performanceKind: "performance"
    readonly property int logoSize: Theme.icon.md
    // As many icons as fit on the row's one line; the summary names the rest.
    readonly property int maxIcons: 8

    readonly property Item selectedItem: {
        if (root.current === root.performanceKind)
            return performanceRow
        for (var i = 0; i < loaderRepeater.count; ++i) {
            var item = loaderRepeater.itemAt(i)
            if (item && item.modelData.value === root.current)
                return item
        }
        return null
    }

    function modSummary() {
        var names = root.pickedMods.map((mod) => mod.name)
        if (names.length <= 2)
            return names.join(", ")
        return qsTr("%1, %2 and %3 more").arg(names[0]).arg(names[1]).arg(names.length - 2)
    }

    implicitWidth: 260
    implicitHeight: rows.implicitHeight

    NavSelectionIndicator {
        target: root.selectedItem
    }

    Column {
        id: rows
        width: parent.width
        spacing: Theme.space.xxs

        KindRow {
            id: performanceRow
            width: rows.width
            label: qsTr("Kite Performance")
            selected: root.current === root.performanceKind
            enabled: root.performanceAvailable
            mods: performanceRow.selected ? root.pickedMods.slice(0, root.maxIcons) : []
            detail: !root.performanceAvailable ? qsTr("Not for this version")
                  : performanceRow.selected ? root.modSummary()
                  : qsTr("Fabric with the performance mods")
            actionText: performanceRow.selected ? qsTr("Edit mods") : ""
            onClicked: root.picked(root.performanceKind)
            onActionTriggered: root.editModsRequested()

            TapHandler {
                enabled: performanceRow.enabled
                acceptedButtons: Qt.RightButton
                onTapped: {
                    root.picked(root.performanceKind)
                    root.editModsRequested()
                }
            }
        }

        Item {
            width: rows.width
            height: Theme.space.sm * 2 + 1
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.space.md
                width: parent.width - Theme.space.md * 2
                height: 1
                color: Theme.palette.divider
            }
        }

        Repeater {
            id: loaderRepeater
            model: root.loaders

            delegate: KindRow {
                required property var modelData
                width: rows.width
                label: modelData.label
                iconKey: modelData.iconKey
                fit: modelData.fit
                selected: root.current === modelData.value
                onClicked: root.picked(modelData.value)
            }
        }
    }

    component KindRow: AbstractButton {
        id: row

        property bool selected: false
        property string iconKey: ""
        property real fit: 1.0
        property string label
        property string detail: ""
        property var mods: []
        property string actionText: ""

        signal actionTriggered()

        readonly property int lineHeight: Math.max(root.logoSize, Math.ceil(Theme.type.body.lineHeightPx))
        readonly property int textX: root.logoSize + Theme.space.md
        readonly property color ink: !row.enabled ? Theme.palette.textTertiary
                                   : row.selected ? Theme.palette.accentText
                                   : row.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary

        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        leftPadding: Theme.space.md
        rightPadding: Theme.space.md
        topPadding: (Theme.control.height - row.lineHeight) / 2
        bottomPadding: row.topPadding
        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

        Accessible.role: Accessible.RadioButton
        Accessible.name: row.label
        Accessible.description: row.detail
        Accessible.checked: row.selected

        background: StateFill {
            radius: Theme.radius.md
            hovered: row.hovered
            down: row.down
        }

        contentItem: Column {
            spacing: Theme.space.xs

            Row {
                height: row.lineHeight
                spacing: Theme.space.md

                Item {
                    width: root.logoSize
                    height: parent.height

                    Image {
                        anchors.centerIn: parent
                        visible: row.iconKey.length > 0
                        width: Math.round(root.logoSize * row.fit)
                        height: width
                        source: row.iconKey.length > 0 ? Icons.instanceIcon(row.iconKey) : ""
                        sourceSize: Qt.size(width * 2, height * 2)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        opacity: row.enabled ? 1 : Theme.opacity.disabled
                    }
                    MeshIcon {
                        anchors.centerIn: parent
                        visible: row.iconKey.length === 0
                        iconName: "zap"
                        size: root.logoSize
                        color: row.enabled ? Theme.palette.accentText : Theme.palette.textTertiary
                    }
                }

                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.label
                    color: row.ink
                    font.weight: row.selected ? Font.DemiBold : Font.Medium
                }
            }

            PerformanceModIcons {
                x: row.textX
                width: parent.width - x
                visible: row.mods.length > 0
                iconSize: Theme.icon.lg
                spacing: Theme.space.xs
                mods: row.mods
            }

            Copy {
                x: row.textX
                width: parent.width - x
                visible: row.detail.length > 0
                text: row.detail
                role: "caption"
                tone: "tertiary"
                wrapMode: Text.Wrap
            }

            // Its text, not the box it pulls out for the chevron, aligns with the other lines' edge.
            LinkButton {
                id: link
                x: row.textX - link.chevronBearing
                visible: row.actionText.length > 0
                text: row.actionText
                onClicked: row.actionTriggered()
            }
        }
    }
}
