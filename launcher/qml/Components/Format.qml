// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton

import QtQuick

/*
 * Turns InstanceList's raw numbers into short text. Units are written compactly ("3 h ago") so no
 * string needs a plural form that an untranslated build would print as "hour(s)".
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

    // Always an age, never lastPlayed()'s calendar date after a month ("today", "5 days ago", "2 months ago", "1 year
    // ago"); takes ms since the epoch, a Date or an ISO date string, and gives "" when it is none of those.
    function ageText(dateOrMs) {
        var ms = typeof dateOrMs === "string" ? Date.parse(dateOrMs) : Number(dateOrMs)
        if (!(ms > 0))
            return ""
        var days = Math.floor((Date.now() - ms) / 86400000)
        if (days < 1)
            return qsTr("today")
        if (days === 1)
            return qsTr("yesterday")
        if (days < 30)
            return qsTr("%1 days ago").arg(days)
        if (days < 365) {
            var months = Math.max(1, Math.floor(days / 30))
            return months === 1 ? qsTr("1 month ago") : qsTr("%1 months ago").arg(months)
        }
        var years = Math.floor(days / 365)
        return years === 1 ? qsTr("1 year ago") : qsTr("%1 years ago").arg(years)
    }

    // "fabric" -> "Fabric": the first letter upper-cased, the rest as it is.
    function titleCase(word) {
        return word.length > 0 ? word.charAt(0).toUpperCase() + word.slice(1) : word
    }

    // Older than a month: "Sep 15", plus the year when it is not this year. Never the locale's numeric form, which
    // differs per locale.
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
    // The warning line after a crash: the crash doctor's first cause when it found one
    // (InstanceList's crashCause), else the plain words.
    function crashLine(cause) {
        return cause && cause.length > 0 ? qsTr("Crashed: %1").arg(cause) : qsTr("Crashed last time")
    }

    // The crash doctor's "couldn't tell" as a cause (CrashCauseView), with the exit code when known.
    function unknownCrashCause(doctor) {
        return {
            title: qsTr("Kite couldn't find the cause"),
            explanation: qsTr("This crash doesn't look like one Kite knows. Share the log to show it to someone who can help; your account details and your user folder are hidden."),
            // A raw number is a detail, not part of the sentence.
            detail: doctor && doctor.hasExitCode ? qsTr("Exit code %1").arg(doctor.exitCode) : "",
            line: "", count: 1, rows: [], fixes: []
        }
    }

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
     * A backdrop colour from an icon's average colour: same hue, saturation capped, lightness forced to
     * `lightness` so text on top stays readable. Achromatic icons (hslHue < 0) come out grey.
     */
    function shade(tint, lightness, saturationScale) {
        var hue = tint.hslHue < 0 ? 0 : tint.hslHue
        // Capped low: a fully saturated plate at low lightness turns murky.
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

    // A local path as a url QML can load: "/a/b" and "C:/a/b" alike.
    function fileUrl(path) {
        if (!path || path.length === 0)
            return ""
        var p = String(path).replace(/\\/g, "/")
        return "file://" + (p.charAt(0) === "/" ? "" : "/") + p
    }

    // The reverse of fileUrl(): a "file://" url as a plain local path ("file:///C:/a/b" -> "C:/a/b").
    function localPath(url) {
        var s = String(url)
        if (s.indexOf("file://") !== 0)
            return s
        s = s.substring(7)
        if (/^\/[A-Za-z]:/.test(s))
            s = s.substring(1)
        return decodeURIComponent(s)
    }

    // The friends in three groups -- playing, online, offline -- keeping the server's order (by name) inside each.
    function friendsGrouped(friends) {
        var groups = { playing: [], online: [], offline: [] }
        for (var i = 0; i < (friends || []).length; ++i) {
            var state = friends[i].state
            groups[state === "playing" || state === "online" ? state : "offline"].push(friends[i])
        }
        return groups
    }

    // The second line of a friend's row: { text, suffix, tone }. On a server it is the server's name from @p place
    // (FriendPlaces.of(), optional) or else the host, and the game version; singleplayer and a hidden server say so;
    // online and offline use FriendsDisplay's wording. Only the offline line is "faint", the rest "secondary".
    function friendStatus(friend, place) {
        if (friend.state === "playing") {
            if (friend.worldType === "server" && friend.host)
                return { text: place && place.name ? place.name : friend.host, suffix: friend.version,
                         tone: "secondary" }
            return {
                text: friend.worldType === "singleplayer" ? qsTr("Singleplayer")
                    : friend.worldType === "server" ? qsTr("On a server") : qsTr("Playing"),
                suffix: friend.version,
                tone: "secondary"
            }
        }
        return { text: friend.statusText, suffix: "", tone: friend.state === "offline" ? "faint" : "secondary" }
    }

    // A friend's state in a word or two, for their card: { text, tone }. "In game" and "In Kite" are "success",
    // last seen or offline "tertiary".
    function friendPresence(friend) {
        if (friend.state === "playing")
            return { text: qsTr("In game"), tone: "success" }
        return { text: friend.statusText || "", tone: friend.state === "online" ? "success" : "tertiary" }
    }

    // "1 unread message", "3 unread messages"; "" for none.
    function unreadMessages(count) {
        return count > 0 ? (count === 1 ? qsTr("1 unread message") : qsTr("%1 unread messages").arg(count)) : ""
    }

    // How long an invite still lasts, compactly: "8 min left" (its expiresAt is an ISO time; never under a minute).
    readonly property int minuteMs: 60 * 1000
    // @p now (ms since the epoch) is optional: a binding that passes a ticking clock counts down.
    function inviteTimeLeft(invite, now) {
        var minutes = Math.max(1, Math.ceil((Date.parse(invite.expiresAt) - (now || Date.now())) / root.minuteMs))
        return qsTr("%1 min left").arg(minutes)
    }

    // The same line on one line, for a tip or a card.
    function friendStatusLine(friend, place) {
        var status = root.friendStatus(friend, place)
        return status.suffix ? status.text + " · " + status.suffix : status.text
    }

    // Join shows for a visible server whose player lets others join; the game version picks the instance, so it must
    // be known too.
    function friendJoinable(friend) {
        return friend.state === "playing" && friend.worldType === "server"
                && !!friend.host && !!friend.version && friend.joinable === true
    }

    // A visible server whose player turned joining off (a row says so).
    function friendJoinOff(friend) {
        return friend.state === "playing" && friend.worldType === "server"
                && !!friend.host && friend.joinOff === true
    }

    readonly property string friendsNeedAccount: qsTr("Friends needs a Minecraft account that is signed in with Microsoft.")

    // Where a friends view is: "off" (no controller, or turned off),
    // "noAccount", "signingIn", "error" or "ready". The controller's own
    // "off" while it is enabled is only the moment before it connects.
    function friendsPhase(controller) {
        if (!controller || !controller.enabled)
            return "off"
        return controller.state === "off" ? "signingIn" : controller.state
    }

    // Signed out, or failed in a way only signing in again fixes: the view offers "Sign in", not "Try again".
    function friendsSignInFirst(controller) {
        var phase = root.friendsPhase(controller)
        return phase === "noAccount" || (phase === "error" && controller.needsSignIn)
    }

    // A failed poll while the lists are already there: they stay on screen under a bar instead of the full-page
    // error.
    function friendsStale(controller) {
        return root.friendsPhase(controller) === "error" && !root.friendsSignInFirst(controller)
                && controller.loaded
    }

    // Whether the lists have anyone in them (friends and requests; with @p withBlocked, blocked people too). False
    // before they are read.
    function friendsHasAnyone(controller, withBlocked) {
        return !!controller && controller.friends.length + controller.incoming.length
                + controller.outgoing.length + (withBlocked ? controller.blocked.length : 0) > 0
    }

    // Time of day the lists were last read, in the user's format, or "".
    function friendsUpdatedAt(controller) {
        var at = controller ? controller.updatedAtMs : 0
        // (Locale.ShortFormat given to Qt.formatTime() reads as Qt.ISODate:
        // "17:30:08" whatever the language.)
        return at > 0 ? new Date(at).toLocaleTimeString(Qt.locale(), Locale.ShortFormat) : ""
    }

    // Who of `friends` (ServerPinger.friendsOn) is on a server, in words: "Alex is here", "Alex and Kai are here",
    // "Alex and 2 others are here"; "" for no one.
    function friendsHere(friends) {
        var names = (friends || []).map((one) => one.name)
        if (names.length === 0)
            return ""
        if (names.length === 1)
            return qsTr("%1 is here").arg(names[0])
        if (names.length === 2)
            return qsTr("%1 and %2 are here").arg(names[0]).arg(names[1])
        return qsTr("%1 and %2 others are here").arg(names[0]).arg(names.length - 1)
    }

    // Players online in the user's locale ("28,058", "28.058"); "" while a server hides them (-1).
    function playerCount(online) {
        return online >= 0 ? Number(online).toLocaleString(Qt.locale(), "f", 0) : ""
    }

    // Where Join sends the game: the host, and the port unless it is the game's own. An IPv6 host goes in brackets,
    // as parse() reads server addresses.
    readonly property int defaultServerPort: 25565
    function friendAddress(friend) {
        var host = friend.host.indexOf(":") >= 0 ? "[" + friend.host + "]" : friend.host
        return friend.port > 0 && friend.port !== root.defaultServerPort
                ? host + ":" + friend.port : host
    }
}
