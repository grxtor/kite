// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import MeshMC.Theme

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    leftPadding: Theme.space.sm + (!control.mirrored || !indicator || !indicator.visible ? 0 : indicator.width + spacing)
    rightPadding: Theme.space.sm + (control.mirrored || !indicator || !indicator.visible ? 0 : indicator.width + spacing)
    spacing: Theme.space.xs

    // This ComboBox uses our own ItemDelegate.qml (same directory, so no
    // import needed) for popup rows instead of Basic's, otherwise every row
    // would suddenly look unstyled next to a themed field.
    delegate: ItemDelegate {
        required property var model
        required property int index

        width: ListView.view.width
        text: model[control.textRole]
        font.weight: control.currentIndex === index ? Theme.type.bodyStrong.weight : Theme.type.body.weight
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
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
        color: Theme.palette.textPrimary
        selectionColor: Theme.palette.selection
        selectedTextColor: Theme.palette.selectionText
        verticalAlignment: Text.AlignVCenter

        background: Item {}
    }

    background: Rectangle {
        implicitWidth: Theme.control.heightLg * 4
        implicitHeight: Theme.control.height
        radius: Theme.radius.md
        color: control.down ? Theme.palette.pressedOverlay : Theme.palette.surfaceRaised
        border.width: 1
        border.color: control.activeFocus ? Theme.palette.borderStrong : Theme.palette.border
        opacity: control.enabled ? 1.0 : 0.45

        FocusRing {
            anchors.fill: parent
            anchors.margins: -Theme.space.xxs
            radius: parent.radius + Theme.space.xxs
            visible: control.visualFocus
        }
    }

    popup: T.Popup {
        y: control.height + Theme.space.xxs
        width: control.width
        height: Math.min(contentItem.implicitHeight, control.Window.height - topMargin - bottomMargin)
        topMargin: Theme.space.sm
        bottomMargin: Theme.space.sm
        padding: Theme.space.xxs

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

        background: Rectangle {
            radius: Theme.radius.lg
            color: Theme.palette.surfaceOverlay
            border.width: 1
            border.color: Theme.palette.border

            PopupShadow { radius: parent.radius }
        }
    }
}
