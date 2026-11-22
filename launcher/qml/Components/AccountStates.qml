// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton

import QtQuick

/*
 * What an account's state (AccountList `stateKey`) asks of the player: the three states that stop online play
 * -- "expired", "errored", "gone" -- with their tone, a sentence that says what is wrong and the action that
 * mends it. The profile button, its menu and the Accounts page all word it from here.
 */
QtObject {
    function needsAttention(key) {
        return key === "expired" || key === "errored" || key === "gone"
    }

    function tone(key) {
        return key === "gone" ? "danger" : "warning"
    }

    function summary(key) {
        switch (key) {
        case "expired": return qsTr("Your sign-in has expired. Sign in again to play online.")
        case "errored": return qsTr("Kite could not check this account. Sign in again to play online.")
        case "gone": return qsTr("This account no longer exists. Remove it or use another account.")
        }
        return ""
    }

    // Signing in again cannot bring back an account that is gone; that one is sorted out on the Accounts page.
    function canSignInAgain(key) {
        return key === "expired" || key === "errored"
    }
}
