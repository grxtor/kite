// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The Home page: one purpose, "get back to what I was doing". The default
 * start page and the sidebar's first item. A welcome line, the most
 * recently played instances ("Jump back in"), the worlds played most
 * recently across all of them ("Recent worlds"), and a slim strip of the
 * whole library -- all from models the shell already keeps locally
 * (recentModel/recentWorlds), so opening this page makes no network call.
 *
 * The page's own header (title + profile button) is TopBar, in Main.qml,
 * the same as every other page -- this file only owns the content below it.
 */
Item {
    id: root

    /*
     * The identical look every section heading on this page shares ("Jump
     * back in", "Recent worlds", "Your library") -- extracted once the
     * third copy made it a repeat rather than a one-off (project DRY rule).
     */
    component SectionHeading: Text {
        color: Theme.palette.textPrimary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.title.pixelSize + 1
        font.weight: Font.Bold
    }

    // Most recently played instances first; never-played ones left out
    // (QmlShell.recentModel) -- "Jump back in" takes its first three.
    property var recentModel: null
    // Every instance, for "Your library" and the empty-library check.
    property var instanceModel: null
    // QmlShell.recentWorlds -- most recently played worlds across every
    // instance; see RecentWorldsModel's own class comment.
    property var recentWorldsModel: null
    property string accountName: ""

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
            x: root.pagePadding
            y: Theme.space.md
            width: flick.width - root.pagePadding * 2
            spacing: Theme.space.xl + Theme.space.sm

            Text {
                width: parent.width
                text: root.accountName.length > 0 ? qsTr("Welcome back, %1").arg(root.accountName)
                                                   : qsTr("Welcome back")
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.heading.pixelSize
                font.weight: Theme.type.heading.weight
            }

            // -- Jump back in ----------------------------------------------
            Column {
                width: parent.width
                spacing: Theme.space.md

                SectionHeading {
                    visible: root.recentCount > 0
                    text: qsTr("Jump back in")
                }

                // A horizontal strip rather than a grid: at narrow widths it
                // scrolls sideways instead of wrapping the cards onto a
                // second row.
                Flickable {
                    visible: root.recentCount > 0
                    // Bled by the page's own right padding rather than
                    // stopping flush at the content column's edge: with
                    // width: parent.width alone, whenever the 3rd card
                    // almost-but-not-quite fits it clips with a hard edge
                    // and zero visible hint that it exists -- the
                    // horizontal ScrollBar stays invisible until hovered,
                    // and this is the only sideways-scrolling row in the
                    // app, so there is no other affordance to notice by.
                    // root.pagePadding of page gutter is otherwise idle
                    // whitespace here, so reclaiming it costs nothing and
                    // guarantees a real peek of the next card (see review
                    // finding on this row).
                    width: parent.width + root.pagePadding
                    height: jumpRow.height
                    contentWidth: jumpRow.width
                    contentHeight: height
                    flickableDirection: Flickable.HorizontalFlick
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    ScrollBar.horizontal: ScrollBar {}

                    Row {
                        id: jumpRow
                        spacing: Theme.space.lg

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
                                id: jumpDelegate
                                // Only the three most recently played, most
                                // recent first -- recentModel already sorts
                                // that way.
                                visible: model.index < 3
                                width: visible ? card.width : 0
                                height: visible ? card.height : 0

                                HomeJumpCard {
                                    id: card
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

            // -- Recent worlds -----------------------------------------------
            // Hidden entirely when there is nothing to show -- no empty
            // section, no placeholder.
            Column {
                width: parent.width
                spacing: Theme.space.md
                visible: worldsRepeater.count > 0

                SectionHeading {
                    text: qsTr("Recent worlds")
                }

                Flickable {
                    width: parent.width
                    height: worldsRow.height
                    contentWidth: worldsRow.width
                    contentHeight: height
                    flickableDirection: Flickable.HorizontalFlick
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    ScrollBar.horizontal: ScrollBar {}

                    Row {
                        id: worldsRow
                        spacing: Theme.space.sm

                        Repeater {
                            id: worldsRepeater
                            model: root.recentWorldsModel
                            delegate: HomeWorldTile {
                                onClicked: root.openInstanceWorldsRequested(instanceId)
                            }
                        }
                    }
                }
            }

            // -- Your library -------------------------------------------------
            Column {
                width: parent.width
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
                    Button {
                        id: seeAllButton
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        flat: true
                        text: qsTr("See all")
                        onClicked: root.libraryRequested()
                    }
                }

                Flickable {
                    width: parent.width
                    height: libraryRow.height
                    contentWidth: libraryRow.width
                    contentHeight: height
                    flickableDirection: Flickable.HorizontalFlick
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    ScrollBar.horizontal: ScrollBar {}

                    Row {
                        id: libraryRow
                        spacing: Theme.space.xs

                        Repeater {
                            model: root.instanceModel
                            delegate: RecentItem {
                                // RecentItem's own implicitWidth (200) is
                                // sized for its expanded row form and does
                                // not shrink for `compact` -- only its
                                // internal square does. Matching that square
                                // here keeps the strip tight instead of
                                // spacing icons 200px apart.
                                width: 40
                                compact: true
                                onClicked: root.openInstanceRequested(instanceId)
                            }
                        }
                    }
                }
            }

            // Calm, empty breathing room -- reserved for a future roaming
            // 3D cat. Nothing is drawn here on purpose.
            Item {
                width: 1
                height: 160
            }
        }
    }

    EmptyState {
        anchors.centerIn: parent
        visible: !root.hasLibrary
        title: qsTr("Your library is empty")
        body: qsTr("An instance is one Minecraft setup: a version, a mod loader and its mods. Create one to start playing.")
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
