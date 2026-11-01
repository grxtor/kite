// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Kite's news on Home's side panel: the latest few posts as cards, newest first, each opening its post in the
 * browser. A card shows a picture only when the post has an `image` role. Release posts for the running version or
 * older, and posts older than `maxAgeDays`, are skipped; with nothing left the section hides.
 */
Column {
    id: root

    // NewsModel, or null.
    property var news: null
    // No fill or hairline at rest, for a host that is already a panel; hover still lifts a card.
    property bool flat: false

    readonly property int maxEntries: 3
    readonly property int maxAgeDays: 90
    readonly property real dayMs: 24 * 60 * 60 * 1000
    readonly property bool hasNews: root.entries.length > 0

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
            readonly property string image: model.image || ""
        }
        onObjectAdded: Qt.callLater(root.readNews)
        onObjectRemoved: Qt.callLater(root.readNews)
    }

    // [major, minor, patch] from the first "1.2" or "1.2.3" in @p text, or null.
    function versionOf(text) {
        var match = /(\d+)\.(\d+)(?:\.(\d+))?/.exec(text || "")
        return match ? [parseInt(match[1]), parseInt(match[2]), parseInt(match[3] || "0")] : null
    }
    // A release post ("Kite Release 9.1.0, now available") for the running version or an older one.
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
            id: post
            required property var modelData
            readonly property bool hasImage: post.modelData.image.length > 0 && picture.status !== Image.Error
            readonly property real imageAspect: 16 / 9

            width: root.width
            hoverEnabled: true
            padding: Theme.space.md
            Accessible.name: post.modelData.title
            onClicked: if (post.modelData.link) Qt.openUrlExternally(post.modelData.link)

            background: StateFill {
                id: cardFill
                card: true
                hovered: post.hovered
                active: post.down
                activeFill: Theme.palette.surfaceRaised
                rest: root.flat ? "transparent" : Theme.palette.surface
                radius: Theme.radius.lg
                border.width: post.visualFocus ? Theme.focus.width : root.flat ? 0 : 1
                border.color: post.visualFocus ? Theme.palette.focusRing
                            : post.hovered ? Theme.neutral.borderHover : Theme.palette.border
            }

            contentItem: Column {
                spacing: Theme.space.sm

                Item {
                    visible: post.hasImage
                    width: parent.width
                    height: visible ? Math.round(width / post.imageAspect) : 0

                    Image {
                        id: picture
                        anchors.fill: parent
                        source: post.modelData.image
                        sourceSize: Qt.size(width * 2, height * 2)
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        mipmap: true
                        asynchronous: true
                    }
                    CornerMatte {
                        cornerRadius: Theme.radius.md
                        matte: cardFill.color
                    }
                }

                Copy {
                    width: parent.width
                    text: post.modelData.title
                    role: "bodyStrong"
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }
                Copy {
                    width: parent.width
                    visible: !!post.modelData.date
                    text: post.modelData.date ? Format.calendarDate(new Date(post.modelData.date)) : ""
                    role: "label"
                    tone: "tertiary"
                }
            }
        }
    }
}
