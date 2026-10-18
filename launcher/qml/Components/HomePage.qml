// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Home page: one purpose, "get back to what I was doing". The default
 * start page and the sidebar's first item. Kite's current news (when
 * there is any worth showing), the most recently played instances ("Jump
 * back in") and the worlds played most recently across all of them
 * ("Recent worlds") -- all from models the shell already keeps locally
 * (recentModel/recentWorlds/news), so opening this page makes no network
 * call. The library itself is not repeated here: it is one click away in
 * the sidebar, which also lists the recent instances.
 *
 * Laid out like Modrinth App's Home: rows, not covers. "Jump back in" is a
 * list of 80px rows (HomeJumpCard) with Play on each, and "Recent worlds"
 * uses the same row grammar (HomeWorldTile).
 *
 * HEADER. Only real art earns a picture: when the instance played last has
 * a screenshot of its own, it leads the page as a banner (its name, what it
 * runs, Play and Open) and "Jump back in" starts with the one after it, so
 * nothing is listed twice. Without one there is no banner at all -- never
 * a big empty surface -- and that instance is simply the first row.
 *
 * NEWS. Kite's own news is a compact card above "Jump back in": the
 * headline, its date, Read more and, with more than one post, a "More news"
 * menu. Only news that is still news counts: a release post for the version
 * already running (or an older one) is skipped, and so is anything older
 * than `newsMaxAgeDays`.
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
        font.family: Theme.font.heading
        font.pixelSize: Theme.type.heading.pixelSize
        font.weight: Theme.type.heading.weight
    }

    /*
     * A quiet fact on the header's photo: a line icon and a few words, in
     * the media colours (the clock for when, the hourglass for how long).
     */
    component HeroFact: Row {
        property string iconName
        property string text
        spacing: Theme.space.xs
        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            iconName: parent.iconName
            size: Theme.icon.sm
            color: Theme.media.textSecondary
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: parent.text
            color: Theme.media.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Font.Medium
        }
    }

    /*
     * "More news ▾": the latest few headlines (`entries`, newest first),
     * each opening its post. An inline component sees none of this file's
     * ids, so the entries come in as a property.
     */
    component NewsMenuButton: AbstractButton {
        id: moreNews
        property var entries: []
        readonly property int maxEntries: 8
        visible: moreNews.entries.length > 1
        implicitWidth: moreRow.implicitWidth + Theme.space.sm * 2
        implicitHeight: Theme.control.heightSm
        hoverEnabled: true
        Accessible.name: qsTr("More news")
        onClicked: newsMenu.popup(moreNews, 0, moreNews.height)

        background: Rectangle {
            radius: Theme.radius.sm
            color: moreNews.down ? Theme.palette.pressedOverlay
                 : moreNews.hovered ? Theme.palette.hoverOverlay : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
        contentItem: Item {
            Row {
                id: moreRow
                anchors.centerIn: parent
                spacing: Theme.space.xxs
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("More news")
                    color: moreNews.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                }
                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "chevron-down"
                    size: Theme.icon.sm
                    color: Theme.palette.textSecondary
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
    // (QmlShell.recentModel) -- "Jump back in" takes the first few.
    property var recentModel: null
    // Every instance, for the empty-library check.
    property var instanceModel: null
    // QmlShell.recentWorlds -- most recently played worlds across every
    // instance; see RecentWorldsModel's own class comment.
    property var recentWorldsModel: null
    // QmlShell.news (NewsModel): Kite's news, newest first, or null.
    property var news: null

    signal launchRequested(string id)
    signal stopRequested(string id)
    // Opens the instance page -- a Jump back in row, or the header.
    signal openInstanceRequested(string id)
    // Opens the instance page on its Worlds tab -- a Recent worlds row.
    signal openInstanceWorldsRequested(string id)
    signal createRequested()
    signal discoverRequested()

    readonly property int pagePadding: Theme.space.xl + Theme.space.xs
    readonly property int emptyStateWidth: 400
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
    // The newest entry still worth showing, or null.
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

    function openNews() {
        if (root.hasNews && root.featuredNews.link)
            Qt.openUrlExternally(root.featuredNews.link)
    }

    // -- Header -----------------------------------------------------------
    // The first "Jump back in" row, the most recently played instance,
    // claims this (see the Repeater below); null with nothing played.
    property Item featured: null
    // Only a real screenshot earns the header.
    readonly property bool hasHero: !!root.featured && root.recentCount > 0
                                    && root.featured.itemCoverImage.length > 0
    // A share of the page height within bounds, never shorter than its own
    // words.
    readonly property real heroShare: 0.3
    readonly property int heroMinHeight: 176
    readonly property int heroMaxHeight: 260
    readonly property int heroIconSize: 56
    readonly property string featuredPlayTime: root.featured && SettingsStore.bool("ShowGameTime")
                                               ? Format.playTime(root.featured.itemPlayTime) : ""

    // -- Jump back in -----------------------------------------------------
    // Up to five rows, as in Modrinth App, the header's instance not
    // listed again under it. One column, two on a window wide enough that
    // each row keeps a readable width.
    readonly property int jumpOffset: root.hasHero ? 1 : 0
    readonly property int jumpMax: 5
    readonly property int jumpCount: Math.max(0, Math.min(root.jumpMax, root.recentCount - root.jumpOffset))
    readonly property int rowGap: Theme.space.md
    readonly property int rowMinWidth: 540
    readonly property int jumpColumns: root.contentWidth >= root.rowMinWidth * 2 + root.rowGap ? 2 : 1
    readonly property int jumpRowWidth: Math.floor((root.contentWidth - root.rowGap * (root.jumpColumns - 1)) / root.jumpColumns)

    // -- Recent worlds ----------------------------------------------------
    // As many columns as fit at a readable width; two rows of them, or one
    // on a short window.
    readonly property bool hasWorlds: worldsRepeater.count > 0
    readonly property int worldMinWidth: 300
    readonly property int worldColumns: Math.max(1, Math.floor((root.contentWidth + root.rowGap) / (root.worldMinWidth + root.rowGap)))
    readonly property int compactBelow: 560
    readonly property int worldRows: root.height < root.compactBelow ? 1 : 2
    readonly property int worldMax: root.worldColumns * root.worldRows
    readonly property int worldWidth: Math.floor((root.contentWidth - root.rowGap * (root.worldColumns - 1)) / root.worldColumns)

    Flickable {
        id: flick
        anchors.fill: parent
        visible: root.hasLibrary
        contentWidth: width
        contentHeight: content.y + content.height + Theme.space.xxl
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: content
            x: root.contentX
            // The shell's panel adds no padding; the page pads itself.
            y: Theme.space.xl
            width: root.contentWidth
            spacing: Theme.space.xl

            // -- Header -----------------------------------------------------
            // The played-last instance's own newest screenshot, full width
            // and rounded like every other large surface, with the instance
            // on its darker left side.
            Item {
                id: hero
                visible: root.hasHero
                width: parent.width
                height: Math.round(Math.max(root.heroMinHeight,
                                            heroContent.implicitHeight + Theme.space.xl * 2,
                                            Math.min(root.heroMaxHeight, root.height * root.heroShare)))

                CoverArt {
                    anchors.fill: parent
                    source: root.hasHero ? root.featured.itemCoverImage : ""
                    glyph: ""
                    showLogo: false
                    radius: Theme.radius.xl
                    scrim: "horizontal"
                    matte: Theme.palette.canvas
                }

                Column {
                    id: heroContent
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space.xxl
                    anchors.right: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.lg

                    Row {
                        width: parent.width
                        spacing: Theme.space.md

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: root.heroIconSize
                            height: root.heroIconSize
                            radius: Theme.radius.md
                            color: Theme.media.chip
                            border.width: 1
                            border.color: Theme.media.chipBorder

                            Image {
                                anchors.fill: parent
                                anchors.margins: Theme.space.sm
                                source: root.featured
                                        ? "image://instanceicon/" + (root.featured.itemIconKey.length > 0 ? root.featured.itemIconKey : "default")
                                        : ""
                                sourceSize: Qt.size(width * 2, height * 2)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                                asynchronous: true
                            }
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - root.heroIconSize - parent.spacing
                            spacing: Theme.space.xxs

                            Text {
                                width: parent.width
                                text: root.featured ? root.featured.itemName : ""
                                elide: Text.ElideRight
                                color: Theme.media.text
                                font.family: Theme.font.heading
                                font.pixelSize: Theme.type.display.pixelSize
                                font.weight: Theme.type.display.weight
                            }
                            Text {
                                width: parent.width
                                text: root.featured ? Format.versionLine(root.featured.itemLoader, root.featured.itemVersion) : ""
                                elide: Text.ElideRight
                                color: Theme.media.textSecondary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.label.pixelSize
                            }
                        }
                    }

                    Row {
                        spacing: Theme.space.sm

                        PlayButton {
                            round: false
                            size: Theme.control.heightLg
                            running: !!root.featured && root.featured.itemRunning
                            enabled: !!root.featured && (root.featured.itemRunning || root.featured.itemCanLaunch)
                            onClicked: {
                                if (!root.featured)
                                    return
                                if (root.featured.itemRunning)
                                    root.stopRequested(root.featured.itemId)
                                else
                                    root.launchRequested(root.featured.itemId)
                            }
                        }
                        Button {
                            anchors.verticalCenter: parent.verticalCenter
                            buttonSize: "lg"
                            text: qsTr("Open")
                            onClicked: if (root.featured) root.openInstanceRequested(root.featured.itemId)
                        }
                        // When (clock) -- or that it crashed -- and how
                        // long (hourglass), quietly beside the buttons.
                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            leftPadding: Theme.space.sm
                            spacing: Theme.space.md

                            HeroFact {
                                iconName: root.featured && root.featured.itemCrashed ? "alert-triangle" : "clock"
                                text: !root.featured ? ""
                                    : root.featured.itemCrashed ? qsTr("Crashed last time")
                                    : Format.lastPlayed(root.featured.itemLastLaunch)
                            }
                            HeroFact {
                                visible: root.featuredPlayTime.length > 0
                                iconName: "hourglass"
                                text: root.featuredPlayTime
                            }
                        }
                    }
                }
            }

            // -- News -------------------------------------------------------
            // A compact card: no picture of its own to show, so none.
            Rectangle {
                id: newsCard
                visible: root.hasNews
                width: parent.width
                height: Theme.space.md * 2 + newsIcon.height
                radius: Theme.radius.lg
                color: Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                // Kite's own mark: the news is Kite's.
                Item {
                    id: newsIcon
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space.lg
                    anchors.verticalCenter: parent.verticalCenter
                    width: 48
                    height: 48

                    PlaceholderSurface {
                        anchors.fill: parent
                        radius: Theme.radius.md
                    }
                    Image {
                        anchors.centerIn: parent
                        width: Theme.icon.lg + Theme.space.xs
                        height: width
                        source: "image://instanceicon/kite"
                        sourceSize: Qt.size(width, height)
                        smooth: true
                        mipmap: true
                    }
                }

                Column {
                    anchors.left: newsIcon.right
                    anchors.leftMargin: Theme.space.md
                    anchors.right: newsActions.left
                    anchors.rightMargin: Theme.space.lg
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.xxs

                    Text {
                        width: parent.width
                        text: root.hasNews ? root.featuredNews.title : ""
                        elide: Text.ElideRight
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.title.pixelSize
                        font.weight: Theme.type.title.weight
                    }
                    Text {
                        width: parent.width
                        text: root.newsDate.length > 0 ? qsTr("Kite news  •  %1").arg(root.newsDate) : qsTr("Kite news")
                        elide: Text.ElideRight
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                }

                Row {
                    id: newsActions
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space.lg
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.sm

                    NewsMenuButton {
                        anchors.verticalCenter: parent.verticalCenter
                        entries: root.newsEntries
                    }
                    Button {
                        text: qsTr("Read more")
                        icon.source: Icons.url("external-link")
                        onClicked: root.openNews()
                    }
                }
            }

            // -- Jump back in ----------------------------------------------
            Column {
                visible: root.jumpCount > 0
                width: parent.width
                spacing: Theme.space.md

                SectionHeading {
                    text: qsTr("Jump back in")
                }

                Grid {
                    columns: root.jumpColumns
                    columnSpacing: root.rowGap
                    rowSpacing: root.rowGap

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
                            id: slot
                            // The most recently played first (recentModel
                            // already sorts that way), after the one the
                            // header shows.
                            readonly property int position: model.index - root.jumpOffset
                            visible: slot.position >= 0 && slot.position < root.jumpCount
                            width: visible ? row.width : 0
                            height: visible ? row.height : 0

                            // The first row is the header's instance: what it
                            // shows, live from the row.
                            readonly property bool isFirst: model.index === 0
                            readonly property string itemId: model.instanceId
                            readonly property string itemName: model.name
                            readonly property string itemIconKey: model.iconKey
                            readonly property string itemVersion: model.gameVersion
                            readonly property string itemLoader: model.loader
                            readonly property var itemLastLaunch: model.lastLaunch
                            readonly property var itemPlayTime: model.totalTimePlayed
                            readonly property bool itemCrashed: model.hasCrashed
                            readonly property bool itemRunning: model.isRunning
                            readonly property bool itemCanLaunch: model.canLaunch
                            readonly property string itemCoverImage: model.coverImage
                            function claimHero() { if (slot.isFirst) root.featured = slot }
                            onIsFirstChanged: claimHero()
                            Component.onCompleted: claimHero()
                            Component.onDestruction: if (root.featured === slot) root.featured = null

                            HomeJumpCard {
                                id: row
                                width: root.jumpRowWidth
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
            }

            // The library has instances but none has ever been played:
            // never dress one of them up as "continue playing".
            Rectangle {
                visible: root.recentCount === 0
                width: parent.width
                height: Theme.space.md * 2 + Theme.control.height
                radius: Theme.radius.lg
                color: Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                MeshIcon {
                    id: firstPlayIcon
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space.lg
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "compass"
                    size: Theme.icon.md
                    color: Theme.palette.textTertiary
                }
                Text {
                    anchors.left: firstPlayIcon.right
                    anchors.leftMargin: Theme.space.sm
                    anchors.right: discoverButton.left
                    anchors.rightMargin: Theme.space.md
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Nothing played yet. Pick an instance in your library, or find a modpack in Discover.")
                    elide: Text.ElideRight
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                }
                Button {
                    id: discoverButton
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space.md
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Discover")
                    icon.source: Icons.url("compass")
                    onClicked: root.discoverRequested()
                }
            }

            // -- Recent worlds ---------------------------------------------
            // Hidden entirely when there is nothing to show -- no empty
            // section, no placeholder; the instance page has every world.
            Column {
                visible: root.hasWorlds
                width: parent.width
                spacing: Theme.space.md

                SectionHeading {
                    text: qsTr("Recent worlds")
                }

                Grid {
                    columns: root.worldColumns
                    columnSpacing: root.rowGap
                    rowSpacing: root.rowGap

                    Repeater {
                        id: worldsRepeater
                        model: root.recentWorldsModel
                        // Wrapped for the same reason as the jump rows:
                        // HomeWorldTile's required properties would hide
                        // `index` from a direct delegate.
                        delegate: Item {
                            visible: model.index < root.worldMax
                            width: visible ? tile.width : 0
                            height: visible ? tile.height : 0

                            HomeWorldTile {
                                id: tile
                                width: root.worldWidth
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
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.hasLibrary
        flickable: flick
    }

    // Nothing in the library yet: the way in, in the middle of the page.
    EmptyState {
        anchors.centerIn: parent
        // A readable measure for the body text; EmptyState caps it.
        width: Math.min(root.width - root.pagePadding * 2, root.emptyStateWidth)
        visible: !root.hasLibrary
        title: qsTr("Your library is empty")
        body: qsTr("An instance is one Minecraft setup: a version, a mod loader and its mods. Create one to start playing, or find a modpack in Discover.")
        actionText: qsTr("Create your first instance")
        actionIcon: "plus"
        onActionTriggered: root.createRequested()

        MeshIcon {
            iconName: "cube"
            size: 40
            color: Theme.palette.textTertiary
        }
    }
}
