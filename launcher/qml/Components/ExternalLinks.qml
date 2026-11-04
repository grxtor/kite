// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton

import QtQuick

/*
 * The one way the launcher hands a link to the browser or the mail app: call open(item, url) instead of
 * Qt.openUrlExternally. Only http, https and mailto links go out; any other scheme (a file, an app's own scheme) and
 * anything that does not parse is refused. A link that came from the network or from files (a mod's homepage, a
 * changelog, a review, a plugin's source) first asks, naming the host it leads to; the person can stop that for web
 * links (the ConfirmWebLinks setting). `trusted` skips the question for the links Kite ships itself (Help, About).
 *
 * Local files the launcher made itself (screenshots, folders) are not links and keep their own way out.
 */
QtObject {
    id: root

    readonly property var schemes: ["http", "https", "mailto"]
    readonly property string askSetting: "ConfirmWebLinks"

    readonly property Component confirmation: Component {
        ExternalLinkDialog {}
    }

    // {href, scheme, destination, web} for a link that may go out, or null. `destination` is what the question
    // names: a web link's host (punycode, so a lookalike host shows as one) or a mail link's address.
    function parse(url) {
        let parsed = null
        try {
            parsed = new URL(String(url === undefined || url === null ? "" : url).trim())
        } catch (e) {
            return null
        }
        const scheme = parsed.protocol.replace(/:$/, "").toLowerCase()
        if (root.schemes.indexOf(scheme) < 0)
            return null
        const web = scheme !== "mailto"
        const destination = web ? parsed.hostname : parsed.pathname
        if (destination.length === 0)
            return null
        return { href: parsed.href, scheme: scheme, destination: destination, web: web }
    }

    // The topmost item above `from`: the dialog lives there, so it outlasts a menu or a row that closes.
    function hostOf(from) {
        let item = from
        while (item && item.parent)
            item = item.parent
        return item
    }

    // Opens `url`, asking first unless it is `trusted` or a web link the person stopped asking about. `from` is any
    // item in the window the question shows in. False when the link was refused.
    function open(from, url, trusted) {
        const link = root.parse(url)
        if (!link) {
            console.warn("ExternalLinks: refusing a link that is not http, https or mailto")
            return false
        }
        if (trusted === true || !root.asks(link)) {
            Qt.openUrlExternally(link.href)
            return true
        }
        const host = root.hostOf(from)
        if (!host) {
            console.warn("ExternalLinks: no window to ask in")
            return false
        }
        const dialog = root.confirmation.createObject(host, {
            destination: link.destination,
            address: link.href,
            web: link.web
        })
        dialog.confirmed.connect((stopAsking) => {
            if (stopAsking && link.web)
                SettingsStore.setValue(root.askSetting, false)
            Qt.openUrlExternally(link.href)
        })
        dialog.closed.connect(() => dialog.destroy())
        dialog.open()
        return true
    }

    // Whether `link` asks first: a mail link always, a web link unless the setting is off; without settings, all do.
    function asks(link) {
        if (!link.web || !SettingsStore.adapter)
            return true
        return SettingsStore.bool(root.askSetting)
    }
}
