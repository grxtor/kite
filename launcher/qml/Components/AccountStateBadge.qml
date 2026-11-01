// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The state of a Microsoft account (AccountsController `stateKey`) as a StatusBadge: online green, expired
 * amber, errored or gone red, anything else neutral. `text` is the state's own wording.
 */
StatusBadge {
    property string stateKey: ""

    tone: stateKey === "online" ? "success"
        : stateKey === "errored" || stateKey === "gone" ? "danger"
        : stateKey === "expired" ? "warning" : "neutral"
}
