// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The Home page: one purpose, "get back to what I was doing". The default
 * start page and the sidebar's first item. A welcome card over MeshMC's own
 * rendered artwork for the time of day, the most recently played instances
 * ("Jump back in"), the worlds played most recently across all of them
 * ("Recent worlds"), and the library at a glance -- all from models the
 * shell already keeps locally (recentModel/recentWorlds), so opening this
 * page makes no network call.
 *
 * Like every other page it sits under TopBar (title + account button), and
 * its content stops growing at Theme.layout.maxContentWidth and centres, so
 * a wide window does not leave everything hugging the left edge.
 */
Item {
    id: root

    /*
     * The identical look every section heading on this page shares ("Jump
     * back in", "Recent worlds", "Your library") -- extracted once the
     * third copy made it a repeat rather than a one-off (project DRY rule).
     */
    component SectionHeading: Text {
        // One height for every heading, the same as a header's trailing
        // LinkButton, so every heading-to-content gap on the page matches.
        height: Theme.control.heightSm
        verticalAlignment: Text.AlignVCenter
        color: Theme.palette.textPrimary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.title.pixelSize + 1
        font.weight: Font.Bold
    }

    /*
     * One entry of "Your library": the instance's icon, its name and its
     * version line, the whole row clickable -- a labelled list rather than
     * a strip of bare icons, which read as a debug view.
     */
    component LibraryEntry: AbstractButton {
        id: entry

        required property string instanceId
        required property string name
        required property string iconKey
        required property bool isRunning
        required property string gameVersion
        required property string loader

        implicitHeight: Theme.control.heightLg + Theme.space.sm
        hoverEnabled: true

        Accessible.name: entry.name

        background: Rectangle {
            radius: Theme.radius.sm
            color: entry.down ? Theme.palette.pressedOverlay
                 : entry.hovered ? Theme.palette.hoverOverlay : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }

        contentItem: Row {
            spacing: Theme.space.md
            leftPadding: Theme.space.xs

            HomeLibraryTile {
                anchors.verticalCenter: parent.verticalCenter
                instanceId: entry.instanceId
                name: entry.name
                iconKey: entry.iconKey
                isRunning: entry.isRunning
                // The row is the button; the tile is only the picture.
                enabled: false
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: entry.width - Theme.control.heightLg - Theme.space.md * 2
                spacing: Theme.space.xxs

                Text {
                    width: parent.width
                    text: entry.name
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.weight: Theme.type.label.weight
                }
                Text {
                    width: parent.width
                    text: Format.versionLine(entry.loader, entry.gameVersion)
                    elide: Text.ElideRight
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                }
            }
        }
    }

    // Most recently played instances first; never-played ones left out
    // (QmlShell.recentModel) -- "Jump back in" takes its first three.
    property var recentModel: null
    // Every instance, for "Your library" and the empty-library check.
    property var instanceModel: null
    // QmlShell.recentWorlds -- most recently played worlds across every
    // instance; see RecentWorldsModel's own class comment.
    property var recentWorldsModel: null
    // The signed-in account's name, for the welcome line.
    property string accountName: ""

    // "day", "golden" or "night" to pin the header scene (review snapshots);
    // empty follows the local clock.
    property string moodOverride: ""

    signal launchRequested(string id)
    signal stopRequested(string id)
    // Opens the instance page on its Overview tab.
    signal openInstanceRequested(string id)
    // Opens the instance page on its Worlds tab -- a Recent worlds tile.
    signal openInstanceWorldsRequested(string id)
    signal createRequested()
    signal discoverRequested()
    signal libraryRequested()

    readonly property int pagePadding: Theme.space.xl + Theme.space.xs
    readonly property int libraryCount: root.instanceModel && root.instanceModel.count !== undefined ? root.instanceModel.count : 0
    readonly property int recentCount: root.recentModel && root.recentModel.count !== undefined ? root.recentModel.count : 0
    readonly property bool hasLibrary: root.libraryCount > 0

    // The content column: the page minus its padding, never wider than
    // Theme.layout.maxContentWidth, centred.
    readonly property int contentWidth: Math.min(root.width - root.pagePadding * 2, Theme.layout.maxContentWidth)
    readonly property int contentX: Math.round((root.width - root.contentWidth) / 2)

    // The welcome card: a share of the window's height, within bounds, so a
    // short window keeps "Jump back in" above the fold.
    readonly property real heroShare: 0.26
    readonly property int heroMinHeight: 168
    readonly property int heroMaxHeight: 240
    readonly property int heroHeight: Math.round(Math.max(root.heroMinHeight,
                                                          Math.min(root.heroMaxHeight, root.height * root.heroShare)))

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

    // "Recent worlds" and "Your library" share one band: side by side on a
    // wide enough page (worlds as a 2x2 grid on the left, the library as a
    // short list on the right), stacked below that -- so the default window
    // shows the whole page instead of a library heading cut by the play bar.
    readonly property bool hasWorlds: worldsRepeater.count > 0
    readonly property int splitMinWidth: 880
    readonly property bool split: root.hasWorlds && root.contentWidth >= root.splitMinWidth
    readonly property real libraryShare: 0.36
    readonly property int bandGap: Theme.space.xl
    readonly property int libraryWidth: root.split ? Math.round(root.contentWidth * root.libraryShare) : root.contentWidth
    readonly property int worldsWidth: root.split ? root.contentWidth - root.libraryWidth - root.bandGap : root.contentWidth

    // When split, the band shows as many rows as the page has room for
    // below "Jump back in", so it ends above the play bar instead of being
    // cut by it: worlds two to a row, the library one entry per row.
    readonly property int bandHeaderHeight: Theme.control.heightSm + Theme.space.md
    readonly property int bandAvailable: root.height - content.y - band.y - Theme.space.xxl
    // HomeWorldTile's and LibraryEntry's own heights.
    readonly property int worldTileHeight: 88
    readonly property int libraryEntryHeight: Theme.control.heightLg + Theme.space.sm
    readonly property int libraryRowSpacing: Theme.space.xxs
    // HomeLibraryTile's own size, centred in each entry.
    readonly property int libraryTileSize: 40

    // Worlds: two columns when split, otherwise one row of as many whole
    // tiles as fit.
    readonly property int worldMinWidth: 200
    readonly property int worldSpacing: Theme.space.lg
    readonly property int worldColumns: root.split ? 2
            : Math.max(1, Math.floor((root.worldsWidth + root.worldSpacing) / (root.worldMinWidth + root.worldSpacing)))
    readonly property int worldRows: Math.max(1, Math.min(3, Math.floor((root.bandAvailable - root.bandHeaderHeight + root.worldSpacing)
                                                                        / (root.worldTileHeight + root.worldSpacing))))
    readonly property int worldMax: root.split ? root.worldRows * root.worldColumns : root.worldColumns
    readonly property int worldTileWidth: Math.floor((root.worldsWidth - root.worldSpacing * (root.worldColumns - 1)) / root.worldColumns)

    // Library: a few labelled entries, the rest one click away behind "See
    // all" -- one column when split, as many as fit when stacked.
    readonly property int libraryColumnWidth: 240
    readonly property int libraryColumns: root.split ? 1
            : Math.max(1, Math.floor((root.libraryWidth + Theme.space.sm) / (root.libraryColumnWidth + Theme.space.sm)))
    readonly property int libraryRows: Math.max(2, Math.min(8, Math.floor((root.bandAvailable - root.bandHeaderHeight + root.libraryRowSpacing)
                                                                          / (root.libraryEntryHeight + root.libraryRowSpacing))))
    readonly property int libraryMax: root.split ? root.libraryRows : root.libraryColumns * 2
    readonly property int libraryEntryWidth: Math.floor((root.libraryWidth + Theme.space.xs * 2 - Theme.space.sm * (root.libraryColumns - 1)) / root.libraryColumns)

    // The most recently played instance's name, for the welcome line.
    property string lastName: ""

    // Re-read once a minute so the sky turns with the day while the
    // launcher stays open; the timer only runs while Home is on screen.
    property int hour: new Date().getHours()
    readonly property string mood: root.moodOverride.length > 0 ? root.moodOverride
                                                                : PixelArt.homeMood(root.hour)
    Timer {
        interval: 60 * 1000
        repeat: true
        running: root.visible && root.moodOverride.length === 0
        onTriggered: root.hour = new Date().getHours()
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

            // -- Welcome card -------------------------------------------------
            // The time of day's rendered scene, rounded like every other
            // large surface, with the greeting on its darker left side.
            Item {
                width: parent.width
                height: root.heroHeight

                CoverArt {
                    anchors.fill: parent
                    source: PixelArt.homeHeroUrl(root.mood)
                    radius: Theme.radius.xl
                    scrim: "horizontal"
                    matte: Theme.palette.canvas
                }

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space.xxl
                    anchors.right: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.sm

                    Text {
                        width: parent.width
                        text: root.accountName.length > 0 ? qsTr("Welcome back, %1").arg(root.accountName)
                                                           : qsTr("Welcome back")
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: Theme.media.text
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.banner.pixelSize
                        font.weight: Theme.type.banner.weight
                        lineHeight: Theme.type.banner.lineHeight
                    }
                    Text {
                        width: parent.width
                        visible: text.length > 0
                        text: root.lastName.length > 0 ? qsTr("Pick up where you left off in %1.").arg(root.lastName)
                            : root.hasLibrary ? qsTr("Pick an instance and press Play.")
                            : qsTr("Create an instance to start playing.")
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: Theme.media.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.title.pixelSize
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

                            // The first card is the instance the welcome
                            // line names, so the two can never disagree.
                            readonly property bool isFirst: model.index === 0
                            readonly property string cardName: model.name
                            function claimWelcome() { if (isFirst) root.lastName = cardName }
                            onIsFirstChanged: claimWelcome()
                            onCardNameChanged: claimWelcome()
                            Component.onCompleted: claimWelcome()

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

            // -- Recent worlds + Your library --------------------------------
            Flow {
                id: band
                visible: root.hasLibrary
                width: parent.width
                spacing: root.bandGap

                // Hidden entirely when there is nothing to show -- no empty
                // section, no placeholder; the instance page has every world.
                Column {
                    visible: root.hasWorlds
                    width: root.worldsWidth
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
                                    // Last column takes the rounding remainder.
                                    width: model.index % root.worldColumns === root.worldColumns - 1
                                           ? root.worldsWidth - (root.worldColumns - 1) * (root.worldTileWidth + root.worldSpacing)
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

                Column {
                    width: root.libraryWidth
                    spacing: Theme.space.md

                    Item {
                        width: parent.width
                        height: Math.max(libraryHeading.implicitHeight, seeAllButton.implicitHeight)

                        SectionHeading {
                            id: libraryHeading
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Your library")
                        }
                        LinkButton {
                            id: seeAllButton
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.libraryCount > root.libraryMax ? qsTr("See all %1").arg(root.libraryCount)
                                                                      : qsTr("Open library")
                            onClicked: root.libraryRequested()
                        }
                    }

                    // Pulled out by the entries' own inner padding, so their
                    // icons line up with the heading above, while the hover
                    // background still has room around them.
                    Grid {
                        x: -Theme.space.xs
                        // And up by the entry's own vertical inset, so the
                        // first icon's top lines up with the world tiles'.
                        y: -(root.libraryEntryHeight - root.libraryTileSize) / 2
                        width: parent.width + Theme.space.xs * 2
                        columns: root.libraryColumns
                        columnSpacing: Theme.space.sm
                        rowSpacing: root.libraryRowSpacing

                        Repeater {
                            model: root.instanceModel
                            delegate: Item {
                                visible: model.index < root.libraryMax
                                width: visible ? entryButton.width : 0
                                height: visible ? entryButton.height : 0

                                LibraryEntry {
                                    id: entryButton
                                    width: root.libraryEntryWidth
                                    instanceId: model.instanceId
                                    name: model.name
                                    iconKey: model.iconKey
                                    isRunning: model.isRunning
                                    gameVersion: model.gameVersion
                                    loader: model.loader
                                    onClicked: root.openInstanceRequested(model.instanceId)
                                }
                            }
                        }
                    }
                }
            }

            // Nothing in the library yet: the welcome card stays, and the
            // way in sits right under it.
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
