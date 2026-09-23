// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
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
        return Qt.formatDate(new Date(ms), Qt.locale().dateFormat(Locale.ShortFormat))
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
}
