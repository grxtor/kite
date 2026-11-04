// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

/*
 * What a button, a menu entry, a delegate or a tab shows: an icon, the text beside or under it, or either alone,
 * laid out as IconLabel lays them out, but with the text always plain. IconLabel has no textFormat: it reads tags
 * in its text as markup, and that text is often a name from someone else (an instance, a mod, a pack).
 *
 * The icon and the text are one block, placed by `alignment` inside the paddings (an axis left out is centred);
 * the text is elided when the block is wider than the room there is.
 */
Item {
    id: root

    property string text
    property font font
    property color color
    // An AbstractButton.display value.
    property int display: T.AbstractButton.TextBesideIcon
    property real spacing: 0
    property bool mirrored: false
    property int alignment: Qt.AlignCenter
    property real leftPadding: 0
    property real rightPadding: 0

    // The fields of the control's `icon`, which a QML property cannot hold whole.
    property string iconName
    property url iconSource
    property int iconWidth: 0
    property int iconHeight: 0
    property color iconColor: "transparent"
    property bool iconCache: true

    readonly property bool showIcon: root.display !== T.AbstractButton.TextOnly
                                     && (root.iconName.length > 0 || root.iconSource.toString().length > 0)
    readonly property bool showText: root.display !== T.AbstractButton.IconOnly && root.text.length > 0
    readonly property bool stacked: root.display === T.AbstractButton.TextUnderIcon
    readonly property real gap: root.showIcon && root.showText && image.implicitWidth > 0 ? root.spacing : 0

    readonly property real roomWidth: Math.max(0, root.width - root.leftPadding - root.rightPadding)
    readonly property real iconBoxWidth: root.showIcon ? Math.min(image.implicitWidth, root.roomWidth) : 0
    readonly property real iconBoxHeight: root.showIcon ? Math.min(image.implicitHeight, root.height) : 0
    readonly property real textBoxWidth: !root.showText ? 0
        : Math.max(0, Math.min(natural.implicitWidth, root.stacked ? root.roomWidth
                                                                   : root.roomWidth - root.iconBoxWidth - root.gap))
    readonly property real textBoxHeight: !root.showText ? 0
        : Math.max(0, Math.min(natural.implicitHeight, root.stacked ? root.height - root.iconBoxHeight - root.gap
                                                                    : root.height))
    readonly property real blockWidth: root.stacked ? Math.max(root.iconBoxWidth, root.textBoxWidth)
                                                    : root.iconBoxWidth + root.gap + root.textBoxWidth
    readonly property real blockHeight: root.stacked ? root.iconBoxHeight + root.gap + root.textBoxHeight
                                                     : Math.max(root.iconBoxHeight, root.textBoxHeight)

    // `alignment` as drawn: mirroring swaps left and right.
    readonly property int horizontal: {
        const h = root.alignment & Qt.AlignHorizontal_Mask
        if (h & Qt.AlignHCenter || !h)
            return Qt.AlignHCenter
        const right = (h & Qt.AlignRight) !== 0
        return right !== root.mirrored ? Qt.AlignRight : Qt.AlignLeft
    }
    readonly property int vertical: {
        const v = root.alignment & Qt.AlignVertical_Mask
        return v & Qt.AlignTop ? Qt.AlignTop : v & Qt.AlignBottom ? Qt.AlignBottom : Qt.AlignVCenter
    }
    readonly property real blockX: root.leftPadding + (root.horizontal === Qt.AlignLeft ? 0
        : root.horizontal === Qt.AlignRight ? root.roomWidth - root.blockWidth
        : (root.roomWidth - root.blockWidth) / 2)
    readonly property real blockY: root.vertical === Qt.AlignTop ? 0
        : root.vertical === Qt.AlignBottom ? root.height - root.blockHeight
        : (root.height - root.blockHeight) / 2

    implicitWidth: root.leftPadding + root.rightPadding
                   + (root.stacked ? Math.max(root.showIcon ? image.implicitWidth : 0, root.showText ? natural.implicitWidth : 0)
                                   : (root.showIcon ? image.implicitWidth : 0) + root.gap
                                     + (root.showText ? natural.implicitWidth : 0))
    implicitHeight: root.stacked ? (root.showIcon ? image.implicitHeight : 0) + root.gap
                                   + (root.showText ? natural.implicitHeight : 0)
                                 : Math.max(root.showIcon ? image.implicitHeight : 0,
                                            root.showText ? natural.implicitHeight : 0)
    baselineOffset: label.y + label.baselineOffset

    IconImage {
        id: image
        visible: root.showIcon
        // Beside the text it leads the block (trails it when mirrored); over the text it is centred. On whole
        // pixels, as IconLabel puts it, so a line icon stays sharp.
        x: Math.round(root.stacked ? root.blockX + (root.blockWidth - width) / 2
                      : root.mirrored ? root.blockX + root.blockWidth - width : root.blockX)
        y: Math.round(root.stacked ? root.blockY : root.blockY + (root.blockHeight - height) / 2)
        width: root.iconBoxWidth
        height: root.iconBoxHeight
        name: root.iconName
        source: root.iconSource
        sourceSize: Qt.size(root.iconWidth, root.iconHeight)
        color: root.iconColor
        cache: root.iconCache
    }

    // The text's own size, never squeezed: an eliding Text laid out at width 0 reports an implicit width of 0, so
    // the drawn label cannot measure itself once it has been hidden.
    Text {
        id: natural
        visible: false
        text: root.text
        textFormat: Text.PlainText
        font: root.font
    }

    Text {
        id: label
        visible: root.showText
        x: root.stacked ? root.blockX + (root.blockWidth - width) / 2
           : root.mirrored ? root.blockX : root.blockX + root.blockWidth - width
        y: root.stacked ? root.blockY + root.blockHeight - height : root.blockY + (root.blockHeight - height) / 2
        width: root.textBoxWidth
        height: root.textBoxHeight
        text: root.text
        textFormat: Text.PlainText
        font: root.font
        color: root.color
        elide: Text.ElideRight
    }
}
