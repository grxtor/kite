// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    // The same text inset as TextField, so a combo stacked above or below a
    // text field starts its text on the same column.
    leftPadding: Theme.space.md + (!control.mirrored || !indicator || !indicator.visible ? 0 : indicator.width + spacing)
    rightPadding: Theme.space.md + (control.mirrored || !indicator || !indicator.visible ? 0 : indicator.width + spacing)
    spacing: Theme.space.xs

    // This ComboBox uses our own ItemDelegate.qml (same directory, so no
    // import needed) for popup rows instead of Basic's, otherwise every row
    // would suddenly look unstyled next to a themed field. The rows read
    // like Menu's: the chosen value carries the soft accent pill (the tab
    // grammar), the row under the pointer or the keyboard a neutral overlay.
    delegate: ItemDelegate {
        id: row
        required property var model
        required property int index
        readonly property bool chosen: control.currentIndex === index

        width: ListView.view.width
        text: model[control.textRole]
        font.weight: chosen ? Theme.type.bodyStrong.weight : Theme.type.body.weight
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled

        contentItem: Text {
            text: row.text
            font: row.font
            color: row.chosen ? Theme.palette.accentText : Theme.palette.textPrimary
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            implicitHeight: Theme.control.menuItemHeight
            radius: Theme.radius.sm
            color: row.chosen ? Theme.palette.accentSubtle
                 : row.down ? Theme.palette.pressedOverlay
                 : row.highlighted ? Theme.palette.hoverOverlay
                 : "transparent"
        }
    }

    indicator: Chevron {
        // Theme.space.sm, not control.padding: leftPadding/rightPadding
        // above are set explicitly (asymmetric, to leave room for this
        // indicator), which leaves the generic `padding` itself at its
        // unset default of 0 -- reading it here silently put the indicator
        // flush against the control's own edge, right on top of its
        // border, instead of inset by the margin rightPadding actually
        // reserved for it.
        x: control.mirrored ? Theme.space.sm : control.width - width - Theme.space.sm
        y: control.topPadding + (control.availableHeight - height) / 2
        width: Theme.icon.sm
        height: Theme.icon.sm
        color: control.enabled ? Theme.palette.textSecondary : Theme.palette.textDisabled
        direction: 0
        // A quiet nod to the popup's open/closed state instead of a second
        // glyph: flipping the same chevron costs nothing and reads as "this
        // is now showing its other side".
        rotation: control.popup.visible ? 180 : 0
        Behavior on rotation {
            NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }
    }

    contentItem: T.TextField {
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0

        text: control.editable ? control.editText : control.displayText

        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        selectByMouse: control.selectTextByMouse

        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        font.weight: Theme.type.body.weight
        color: control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled
        selectionColor: Theme.palette.selection
        selectedTextColor: Theme.palette.selectionText
        verticalAlignment: Text.AlignVCenter

        background: Item {}
    }

    // The field look (see TextField.qml). Open, it keeps the accent edge a
    // focused field has, so the list visibly belongs to this box.
    background: Rectangle {
        implicitWidth: Theme.control.heightLg * 4
        implicitHeight: Theme.control.height
        radius: Theme.radius.md
        color: Theme.palette.surface
        border.width: 1
        border.color: control.popup.visible || (control.editable && control.activeFocus) ? Theme.palette.accent
                    : control.hovered && control.enabled ? Theme.neutral.borderHover : Theme.palette.border
        opacity: control.enabled ? 1.0 : Theme.opacity.disabled

        Behavior on border.color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        // The press itself, under the text: a quick neutral overlay.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.pressed ? Theme.palette.pressedOverlay : "transparent"
        }

        // A typed-into (editable) combo is a text field and shows the halo
        // on any focus; a plain one is a button, so keyboard focus only.
        FocusRing {
            halo: true
            visible: control.visualFocus || (control.editable && control.activeFocus)
        }
    }

    // How many rows the open list shows before it scrolls. A long list (a
    // pack with sixty versions) used to grow to the full window height and
    // cover the page; launchers keep a dropdown to a handful of rows.
    property int maxVisibleRows: 8

    popup: T.Popup {
        y: control.height + Theme.space.xs
        width: control.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding,
                         control.maxVisibleRows * Theme.control.menuItemHeight + topPadding + bottomPadding,
                         control.Window.height - topMargin - bottomMargin)
        topMargin: Theme.space.sm
        bottomMargin: Theme.space.sm
        padding: Theme.space.xs

        // This popup is built inline rather than reused from Popup.qml (the
        // ListView content needs control.delegateModel wired in), so it
        // repeats that style's open/close fade instead of inheriting it.
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0

            T.ScrollIndicator.vertical: ScrollIndicator { }
        }

        // The popup glass treatment Menu.qml uses.
        background: GlassSurface {
            radius: Theme.radius.lg
            level: Theme.elevation.md
        }
    }
}
