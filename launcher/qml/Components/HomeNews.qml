// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Kite's news on Home's side panel, under "Set up Kite": the latest few
 * posts as cards, newest first, each opening its post in the browser. A
 * card shows the post's own picture only when the post has one (an `image`
 * role -- NewsModel has none yet, so today no card does); never a stand-in.
 *
 * Only news that is still news counts: a release post for the version
 * already running (or an older one) is skipped, and so is anything older
 * than `maxAgeDays`. With nothing left the whole section hides.
 */
Column {
    id: root

    // QmlShell.news (NewsModel): Kite's news, newest first, or null.
    property var news: null

    readonly property int maxEntries: 3
    // Older than this, a post is history rather than news.
    readonly property int maxAgeDays: 90
    readonly property real dayMs: 24 * 60 * 60 * 1000
    readonly property bool hasNews: root.entries.length > 0

    // What NewsModel holds, as plain objects, newest first.
    property var allEntries: []
    function readNews() {
        var entries = []
        for (var i = 0; i < reader.count; ++i) {
            var entry = reader.objectAt(i)
            if (entry)
                entries.push({ title: entry.title, link: entry.link, date: entry.date, image: entry.image })
        }
        root.allEntries = entries
    }
    Instantiator {
        id: reader
        model: root.news
        delegate: QtObject {
            required property var model
            readonly property string title: model.title || ""
            readonly property string link: model.link || ""
            readonly property var date: model.date
            // A picture of the post's own, once the model has one.
            readonly property string image: model.image || ""
        }
        onObjectAdded: Qt.callLater(root.readNews)
        onObjectRemoved: Qt.callLater(root.readNews)
    }

    // [major, minor, patch] from the first "1.2" or "1.2.3" in @p text, or
    // null.
    function versionOf(text) {
        var match = /(\d+)\.(\d+)(?:\.(\d+))?/.exec(text || "")
        return match ? [parseInt(match[1]), parseInt(match[2]), parseInt(match[3] || "0")] : null
    }
    // A release post ("Kite Release 9.1.0, now available") for the
    // version running now or an older one: nothing new to the reader.
    function isOutdatedRelease(title) {
        if (!/releas|available|update/i.test(title))
            return false
        var posted = root.versionOf(title)
        var running = root.versionOf(Qt.application.version)
        if (!posted || !running)
            return false
        for (var i = 0; i < posted.length; ++i) {
            if (posted[i] !== running[i])
                return posted[i] < running[i]
        }
        return true
    }
    // The entries still worth showing, newest first, at most maxEntries.
    readonly property var entries: {
        var now = Date.now()
        var shown = []
        for (var i = 0; i < root.allEntries.length && shown.length < root.maxEntries; ++i) {
            var entry = root.allEntries[i]
            var posted = entry.date ? new Date(entry.date).getTime() : NaN
            // Newest first: everything after a stale entry is staler.
            if (posted > 0 && now - posted > root.maxAgeDays * root.dayMs)
                break
            if (!root.isOutdatedRelease(entry.title))
                shown.push(entry)
        }
        return shown
    }

    visible: root.hasNews
    spacing: Theme.space.md

    HomePanelHeading {
        width: parent.width
        text: qsTr("Kite news")
    }

    Repeater {
        model: root.entries
        delegate: AbstractButton {
            id: card
            required property var modelData
            readonly property bool hasImage: card.modelData.image.length > 0 && picture.status !== Image.Error
            readonly property real imageAspect: 16 / 9

            width: root.width
            hoverEnabled: true
            padding: Theme.space.md
            Accessible.name: card.modelData.title
            onClicked: if (card.modelData.link) Qt.openUrlExternally(card.modelData.link)

            background: Rectangle {
                id: cardFill
                radius: Theme.radius.lg
                color: card.down ? Theme.palette.surfaceRaised
                     : card.hovered ? Theme.palette.surfaceHover : Theme.palette.surface
                border.width: card.visualFocus ? Theme.focus.width : 1
                border.color: card.visualFocus ? Theme.palette.focusRing
                            : card.hovered ? Theme.neutral.borderHover : Theme.palette.border
                Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            }

            contentItem: Column {
                spacing: Theme.space.sm

                Item {
                    visible: card.hasImage
                    width: parent.width
                    height: visible ? Math.round(width / card.imageAspect) : 0

                    Image {
                        id: picture
                        anchors.fill: parent
                        source: card.modelData.image
                        sourceSize: Qt.size(width * 2, height * 2)
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        mipmap: true
                        asynchronous: true
                    }
                    // Rounds the picture's corners in the card's own colour.
                    CornerMatte {
                        cornerRadius: Theme.radius.md
                        matte: cardFill.color
                    }
                }

                Text {
                    width: parent.width
                    text: card.modelData.title
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.bodyStrong.pixelSize
                    font.weight: Theme.type.bodyStrong.weight
                }
                Text {
                    width: parent.width
                    visible: !!card.modelData.date
                    text: card.modelData.date ? Format.calendarDate(new Date(card.modelData.date)) : ""
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }
        }
    }
}
