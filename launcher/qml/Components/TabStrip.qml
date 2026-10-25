// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Page tabs, Modrinth style: plain labels in a row, the current one on a
 * soft pill (accentSubtle fill, accentText label) and the others with only a
 * hover overlay. For switching views of one thing (an instance's mods,
 * worlds, log...), where the sidebar's rail would read as navigation away.
 *
 * More tabs than fit (an instance page can carry fourteen) are never
 * clipped or scrolled: the ones that do not fit move, in order, into a
 * "More" menu at the end of the strip. Which tabs fit depends only on the
 * width, never on the selection, so the strip's order never changes under
 * the cursor: a tab picked from the menu stays in the menu, and the More
 * button takes its name ("Settings ▾") in the selected style instead.
 *
 * A tab given `more: true` lives in that menu at every width -- for the
 * ones used less often (an instance's Version or Backups), so the strip
 * keeps to the everyday tabs even on a wide window.
 *
 * A tab given `attention: true` draws its count on an accent pill, like the
 * rail's: something there waits for an answer (Friends' requests).
 */
Item {
    id: root

    // [{ id, label, count?, more?, attention? }]
    property var tabs: []
    property string current
    signal activated(string id)

    // Ids of the tabs that did not fit and live in the More menu instead,
    // recomputed by relayout() whenever the width or the tabs change.
    property var overflowIds: []
    readonly property var overflowTabs: root.tabs.filter((t) => root.overflowIds.indexOf(t.id) >= 0)
    // The current tab when it is one of those, so More can carry its name.
    readonly property var currentOverflowTab: root.overflowTabs.find((t) => t.id === root.current) || null

    // A tab's pill: its label with this much air either side.
    readonly property int pillPadding: Theme.space.md + Theme.space.xxs

    implicitHeight: Theme.control.height
    implicitWidth: row.implicitWidth

    Accessible.role: Accessible.PageTabList

    // Width the More button needs to show any of the labels it may hold --
    // the `more` tabs and the tabs at @p hiddenIndexes -- (or "More"
    // itself), selected, so whichever of them gets picked fits.
    function moreWidthFor(hiddenIndexes) {
        let label = Math.ceil(moreMetrics.advanceWidth)
        for (let i = 0; i < root.tabs.length; ++i) {
            if (root.tabs[i].more === true || hiddenIndexes.indexOf(i) >= 0)
                label = Math.max(label, repeater.itemAt(i).selectedLabelWidth)
        }
        return label + moreRow.spacing + Theme.icon.sm + root.pillPadding * 2
    }

    // Fits as many tabs as the width allows, in order, leaving room for a
    // More button wide enough for the name of any tab it holds. `more` tabs
    // always go to the menu. The current tab plays no part, so selecting
    // one never moves another.
    function relayout() {
        const n = root.tabs.length
        if (root.width <= 0 || repeater.count !== n)
            return
        const gap = row.spacing
        // The tabs that may sit in the row, by index, and whether any must
        // go to the menu whatever the width.
        const free = []
        for (let i = 0; i < n; ++i) {
            if (root.tabs[i].more !== true)
                free.push(i)
        }
        const anyPinned = free.length < n
        // ends[k]: width of the first k free tabs laid out side by side.
        const ends = [0]
        for (let j = 0; j < free.length; ++j)
            ends.push(ends[j] + repeater.itemAt(free[j]).implicitWidth + (j > 0 ? gap : 0))

        let shown = free.length
        if (ends[shown] + (anyPinned ? gap + root.moreWidthFor([]) : 0) > root.width) {
            shown = 0
            for (let k = free.length - 1; k > 0; --k) {
                if (ends[k] + gap + root.moreWidthFor(free.slice(k)) <= root.width) {
                    shown = k
                    break
                }
            }
        }

        const hiddenFree = free.slice(shown)
        const hidden = root.tabs.filter((t, i) => t.more === true || hiddenFree.indexOf(i) >= 0)
                                .map((t) => t.id)
        if (JSON.stringify(hidden) !== JSON.stringify(root.overflowIds))
            root.overflowIds = hidden
    }

    // The tabs binding is rebuilt whenever a count changes, so wait for the
    // repeater to have settled before measuring.
    onTabsChanged: Qt.callLater(relayout)
    onWidthChanged: Qt.callLater(relayout)
    Component.onCompleted: Qt.callLater(relayout)

    // A selected label's weight, for sizing the More button's slot.
    TextMetrics {
        id: moreMetrics
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        font.weight: Font.DemiBold
        text: qsTr("More")
    }

    Row {
        id: row
        height: parent.height
        spacing: Theme.space.xs

        Repeater {
            id: repeater
            model: root.tabs
            delegate: AbstractButton {
                id: tab
                required property var modelData
                readonly property bool selected: modelData.id === root.current
                readonly property bool hasCount: modelData.count !== undefined && modelData.count >= 0
                readonly property bool attention: tab.hasCount && modelData.count > 0 && modelData.attention === true
                readonly property color inkColor: tab.selected ? Theme.palette.accentText
                                                : tab.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary

                // A tab's slot is its label's width unselected, whether it
                // is selected or not, so selecting one never nudges the tabs
                // after it: the bolder selected label just runs a pixel or
                // two into the pill's padding. Its selected width sizes
                // More's slot.
                readonly property real labelWidth: Math.ceil(labelMetrics.advanceWidth)
                readonly property real selectedLabelWidth: Math.ceil(selectedMetrics.advanceWidth)

                // A tab in the More menu takes no room in the row, but
                // relayout() still needs its width. A Row lays out (and
                // measures) only visible children, so the width comes from
                // the label metrics, which keep it while hidden.
                visible: root.overflowIds.indexOf(modelData.id) < 0
                implicitWidth: labelWidth + (hasCount ? labelRow.spacing + countBox.width : 0)
                               + root.pillPadding * 2
                onImplicitWidthChanged: Qt.callLater(root.relayout)

                TextMetrics {
                    id: labelMetrics
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    font.weight: Font.Medium
                    text: tab.modelData.label
                }
                TextMetrics {
                    id: selectedMetrics
                    font: moreMetrics.font
                    text: tab.modelData.label
                }

                height: row.height
                hoverEnabled: true
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.label
                onClicked: root.activated(modelData.id)

                contentItem: Row {
                    id: labelRow
                    spacing: Theme.space.sm
                    leftPadding: root.pillPadding
                    Text {
                        id: label
                        anchors.verticalCenter: parent.verticalCenter
                        width: tab.labelWidth
                        text: tab.modelData.label
                        color: tab.inkColor
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.body.pixelSize
                        font.weight: tab.selected ? Font.DemiBold : Font.Medium
                    }
                    // The count, on an accent pill when something waits.
                    Item {
                        id: countBox
                        anchors.verticalCenter: parent.verticalCenter
                        visible: tab.hasCount
                        width: tab.attention ? Math.max(height, countLabel.implicitWidth + Theme.space.sm) : countLabel.implicitWidth
                        height: tab.attention ? Math.ceil(countLabel.implicitHeight) + Theme.space.xs : countLabel.implicitHeight

                        Rectangle {
                            anchors.fill: parent
                            visible: tab.attention
                            radius: Theme.radius.pill
                            color: Theme.palette.accent
                        }
                        Text {
                            id: countLabel
                            anchors.centerIn: parent
                            text: tab.hasCount ? tab.modelData.count : ""
                            color: tab.attention ? Theme.palette.textOnAccent
                                 : tab.selected ? tab.inkColor : Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                            font.weight: tab.attention ? Font.DemiBold : Font.Medium
                        }
                    }
                }

                background: Item {
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radius.pill
                        color: tab.selected ? Theme.palette.accentSubtle
                             : tab.down ? Theme.palette.pressedOverlay
                             : tab.hovered ? Theme.palette.hoverOverlay : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                    }
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -Theme.focus.offset
                        radius: Theme.radius.pill
                        color: "transparent"
                        border.width: Theme.focus.width
                        border.color: Theme.palette.focusRing
                        visible: tab.visualFocus
                    }
                }
            }
        }

        // Styled as one more tab, so the strip reads as a single row; only
        // there while something has overflowed into it. While the current
        // tab is one of those it carries that tab's name and the selected
        // look, so the selection is always visible in the strip itself.
        AbstractButton {
            id: moreButton
            readonly property bool selected: root.currentOverflowTab !== null
            readonly property color inkColor: selected ? Theme.palette.accentText
                                            : hovered || moreMenu.visible ? Theme.palette.textPrimary
                                            : Theme.palette.textSecondary
            visible: root.overflowIds.length > 0
            implicitWidth: moreLabel.implicitWidth + moreRow.spacing + Theme.icon.sm + root.pillPadding * 2
            height: row.height
            hoverEnabled: true
            Accessible.role: Accessible.ButtonMenu
            Accessible.name: selected ? qsTr("%1, more tabs").arg(root.currentOverflowTab.label) : qsTr("More tabs")
            onClicked: moreMenu.visible ? moreMenu.close() : moreMenu.open()

            contentItem: Row {
                id: moreRow
                spacing: Theme.space.xs
                leftPadding: root.pillPadding
                Text {
                    id: moreLabel
                    anchors.verticalCenter: parent.verticalCenter
                    text: moreButton.selected ? root.currentOverflowTab.label : moreMetrics.text
                    color: moreButton.inkColor
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    font.weight: moreButton.selected ? Font.DemiBold : Font.Medium
                }
                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "chevron-down"
                    size: Theme.icon.sm
                    color: moreButton.inkColor
                }
            }

            background: Item {
                // The same soft pill a selected tab has.
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radius.pill
                    color: moreButton.selected ? Theme.palette.accentSubtle
                         : moreButton.down ? Theme.palette.pressedOverlay
                         : moreButton.hovered || moreMenu.visible ? Theme.palette.hoverOverlay : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                }
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -Theme.focus.offset
                    radius: Theme.radius.pill
                    color: "transparent"
                    border.width: Theme.focus.width
                    border.color: Theme.palette.focusRing
                    visible: moreButton.visualFocus
                }
            }

            Menu {
                id: moreMenu
                y: moreButton.height

                Instantiator {
                    model: root.overflowTabs
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.count !== undefined && modelData.count >= 0
                              ? qsTr("%1 (%2)").arg(modelData.label).arg(modelData.count)
                              : modelData.label
                        onTriggered: root.activated(modelData.id)
                    }
                    onObjectAdded: (index, object) => moreMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => moreMenu.removeItem(object)
                }
            }
        }
    }
}
