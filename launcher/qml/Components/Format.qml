// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton

import QtQuick

/*
 * Turns the raw numbers InstanceList hands out into short, human text.
 * Units are written out compactly ("3 h ago", "12 h 30 min") so no string
 * here needs a plural form that an untranslated build would print as
 * "hour(s)".
 */
QtObject {
    id: root

    // lastLaunch: milliseconds since the epoch, 0 when never launched.
    function lastPlayed(lastLaunchMs) {
        var ms = Number(lastLaunchMs)
        if (!(ms > 0))
            return qsTr("Never played")
        var minutes = Math.floor((Date.now() - ms) / 60000)
        if (minutes < 1)
            return qsTr("Just now")
        if (minutes < 60)
            return qsTr("%1 min ago").arg(minutes)
        var hours = Math.floor(minutes / 60)
        if (hours < 24)
            return qsTr("%1 h ago").arg(hours)
        var days = Math.floor(hours / 24)
        if (days === 1)
            return qsTr("Yesterday")
        if (days < 30)
            return qsTr("%1 days ago").arg(days)
        return root.calendarDate(new Date(ms))
    }

    // Older than a month: the month spelled out ("Sep 15"), plus the year
    // once it is not this year ("Sep 15, 2025") -- never the locale's
    // numeric short form, which reads "9/15/26" in one place and "15.09.26"
    // in another, next to "22 days ago" on the same screen.
    function calendarDate(date) {
        var sameYear = date.getFullYear() === new Date().getFullYear()
        return date.toLocaleDateString(Qt.locale(), sameYear ? "MMM d" : "MMM d, yyyy")
    }

    // totalTimePlayed: seconds.
    function playTime(seconds) {
        var s = Number(seconds)
        if (!(s >= 60))
            return ""
        var minutes = Math.floor(s / 60) % 60
        var hours = Math.floor(s / 3600)
        if (hours === 0)
            return qsTr("%1 min").arg(minutes)
        if (minutes === 0)
            return qsTr("%1 h").arg(hours)
        return qsTr("%1 h %2 min").arg(hours).arg(minutes)
    }

    // "Fabric 1.21.4", or "1.21.4" for vanilla (loader is empty then).
    function versionLine(loader, gameVersion) {
        var parts = []
        if (loader && loader.length > 0)
            parts.push(loader)
        if (gameVersion && gameVersion.length > 0)
            parts.push(gameVersion)
        else if (parts.length === 0)
            parts.push(qsTr("Minecraft"))
        return parts.join(" ")
    }

    /*
     * A backdrop colour cut from an instance icon's average colour: same
     * hue, saturation kept in check so neon icons don't glare, lightness
     * forced to `lightness` so text on top stays readable whatever the
     * icon looked like. Achromatic icons (hslHue < 0) come out grey.
     */
    function shade(tint, lightness, saturationScale) {
        var hue = tint.hslHue < 0 ? 0 : tint.hslHue
        // Capped low: a fully saturated plate at low lightness turns into
        // the murky teal/olive the launcher used to be full of.
        var saturation = Math.min(0.5, tint.hslSaturation * saturationScale)
        return Qt.hsla(hue, saturation, lightness, 1)
    }

    // 1234 -> "1.2K", 3456789 -> "3.5M": download counts in a card.
    function compactNumber(value) {
        var n = Number(value)
        if (!(n >= 0))
            return ""
        if (n < 1000)
            return String(n)
        var units = ["K", "M", "B"]
        var unit = -1
        while (n >= 1000 && unit < units.length - 1) {
            n /= 1000
            unit++
        }
        return Number(n).toLocaleString(Qt.locale(), "f", n < 10 ? 1 : 0) + units[unit]
    }

    /*
     * A stable colour derived from an id/name string, for a designed
     * fallback that still tells two accounts (or anything else keyed by an
     * id rather than an icon) apart -- the same purpose an instance's own
     * randomly-assigned iconTint role serves, for something that has no
     * icon of its own to carry one. Feed the result through shade() above
     * for the actual fill/tint, same as any other tint this file hands out.
     */
    function hashTint(seed) {
        var s = String(seed || "")
        var hash = 0
        for (var i = 0; i < s.length; ++i)
            hash = (hash * 31 + s.charCodeAt(i)) >>> 0
        return Qt.hsla((hash % 360) / 360, 0.55, 0.55, 1)
    }

    // A local path as a url QML can load: "/a/b" and "C:/a/b" alike.
    function fileUrl(path) {
        if (!path || path.length === 0)
            return ""
        var p = String(path).replace(/\\/g, "/")
        return "file://" + (p.charAt(0) === "/" ? "" : "/") + p
    }

    // The reverse of fileUrl() above: a "file://" url (from a FolderDialog/
    // FileDialog's selectedFolder/selectedFile) as a plain local path --
    // "file:///a/b" -> "/a/b", "file:///C:/a/b" -> "C:/a/b".
    function localPath(url) {
        var s = String(url)
        if (s.indexOf("file://") !== 0)
            return s
        s = s.substring(7)
        if (/^\/[A-Za-z]:/.test(s))
            s = s.substring(1)
        return decodeURIComponent(s)
    }

    // The friends list in the three groups the Friends page and Home's panel
    // show -- playing, online, offline -- the order the server gave (by
    // name) holding inside each.
    function friendsGrouped(friends) {
        var groups = { playing: [], online: [], offline: [] }
        for (var i = 0; i < (friends || []).length; ++i) {
            var state = friends[i].state
            groups[state === "playing" || state === "online" ? state : "offline"].push(friends[i])
        }
        return groups
    }

    // The second line of a friend's row: { text, suffix, tone }. On a
    // server it is the host and the game version after it; singleplayer and
    // a hidden server say so, with the version too; online and offline are
    // the line FriendsDisplay worded ("In Kite", "Last seen 3 h ago"), the
    // offline one "faint". Every line but the offline one is "secondary",
    // as the other lists' second lines are: the name is the row's loudest
    // text, and Join marks what can be acted on.
    function friendStatus(friend) {
        if (friend.state === "playing") {
            if (friend.worldType === "server" && friend.host)
                return { text: friend.host, suffix: friend.version, tone: "secondary" }
            return {
                text: friend.worldType === "singleplayer" ? qsTr("Singleplayer")
                    : friend.worldType === "server" ? qsTr("On a server") : qsTr("Playing"),
                suffix: friend.version,
                tone: "secondary"
            }
        }
        return { text: friend.statusText, suffix: "", tone: friend.state === "offline" ? "faint" : "secondary" }
    }

    // The same second line on one line, for a tip or a card: "mc.example.net
    // · 1.21.4", "In Kite", "Last seen 3 h ago".
    function friendStatusLine(friend) {
        var status = root.friendStatus(friend)
        return status.suffix ? status.text + " · " + status.suffix : status.text
    }

    // Whether Join shows: playing on a server whose host is visible, and the
    // friend lets others join (FriendsDisplay already ands the three). The
    // game version is what picks the instance, so it has to be known too.
    function friendJoinable(friend) {
        return friend.state === "playing" && friend.worldType === "server"
                && !!friend.host && !!friend.version && friend.joinable === true
    }

    // Whether there is no Join for a visible server because its player
    // turned joining off (a row says so, instead of looking like a bug).
    function friendJoinOff(friend) {
        return friend.state === "playing" && friend.worldType === "server"
                && !!friend.host && friend.joinOff === true
    }

    // Why friends ask for a sign-in: said by the Friends page and by Home's
    // panel alike.
    readonly property string friendsNeedAccount: qsTr("Friends needs a Minecraft account that is signed in with Microsoft.")

    // Where a friends view is: "off" (no controller, or turned off),
    // "noAccount", "signingIn", "error" or "ready". The controller's own
    // "off" while it is enabled is only the moment before it connects.
    function friendsPhase(controller) {
        if (!controller || !controller.enabled)
            return "off"
        return controller.state === "off" ? "signingIn" : controller.state
    }

    // Signed out, or failed in a way only signing in to the Minecraft
    // account again fixes: the view offers "Sign in", not "Try again".
    function friendsSignInFirst(controller) {
        var phase = root.friendsPhase(controller)
        return phase === "noAccount" || (phase === "error" && controller.needsSignIn)
    }

    // A failed poll while the lists are already there: they stay on screen
    // (a bar says the server is not answering) instead of giving way to the
    // full-page error.
    function friendsStale(controller) {
        return root.friendsPhase(controller) === "error" && !root.friendsSignInFirst(controller)
                && controller.loaded
    }

    // Whether the friends lists have anyone in them: friends and the
    // requests either way, and with @p withBlocked the people you blocked.
    // (Not before the lists are read, which is "no one" only by default.)
    function friendsHasAnyone(controller, withBlocked) {
        return !!controller && controller.friends.length + controller.incoming.length
                + controller.outgoing.length + (withBlocked ? controller.blocked.length : 0) > 0
    }

    // When the friends lists were last read, as a time of day in the
    // user's format ("14:32"), or "" if they have not been.
    function friendsUpdatedAt(controller) {
        var at = controller ? controller.updatedAtMs : 0
        // (Locale.ShortFormat given to Qt.formatTime() reads as Qt.ISODate:
        // "17:30:08" whatever the language.)
        return at > 0 ? new Date(at).toLocaleTimeString(Qt.locale(), Locale.ShortFormat) : ""
    }

    // Where Join sends the game: the host, and the port unless it is the
    // game's own. An IPv6 host goes in brackets, as parse() of a server
    // address reads it (it takes them off again for the game).
    readonly property int defaultServerPort: 25565
    function friendAddress(friend) {
        var host = friend.host.indexOf(":") >= 0 ? "[" + friend.host + "]" : friend.host
        return friend.port > 0 && friend.port !== root.defaultServerPort
                ? host + ":" + friend.port : host
    }
}
