// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The Home page: one purpose, "get back to what I was doing". The default
 * start page and the sidebar's first item. MeshMC's current news (when
 * there is any worth showing), the most recently played instances ("Jump
 * back in") and the worlds played most recently across all of them
 * ("Recent worlds") -- all from models the shell already keeps locally
 * (recentModel/recentWorlds/news), so opening this page makes no network
 * call. The library itself is not repeated here: it is one click away in
 * the sidebar, which also lists the recent instances.
 *
 * NEWS. The banner is the news item itself -- its headline and date over
 * MeshMC's own rendered scene for the time of day, with Read more -- not a
 * greeting. It shows only news that is still news: a release post for the
 * version already running (or an older one) is skipped, and so is anything
 * older than `newsMaxAgeDays`. Without such an entry there is no banner at
 * all, and Home starts with "Jump back in". On a short window the banner
 * shrinks to one line (`compact`), so it never pushes the cards under the
 * play bar.
 *
 * Like every other page it sits under TopBar (title + account button), and
 * its content stops growing at Theme.layout.maxContentWidth and centres, so
 * a wide window does not leave everything hugging the left edge.
 */
Item {
    id: root

    /*
     * The identical look every section heading on this page shares ("Jump
     * back in", "Recent worlds").
     */
    component SectionHeading: Text {
        // One height for every heading, so every heading-to-content gap on
        // the page matches.
        height: Theme.control.heightSm
        verticalAlignment: Text.AlignVCenter
        color: Theme.palette.textPrimary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.title.pixelSize + 1
        font.weight: Font.Bold
    }

    /*
     * "More news ▾": the latest few headlines (`entries`, newest first),
     * each opening its post. On the banner's photo (`onMedia`) or on the
     * compact line. An inline component sees none of this file's ids, so
     * the entries come in as a property.
     */
    component NewsMenuButton: AbstractButton {
        id: moreNews
        property var entries: []
        property bool onMedia: false
        readonly property int maxEntries: 8
        visible: moreNews.entries.length > 1
        implicitWidth: moreRow.implicitWidth
        implicitHeight: Theme.control.heightSm
        hoverEnabled: true
        Accessible.name: qsTr("More news")
        onClicked: newsMenu.popup(moreNews, 0, moreNews.height)

        readonly property color restColor: moreNews.onMedia ? Theme.media.textSecondary : Theme.palette.textSecondary
        readonly property color hoverColor: moreNews.onMedia ? Theme.media.text : Theme.palette.textPrimary

        contentItem: Item {
            Row {
                id: moreRow
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space.xxs
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("More news")
                    color: moreNews.hovered ? moreNews.hoverColor : moreNews.restColor
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                }
                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "chevron-down"
                    size: Theme.icon.sm
                    color: moreNews.restColor
                }
            }
        }

        Menu {
            id: newsMenu
            Instantiator {
                model: moreNews.entries.slice(0, moreNews.maxEntries)
                delegate: MenuItem {
                    required property var modelData
                    text: modelData.title
                    onTriggered: if (modelData.link) Qt.openUrlExternally(modelData.link)
                }
                onObjectAdded: (index, object) => newsMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => newsMenu.removeItem(object)
            }
        }
    }

    // Most recently played instances first; never-played ones left out
    // (QmlShell.recentModel) -- "Jump back in" takes its first three.
    property var recentModel: null
    // Every instance, for the empty-library check.
    property var instanceModel: null
    // QmlShell.recentWorlds -- most recently played worlds across every
    // instance; see RecentWorldsModel's own class comment.
    property var recentWorldsModel: null
    // QmlShell.news (NewsModel): MeshMC's news, newest first, or null.
    property var news: null

    // "day", "golden" or "night" to pin the banner's scene (review
    // snapshots); empty follows the local clock.
    property string moodOverride: ""

    signal launchRequested(string id)
    signal stopRequested(string id)
    // Opens the instance page -- a Jump back in card.
    signal openInstanceRequested(string id)
    // Opens the instance page on its Worlds tab -- a Recent worlds tile.
    signal openInstanceWorldsRequested(string id)
    signal createRequested()
    signal discoverRequested()

    readonly property int pagePadding: Theme.space.xl + Theme.space.xs
    readonly property int libraryCount: root.instanceModel && root.instanceModel.count !== undefined ? root.instanceModel.count : 0
    readonly property int recentCount: root.recentModel && root.recentModel.count !== undefined ? root.recentModel.count : 0
    readonly property bool hasLibrary: root.libraryCount > 0

    // The content column: the page minus its padding, never wider than
    // Theme.layout.maxContentWidth, centred.
    readonly property int contentWidth: Math.min(root.width - root.pagePadding * 2, Theme.layout.maxContentWidth)
    readonly property int contentX: Math.round((root.width - root.contentWidth) / 2)

    // -- News -------------------------------------------------------------
    // What NewsModel holds, as plain objects, newest first.
    property var newsEntries: []
    function readNews() {
        var entries = []
        for (var i = 0; i < newsReader.count; ++i) {
            var entry = newsReader.objectAt(i)
            if (entry)
                entries.push({ title: entry.title, link: entry.link, date: entry.date })
        }
        root.newsEntries = entries
    }
    Instantiator {
        id: newsReader
        model: root.news
        delegate: QtObject {
            required property string title
            required property string link
            required property var date
        }
        onObjectAdded: Qt.callLater(root.readNews)
        onObjectRemoved: Qt.callLater(root.readNews)
    }

    // Older than this, a post is history rather than news.
    readonly property int newsMaxAgeDays: 90
    readonly property real dayMs: 24 * 60 * 60 * 1000

    // [major, minor, patch] from the first "1.2" or "1.2.3" in @p text, or
    // null.
    function versionOf(text) {
        var match = /(\d+)\.(\d+)(?:\.(\d+))?/.exec(text || "")
        return match ? [parseInt(match[1]), parseInt(match[2]), parseInt(match[3] || "0")] : null
    }
    // A release post ("MeshMC Release 9.1.0, now available") for the
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
    // The newest entry still worth a banner, or null.
    readonly property var featuredNews: {
        var now = Date.now()
        for (var i = 0; i < root.newsEntries.length; ++i) {
            var entry = root.newsEntries[i]
            if (root.isOutdatedRelease(entry.title))
                continue
            var posted = entry.date ? new Date(entry.date).getTime() : NaN
            // Newest first: everything after a stale entry is staler.
            if (posted > 0 && now - posted > root.newsMaxAgeDays * root.dayMs)
                return null
            return entry
        }
        return null
    }
    readonly property bool hasNews: !!root.featuredNews
    readonly property string newsDate: root.hasNews && root.featuredNews.date
                                       ? Format.calendarDate(new Date(root.featuredNews.date)) : ""

    // Below this page height the banner is a single line: the cards keep
    // the room (860x540 leaves the page about 400px).
    readonly property int compactBelow: 560
    readonly property bool compact: root.height < root.compactBelow
    // The full banner: a share of the page height, within bounds.
    readonly property real bannerShare: 0.24
    readonly property int bannerMinHeight: 148
    readonly property int bannerMaxHeight: 200

    // "Jump back in": as many whole cards as fit (up to three), stretched to
    // fill the row -- never a card sliced at the edge.
    readonly property int jumpMax: 3
    readonly property int jumpMinWidth: 300
    readonly property int jumpMaxWidth: 440
    readonly property int jumpSpacing: Theme.space.lg
    readonly property int jumpFit: Math.max(1, Math.min(root.jumpMax,
            Math.floor((root.contentWidth + root.jumpSpacing) / (root.jumpMinWidth + root.jumpSpacing))))
    readonly property int jumpCount: Math.max(1, Math.min(root.jumpFit, root.recentCount))
    readonly property int jumpCardWidth: Math.min(root.jumpMaxWidth,
            Math.floor((root.contentWidth - root.jumpSpacing * (root.jumpFit - 1)) / root.jumpFit))

    // "Recent worlds": rows of as many whole tiles as fit -- two rows, or
    // one on a short window.
    readonly property bool hasWorlds: worldsRepeater.count > 0
    readonly property int worldMinWidth: 200
    readonly property int worldSpacing: Theme.space.lg
    readonly property int worldColumns: Math.max(1, Math.floor((root.contentWidth + root.worldSpacing)
                                                               / (root.worldMinWidth + root.worldSpacing)))
    readonly property int worldRows: root.compact ? 1 : 2
    readonly property int worldMax: root.worldColumns * root.worldRows
    readonly property int worldTileWidth: Math.floor((root.contentWidth - root.worldSpacing * (root.worldColumns - 1)) / root.worldColumns)

    // Re-read once a minute so the banner's sky turns with the day while
    // the launcher stays open; the timer only runs while Home is on screen.
    property int hour: new Date().getHours()
    readonly property string mood: root.moodOverride.length > 0 ? root.moodOverride
                                                                : PixelArt.homeMood(root.hour)
    Timer {
        interval: 60 * 1000
        repeat: true
        running: root.visible && root.hasNews && root.moodOverride.length === 0
        onTriggered: root.hour = new Date().getHours()
    }

    function openNews() {
        if (root.hasNews && root.featuredNews.link)
            Qt.openUrlExternally(root.featuredNews.link)
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.y + content.height + Theme.space.xxl
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: content
            x: root.contentX
            y: Theme.space.xs
            width: root.contentWidth
            spacing: Theme.space.xl

            // -- News banner (full) ----------------------------------------
            // The time of day's rendered scene, rounded like every other
            // large surface, with the headline on its darker left side.
            Item {
                id: banner
                visible: root.hasNews && !root.compact
                width: parent.width
                height: Math.round(Math.max(root.bannerMinHeight, bannerText.implicitHeight + Theme.space.xl * 2,
                                            Math.min(root.bannerMaxHeight, root.height * root.bannerShare)))

                CoverArt {
                    anchors.fill: parent
                    source: banner.visible ? PixelArt.homeHeroUrl(root.mood) : ""
                    radius: Theme.radius.xl
                    scrim: "horizontal"
                    matte: Theme.palette.canvas
                }

                Column {
                    id: bannerText
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space.xxl
                    anchors.right: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.xs

                    Text {
                        width: parent.width
                        text: root.hasNews ? root.featuredNews.title : ""
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: Theme.media.text
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.heading.pixelSize
                        font.weight: Font.Bold
                    }
                    Text {
                        width: parent.width
                        visible: text.length > 0
                        text: root.newsDate
                        color: Theme.media.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                    Row {
                        topPadding: Theme.space.sm
                        spacing: Theme.space.md

                        Button {
                            text: qsTr("Read more")
                            icon.source: Icons.url("external-link")
                            onClicked: root.openNews()
                        }
                        NewsMenuButton {
                            anchors.verticalCenter: parent.verticalCenter
                            entries: root.newsEntries
                            onMedia: true
                        }
                    }
                }
            }

            // -- News line (compact) ---------------------------------------
            // A short window: the same headline as one row on the page.
            Rectangle {
                visible: root.hasNews && root.compact
                width: parent.width
                height: Theme.control.heightLg + Theme.space.sm
                radius: Theme.radius.lg
                color: Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                MeshIcon {
                    id: compactIcon
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space.lg
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "bell"
                    size: Theme.icon.sm
                    color: Theme.palette.textTertiary
                }
                Text {
                    id: compactTitle
                    anchors.left: compactIcon.right
                    anchors.leftMargin: Theme.space.sm
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, compactActions.x - x - compactDate.implicitWidth - Theme.space.md * 2)
                    text: root.hasNews ? root.featuredNews.title : ""
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    font.weight: Font.Medium
                }
                Text {
                    id: compactDate
                    anchors.left: compactTitle.right
                    anchors.leftMargin: Theme.space.md
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.newsDate
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                }
                Row {
                    id: compactActions
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space.sm
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.xs

                    NewsMenuButton {
                        anchors.verticalCenter: parent.verticalCenter
                        entries: root.newsEntries
                    }
                    Button {
                        flat: true
                        text: qsTr("Read more")
                        onClicked: root.openNews()
                    }
                }
            }

            // -- Jump back in ----------------------------------------------
            Column {
                visible: root.hasLibrary
                width: parent.width
                spacing: Theme.space.md

                SectionHeading {
                    visible: root.recentCount > 0
                    text: qsTr("Jump back in")
                }

                Row {
                    visible: root.recentCount > 0
                    spacing: root.jumpSpacing

                    Repeater {
                        model: root.recentModel
                        // A plain Item wrapper, not HomeJumpCard directly:
                        // HomeJumpCard's own required properties put a
                        // direct delegate in "bound" mode, where a bare
                        // `index` reference no longer resolves for
                        // Repeater. The wrapper carries no required
                        // properties of its own, so it stays "unbound",
                        // and `model.<role>` -- always valid regardless
                        // of binding mode -- forwards each role into
                        // HomeJumpCard explicitly.
                        delegate: Item {
                            // The most recently played first (recentModel
                            // already sorts that way), only as many as fit
                            // whole.
                            visible: model.index < root.jumpCount
                            width: visible ? card.width : 0
                            height: visible ? card.height : 0

                            HomeJumpCard {
                                id: card
                                // The last card of a full row takes the
                                // pixels the even split rounded away, so the
                                // row ends exactly on the content edge.
                                width: model.index === root.jumpFit - 1 && root.jumpCount === root.jumpFit
                                       && root.jumpCardWidth < root.jumpMaxWidth
                                       ? root.contentWidth - (root.jumpFit - 1) * (root.jumpCardWidth + root.jumpSpacing)
                                       : root.jumpCardWidth
                                instanceId: model.instanceId
                                name: model.name
                                iconKey: model.iconKey
                                isRunning: model.isRunning
                                canLaunch: model.canLaunch
                                lastLaunch: model.lastLaunch
                                totalTimePlayed: model.totalTimePlayed
                                gameVersion: model.gameVersion
                                loader: model.loader
                                iconTint: model.iconTint
                                coverImage: model.coverImage
                                packArt: model.packArt || ""
                                hasCrashed: model.hasCrashed
                                onClicked: root.openInstanceRequested(model.instanceId)
                                onPlayRequested: root.launchRequested(model.instanceId)
                                onStopRequested: root.stopRequested(model.instanceId)
                            }
                        }
                    }
                }

                // The library has instances but none has ever been played:
                // never dress one of them up as "continue playing".
                Row {
                    visible: root.recentCount === 0
                    spacing: Theme.space.sm

                    MeshIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: "compass"
                        size: Theme.icon.md
                        color: Theme.palette.textTertiary
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("New here? Start with Discover.")
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.body.pixelSize
                    }
                    Button {
                        anchors.verticalCenter: parent.verticalCenter
                        flat: true
                        text: qsTr("Discover")
                        onClicked: root.discoverRequested()
                    }
                }
            }

            // -- Recent worlds ---------------------------------------------
            // Hidden entirely when there is nothing to show -- no empty
            // section, no placeholder; the instance page has every world.
            Column {
                visible: root.hasLibrary && root.hasWorlds
                width: parent.width
                spacing: Theme.space.md

                SectionHeading {
                    text: qsTr("Recent worlds")
                }

                Grid {
                    columns: root.worldColumns
                    columnSpacing: root.worldSpacing
                    rowSpacing: root.worldSpacing

                    Repeater {
                        id: worldsRepeater
                        model: root.recentWorldsModel
                        // Wrapped for the same reason as the jump cards:
                        // HomeWorldTile's required properties would hide
                        // `index` from a direct delegate.
                        delegate: Item {
                            visible: model.index < root.worldMax
                            width: visible ? tile.width : 0
                            height: visible ? tile.height : 0

                            HomeWorldTile {
                                id: tile
                                // The last column takes the rounding
                                // remainder, so the row ends on the edge.
                                width: model.index % root.worldColumns === root.worldColumns - 1
                                       ? root.contentWidth - (root.worldColumns - 1) * (root.worldTileWidth + root.worldSpacing)
                                       : root.worldTileWidth
                                worldName: model.worldName
                                folderName: model.folderName
                                iconUrl: model.iconUrl
                                lastPlayed: model.lastPlayed
                                instanceId: model.instanceId
                                instanceName: model.instanceName
                                onClicked: root.openInstanceWorldsRequested(model.instanceId)
                            }
                        }
                    }
                }
            }

            // Nothing in the library yet: the way in sits right at the top.
            EmptyState {
                visible: !root.hasLibrary
                width: parent.width
                title: qsTr("Your library is empty")
                body: qsTr("An instance is one Minecraft setup: a version, a mod loader and its mods. Create one to start playing.")
                actionText: qsTr("Create your first instance")
                actionIcon: "plus"
                onActionTriggered: root.createRequested()

                Image {
                    // Only fetched when the empty state is actually shown.
                    source: root.hasLibrary ? "" : PixelArt.emptyUrl("no_instances")
                    sourceSize: Qt.size(64, 64)
                    width: 64
                    height: 64
                    smooth: false
                }
            }
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: flick
    }
}
