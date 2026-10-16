// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

pragma Singleton

import QtQuick

/*
 * Lookup for the launcher's own artwork under art/, the same shape as
 * Icons.qml's url() for SVGs. The ambient wash PNG is procedurally generated
 * by the checked-in art/generate.py; the rendered artwork (scenes, world
 * thumbnails, pack covers, empty states) is made offline and only cropped and
 * compressed by art/scenes/build.py. Nothing here is generated at runtime.
 *
 * landscapeUrl() is the one callers reach for most: CoverArt's fallback art
 * -- the cover of an instance with no screenshot yet, on Library cards,
 * Home's jump cards, the instance page and PlayDock's bar -- picks one of 32
 * rendered voxel-world scenes (art/scenes/, see build.py there) by hashing
 * the instance/world id, so a given item always draws the same scene rather
 * than a new one on every repaint (design-plan.md §6 "fallback covers":
 * bounded and designed, not random-looking). The 32 differ in biome and in
 * time of day or weather, so a grid of them never reads as one picture
 * repeated. Each scene exists at three crops -- "card", "band" and "strip"
 * -- because a 16:9 frame filled into a 12:1 dock bar only ever shows a
 * sliver of it; build.py places each crop per scene so it keeps the horizon
 * and the subject, and shapeForAspect() picks the one that fits its host.
 * They are never mirrored: these are lit scenes with buildings in them, and
 * a flipped copy next to its original reads as an obvious trick.
 *
 * worldUrlFor() serves the small square spots (a world's thumbnail) where a
 * whole scene would only be a smear: eight biome renders composed for a
 * centred square (plains, desert, snow, jungle, ocean, cave, nether, end),
 * picked by hashing the world's folder name. packCoverUrl() is the same idea
 * for a Discover modpack with no gallery image: three generic 16:9 covers.
 */
QtObject {
    readonly property int sceneCount: 32
    readonly property int packCoverCount: 3
    readonly property var worldBiomes: [
        "plains", "desert", "snow", "jungle", "ocean", "cave", "nether", "end"
    ]
    readonly property string _root: "qrc:/qt/qml/MeshMC/Components/art/"

    // A plain, fast string hash (djb2-ish) -- only needs to be stable across
    // calls in this process, never to match anything outside QML.
    function _hash(text) {
        var h = 5381
        for (var i = 0; i < text.length; ++i)
            h = ((h * 33) ^ text.charCodeAt(i)) >>> 0
        return h
    }

    function _key(seed) {
        return seed && seed.length > 0 ? seed : "mesh"
    }

    // "card" (about 16:9), "band" (about 5:1) or "strip" (about 12:1) --
    // whichever of the three scene crops a host of this width/height ratio
    // would crop the least.
    function shapeForAspect(aspect) {
        return aspect > 8 ? "strip" : aspect > 3 ? "band" : "card"
    }

    // @p seed is normally an instance or world id; "" still returns a valid,
    // stable pick rather than an empty source. @p shape is one of
    // shapeForAspect()'s three names (default "card").
    function landscapeUrl(seed, shape) {
        var idx = _hash(_key(seed)) % sceneCount
        return _root + "scenes/" + (shape || "card") + "/" + (idx < 10 ? "0" : "") + idx + ".jpg"
    }

    // A biome thumbnail (art/worlds/<biome>.jpg, 256x256, subject centred)
    // picked by hashing @p seed -- normally the world's folder name -- so a
    // world with no icon of its own always shows the same one.
    function worldUrlFor(seed) {
        return _root + "worlds/" + worldBiomes[_hash(_key(seed)) % worldBiomes.length] + ".jpg"
    }

    // A generic modpack cover (art/packs/NN.jpg, 1280x720) for a Discover
    // entry with no gallery; @p seed is normally the project id or slug.
    function packCoverUrl(seed) {
        return _root + "packs/0" + (_hash(_key(seed)) % packCoverCount) + ".jpg"
    }

    // AmbientPattern's own tileable wash: a dirt/deepslate checkerboard
    // pre-converted to a luminance alpha mask (see generate.py's
    // ambient_tile()), so IconImage's alpha-channel recolouring -- the same
    // mechanism MeshIcon.qml already uses for every SVG icon -- can tint it
    // to any palette colour at draw time.
    function ambientMaskUrl() {
        return _root + "ambient/blocks_mask.png"
    }

    function heroUrl() {
        return _root + "hero/home_hero.png"
    }

    // Home's header artwork: one rendered landscape per time of day
    // (art/hero/home_<mood>.jpg, 1920x640), cropped to fill the header.
    function homeHeroUrl(mood) {
        return _root + "hero/home_" + (mood || "day") + ".jpg"
    }

    // The scene for a local hour: daylight, the golden hours either side of
    // it, and night.
    function homeMood(hour) {
        if (hour >= 7 && hour < 17)
            return "day"
        if ((hour >= 17 && hour < 20) || (hour >= 5 && hour < 7))
            return "golden"
        return "night"
    }

    // @p kind is one of art/empty/*.jpg without its extension (480x360):
    // "no_instances", "no_worlds" or "not_found".
    function emptyUrl(kind) {
        return _root + "empty/" + kind + ".jpg"
    }
}
